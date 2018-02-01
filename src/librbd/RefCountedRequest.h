// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#ifndef CEPH_LIBRBD_REF_COUNTED_REQUEST_H
#define CEPH_LIBRBD_REF_COUNTED_REQUEST_H

#include <atomic>
#include "common/dout.h"
#include "librbd/ImageCtx.h"
#include "librbd/Utils.h"

#include "include/assert.h"

namespace librbd {

template <typename ImageCtxT = ImageCtx>
class RefCountedRequest {
public:
  RefCountedRequest(ImageCtxT &image_ctx)
    : m_ref(1),
      m_image_ctx(image_ctx) {
  }

  virtual ~RefCountedRequest() {
  }

  RefCountedRequest *get() {
    int v = ++m_ref;
    if (m_image_ctx.cct) {
      lsubdout(m_image_ctx.cct, rbd, 20) << "RefCountedRequest::get " << this << " "
                                          << (v - 1) << " -> " << v << dendl;
    }

    return this;
  }
  const RefCountedRequest *get() const {
    int v = ++m_ref;
    if (m_image_ctx.cct) {
      lsubdout(m_image_ctx.cct, rbd, 20) << "RefCountedRequest::get " << this << " "
                                          << (v - 1) << " -> " << v << dendl;
    }

    return this;
  }

  void put() {
    assert(m_ref > 0);
    int v = --m_ref;
    if (m_image_ctx.cct) {
      lsubdout(m_image_ctx.cct, rbd, 20) << "RefCountedRequest::put " << this << " "
                                          << (v + 1) << " -> " << v << dendl;
    }
    if (v == 0) {
      wait_for_completion();
      delete this;
    }
  }

protected:
  virtual void wait_for_completion() {
  }

private:
  mutable std::atomic<uint64_t> m_ref;
  ImageCtxT &m_image_ctx;
};

} // namespace librbd

#endif // CEPH_LIBRBD_REF_COUNTED_REQUEST_H
