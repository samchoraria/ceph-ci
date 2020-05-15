#!/bin/sh -e

if [ -n "${VALGRIND}" ]; then
  valgrind ${VALGRIND} --suppressions=${TESTDIR}/valgrind.supp \
    --error-exitcode=1 ceph_test_librbd
else
  CEPH_ARGS='--debug-rbd=30' ceph_test_librbd --gtest_filter=TestLibRBD.QuiesceWatchTimeout --gtest_repeat=100
fi
exit 0
