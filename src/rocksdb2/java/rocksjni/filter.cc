// copyright (c) 2014, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
//
// this file implements the "bridge" between java and c++ for
// rocksdb::filterpolicy.

#include <stdio.h>
#include <stdlib.h>
#include <jni.h>
#include <string>

#include "include/org_rocksdb_filter.h"
#include "include/org_rocksdb_bloomfilter.h"
#include "rocksjni/portal.h"
#include "rocksdb/filter_policy.h"

/*
 * class:     org_rocksdb_bloomfilter
 * method:    createnewfilter0
 * signature: (i)v
 */
void java_org_rocksdb_bloomfilter_createnewfilter0(
    jnienv* env, jobject jobj, jint bits_per_key) {
  const rocksdb::filterpolicy* fp = rocksdb::newbloomfilterpolicy(bits_per_key);
  rocksdb::filterjni::sethandle(env, jobj, fp);
}

/*
 * class:     org_rocksdb_filter
 * method:    disposeinternal
 * signature: (j)v
 */
void java_org_rocksdb_filter_disposeinternal(
    jnienv* env, jobject jobj, jlong handle) {
  delete reinterpret_cast<rocksdb::filterpolicy*>(handle);
}
