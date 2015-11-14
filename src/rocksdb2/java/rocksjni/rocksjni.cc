// copyright (c) 2014, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
//
// this file implements the "bridge" between java and c++ and enables
// calling c++ rocksdb::db methods from java side.

#include <stdio.h>
#include <stdlib.h>
#include <jni.h>
#include <string>
#include <vector>

#include "include/org_rocksdb_rocksdb.h"
#include "rocksjni/portal.h"
#include "rocksdb/db.h"
#include "rocksdb/cache.h"

//////////////////////////////////////////////////////////////////////////////
// rocksdb::db::open

/*
 * class:     org_rocksdb_rocksdb
 * method:    open
 * signature: (jljava/lang/string;)v
 */
void java_org_rocksdb_rocksdb_open(
    jnienv* env, jobject jdb, jlong jopt_handle, jstring jdb_path) {
  auto opt = reinterpret_cast<rocksdb::options*>(jopt_handle);
  rocksdb::db* db = nullptr;
  const char* db_path = env->getstringutfchars(jdb_path, 0);
  rocksdb::status s = rocksdb::db::open(*opt, db_path, &db);
  env->releasestringutfchars(jdb_path, db_path);

  if (s.ok()) {
    rocksdb::rocksdbjni::sethandle(env, jdb, db);
    return;
  }
  rocksdb::rocksdbexceptionjni::thrownew(env, s);
}

//////////////////////////////////////////////////////////////////////////////
// rocksdb::db::put

void rocksdb_put_helper(
    jnienv* env, rocksdb::db* db, const rocksdb::writeoptions& write_options,
    jbytearray jkey, jint jkey_len,
    jbytearray jvalue, jint jvalue_len) {

  jbyte* key = env->getbytearrayelements(jkey, 0);
  jbyte* value = env->getbytearrayelements(jvalue, 0);
  rocksdb::slice key_slice(reinterpret_cast<char*>(key), jkey_len);
  rocksdb::slice value_slice(reinterpret_cast<char*>(value), jvalue_len);

  rocksdb::status s = db->put(write_options, key_slice, value_slice);

  // trigger java unref on key and value.
  // by passing jni_abort, it will simply release the reference without
  // copying the result back to the java byte array.
  env->releasebytearrayelements(jkey, key, jni_abort);
  env->releasebytearrayelements(jvalue, value, jni_abort);

  if (s.ok()) {
    return;
  }
  rocksdb::rocksdbexceptionjni::thrownew(env, s);
}

/*
 * class:     org_rocksdb_rocksdb
 * method:    put
 * signature: (j[bi[bi)v
 */
void java_org_rocksdb_rocksdb_put__j_3bi_3bi(
    jnienv* env, jobject jdb, jlong jdb_handle,
    jbytearray jkey, jint jkey_len,
    jbytearray jvalue, jint jvalue_len) {
  auto db = reinterpret_cast<rocksdb::db*>(jdb_handle);
  static const rocksdb::writeoptions default_write_options =
      rocksdb::writeoptions();

  rocksdb_put_helper(env, db, default_write_options,
                     jkey, jkey_len,
                     jvalue, jvalue_len);
}

/*
 * class:     org_rocksdb_rocksdb
 * method:    put
 * signature: (jj[bi[bi)v
 */
void java_org_rocksdb_rocksdb_put__jj_3bi_3bi(
    jnienv* env, jobject jdb,
    jlong jdb_handle, jlong jwrite_options_handle,
    jbytearray jkey, jint jkey_len,
    jbytearray jvalue, jint jvalue_len) {
  auto db = reinterpret_cast<rocksdb::db*>(jdb_handle);
  auto write_options = reinterpret_cast<rocksdb::writeoptions*>(
      jwrite_options_handle);

  rocksdb_put_helper(env, db, *write_options,
                     jkey, jkey_len,
                     jvalue, jvalue_len);
}

//////////////////////////////////////////////////////////////////////////////
// rocksdb::db::write
/*
 * class:     org_rocksdb_rocksdb
 * method:    write
 * signature: (jj)v
 */
void java_org_rocksdb_rocksdb_write(
    jnienv* env, jobject jdb,
    jlong jwrite_options_handle, jlong jbatch_handle) {
  rocksdb::db* db = rocksdb::rocksdbjni::gethandle(env, jdb);
  auto write_options = reinterpret_cast<rocksdb::writeoptions*>(
      jwrite_options_handle);
  auto batch = reinterpret_cast<rocksdb::writebatch*>(jbatch_handle);

  rocksdb::status s = db->write(*write_options, batch);

  if (!s.ok()) {
    rocksdb::rocksdbexceptionjni::thrownew(env, s);
  }
}

//////////////////////////////////////////////////////////////////////////////
// rocksdb::db::get

jbytearray rocksdb_get_helper(
    jnienv* env, rocksdb::db* db, const rocksdb::readoptions& read_opt,
    jbytearray jkey, jint jkey_len) {
  jboolean iscopy;
  jbyte* key = env->getbytearrayelements(jkey, &iscopy);
  rocksdb::slice key_slice(
      reinterpret_cast<char*>(key), jkey_len);

  std::string value;
  rocksdb::status s = db->get(
      read_opt, key_slice, &value);

  // trigger java unref on key.
  // by passing jni_abort, it will simply release the reference without
  // copying the result back to the java byte array.
  env->releasebytearrayelements(jkey, key, jni_abort);

  if (s.isnotfound()) {
    return nullptr;
  }

  if (s.ok()) {
    jbytearray jvalue = env->newbytearray(value.size());
    env->setbytearrayregion(
        jvalue, 0, value.size(),
        reinterpret_cast<const jbyte*>(value.c_str()));
    return jvalue;
  }
  rocksdb::rocksdbexceptionjni::thrownew(env, s);

  return nullptr;
}

/*
 * class:     org_rocksdb_rocksdb
 * method:    get
 * signature: (j[bi)[b
 */
jbytearray java_org_rocksdb_rocksdb_get__j_3bi(
    jnienv* env, jobject jdb, jlong jdb_handle,
    jbytearray jkey, jint jkey_len) {
  return rocksdb_get_helper(env,
      reinterpret_cast<rocksdb::db*>(jdb_handle),
      rocksdb::readoptions(),
      jkey, jkey_len);
}

/*
 * class:     org_rocksdb_rocksdb
 * method:    get
 * signature: (jj[bi)[b
 */
jbytearray java_org_rocksdb_rocksdb_get__jj_3bi(
    jnienv* env, jobject jdb, jlong jdb_handle, jlong jropt_handle,
    jbytearray jkey, jint jkey_len) {
  return rocksdb_get_helper(env,
      reinterpret_cast<rocksdb::db*>(jdb_handle),
      *reinterpret_cast<rocksdb::readoptions*>(jropt_handle),
      jkey, jkey_len);
}

jint rocksdb_get_helper(
    jnienv* env, rocksdb::db* db, const rocksdb::readoptions& read_options,
    jbytearray jkey, jint jkey_len,
    jbytearray jvalue, jint jvalue_len) {
  static const int knotfound = -1;
  static const int kstatuserror = -2;

  jbyte* key = env->getbytearrayelements(jkey, 0);
  rocksdb::slice key_slice(
      reinterpret_cast<char*>(key), jkey_len);

  // todo(yhchiang): we might save one memory allocation here by adding
  // a db::get() function which takes preallocated jbyte* as input.
  std::string cvalue;
  rocksdb::status s = db->get(
      read_options, key_slice, &cvalue);

  // trigger java unref on key.
  // by passing jni_abort, it will simply release the reference without
  // copying the result back to the java byte array.
  env->releasebytearrayelements(jkey, key, jni_abort);

  if (s.isnotfound()) {
    return knotfound;
  } else if (!s.ok()) {
    // here since we are throwing a java exception from c++ side.
    // as a result, c++ does not know calling this function will in fact
    // throwing an exception.  as a result, the execution flow will
    // not stop here, and codes after this throw will still be
    // executed.
    rocksdb::rocksdbexceptionjni::thrownew(env, s);

    // return a dummy const value to avoid compilation error, although
    // java side might not have a chance to get the return value :)
    return kstatuserror;
  }

  int cvalue_len = static_cast<int>(cvalue.size());
  int length = std::min(jvalue_len, cvalue_len);

  env->setbytearrayregion(
      jvalue, 0, length,
      reinterpret_cast<const jbyte*>(cvalue.c_str()));
  return cvalue_len;
}

jobject multi_get_helper(jnienv* env, jobject jdb, rocksdb::db* db,
    const rocksdb::readoptions& ropt, jobject jkey_list, jint jkeys_count) {
  std::vector<rocksdb::slice> keys;
  std::vector<jbyte*> keys_to_free;

  // get iterator
  jobject iteratorobj = env->callobjectmethod(
      jkey_list, rocksdb::listjni::getiteratormethod(env));

  // iterate over keys and convert java byte array to slice
  while(env->callbooleanmethod(
      iteratorobj, rocksdb::listjni::gethasnextmethod(env)) == jni_true) {
    jbytearray jkey = (jbytearray) env->callobjectmethod(
       iteratorobj, rocksdb::listjni::getnextmethod(env));
    jint key_length = env->getarraylength(jkey);

    jbyte* key = new jbyte[key_length];
    env->getbytearrayregion(jkey, 0, key_length, key);
    // store allocated jbyte to free it after multiget call
    keys_to_free.push_back(key);

    rocksdb::slice key_slice(
      reinterpret_cast<char*>(key), key_length);
    keys.push_back(key_slice);
  }

  std::vector<std::string> values;
  std::vector<rocksdb::status> s = db->multiget(ropt, keys, &values);

  // don't reuse class pointer
  jclass jclazz = env->findclass("java/util/arraylist");
  jmethodid mid = rocksdb::listjni::getarraylistconstructormethodid(
      env, jclazz);
  jobject jvalue_list = env->newobject(jclazz, mid, jkeys_count);

  // insert in java list
  for(std::vector<rocksdb::status>::size_type i = 0; i != s.size(); i++) {
    if(s[i].ok()) {
      jbytearray jvalue = env->newbytearray(values[i].size());
      env->setbytearrayregion(
          jvalue, 0, values[i].size(),
          reinterpret_cast<const jbyte*>(values[i].c_str()));
      env->callbooleanmethod(
          jvalue_list, rocksdb::listjni::getlistaddmethodid(env), jvalue);
    }
    else {
      env->callbooleanmethod(
          jvalue_list, rocksdb::listjni::getlistaddmethodid(env), nullptr);
    }
  }

  // free up allocated byte arrays
  for(std::vector<jbyte*>::size_type i = 0; i != keys_to_free.size(); i++) {
    delete[] keys_to_free[i];
  }
  keys_to_free.clear();

  return jvalue_list;
}

/*
 * class:     org_rocksdb_rocksdb
 * method:    multiget
 * signature: (jljava/util/list;i)ljava/util/list;
 */
jobject java_org_rocksdb_rocksdb_multiget__jljava_util_list_2i(
    jnienv* env, jobject jdb, jlong jdb_handle,
    jobject jkey_list, jint jkeys_count) {
  return multi_get_helper(env, jdb, reinterpret_cast<rocksdb::db*>(jdb_handle),
      rocksdb::readoptions(), jkey_list, jkeys_count);
}

/*
 * class:     org_rocksdb_rocksdb
 * method:    multiget
 * signature: (jjljava/util/list;i)ljava/util/list;
 */
jobject java_org_rocksdb_rocksdb_multiget__jjljava_util_list_2i(
    jnienv* env, jobject jdb, jlong jdb_handle,
    jlong jropt_handle, jobject jkey_list, jint jkeys_count) {
  return multi_get_helper(env, jdb, reinterpret_cast<rocksdb::db*>(jdb_handle),
      *reinterpret_cast<rocksdb::readoptions*>(jropt_handle), jkey_list,
      jkeys_count);
}

/*
 * class:     org_rocksdb_rocksdb
 * method:    get
 * signature: (j[bi[bi)i
 */
jint java_org_rocksdb_rocksdb_get__j_3bi_3bi(
    jnienv* env, jobject jdb, jlong jdb_handle,
    jbytearray jkey, jint jkey_len,
    jbytearray jvalue, jint jvalue_len) {
  return rocksdb_get_helper(env,
      reinterpret_cast<rocksdb::db*>(jdb_handle),
      rocksdb::readoptions(),
      jkey, jkey_len, jvalue, jvalue_len);
}

/*
 * class:     org_rocksdb_rocksdb
 * method:    get
 * signature: (jj[bi[bi)i
 */
jint java_org_rocksdb_rocksdb_get__jj_3bi_3bi(
    jnienv* env, jobject jdb, jlong jdb_handle, jlong jropt_handle,
    jbytearray jkey, jint jkey_len,
    jbytearray jvalue, jint jvalue_len) {
  return rocksdb_get_helper(env,
      reinterpret_cast<rocksdb::db*>(jdb_handle),
      *reinterpret_cast<rocksdb::readoptions*>(jropt_handle),
      jkey, jkey_len, jvalue, jvalue_len);
}

//////////////////////////////////////////////////////////////////////////////
// rocksdb::db::delete()
void rocksdb_remove_helper(
    jnienv* env, rocksdb::db* db, const rocksdb::writeoptions& write_options,
    jbytearray jkey, jint jkey_len) {
  jbyte* key = env->getbytearrayelements(jkey, 0);
  rocksdb::slice key_slice(reinterpret_cast<char*>(key), jkey_len);

  rocksdb::status s = db->delete(write_options, key_slice);

  // trigger java unref on key and value.
  // by passing jni_abort, it will simply release the reference without
  // copying the result back to the java byte array.
  env->releasebytearrayelements(jkey, key, jni_abort);

  if (!s.ok()) {
    rocksdb::rocksdbexceptionjni::thrownew(env, s);
  }
  return;
}

/*
 * class:     org_rocksdb_rocksdb
 * method:    remove
 * signature: (j[bi)v
 */
void java_org_rocksdb_rocksdb_remove__j_3bi(
    jnienv* env, jobject jdb, jlong jdb_handle,
    jbytearray jkey, jint jkey_len) {
  auto db = reinterpret_cast<rocksdb::db*>(jdb_handle);
  static const rocksdb::writeoptions default_write_options =
      rocksdb::writeoptions();

  rocksdb_remove_helper(env, db, default_write_options, jkey, jkey_len);
}

/*
 * class:     org_rocksdb_rocksdb
 * method:    remove
 * signature: (jj[bi)v
 */
void java_org_rocksdb_rocksdb_remove__jj_3bi(
    jnienv* env, jobject jdb, jlong jdb_handle,
    jlong jwrite_options, jbytearray jkey, jint jkey_len) {
  auto db = reinterpret_cast<rocksdb::db*>(jdb_handle);
  auto write_options = reinterpret_cast<rocksdb::writeoptions*>(jwrite_options);

  rocksdb_remove_helper(env, db, *write_options, jkey, jkey_len);
}

//////////////////////////////////////////////////////////////////////////////
// rocksdb::db::~db()

/*
 * class:     org_rocksdb_rocksdb
 * method:    disposeinternal
 * signature: (j)v
 */
void java_org_rocksdb_rocksdb_disposeinternal(
    jnienv* env, jobject java_db, jlong jhandle) {
  delete reinterpret_cast<rocksdb::db*>(jhandle);
}

/*
 * class:     org_rocksdb_rocksdb
 * method:    iterator0
 * signature: (j)j
 */
jlong java_org_rocksdb_rocksdb_iterator0(
    jnienv* env, jobject jdb, jlong db_handle) {
  auto db = reinterpret_cast<rocksdb::db*>(db_handle);
  rocksdb::iterator* iterator = db->newiterator(rocksdb::readoptions());
  return reinterpret_cast<jlong>(iterator);
}
