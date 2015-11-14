#!/bin/bash
#a shell script for jenknis to run valgrind on rocksdb tests
#returns 0 on success when there are no failed tests 

valgrind_dir=build_tools/valgrind_logs
make clean
make -j$(nproc) valgrind_check
num_failed_tests=$((`wc -l $valgrind_dir/valgrind_failed_tests | awk '{print $1}'` - 1))
if [ $num_failed_tests -lt 1 ]; then
  echo no tests have valgrind errors
  exit 0
else
  cat $valgrind_dir/valgrind_failed_tests
  exit 1
fi
