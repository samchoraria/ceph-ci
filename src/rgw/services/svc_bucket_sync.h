
// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab

/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2019 Red Hat, Inc.
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation. See file COPYING.
 *
 */


#pragma once

#include "rgw/rgw_service.h"

#include "svc_bucket_types.h"

class RGWBucketSyncPolicyHandler;
using RGWBucketSyncPolicyHandlerRef = std::shared_ptr<RGWBucketSyncPolicyHandler>;


class RGWSI_Bucket_Sync : public RGWServiceInstance
{
public:
  RGWSI_Bucket_Sync(CephContext *cct) : RGWServiceInstance(cct) {}

  virtual int get_policy_handler(RGWSI_Bucket_BI_Ctx& ctx,
                                 std::optional<string> zone,
                                 std::optional<rgw_bucket> bucket,
                                 RGWBucketSyncPolicyHandlerRef *handler,
                                 optional_yield y) = 0;
  virtual int handle_bi_update(RGWBucketInfo& bucket_info,
                               RGWBucketInfo *orig_bucket_info) = 0;
};


