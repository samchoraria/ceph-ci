// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
#include "PG.h"

#include "include/types.h"
#include "messages/MWatchNotify.h"

#include <map>

#include "OSD.h"
#include "PrimaryLogPG.h"
#include "Watch.h"
#include "Session.h"

#include "common/config.h"

struct CancelableContext : public Context {
  virtual void cancel() = 0;
};

#define dout_context osd->cct
#define dout_subsys ceph_subsys_osd
#undef dout_prefix
#define dout_prefix _prefix(_dout, this)

static ostream& _prefix(
  std::ostream* _dout,
  Notify *notify) {
  return notify->gen_dbg_prefix(*_dout);
}

Notify::Notify(
  ConnectionRef client,
  uint64_t client_gid,
  bufferlist &payload,
  uint32_t timeout,
  uint64_t cookie,
  uint64_t notify_id,
  uint64_t version,
  OSDService *osd)
  : client(client), client_gid(client_gid),
    complete(false),
    discarded(false),
    timed_out(false),
    payload(payload),
    timeout(timeout),
    cookie(cookie),
    notify_id(notify_id),
    version(version),
    osd(osd),
    cb(NULL),
    lock("Notify::lock") {}

NotifyRef Notify::makeNotifyRef(
  ConnectionRef client,
  uint64_t client_gid,
  bufferlist &payload,
  uint32_t timeout,
  uint64_t cookie,
  uint64_t notify_id,
  uint64_t version,
  OSDService *osd) {
  NotifyRef ret(
    new Notify(
      client, client_gid,
      payload, timeout,
      cookie, notify_id,
      version, osd));
  ret->set_self(ret);
  return ret;
}

class NotifyTimeoutCB : public CancelableContext {
  NotifyRef notif;
  bool canceled; // protected by notif lock
public:
  explicit NotifyTimeoutCB(NotifyRef notif) : notif(notif), canceled(false) {}
  void finish(int) override {
    notif->osd->watch_lock.Unlock();
    notif->lock.Lock();
    if (!canceled)
      notif->do_timeout(); // drops lock
    else
      notif->lock.Unlock();
    notif->osd->watch_lock.Lock();
  }
  void cancel() override {
    assert(notif->lock.is_locked_by_me());
    canceled = true;
  }
};

void Notify::do_timeout()
{
  assert(lock.is_locked_by_me());
  dout(10) << "timeout" << dendl;
  cb = nullptr;
  if (is_discarded()) {
    lock.Unlock();
    return;
  }

  timed_out = true;         // we will send the client an error code
  maybe_complete_notify();
  assert(complete);
  set<WatchRef> _watchers;
  _watchers.swap(watchers);
  lock.Unlock();

  for (set<WatchRef>::iterator i = _watchers.begin();
       i != _watchers.end();
       ++i) {
    boost::intrusive_ptr<PrimaryLogPG> pg((*i)->get_pg());
    pg->lock();
    if (!(*i)->is_discarded()) {
      (*i)->cancel_notify(self.lock());
    }
    pg->unlock();
  }
}

void Notify::register_cb()
{
  assert(lock.is_locked_by_me());
  {
    osd->watch_lock.Lock();
    cb = new NotifyTimeoutCB(self.lock());
    if (!osd->watch_timer.add_event_after(timeout, cb)) {
      cb = nullptr;
    }
    osd->watch_lock.Unlock();
  }
}

void Notify::unregister_cb()
{
  assert(lock.is_locked_by_me());
  if (!cb)
    return;
  cb->cancel();
  {
    osd->watch_lock.Lock();
    osd->watch_timer.cancel_event(cb);
    cb = nullptr;
    osd->watch_lock.Unlock();
  }
}

void Notify::start_watcher(WatchRef watch)
{
  Mutex::Locker l(lock);
  dout(10) << "start_watcher" << dendl;
  watchers.insert(watch);
}

void Notify::complete_watcher(WatchRef watch, bufferlist& reply_bl)
{
  Mutex::Locker l(lock);
  dout(10) << "complete_watcher" << dendl;
  if (is_discarded())
    return;
  assert(watchers.count(watch));
  watchers.erase(watch);
  notify_replies.insert(make_pair(make_pair(watch->get_watcher_gid(),
					    watch->get_cookie()),
				  reply_bl));
  maybe_complete_notify();
}

void Notify::complete_watcher_remove(WatchRef watch)
{
  Mutex::Locker l(lock);
  dout(10) << __func__ << dendl;
  if (is_discarded())
    return;
  assert(watchers.count(watch));
  watchers.erase(watch);
  maybe_complete_notify();
}

void Notify::maybe_complete_notify()
{
  dout(10) << "maybe_complete_notify -- "
	   << watchers.size()
	   << " in progress watchers " << dendl;
  if (watchers.empty() || timed_out) {
    // prepare reply
    bufferlist bl;
    encode(notify_replies, bl);
    list<pair<uint64_t,uint64_t> > missed;
    for (set<WatchRef>::iterator p = watchers.begin(); p != watchers.end(); ++p) {
      missed.push_back(make_pair((*p)->get_watcher_gid(),
				 (*p)->get_cookie()));
    }
    encode(missed, bl);

    bufferlist empty;
    MWatchNotify *reply(new MWatchNotify(cookie, version, notify_id,
					 CEPH_WATCH_EVENT_NOTIFY_COMPLETE, empty));
    reply->notifier_gid = client_gid;
    reply->set_data(bl);
    if (timed_out)
      reply->return_code = -ETIMEDOUT;
    client->send_message(reply);
    unregister_cb();

    complete = true;
  }
}

void Notify::discard()
{
  Mutex::Locker l(lock);
  discarded = true;
  unregister_cb();
  watchers.clear();
}

void Notify::init()
{
  Mutex::Locker l(lock);
  register_cb();
  maybe_complete_notify();
}

#define dout_subsys ceph_subsys_osd
#undef dout_prefix
#define dout_prefix _prefix(_dout, watch.get())

static ostream& _prefix(
  std::ostream* _dout,
  Watch *watch) {
  return watch->gen_dbg_prefix(*_dout);
}

class HandleWatchTimeout : public CancelableContext {
  WatchRef watch;
public:
  bool canceled; // protected by watch->pg->lock
  explicit HandleWatchTimeout(WatchRef watch) : watch(watch), canceled(false) {}
  void cancel() override {
    canceled = true;
  }
  void finish(int) override { ceph_abort(); /* not used */ }
  void complete(int) override {
    OSDService *osd(watch->osd);
    ldout(osd->cct, 10) << "HandleWatchTimeout" << dendl;
    boost::intrusive_ptr<PrimaryLogPG> pg(watch->pg);
    osd->watch_lock.Unlock();
    pg->lock();
    watch->cb = nullptr;
    if (!watch->is_discarded() && !canceled)
      watch->pg->handle_watch_timeout(watch);
    delete this; // ~Watch requires pg lock!
    pg->unlock();
    osd->watch_lock.Lock();
  }
};

class HandleDelayedWatchTimeout : public CancelableContext {
  WatchRef watch;
public:
  bool canceled;
  explicit HandleDelayedWatchTimeout(WatchRef watch) : watch(watch), canceled(false) {}
  void cancel() override {
    canceled = true;
  }
  void finish(int) override {
    OSDService *osd(watch->osd);
    dout(10) << "HandleWatchTimeoutDelayed" << dendl;
    assert(watch->pg->is_locked());
    watch->cb = nullptr;
    if (!watch->is_discarded() && !canceled)
      watch->pg->handle_watch_timeout(watch);
  }
};

#define dout_subsys ceph_subsys_osd
#undef dout_prefix
#define dout_prefix _prefix(_dout, this)

std::ostream& Watch::gen_dbg_prefix(std::ostream& out) {
  return pg->gen_prefix(out) << " -- Watch("
      << make_pair(cookie, entity) << ") ";
}

Watch::Watch(
  PrimaryLogPG *pg,
  OSDService *osd,
  ObjectContextRef obc,
  uint32_t timeout,
  uint64_t cookie,
  entity_name_t entity,
  const entity_addr_t &addr)
  : cb(NULL),
    osd(osd),
    pg(pg),
    obc(obc),
    timeout(timeout),
    cookie(cookie),
    addr(addr),
    will_ping(false),
    entity(entity),
    discarded(false) {
  dout(10) << "Watch()" << dendl;
}

Watch::~Watch() {
  dout(10) << "~Watch" << dendl;
  // users must have called remove() or discard() prior to this point
  assert(!obc);
  assert(!conn);
}

bool Watch::connected() { return !!conn; }

Context *Watch::get_delayed_cb()
{
  assert(!cb);
  cb = new HandleDelayedWatchTimeout(self.lock());
  return cb;
}

void Watch::register_cb()
{
  Mutex::Locker l(osd->watch_lock);
  if (cb) {
    dout(15) << "re-registering callback, timeout: " << timeout << dendl;
    cb->cancel();
    osd->watch_timer.cancel_event(cb);
  } else {
    dout(15) << "registering callback, timeout: " << timeout << dendl;
  }
  cb = new HandleWatchTimeout(self.lock());
  if (!osd->watch_timer.add_event_after(timeout, cb)) {
    cb = nullptr;
  }
}

void Watch::unregister_cb()
{
  dout(15) << "unregister_cb" << dendl;
  if (!cb)
    return;
  dout(15) << "actually registered, cancelling" << dendl;
  cb->cancel();
  {
    Mutex::Locker l(osd->watch_lock);
    osd->watch_timer.cancel_event(cb); // harmless if not registered with timer
  }
  cb = nullptr;
}

void Watch::got_ping(utime_t t)
{
  last_ping = t;
  if (conn) {
    register_cb();
  }
}

void Watch::connect(ConnectionRef con, bool _will_ping)
{
  if (conn == con) {
    dout(10) << __func__ << " con " << con << " - already connected" << dendl;
    return;
  }
  dout(10) << __func__ << " con " << con << dendl;
  conn = con;
  will_ping = _will_ping;
  auto priv = con->get_priv();
  if (priv) {
    auto sessionref = static_cast<Session*>(priv.get());
    sessionref->wstate.addWatch(self.lock());
    priv.reset();
    for (map<uint64_t, NotifyRef>::iterator i = in_progress_notifies.begin();
	 i != in_progress_notifies.end();
	 ++i) {
      send_notify(i->second);
    }
  }
  if (will_ping) {
    last_ping = ceph_clock_now();
    register_cb();
  } else {
    unregister_cb();
  }
}

void Watch::disconnect()
{
  dout(10) << "disconnect (con was " << conn << ")" << dendl;
  conn = ConnectionRef();
  if (!will_ping)
    register_cb();
}

void Watch::discard()
{
  dout(10) << "discard" << dendl;
  for (map<uint64_t, NotifyRef>::iterator i = in_progress_notifies.begin();
       i != in_progress_notifies.end();
       ++i) {
    i->second->discard();
  }
  discard_state();
}

void Watch::discard_state()
{
  assert(pg->is_locked());
  assert(!discarded);
  assert(obc);
  in_progress_notifies.clear();
  unregister_cb();
  discarded = true;
  if (conn) {
    if (auto priv = conn->get_priv(); priv) {
      auto session = static_cast<Session*>(priv.get());
      session->wstate.removeWatch(self.lock());
    }
    conn = ConnectionRef();
  }
  obc = ObjectContextRef();
}

bool Watch::is_discarded() const
{
  return discarded;
}

void Watch::remove(bool send_disconnect)
{
  dout(10) << "remove" << dendl;
  if (send_disconnect && conn) {
    bufferlist empty;
    MWatchNotify *reply(new MWatchNotify(cookie, 0, 0,
					 CEPH_WATCH_EVENT_DISCONNECT, empty));
    conn->send_message(reply);
  }
  for (map<uint64_t, NotifyRef>::iterator i = in_progress_notifies.begin();
       i != in_progress_notifies.end();
       ++i) {
    i->second->complete_watcher_remove(self.lock());
  }
  discard_state();
}

void Watch::start_notify(NotifyRef notif)
{
  assert(in_progress_notifies.find(notif->notify_id) ==
	 in_progress_notifies.end());
  if (will_ping) {
    utime_t cutoff = ceph_clock_now();
    cutoff.sec_ref() -= timeout;
    if (last_ping < cutoff) {
      dout(10) << __func__ << " " << notif->notify_id
	       << " last_ping " << last_ping << " < cutoff " << cutoff
	       << ", disconnecting" << dendl;
      disconnect();
      return;
    }
  }
  dout(10) << "start_notify " << notif->notify_id << dendl;
  in_progress_notifies[notif->notify_id] = notif;
  notif->start_watcher(self.lock());
  if (connected())
    send_notify(notif);
}

void Watch::cancel_notify(NotifyRef notif)
{
  dout(10) << "cancel_notify " << notif->notify_id << dendl;
  in_progress_notifies.erase(notif->notify_id);
}

void Watch::send_notify(NotifyRef notif)
{
  dout(10) << "send_notify" << dendl;
  MWatchNotify *notify_msg = new MWatchNotify(
    cookie, notif->version, notif->notify_id,
    CEPH_WATCH_EVENT_NOTIFY, notif->payload);
  notify_msg->notifier_gid = notif->client_gid;
  conn->send_message(notify_msg);
}

void Watch::notify_ack(uint64_t notify_id, bufferlist& reply_bl)
{
  dout(10) << "notify_ack" << dendl;
  map<uint64_t, NotifyRef>::iterator i = in_progress_notifies.find(notify_id);
  if (i != in_progress_notifies.end()) {
    i->second->complete_watcher(self.lock(), reply_bl);
    in_progress_notifies.erase(i);
  }
}

WatchRef Watch::makeWatchRef(
  PrimaryLogPG *pg, OSDService *osd,
  ObjectContextRef obc, uint32_t timeout, uint64_t cookie, entity_name_t entity, const entity_addr_t& addr)
{
  WatchRef ret(new Watch(pg, osd, obc, timeout, cookie, entity, addr));
  ret->set_self(ret);
  return ret;
}

void WatchConState::addWatch(WatchRef watch)
{
  Mutex::Locker l(lock);
  watches.insert(watch);
}

void WatchConState::removeWatch(WatchRef watch)
{
  Mutex::Locker l(lock);
  watches.erase(watch);
}

void WatchConState::reset(Connection *con)
{
  set<WatchRef> _watches;
  {
    Mutex::Locker l(lock);
    _watches.swap(watches);
  }
  for (set<WatchRef>::iterator i = _watches.begin();
       i != _watches.end();
       ++i) {
    boost::intrusive_ptr<PrimaryLogPG> pg((*i)->get_pg());
    pg->lock();
    if (!(*i)->is_discarded()) {
      if ((*i)->is_connected(con)) {
	(*i)->disconnect();
      } else {
	lgeneric_derr(cct) << __func__ << " not still connected to " << (*i) << dendl;
      }
    }
    pg->unlock();
  }
}
