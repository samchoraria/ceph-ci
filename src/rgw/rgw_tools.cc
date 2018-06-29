// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

#include <errno.h>

#include "common/errno.h"
#include "common/safe_io.h"

#include "include/types.h"

#include "rgw_common.h"
#include "rgw_rados.h"
#include "rgw_tools.h"
#include "rgw_acl_s3.h"
#include "rgw_op.h"

#define dout_subsys ceph_subsys_rgw

#define READ_CHUNK_LEN (512 * 1024)

static std::map<std::string, std::string>* ext_mime_map;

int rgw_put_system_obj(RGWRados *rgwstore, const rgw_pool& pool, const string& oid, const bufferlist& data, bool exclusive,
                       RGWObjVersionTracker *objv_tracker, real_time set_mtime, map<string, bufferlist> *pattrs)
{
  map<string,bufferlist> no_attrs;
  if (!pattrs)
    pattrs = &no_attrs;

  rgw_raw_obj obj(pool, oid);

  int ret = rgwstore->put_system_obj(NULL, obj, data, exclusive, NULL, *pattrs, objv_tracker, set_mtime);

  if (ret == -ENOENT) {
    ret = rgwstore->create_pool(pool);
    if (ret >= 0)
      ret = rgwstore->put_system_obj(NULL, obj, data, exclusive, NULL, *pattrs, objv_tracker, set_mtime);
  }

  return ret;
}

int rgw_get_system_obj(RGWRados *rgwstore, RGWObjectCtx& obj_ctx, const rgw_pool& pool, const string& key, bufferlist& bl,
                       RGWObjVersionTracker *objv_tracker, real_time *pmtime, map<string, bufferlist> *pattrs,
                       rgw_cache_entry_info *cache_info, boost::optional<obj_version> refresh_version)
{
  bufferlist::iterator iter;
  int request_len = READ_CHUNK_LEN;
  rgw_raw_obj obj(pool, key);

  obj_version original_readv;
  if (objv_tracker && !objv_tracker->read_version.empty()) {
    original_readv = objv_tracker->read_version;
  }

  do {
    RGWRados::SystemObject source(rgwstore, obj_ctx, obj);
    RGWRados::SystemObject::Read rop(&source);

    rop.stat_params.attrs = pattrs;
    rop.stat_params.lastmod = pmtime;

    int ret = rop.stat(objv_tracker);
    if (ret < 0)
      return ret;

    rop.read_params.cache_info = cache_info;

    ret = rop.read(0, request_len - 1, bl, objv_tracker, refresh_version);
    if (ret == -ECANCELED) {
      /* raced, restart */
      if (!original_readv.empty()) {
        /* we were asked to read a specific obj_version, failed */
        return ret;
      }
      if (objv_tracker) {
        objv_tracker->read_version.clear();
      }
      source.invalidate_state();
      continue;
    }
    if (ret < 0)
      return ret;

    if (ret < request_len)
      break;
    bl.clear();
    request_len *= 2;
  } while (true);

  return 0;
}

int rgw_delete_system_obj(RGWRados *rgwstore, const rgw_pool& pool, const string& oid,
                          RGWObjVersionTracker *objv_tracker)
{
  rgw_raw_obj obj(pool, oid);
  return rgwstore->delete_system_obj(obj, objv_tracker);
}

void parse_mime_map_line(const char *start, const char *end)
{
  char line[end - start + 1];
  strncpy(line, start, end - start);
  line[end - start] = '\0';
  char *l = line;
#define DELIMS " \t\n\r"

  while (isspace(*l))
    l++;

  char *mime = strsep(&l, DELIMS);
  if (!mime)
    return;

  char *ext;
  do {
    ext = strsep(&l, DELIMS);
    if (ext && *ext) {
      (*ext_mime_map)[ext] = mime;
    }
  } while (ext);
}


void parse_mime_map(const char *buf)
{
  const char *start = buf, *end = buf;
  while (*end) {
    while (*end && *end != '\n') {
      end++;
    }
    parse_mime_map_line(start, end);
    end++;
    start = end;
  }
}

static int ext_mime_map_init(CephContext *cct, const char *ext_map)
{
  int fd = open(ext_map, O_RDONLY);
  char *buf = NULL;
  int ret;
  if (fd < 0) {
    ret = -errno;
    ldout(cct, 0) << __func__ << " failed to open file=" << ext_map
                  << " : " << cpp_strerror(-ret) << dendl;
    return ret;
  }

  struct stat st;
  ret = fstat(fd, &st);
  if (ret < 0) {
    ret = -errno;
    ldout(cct, 0) << __func__ << " failed to stat file=" << ext_map
                  << " : " << cpp_strerror(-ret) << dendl;
    goto done;
  }

  buf = (char *)malloc(st.st_size + 1);
  if (!buf) {
    ret = -ENOMEM;
    ldout(cct, 0) << __func__ << " failed to allocate buf" << dendl;
    goto done;
  }

  ret = safe_read(fd, buf, st.st_size + 1);
  if (ret != st.st_size) {
    // huh? file size has changed?
    ldout(cct, 0) << __func__ << " raced! will retry.." << dendl;
    free(buf);
    close(fd);
    return ext_mime_map_init(cct, ext_map);
  }
  buf[st.st_size] = '\0';

  parse_mime_map(buf);
  ret = 0;
done:
  free(buf);
  close(fd);
  return ret;
}

const char *rgw_find_mime_by_ext(string& ext)
{
  map<string, string>::iterator iter = ext_mime_map->find(ext);
  if (iter == ext_mime_map->end())
    return NULL;

  return iter->second.c_str();
}


RGWDataAccess::RGWDataAccess(RGWRados *_store) : store(_store)
{
  obj_ctx = std::make_unique<RGWObjectCtx>(store);
}


int RGWDataAccess::Bucket::finish_init()
{
  auto iter = attrs.find(RGW_ATTR_ACL);
  if (iter == attrs.end()) {
    return 0;
  }

  bufferlist::iterator bliter = iter->second.begin();
  try {
    policy.decode(bliter);
  } catch (buffer::error& err) {
    return -EIO;
  }

  return 0;
}

int RGWDataAccess::Bucket::init()
{
  int ret = sd->store->get_bucket_info(*sd->obj_ctx,
				       tenant, name,
				       bucket_info,
				       &mtime,
				       &attrs);
  if (ret < 0) {
    return ret;
  }

  return finish_init();
}

int RGWDataAccess::Bucket::init(const RGWBucketInfo& _bucket_info,
				const map<string, bufferlist>& _attrs)
{
  bucket_info = _bucket_info;
  attrs = _attrs;

  return finish_init();
}

int RGWDataAccess::Bucket::get_object(const rgw_obj_key& key,
				      ObjectRef *obj) {
  obj->reset(new Object(sd, self_ref, key));
  return 0;
}

int RGWDataAccess::Object::put(bufferlist& data,
			       map<string, bufferlist>& attrs)
{
  RGWRados *store = sd->store;
  CephContext *cct = store->ctx();

  string tag;
  append_rand_alpha(cct, tag, tag, 32);

  RGWBucketInfo& bucket_info = bucket->bucket_info;

  RGWPutObjProcessor_Atomic processor(*sd->obj_ctx,
                                      bucket_info,
				      bucket_info.bucket,
				      key.name,
                                      cct->_conf->rgw_obj_stripe_size, tag,
				      bucket_info.versioning_enabled());
  if (key.instance.empty()) {
    processor.set_version_id(key.instance);
  }

  if (olh_epoch) {
    processor.set_olh_epoch(*olh_epoch);
  }
  int ret = processor.prepare(store, NULL);
  if (ret < 0)
    return ret;

  off_t ofs = 0;
  auto obj_size = data.length();

  RGWMD5Etag etag_calc;

  do {
    size_t read_len = std::min(data.length(), (unsigned int)cct->_conf->rgw_max_chunk_size);

    bufferlist bl;

    data.splice(0, read_len, &bl);
    etag_calc.update(bl);

    bool again;

    do {
      void *handle;
      rgw_raw_obj obj;

      ret = processor.handle_data(bl, ofs, &handle, &obj, &again);
      if (ret < 0) {
        return ret;
      }
      ret = processor.throttle_data(handle, obj, read_len, false);
      if (ret < 0)
        return ret;
    } while (again);

    ofs += read_len;
  } while (data.length() > 0);

  bool has_etag_attr = false;
  auto iter = attrs.find(RGW_ATTR_ETAG);
  if (iter != attrs.end()) {
    bufferlist& bl = iter->second;
    etag = bl.to_str();
    has_etag_attr = true;
  }

  if (!aclbl) {
    RGWAccessControlPolicy_S3 policy(cct);

    policy.create_canned(bucket->policy.get_owner(), bucket->policy.get_owner(), string()); /* default private policy */

    policy.encode(aclbl.emplace());
  }

  if (etag.empty()) {
    etag_calc.finish(&etag);
  }

  if (!has_etag_attr) {
    bufferlist etagbl;
    etagbl.append(etag);
    attrs[RGW_ATTR_ETAG] = etagbl;
  }
  attrs[RGW_ATTR_ACL] = *aclbl;

  return processor.complete(obj_size, etag,
			    &mtime, mtime,
			    attrs, delete_at);
}

void RGWDataAccess::Object::set_policy(const RGWAccessControlPolicy& policy)
{
  policy.encode(aclbl.emplace());
}

int rgw_tools_init(CephContext *cct)
{
  ext_mime_map = new std::map<std::string, std::string>;
  int ret = ext_mime_map_init(cct, cct->_conf->rgw_mime_types_file.c_str());
  if (ret < 0)
    return ret;

  return 0;
}

void rgw_tools_cleanup()
{
  delete ext_mime_map;
  ext_mime_map = nullptr;
}
