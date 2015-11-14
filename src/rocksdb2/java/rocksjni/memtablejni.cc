// copyright (c) 2014, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
//
// this file implements the "bridge" between java and c++ for memtables.

#include "include/org_rocksdb_hashskiplistmemtableconfig.h"
#include "include/org_rocksdb_hashlinkedlistmemtableconfig.h"
#include "include/org_rocksdb_vectormemtableconfig.h"
#include "include/org_rocksdb_skiplistmemtableconfig.h"
#include "rocksdb/memtablerep.h"

/*
 * class:     org_rocksdb_hashskiplistmemtableconfig
 * method:    newmemtablefactoryhandle
 * signature: (jii)j
 */
jlong java_org_rocksdb_hashskiplistmemtableconfig_newmemtablefactoryhandle(
    jnienv* env, jobject jobj, jlong jbucket_count,
    jint jheight, jint jbranching_factor) {
  return reinterpret_cast<jlong>(rocksdb::newhashskiplistrepfactory(
      static_cast<size_t>(jbucket_count),
      static_cast<int32_t>(jheight),
      static_cast<int32_t>(jbranching_factor)));
}

/*
 * class:     org_rocksdb_hashlinkedlistmemtableconfig
 * method:    newmemtablefactoryhandle
 * signature: (j)j
 */
jlong java_org_rocksdb_hashlinkedlistmemtableconfig_newmemtablefactoryhandle(
    jnienv* env, jobject jobj, jlong jbucket_count) {
  return reinterpret_cast<jlong>(rocksdb::newhashlinklistrepfactory(
       static_cast<size_t>(jbucket_count)));
}

/*
 * class:     org_rocksdb_vectormemtableconfig
 * method:    newmemtablefactoryhandle
 * signature: (j)j
 */
jlong java_org_rocksdb_vectormemtableconfig_newmemtablefactoryhandle(
    jnienv* env, jobject jobj, jlong jreserved_size) {
  return reinterpret_cast<jlong>(new rocksdb::vectorrepfactory(
      static_cast<size_t>(jreserved_size)));
}

/*
 * class:     org_rocksdb_skiplistmemtableconfig
 * method:    newmemtablefactoryhandle0
 * signature: ()j
 */
jlong java_org_rocksdb_skiplistmemtableconfig_newmemtablefactoryhandle0(
    jnienv* env, jobject jobj) {
  return reinterpret_cast<jlong>(new rocksdb::skiplistfactory());
}
