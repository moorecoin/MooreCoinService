// copyright (c) 2014, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
//
// this file implements the "bridge" between java and c++ and enables
// calling c++ rocksdb::iterator methods from java side.

#include <stdio.h>
#include <stdlib.h>
#include <jni.h>

#include "include/org_rocksdb_rocksiterator.h"
#include "rocksjni/portal.h"
#include "rocksdb/iterator.h"

/*
 * class:     org_rocksdb_rocksiterator
 * method:    isvalid0
 * signature: (j)z
 */
jboolean java_org_rocksdb_rocksiterator_isvalid0(
    jnienv* env, jobject jobj, jlong handle) {
  return reinterpret_cast<rocksdb::iterator*>(handle)->valid();
}

/*
 * class:     org_rocksdb_rocksiterator
 * method:    seektofirst0
 * signature: (j)v
 */
void java_org_rocksdb_rocksiterator_seektofirst0(
    jnienv* env, jobject jobj, jlong handle) {
  reinterpret_cast<rocksdb::iterator*>(handle)->seektofirst();
}

/*
 * class:     org_rocksdb_rocksiterator
 * method:    seektofirst0
 * signature: (j)v
 */
void java_org_rocksdb_rocksiterator_seektolast0(
    jnienv* env, jobject jobj, jlong handle) {
  reinterpret_cast<rocksdb::iterator*>(handle)->seektolast();
}

/*
 * class:     org_rocksdb_rocksiterator
 * method:    seektolast0
 * signature: (j)v
 */
void java_org_rocksdb_rocksiterator_next0(
    jnienv* env, jobject jobj, jlong handle) {
  reinterpret_cast<rocksdb::iterator*>(handle)->next();
}

/*
 * class:     org_rocksdb_rocksiterator
 * method:    next0
 * signature: (j)v
 */
void java_org_rocksdb_rocksiterator_prev0(
    jnienv* env, jobject jobj, jlong handle) {
  reinterpret_cast<rocksdb::iterator*>(handle)->prev();
}

/*
 * class:     org_rocksdb_rocksiterator
 * method:    prev0
 * signature: (j)v
 */
jbytearray java_org_rocksdb_rocksiterator_key0(
    jnienv* env, jobject jobj, jlong handle) {
  auto it = reinterpret_cast<rocksdb::iterator*>(handle);
  rocksdb::slice key_slice = it->key();

  jbytearray jkey = env->newbytearray(key_slice.size());
  env->setbytearrayregion(
      jkey, 0, key_slice.size(),
      reinterpret_cast<const jbyte*>(key_slice.data()));
  return jkey;
}

/*
 * class:     org_rocksdb_rocksiterator
 * method:    key0
 * signature: (j)[b
 */
jbytearray java_org_rocksdb_rocksiterator_value0(
    jnienv* env, jobject jobj, jlong handle) {
  auto it = reinterpret_cast<rocksdb::iterator*>(handle);
  rocksdb::slice value_slice = it->value();

  jbytearray jvalue = env->newbytearray(value_slice.size());
  env->setbytearrayregion(
      jvalue, 0, value_slice.size(),
      reinterpret_cast<const jbyte*>(value_slice.data()));
  return jvalue;
}

/*
 * class:     org_rocksdb_rocksiterator
 * method:    value0
 * signature: (j)[b
 */
void java_org_rocksdb_rocksiterator_seek0(
    jnienv* env, jobject jobj, jlong handle,
    jbytearray jtarget, jint jtarget_len) {
  auto it = reinterpret_cast<rocksdb::iterator*>(handle);
  jbyte* target = env->getbytearrayelements(jtarget, 0);
  rocksdb::slice target_slice(
      reinterpret_cast<char*>(target), jtarget_len);

  it->seek(target_slice);

  env->releasebytearrayelements(jtarget, target, jni_abort);
}

/*
 * class:     org_rocksdb_rocksiterator
 * method:    seek0
 * signature: (j[bi)v
 */
void java_org_rocksdb_rocksiterator_status0(
    jnienv* env, jobject jobj, jlong handle) {
  auto it = reinterpret_cast<rocksdb::iterator*>(handle);
  rocksdb::status s = it->status();

  if (s.ok()) {
    return;
  }

  rocksdb::rocksdbexceptionjni::thrownew(env, s);
}

/*
 * class:     org_rocksdb_rocksiterator
 * method:    disposeinternal
 * signature: (j)v
 */
void java_org_rocksdb_rocksiterator_disposeinternal(
    jnienv* env, jobject jobj, jlong handle) {
  auto it = reinterpret_cast<rocksdb::iterator*>(handle);
  delete it;
}
