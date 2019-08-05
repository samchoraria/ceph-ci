// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include <cstdarg>
#include <cstring>
#include "common/ceph_context.h"
#include "common/ceph_releases.h"
#include "common/config.h"
#include "common/debug.h"

#include "crimson/osd/ops_executer.h"
#include "crimson/osd/pg_backend.h"

#include "objclass/objclass.h"
#include "osd/ClassHandler.h"

#include "auth/Crypto.h"
#include "common/armor.h"

int cls_call(cls_method_context_t hctx, const char *cls, const char *method,
                                 char *indata, int datalen,
                                 char **outdata, int *outdatalen)
{
// FIXME, HACK: this is for testing only. Let's use dynamic linker to verify
// our depedencies
  return 0;
}

int cls_getxattr(cls_method_context_t hctx,
                 const char *name,
                 char **outdata,
                 int *outdatalen)
{
  return 0;
}

int cls_setxattr(cls_method_context_t hctx,
                 const char *name,
                 const char *value,
                 int val_len)
{
  return 0;
}

int cls_read(cls_method_context_t hctx,
             int ofs, int len,
             char **outdata,
             int *outdatalen)
{
  return 0;
}

int cls_get_request_origin(cls_method_context_t hctx, entity_inst_t *origin)
{
  return 0;
}

int cls_cxx_create(cls_method_context_t hctx, bool exclusive)
{
  return 0;
}

int cls_cxx_remove(cls_method_context_t hctx)
{
  return 0;
}

int cls_cxx_stat(cls_method_context_t hctx, uint64_t *size, time_t *mtime)
{
  OSDOp op;//{CEPH_OSD_OP_STAT};
  op.op.op = CEPH_OSD_OP_STAT;

  // we're blocking here which presumes execution in Seastar's thread.
  try {
    reinterpret_cast<ceph::osd::OpsExecuter*>(hctx)->do_osd_op(op).get();
  } catch (std::system_error& e) {
    return -e.code().value();
  }

  utime_t ut;
  uint64_t s;
  try {
    auto iter = op.outdata.cbegin();
    decode(s, iter);
    decode(ut, iter);
  } catch (buffer::error& err) {
    return -EIO;
  }
  if (size) {
    *size = s;
  }
  if (mtime) {
    *mtime = ut.sec();
  }
  return 0;
}

int cls_cxx_stat2(cls_method_context_t hctx,
                  uint64_t *size,
                  ceph::real_time *mtime)
{
  return 0;
}

int cls_cxx_read2(cls_method_context_t hctx,
                  int ofs,
                  int len,
                  bufferlist *outbl,
                  uint32_t op_flags)
{
  OSDOp op;
  op.op.op = CEPH_OSD_OP_SYNC_READ;
  op.op.extent.offset = ofs;
  op.op.extent.length = len;
  op.op.flags = op_flags;
  try {
    reinterpret_cast<ceph::osd::OpsExecuter*>(hctx)->do_osd_op(op).get();
  } catch (std::system_error& e) {
    return -e.code().value();
  }
  outbl->claim(op.outdata);
  return outbl->length();
}

int cls_cxx_write2(cls_method_context_t hctx,
                   int ofs,
                   int len,
                   bufferlist *inbl,
                   uint32_t op_flags)
{
  return 0;
}

int cls_cxx_write_full(cls_method_context_t hctx, bufferlist * const inbl)
{
  OSDOp op;
  op.op.op = CEPH_OSD_OP_WRITEFULL;
  op.op.extent.offset = 0;
  op.op.extent.length = inbl->length();
  op.indata = *inbl;
  try {
    reinterpret_cast<ceph::osd::OpsExecuter*>(hctx)->do_osd_op(op).get();
    return 0;
  } catch (std::system_error& e) {
    return -e.code().value();
  }
}

int cls_cxx_replace(cls_method_context_t hctx,
                    int ofs,
                    int len,
                    bufferlist *inbl)
{
  return 0;
}

int cls_cxx_truncate(cls_method_context_t hctx, int ofs)
{
  return 0;
}

int cls_cxx_getxattr(cls_method_context_t hctx,
                     const char *name,
                     bufferlist *outbl)
{
  OSDOp op;
  op.op.op = CEPH_OSD_OP_GETXATTR;
  op.op.xattr.name_len = strlen(name);
  op.indata.append(name, op.op.xattr.name_len);
  try {
    reinterpret_cast<ceph::osd::OpsExecuter*>(hctx)->do_osd_op(op).get();
    outbl->claim(op.outdata);
    return outbl->length();
  } catch (std::system_error& e) {
    return -e.code().value();
  }
}

int cls_cxx_getxattrs(cls_method_context_t hctx,
                      map<string, bufferlist> *attrset)
{
  return 0;
}

int cls_cxx_setxattr(cls_method_context_t hctx,
                     const char *name,
                     bufferlist *inbl)
{
  OSDOp op;
  op.op.op = CEPH_OSD_OP_SETXATTR;
  op.op.xattr.name_len = std::strlen(name);
  op.op.xattr.value_len = inbl->length();
  op.indata.append(name, op.op.xattr.name_len);
  op.indata.append(*inbl);
  try {
    reinterpret_cast<ceph::osd::OpsExecuter*>(hctx)->do_osd_op(op).get();
    return 0;
  } catch (std::system_error& e) {
    return -e.code().value();
  }
}

int cls_cxx_snap_revert(cls_method_context_t hctx, snapid_t snapid)
{
  return 0;
}

int cls_cxx_map_get_all_vals(cls_method_context_t hctx,
                             map<string, bufferlist>* vals,
                             bool *more)
{
  return 0;
}

int cls_cxx_map_get_keys(cls_method_context_t hctx,
                         const string &start_obj,
			 uint64_t max_to_get,
                         set<string> *keys,
                         bool *more)
{
  return 0;
}

int cls_cxx_map_get_vals(cls_method_context_t hctx,
                         const string &start_obj,
                         const string &filter_prefix,
                         uint64_t max_to_get,
                         map<string, bufferlist> *vals,
                         bool *more)
{
  return 0;
}

int cls_cxx_map_read_header(cls_method_context_t hctx, bufferlist *outbl)
{
  return 0;
}

int cls_cxx_map_get_val(cls_method_context_t hctx,
                        const string &key,
                        bufferlist *outbl)
{
  return 0;
}

int cls_cxx_map_set_val(cls_method_context_t hctx,
                        const string &key,
                        bufferlist *inbl)
{
  return 0;
}

int cls_cxx_map_set_vals(cls_method_context_t hctx,
                         const std::map<string,
                         bufferlist> *map)
{
  return 0;
}

int cls_cxx_map_clear(cls_method_context_t hctx)
{
  return 0;
}

int cls_cxx_map_write_header(cls_method_context_t hctx, bufferlist *inbl)
{
  return 0;
}

int cls_cxx_map_remove_key(cls_method_context_t hctx, const string &key)
{
  return 0;
}

int cls_cxx_list_watchers(cls_method_context_t hctx,
                          obj_list_watch_response_t *watchers)
{
  return 0;
}

uint64_t cls_current_version(cls_method_context_t hctx)
{
  return 0;
}


int cls_current_subop_num(cls_method_context_t hctx)
{
  return 0;
}

uint64_t cls_get_features(cls_method_context_t hctx)
{
  return 0;
}

uint64_t cls_get_client_features(cls_method_context_t hctx)
{
  return 0;
}

ceph_release_t cls_get_required_osd_release(cls_method_context_t hctx)
{
  // FIXME
  return ceph_release_t::nautilus;
}

ceph_release_t cls_get_min_compatible_client(cls_method_context_t hctx)
{
  // FIXME
  return ceph_release_t::nautilus;
}

int cls_get_snapset_seq(cls_method_context_t hctx, uint64_t *snap_seq)
{
  return 0;
}

int cls_cxx_chunk_write_and_set(cls_method_context_t hctx,
                                int ofs,
                                int len,
                                bufferlist *write_inbl,
                                uint32_t op_flags,
                                bufferlist *set_inbl,
                                int set_len)
{
  return 0;
}

bool cls_has_chunk(cls_method_context_t hctx, string fp_oid)
{
  return 0;
}

uint64_t cls_get_osd_min_alloc_size(cls_method_context_t hctx) {
  // FIXME
  return 4096;
}
