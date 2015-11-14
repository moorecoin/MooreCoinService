// copyright (c) 2014, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
//
// this file implements the "bridge" between java and c++ and enables
// calling c++ rocksdb::env methods from java side.

#include "include/org_rocksdb_rocksenv.h"
#include "rocksdb/env.h"

/*
 * class:     org_rocksdb_rocksenv
 * method:    getdefaultenvinternal
 * signature: ()j
 */
jlong java_org_rocksdb_rocksenv_getdefaultenvinternal(
    jnienv* env, jclass jclass) {
  return reinterpret_cast<jlong>(rocksdb::env::default());
}

/*
 * class:     org_rocksdb_rocksenv
 * method:    setbackgroundthreads
 * signature: (jii)v
 */
void java_org_rocksdb_rocksenv_setbackgroundthreads(
    jnienv* env, jobject jobj, jlong jhandle,
    jint num, jint priority) {
  auto* rocks_env = reinterpret_cast<rocksdb::env*>(jhandle);
  switch (priority) {
    case org_rocksdb_rocksenv_flush_pool:
      rocks_env->setbackgroundthreads(num, rocksdb::env::priority::low);
      break;
    case org_rocksdb_rocksenv_compaction_pool:
      rocks_env->setbackgroundthreads(num, rocksdb::env::priority::high);
      break;
  }
}

/*
 * class:     org_rocksdb_rocksenv
 * method:    getthreadpoolqueuelen
 * signature: (ji)i
 */
jint java_org_rocksdb_rocksenv_getthreadpoolqueuelen(
    jnienv* env, jobject jobj, jlong jhandle, jint pool_id) {
  auto* rocks_env = reinterpret_cast<rocksdb::env*>(jhandle);
  switch (pool_id) {
    case org_rocksdb_rocksenv_flush_pool:
      return rocks_env->getthreadpoolqueuelen(rocksdb::env::priority::low);
    case org_rocksdb_rocksenv_compaction_pool:
      return rocks_env->getthreadpoolqueuelen(rocksdb::env::priority::high);
  }
  return 0;
}

/*
 * class:     org_rocksdb_rocksenv
 * method:    disposeinternal
 * signature: (j)v
 */
void java_org_rocksdb_rocksenv_disposeinternal(
    jnienv* env, jobject jobj, jlong jhandle) {
  delete reinterpret_cast<rocksdb::env*>(jhandle);
}
