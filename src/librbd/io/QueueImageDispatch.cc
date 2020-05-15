// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include "librbd/io/QueueImageDispatch.h"
#include "common/dout.h"
#include "common/Cond.h"
#include "common/WorkQueue.h"
#include "librbd/ImageCtx.h"
#include "librbd/Utils.h"
#include "librbd/io/AioCompletion.h"
#include "librbd/io/ImageDispatchSpec.h"

#define dout_subsys ceph_subsys_rbd
#undef dout_prefix
#define dout_prefix *_dout << "librbd::io::QueueImageDispatch: " << this \
                           << " " << __func__ << ": "

namespace librbd {
namespace io {

template <typename I>
QueueImageDispatch<I>::QueueImageDispatch(I* image_ctx)
  : m_image_ctx(image_ctx),
    m_lock(ceph::make_shared_mutex(
      util::unique_lock_name("librbd::io::QueueImageDispatch::m_lock",
                             this))) {
  auto cct = m_image_ctx->cct;
  ldout(cct, 5) << "ictx=" << image_ctx << dendl;
}

template <typename I>
void QueueImageDispatch<I>::shut_down(Context* on_finish) {
  on_finish->complete(0);
}

template <typename I>
int QueueImageDispatch<I>::block_writes() {
  C_SaferCond cond_ctx;
  block_writes(&cond_ctx);
  return cond_ctx.wait();
}

template <typename I>
void QueueImageDispatch<I>::block_writes(Context *on_blocked) {
  ceph_assert(ceph_mutex_is_locked(m_image_ctx->owner_lock));
  auto cct = m_image_ctx->cct;

  // ensure onwer lock is not held after block_writes completes
  on_blocked = util::create_async_context_callback(
    *m_image_ctx, on_blocked);

  {
    std::unique_lock locker{m_lock};
    ++m_write_blockers;
    ldout(cct, 5) << m_image_ctx << ", "
                  << "num=" << m_write_blockers << dendl;
    if (!m_write_blocker_contexts.empty() || !m_in_flight_write_tids.empty()) {
      ldout(cct, 5) << "waiting for in-flight writes to complete: "
                    << "write_tids=" << m_in_flight_write_tids << dendl;
      m_write_blocker_contexts.push_back(on_blocked);
      return;
    }
  }

  // ensure that all in-flight IO is flushed
  flush_image(on_blocked);
};

template <typename I>
void QueueImageDispatch<I>::unblock_writes() {
  auto cct = m_image_ctx->cct;

  Contexts waiter_contexts;
  Contexts dispatch_contexts;
  {
    std::unique_lock locker{m_lock};
    ceph_assert(m_write_blockers > 0);
    --m_write_blockers;

    ldout(cct, 5) << m_image_ctx << ", "
                  << "num=" << m_write_blockers << dendl;
    if (m_write_blockers == 0) {
      std::swap(waiter_contexts, m_unblocked_write_waiter_contexts);
      std::swap(dispatch_contexts, m_on_dispatches);
    }
  }

  for (auto ctx : waiter_contexts) {
    ctx->complete(0);
  }

  for (auto ctx : dispatch_contexts) {
    ctx->complete(0);
  }
}

template <typename I>
void QueueImageDispatch<I>::wait_on_writes_unblocked(Context *on_unblocked) {
  ceph_assert(ceph_mutex_is_locked(m_image_ctx->owner_lock));
  auto cct = m_image_ctx->cct;

  {
    std::unique_lock locker{m_lock};
    ldout(cct, 20) << m_image_ctx << ", "
                   << "write_blockers=" << m_write_blockers << dendl;
    if (!m_unblocked_write_waiter_contexts.empty() || m_write_blockers > 0) {
      m_unblocked_write_waiter_contexts.push_back(on_unblocked);
      return;
    }
  }

  on_unblocked->complete(0);
}

template <typename I>
bool QueueImageDispatch<I>::read(
    AioCompletion* aio_comp, Extents &&image_extents, ReadResult &&read_result,
    int op_flags, const ZTracer::Trace &parent_trace, uint64_t tid,
    std::atomic<uint32_t>* image_dispatch_flags,
    DispatchResult* dispatch_result, Context* on_dispatched) {
  auto cct = m_image_ctx->cct;
  ldout(cct, 20) << "tid=" << tid << dendl;

  return enqueue(true, tid, dispatch_result, on_dispatched);
}

template <typename I>
bool QueueImageDispatch<I>::write(
    AioCompletion* aio_comp, Extents &&image_extents, bufferlist &&bl,
    int op_flags, const ZTracer::Trace &parent_trace, uint64_t tid,
    std::atomic<uint32_t>* image_dispatch_flags,
    DispatchResult* dispatch_result, Context* on_dispatched) {
  auto cct = m_image_ctx->cct;
  ldout(cct, 20) << "tid=" << tid << dendl;

  return enqueue(false, tid, dispatch_result, on_dispatched);
}

template <typename I>
bool QueueImageDispatch<I>::discard(
    AioCompletion* aio_comp, Extents &&image_extents,
    uint32_t discard_granularity_bytes, const ZTracer::Trace &parent_trace,
    uint64_t tid, std::atomic<uint32_t>* image_dispatch_flags,
    DispatchResult* dispatch_result, Context* on_dispatched) {
  auto cct = m_image_ctx->cct;
  ldout(cct, 20) << "tid=" << tid << dendl;

  return enqueue(false, tid, dispatch_result, on_dispatched);
}

template <typename I>
bool QueueImageDispatch<I>::write_same(
    AioCompletion* aio_comp, Extents &&image_extents, bufferlist &&bl,
    int op_flags, const ZTracer::Trace &parent_trace, uint64_t tid,
    std::atomic<uint32_t>* image_dispatch_flags,
    DispatchResult* dispatch_result, Context* on_dispatched) {
  auto cct = m_image_ctx->cct;
  ldout(cct, 20) << "tid=" << tid << dendl;

  return enqueue(false, tid, dispatch_result, on_dispatched);
}

template <typename I>
bool QueueImageDispatch<I>::compare_and_write(
    AioCompletion* aio_comp, Extents &&image_extents, bufferlist &&cmp_bl,
    bufferlist &&bl, uint64_t *mismatch_offset, int op_flags,
    const ZTracer::Trace &parent_trace, uint64_t tid,
    std::atomic<uint32_t>* image_dispatch_flags,
    DispatchResult* dispatch_result, Context* on_dispatched) {
  auto cct = m_image_ctx->cct;
  ldout(cct, 20) << "tid=" << tid << dendl;

  return enqueue(false, tid, dispatch_result, on_dispatched);
}

template <typename I>
bool QueueImageDispatch<I>::flush(
    AioCompletion* aio_comp, FlushSource flush_source,
    const ZTracer::Trace &parent_trace, uint64_t tid,
    std::atomic<uint32_t>* image_dispatch_flags,
    DispatchResult* dispatch_result, Context* on_dispatched) {
  auto cct = m_image_ctx->cct;
  ldout(cct, 20) << "tid=" << tid << dendl;

  if (flush_source != FLUSH_SOURCE_USER) {
    return false;
  }

  return enqueue(false, tid, dispatch_result, on_dispatched);
}

template <typename I>
void QueueImageDispatch<I>::handle_finished(int r, uint64_t tid) {
  auto cct = m_image_ctx->cct;
  ldout(cct, 20) << "r=" << r << ", tid=" << tid << dendl;

  std::unique_lock locker{m_lock};
  auto it = m_in_flight_write_tids.find(tid);
  if (it == m_in_flight_write_tids.end()) {
    // assumed to be a read op
    return;
  }
  m_in_flight_write_tids.erase(it);

  Contexts write_blocker_contexts;
  if (m_in_flight_write_tids.empty()) {
    std::swap(write_blocker_contexts, m_write_blocker_contexts);
  }
  locker.unlock();

  for (auto ctx : write_blocker_contexts) {
    ctx->complete(0);
  }
}

template <typename I>
bool QueueImageDispatch<I>::enqueue(
    bool read_op, uint64_t tid, DispatchResult* dispatch_result,
    Context* on_dispatched) {
  std::unique_lock locker{m_lock};
  if (!read_op) {
    if (m_write_blockers > 0 || !m_on_dispatches.empty()) {
      *dispatch_result = DISPATCH_RESULT_RESTART;
      m_on_dispatches.push_back(on_dispatched);
      return true;
    }

    m_in_flight_write_tids.insert(tid);
  }
  locker.unlock();

  if (!m_image_ctx->non_blocking_aio) {
    return false;
  }

  *dispatch_result = DISPATCH_RESULT_CONTINUE;
  m_image_ctx->op_work_queue->queue(on_dispatched, 0);
  return true;
}

template <typename I>
void QueueImageDispatch<I>::flush_image(Context* on_finish) {
  auto aio_comp = AioCompletion::create_and_start(
    on_finish, util::get_image_ctx(m_image_ctx), librbd::io::AIO_TYPE_FLUSH);
  auto req = ImageDispatchSpec<I>::create_flush(
    *m_image_ctx, IMAGE_DISPATCH_LAYER_QUEUE, aio_comp, FLUSH_SOURCE_INTERNAL,
    {});
  req->send();
}

} // namespace io
} // namespace librbd

template class librbd::io::QueueImageDispatch<librbd::ImageCtx>;
