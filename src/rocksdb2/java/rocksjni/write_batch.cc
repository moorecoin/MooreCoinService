// copyright (c) 2014, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
//
// this file implements the "bridge" between java and c++ and enables
// calling c++ rocksdb::writebatch methods from java side.
#include <memory>

#include "include/org_rocksdb_writebatch.h"
#include "include/org_rocksdb_writebatchinternal.h"
#include "include/org_rocksdb_writebatchtest.h"
#include "rocksjni/portal.h"
#include "rocksdb/db.h"
#include "db/memtable.h"
#include "rocksdb/write_batch.h"
#include "db/write_batch_internal.h"
#include "rocksdb/env.h"
#include "rocksdb/memtablerep.h"
#include "util/logging.h"
#include "util/testharness.h"

/*
 * class:     org_rocksdb_writebatch
 * method:    newwritebatch
 * signature: (i)v
 */
void java_org_rocksdb_writebatch_newwritebatch(
    jnienv* env, jobject jobj, jint jreserved_bytes) {
  rocksdb::writebatch* wb = new rocksdb::writebatch(
      static_cast<size_t>(jreserved_bytes));

  rocksdb::writebatchjni::sethandle(env, jobj, wb);
}

/*
 * class:     org_rocksdb_writebatch
 * method:    count
 * signature: ()i
 */
jint java_org_rocksdb_writebatch_count(jnienv* env, jobject jobj) {
  rocksdb::writebatch* wb = rocksdb::writebatchjni::gethandle(env, jobj);
  assert(wb != nullptr);

  return static_cast<jint>(wb->count());
}

/*
 * class:     org_rocksdb_writebatch
 * method:    clear
 * signature: ()v
 */
void java_org_rocksdb_writebatch_clear(jnienv* env, jobject jobj) {
  rocksdb::writebatch* wb = rocksdb::writebatchjni::gethandle(env, jobj);
  assert(wb != nullptr);

  wb->clear();
}

/*
 * class:     org_rocksdb_writebatch
 * method:    put
 * signature: ([bi[bi)v
 */
void java_org_rocksdb_writebatch_put(
    jnienv* env, jobject jobj,
    jbytearray jkey, jint jkey_len,
    jbytearray jvalue, jint jvalue_len) {
  rocksdb::writebatch* wb = rocksdb::writebatchjni::gethandle(env, jobj);
  assert(wb != nullptr);

  jbyte* key = env->getbytearrayelements(jkey, nullptr);
  jbyte* value = env->getbytearrayelements(jvalue, nullptr);
  rocksdb::slice key_slice(reinterpret_cast<char*>(key), jkey_len);
  rocksdb::slice value_slice(reinterpret_cast<char*>(value), jvalue_len);
  wb->put(key_slice, value_slice);
  env->releasebytearrayelements(jkey, key, jni_abort);
  env->releasebytearrayelements(jvalue, value, jni_abort);
}

/*
 * class:     org_rocksdb_writebatch
 * method:    merge
 * signature: ([bi[bi)v
 */
jniexport void jnicall java_org_rocksdb_writebatch_merge(
    jnienv* env, jobject jobj,
    jbytearray jkey, jint jkey_len,
    jbytearray jvalue, jint jvalue_len) {
  rocksdb::writebatch* wb = rocksdb::writebatchjni::gethandle(env, jobj);
  assert(wb != nullptr);

  jbyte* key = env->getbytearrayelements(jkey, nullptr);
  jbyte* value = env->getbytearrayelements(jvalue, nullptr);
  rocksdb::slice key_slice(reinterpret_cast<char*>(key), jkey_len);
  rocksdb::slice value_slice(reinterpret_cast<char*>(value), jvalue_len);
  wb->merge(key_slice, value_slice);
  env->releasebytearrayelements(jkey, key, jni_abort);
  env->releasebytearrayelements(jvalue, value, jni_abort);
}

/*
 * class:     org_rocksdb_writebatch
 * method:    remove
 * signature: ([bi)v
 */
jniexport void jnicall java_org_rocksdb_writebatch_remove(
    jnienv* env, jobject jobj,
    jbytearray jkey, jint jkey_len) {
  rocksdb::writebatch* wb = rocksdb::writebatchjni::gethandle(env, jobj);
  assert(wb != nullptr);

  jbyte* key = env->getbytearrayelements(jkey, nullptr);
  rocksdb::slice key_slice(reinterpret_cast<char*>(key), jkey_len);
  wb->delete(key_slice);
  env->releasebytearrayelements(jkey, key, jni_abort);
}

/*
 * class:     org_rocksdb_writebatch
 * method:    putlogdata
 * signature: ([bi)v
 */
void java_org_rocksdb_writebatch_putlogdata(
    jnienv* env, jobject jobj, jbytearray jblob, jint jblob_len) {
  rocksdb::writebatch* wb = rocksdb::writebatchjni::gethandle(env, jobj);
  assert(wb != nullptr);

  jbyte* blob = env->getbytearrayelements(jblob, nullptr);
  rocksdb::slice blob_slice(reinterpret_cast<char*>(blob), jblob_len);
  wb->putlogdata(blob_slice);
  env->releasebytearrayelements(jblob, blob, jni_abort);
}

/*
 * class:     org_rocksdb_writebatch
 * method:    disposeinternal
 * signature: (j)v
 */
void java_org_rocksdb_writebatch_disposeinternal(
    jnienv* env, jobject jobj, jlong handle) {
  delete reinterpret_cast<rocksdb::writebatch*>(handle);
}

/*
 * class:     org_rocksdb_writebatchinternal
 * method:    setsequence
 * signature: (lorg/rocksdb/writebatch;j)v
 */
void java_org_rocksdb_writebatchinternal_setsequence(
    jnienv* env, jclass jclazz, jobject jobj, jlong jsn) {
  rocksdb::writebatch* wb = rocksdb::writebatchjni::gethandle(env, jobj);
  assert(wb != nullptr);

  rocksdb::writebatchinternal::setsequence(
      wb, static_cast<rocksdb::sequencenumber>(jsn));
}

/*
 * class:     org_rocksdb_writebatchinternal
 * method:    sequence
 * signature: (lorg/rocksdb/writebatch;)j
 */
jlong java_org_rocksdb_writebatchinternal_sequence(
    jnienv* env, jclass jclazz, jobject jobj) {
  rocksdb::writebatch* wb = rocksdb::writebatchjni::gethandle(env, jobj);
  assert(wb != nullptr);

  return static_cast<jlong>(rocksdb::writebatchinternal::sequence(wb));
}

/*
 * class:     org_rocksdb_writebatchinternal
 * method:    append
 * signature: (lorg/rocksdb/writebatch;lorg/rocksdb/writebatch;)v
 */
void java_org_rocksdb_writebatchinternal_append(
    jnienv* env, jclass jclazz, jobject jwb1, jobject jwb2) {
  rocksdb::writebatch* wb1 = rocksdb::writebatchjni::gethandle(env, jwb1);
  assert(wb1 != nullptr);
  rocksdb::writebatch* wb2 = rocksdb::writebatchjni::gethandle(env, jwb2);
  assert(wb2 != nullptr);

  rocksdb::writebatchinternal::append(wb1, wb2);
}

/*
 * class:     org_rocksdb_writebatchtest
 * method:    getcontents
 * signature: (lorg/rocksdb/writebatch;)[b
 */
jbytearray java_org_rocksdb_writebatchtest_getcontents(
    jnienv* env, jclass jclazz, jobject jobj) {
  rocksdb::writebatch* b = rocksdb::writebatchjni::gethandle(env, jobj);
  assert(b != nullptr);

  // todo: currently the following code is directly copied from
  // db/write_bench_test.cc.  it could be implemented in java once
  // all the necessary components can be accessed via jni api.

  rocksdb::internalkeycomparator cmp(rocksdb::bytewisecomparator());
  auto factory = std::make_shared<rocksdb::skiplistfactory>();
  rocksdb::options options;
  options.memtable_factory = factory;
  rocksdb::memtable* mem = new rocksdb::memtable(cmp, options);
  mem->ref();
  std::string state;
  rocksdb::columnfamilymemtablesdefault cf_mems_default(mem, &options);
  rocksdb::status s =
      rocksdb::writebatchinternal::insertinto(b, &cf_mems_default);
  int count = 0;
  rocksdb::iterator* iter = mem->newiterator(rocksdb::readoptions());
  for (iter->seektofirst(); iter->valid(); iter->next()) {
    rocksdb::parsedinternalkey ikey;
    memset(reinterpret_cast<void*>(&ikey), 0, sizeof(ikey));
    assert_true(rocksdb::parseinternalkey(iter->key(), &ikey));
    switch (ikey.type) {
      case rocksdb::ktypevalue:
        state.append("put(");
        state.append(ikey.user_key.tostring());
        state.append(", ");
        state.append(iter->value().tostring());
        state.append(")");
        count++;
        break;
      case rocksdb::ktypemerge:
        state.append("merge(");
        state.append(ikey.user_key.tostring());
        state.append(", ");
        state.append(iter->value().tostring());
        state.append(")");
        count++;
        break;
      case rocksdb::ktypedeletion:
        state.append("delete(");
        state.append(ikey.user_key.tostring());
        state.append(")");
        count++;
        break;
      default:
        assert(false);
        break;
    }
    state.append("@");
    state.append(rocksdb::numbertostring(ikey.sequence));
  }
  delete iter;
  if (!s.ok()) {
    state.append(s.tostring());
  } else if (count != rocksdb::writebatchinternal::count(b)) {
    state.append("countmismatch()");
  }
  delete mem->unref();

  jbytearray jstate = env->newbytearray(state.size());
  env->setbytearrayregion(
      jstate, 0, state.size(),
      reinterpret_cast<const jbyte*>(state.c_str()));

  return jstate;
}
