// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2004-2006 Sage Weil <sage@newdream.net>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

#include <stdio.h>
#include <string.h>
#include <iostream>
#include <time.h>
#include <sys/mount.h>
#include "kv/KeyValueDB.h"
#include "include/Context.h"
#include "common/ceph_argparse.h"
#include "global/global_init.h"
#include "common/Mutex.h"
#include "common/Cond.h"
#include "common/errno.h"
#include "include/stringify.h"
#include <gtest/gtest.h>

#if GTEST_HAS_PARAM_TEST

class KVTest : public ::testing::TestWithParam<const char*> {
public:
  boost::scoped_ptr<KeyValueDB> db;

  KVTest() : db(0) {}

  string _bl_to_str(bufferlist val) {
    string str(val.c_str(), val.length());
    return str;
  }

  void rm_r(string path) {
    string cmd = string("rm -r ") + path;
    cout << "==> " << cmd << std::endl;
    int r = ::system(cmd.c_str());
    if (r) {
      cerr << "failed with exit code " << r
	   << ", continuing anyway" << std::endl;
    }
  }

  void init() {
    cout << "Creating " << string(GetParam()) << "\n";
    db.reset(KeyValueDB::create(g_ceph_context, string(GetParam()),
				"kv_test_temp_dir"));
  }
  void fini() {
    db.reset(NULL);
  }

  virtual void SetUp() {
    int r = ::mkdir("kv_test_temp_dir", 0777);
    if (r < 0 && errno != EEXIST) {
      r = -errno;
      cerr << __func__ << ": unable to create kv_test_temp_dir: "
	   << cpp_strerror(r) << std::endl;
      return;
    }
    init();
  }
  virtual void TearDown() {
    fini();
    rm_r("kv_test_temp_dir");
  }
};

TEST_P(KVTest, OpenClose) {
  ASSERT_EQ(0, db->create_and_open(cout));
  fini();
}

TEST_P(KVTest, OpenCloseReopenClose) {
  ASSERT_EQ(0, db->create_and_open(cout));
  fini();
  init();
  ASSERT_EQ(0, db->open(cout));
  fini();
}

TEST_P(KVTest, PutReopen) {
  ASSERT_EQ(0, db->create_and_open(cout));
  {
    KeyValueDB::Transaction t = db->get_transaction();
    bufferlist value;
    value.append("value");
    t->set("prefix", "key", value);
    t->set("prefix", "key2", value);
    t->set("prefix", "key3", value);
    db->submit_transaction_sync(t);
  }
  fini();

  init();
  ASSERT_EQ(0, db->open(cout));
  {
    bufferlist v1, v2;
    ASSERT_EQ(0, db->get("prefix", "key", &v1));
    ASSERT_EQ(v1.length(), 5u);
    ASSERT_EQ(0, db->get("prefix", "key2", &v2));
    ASSERT_EQ(v2.length(), 5u);
  }
  {
    KeyValueDB::Transaction t = db->get_transaction();
    t->rmkey("prefix", "key");
    t->rmkey("prefix", "key3");
    db->submit_transaction_sync(t);
  }
  fini();

  init();
  ASSERT_EQ(0, db->open(cout));
  {
    bufferlist v1, v2, v3;
    ASSERT_EQ(-ENOENT, db->get("prefix", "key", &v1));
    ASSERT_EQ(0, db->get("prefix", "key2", &v2));
    ASSERT_EQ(v2.length(), 5u);
    ASSERT_EQ(-ENOENT, db->get("prefix", "key3", &v3));
  }
  fini();
}

TEST_P(KVTest, BenchCommit) {
  int n = 1024;
  ASSERT_EQ(0, db->create_and_open(cout));
  utime_t start = ceph_clock_now(NULL);
  {
    cout << "priming" << std::endl;
    // prime
    bufferlist big;
    bufferptr bp(1048576);
    bp.zero();
    big.append(bp);
    for (int i=0; i<30; ++i) {
      KeyValueDB::Transaction t = db->get_transaction();
      t->set("prefix", "big" + stringify(i), big);
      db->submit_transaction_sync(t);
    }
  }
  cout << "now doing small writes" << std::endl;
  bufferlist data;
  bufferptr bp(1024);
  bp.zero();
  data.append(bp);
  for (int i=0; i<n; ++i) {
    KeyValueDB::Transaction t = db->get_transaction();
    t->set("prefix", "key" + stringify(i), data);
    db->submit_transaction_sync(t);
  }
  utime_t end = ceph_clock_now(NULL);
  utime_t dur = end - start;
  cout << n << " commits in " << dur << ", avg latency " << (dur / (double)n)
       << std::endl;
  fini();
}

TEST_P(KVTest, RocksDBColumnFamilyTest) {
  if(string(GetParam()) != "rocksdb")
    return;

  std::vector<KeyValueDB::ColumnFamily> cfs;
  cfs.push_back(KeyValueDB::ColumnFamily("cf1", ""));
  cfs.push_back(KeyValueDB::ColumnFamily("cf2", ""));
  ASSERT_EQ(0, db->init(g_conf->bluestore_rocksdb_options));
  cout << "creating two column families and opening them" << std::endl;
  ASSERT_EQ(0, db->create_and_open(cout, cfs));
  {
    KeyValueDB::Transaction t = db->get_transaction();
    bufferlist value;
    value.append("value");
    cout << "write a transaction includes three keys in different CFs" << std::endl;
    t->set("prefix", "key", value);
    t->set("cf1", "key", value);
    t->set("cf2", "key2", value);
    ASSERT_EQ(0, db->submit_transaction_sync(t));
  }
  fini();

  init();
  ASSERT_EQ(0, db->open(cout, cfs));
  {
    bufferlist v1, v2, v3;
    cout << "reopen db and read those keys" << std::endl;
    ASSERT_EQ(0, db->get("prefix", "key", &v1));
    ASSERT_EQ(0, _bl_to_str(v1) != "value");
    ASSERT_EQ(0, db->get("cf1", "key", &v2));
    ASSERT_EQ(0, _bl_to_str(v2) != "value");
    ASSERT_EQ(0, db->get("cf2", "key2", &v3));
    ASSERT_EQ(0, _bl_to_str(v2) != "value");
  }
  {
    cout << "delete two keys in CFs" << std::endl;
    KeyValueDB::Transaction t = db->get_transaction();
    t->rmkey("prefix", "key");
    t->rmkey("cf2", "key2");
    ASSERT_EQ(0, db->submit_transaction_sync(t));
  }
  fini();

  init();
  ASSERT_EQ(0, db->open(cout, cfs));
  {
    cout << "reopen db and read keys again." << std::endl;
    bufferlist v1, v2, v3;
    ASSERT_EQ(-ENOENT, db->get("prefix", "key", &v1));
    ASSERT_EQ(0, db->get("cf1", "key", &v2));
    ASSERT_EQ(0, _bl_to_str(v2) != "value");
    ASSERT_EQ(-ENOENT, db->get("cf2", "key2", &v3));
  }
  fini();
}

TEST_P(KVTest, RocksDBIteratorTest) {
  if(string(GetParam()) != "rocksdb")
    return;

  std::vector<KeyValueDB::ColumnFamily> cfs;
  cfs.push_back(KeyValueDB::ColumnFamily("cf1", ""));
  ASSERT_EQ(0, db->init(g_conf->bluestore_rocksdb_options));
  cout << "creating one column family and opening it" << std::endl;
  ASSERT_EQ(0, db->create_and_open(cout, cfs));
  {
    KeyValueDB::Transaction t = db->get_transaction();
    bufferlist bl1;
    bl1.append("hello");
    bufferlist bl2;
    bl2.append("world");
    cout << "write some kv pairs into default and new CFs" << std::endl;
    t->set("prefix", "key1", bl1);
    t->set("prefix", "key2", bl2);
    t->set("cf1", "key1", bl1);
    t->set("cf1", "key2", bl2);
    ASSERT_EQ(0, db->submit_transaction_sync(t));
  }
  {
    cout << "iterating the default CF" << std::endl;
    KeyValueDB::Iterator iter = db->get_iterator("prefix");
    iter->seek_to_first();
    ASSERT_EQ(1, iter->valid());
    ASSERT_EQ("key1", iter->key());
    ASSERT_EQ("hello", _bl_to_str(iter->value()));
    ASSERT_EQ(0, iter->next());
    ASSERT_EQ(1, iter->valid());
    ASSERT_EQ("key2", iter->key());
    ASSERT_EQ("world", _bl_to_str(iter->value()));
  }
  {
    cout << "iterating the new CF" << std::endl;
    KeyValueDB::Iterator iter = db->get_iterator("cf1");
    iter->seek_to_first();
    ASSERT_EQ(1, iter->valid());
    ASSERT_EQ("key1", iter->key());
    ASSERT_EQ("hello", _bl_to_str(iter->value()));
    ASSERT_EQ(0, iter->next());
    ASSERT_EQ(1, iter->valid());
    ASSERT_EQ("key2", iter->key());
    ASSERT_EQ("world", _bl_to_str(iter->value()));
  }
  fini();
}


INSTANTIATE_TEST_CASE_P(
  KeyValueDB,
  KVTest,
  ::testing::Values("leveldb", "rocksdb"));

#else

// Google Test may not support value-parameterized tests with some
// compilers. If we use conditional compilation to compile out all
// code referring to the gtest_main library, MSVC linker will not link
// that library at all and consequently complain about missing entry
// point defined in that library (fatal error LNK1561: entry point
// must be defined). This dummy test keeps gtest_main linked in.
TEST(DummyTest, ValueParameterizedTestsAreNotSupportedOnThisPlatform) {}

#endif

int main(int argc, char **argv) {
  vector<const char*> args;
  argv_to_vec(argc, (const char **)argv, args);
  env_to_vec(args);

  auto cct = global_init(NULL, args, CEPH_ENTITY_TYPE_CLIENT,
			 CODE_ENVIRONMENT_UTILITY, 0);
  common_init_finish(g_ceph_context);
  g_ceph_context->_conf->set_val(
    "enable_experimental_unrecoverable_data_corrupting_features",
    "rocksdb");
  g_ceph_context->_conf->apply_changes(NULL);

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
