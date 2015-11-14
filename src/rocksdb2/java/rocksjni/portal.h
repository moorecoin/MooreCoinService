// copyright (c) 2014, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

// this file is designed for caching those frequently used ids and provide
// efficient portal (i.e, a set of static functions) to access java code
// from c++.

#ifndef java_rocksjni_portal_h_
#define java_rocksjni_portal_h_

#include <jni.h>
#include "rocksdb/db.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/utilities/backupable_db.h"

namespace rocksdb {

// the portal class for org.rocksdb.rocksdb
class rocksdbjni {
 public:
  // get the java class id of org.rocksdb.rocksdb.
  static jclass getjclass(jnienv* env) {
    jclass jclazz = env->findclass("org/rocksdb/rocksdb");
    assert(jclazz != nullptr);
    return jclazz;
  }

  // get the field id of the member variable of org.rocksdb.rocksdb
  // that stores the pointer to rocksdb::db.
  static jfieldid gethandlefieldid(jnienv* env) {
    static jfieldid fid = env->getfieldid(
        getjclass(env), "nativehandle_", "j");
    assert(fid != nullptr);
    return fid;
  }

  // get the pointer to rocksdb::db of the specified org.rocksdb.rocksdb.
  static rocksdb::db* gethandle(jnienv* env, jobject jdb) {
    return reinterpret_cast<rocksdb::db*>(
        env->getlongfield(jdb, gethandlefieldid(env)));
  }

  // pass the rocksdb::db pointer to the java side.
  static void sethandle(jnienv* env, jobject jdb, rocksdb::db* db) {
    env->setlongfield(
        jdb, gethandlefieldid(env),
        reinterpret_cast<jlong>(db));
  }
};

// the portal class for org.rocksdb.rocksdbexception
class rocksdbexceptionjni {
 public:
  // get the jclass of org.rocksdb.rocksdbexception
  static jclass getjclass(jnienv* env) {
    jclass jclazz = env->findclass("org/rocksdb/rocksdbexception");
    assert(jclazz != nullptr);
    return jclazz;
  }

  // create and throw a java exception by converting the input
  // status to an rocksdbexception.
  //
  // in case s.ok() is true, then this function will not throw any
  // exception.
  static void thrownew(jnienv* env, status s) {
    if (s.ok()) {
      return;
    }
    jstring msg = env->newstringutf(s.tostring().c_str());
    // get the constructor id of org.rocksdb.rocksdbexception
    static jmethodid mid = env->getmethodid(
        getjclass(env), "<init>", "(ljava/lang/string;)v");
    assert(mid != nullptr);

    env->throw((jthrowable)env->newobject(getjclass(env), mid, msg));
  }
};

class optionsjni {
 public:
  // get the java class id of org.rocksdb.options.
  static jclass getjclass(jnienv* env) {
    jclass jclazz = env->findclass("org/rocksdb/options");
    assert(jclazz != nullptr);
    return jclazz;
  }

  // get the field id of the member variable of org.rocksdb.options
  // that stores the pointer to rocksdb::options
  static jfieldid gethandlefieldid(jnienv* env) {
    static jfieldid fid = env->getfieldid(
        getjclass(env), "nativehandle_", "j");
    assert(fid != nullptr);
    return fid;
  }

  // get the pointer to rocksdb::options
  static rocksdb::options* gethandle(jnienv* env, jobject jobj) {
    return reinterpret_cast<rocksdb::options*>(
        env->getlongfield(jobj, gethandlefieldid(env)));
  }

  // pass the rocksdb::options pointer to the java side.
  static void sethandle(jnienv* env, jobject jobj, rocksdb::options* op) {
    env->setlongfield(
        jobj, gethandlefieldid(env),
        reinterpret_cast<jlong>(op));
  }
};

class writeoptionsjni {
 public:
  // get the java class id of org.rocksdb.writeoptions.
  static jclass getjclass(jnienv* env) {
    jclass jclazz = env->findclass("org/rocksdb/writeoptions");
    assert(jclazz != nullptr);
    return jclazz;
  }

  // get the field id of the member variable of org.rocksdb.writeoptions
  // that stores the pointer to rocksdb::writeoptions
  static jfieldid gethandlefieldid(jnienv* env) {
    static jfieldid fid = env->getfieldid(
        getjclass(env), "nativehandle_", "j");
    assert(fid != nullptr);
    return fid;
  }

  // get the pointer to rocksdb::writeoptions
  static rocksdb::writeoptions* gethandle(jnienv* env, jobject jobj) {
    return reinterpret_cast<rocksdb::writeoptions*>(
        env->getlongfield(jobj, gethandlefieldid(env)));
  }

  // pass the rocksdb::writeoptions pointer to the java side.
  static void sethandle(jnienv* env, jobject jobj, rocksdb::writeoptions* op) {
    env->setlongfield(
        jobj, gethandlefieldid(env),
        reinterpret_cast<jlong>(op));
  }
};


class readoptionsjni {
 public:
  // get the java class id of org.rocksdb.readoptions.
  static jclass getjclass(jnienv* env) {
    jclass jclazz = env->findclass("org/rocksdb/readoptions");
    assert(jclazz != nullptr);
    return jclazz;
  }

  // get the field id of the member variable of org.rocksdb.readoptions
  // that stores the pointer to rocksdb::readoptions
  static jfieldid gethandlefieldid(jnienv* env) {
    static jfieldid fid = env->getfieldid(
        getjclass(env), "nativehandle_", "j");
    assert(fid != nullptr);
    return fid;
  }

  // get the pointer to rocksdb::readoptions
  static rocksdb::readoptions* gethandle(jnienv* env, jobject jobj) {
    return reinterpret_cast<rocksdb::readoptions*>(
        env->getlongfield(jobj, gethandlefieldid(env)));
  }

  // pass the rocksdb::readoptions pointer to the java side.
  static void sethandle(jnienv* env, jobject jobj,
                        rocksdb::readoptions* op) {
    env->setlongfield(
        jobj, gethandlefieldid(env),
        reinterpret_cast<jlong>(op));
  }
};


class writebatchjni {
 public:
  static jclass getjclass(jnienv* env) {
    jclass jclazz = env->findclass("org/rocksdb/writebatch");
    assert(jclazz != nullptr);
    return jclazz;
  }

  static jfieldid gethandlefieldid(jnienv* env) {
    static jfieldid fid = env->getfieldid(
        getjclass(env), "nativehandle_", "j");
    assert(fid != nullptr);
    return fid;
  }

  // get the pointer to rocksdb::writebatch of the specified
  // org.rocksdb.writebatch.
  static rocksdb::writebatch* gethandle(jnienv* env, jobject jwb) {
    return reinterpret_cast<rocksdb::writebatch*>(
        env->getlongfield(jwb, gethandlefieldid(env)));
  }

  // pass the rocksdb::writebatch pointer to the java side.
  static void sethandle(jnienv* env, jobject jwb, rocksdb::writebatch* wb) {
    env->setlongfield(
        jwb, gethandlefieldid(env),
        reinterpret_cast<jlong>(wb));
  }
};

class histogramdatajni {
 public:
  static jmethodid getconstructormethodid(jnienv* env, jclass jclazz) {
    static jmethodid mid = env->getmethodid(jclazz, "<init>", "(ddddd)v");
    assert(mid != nullptr);
    return mid;
  }
};

class backupabledboptionsjni {
 public:
  // get the java class id of org.rocksdb.backupabledboptions.
  static jclass getjclass(jnienv* env) {
    jclass jclazz = env->findclass("org/rocksdb/backupabledboptions");
    assert(jclazz != nullptr);
    return jclazz;
  }

  // get the field id of the member variable of org.rocksdb.backupabledboptions
  // that stores the pointer to rocksdb::backupabledboptions
  static jfieldid gethandlefieldid(jnienv* env) {
    static jfieldid fid = env->getfieldid(
        getjclass(env), "nativehandle_", "j");
    assert(fid != nullptr);
    return fid;
  }

  // get the pointer to rocksdb::backupabledboptions
  static rocksdb::backupabledboptions* gethandle(jnienv* env, jobject jobj) {
    return reinterpret_cast<rocksdb::backupabledboptions*>(
        env->getlongfield(jobj, gethandlefieldid(env)));
  }

  // pass the rocksdb::backupabledboptions pointer to the java side.
  static void sethandle(
      jnienv* env, jobject jobj, rocksdb::backupabledboptions* op) {
    env->setlongfield(
        jobj, gethandlefieldid(env),
        reinterpret_cast<jlong>(op));
  }
};

class iteratorjni {
 public:
  // get the java class id of org.rocksdb.iteartor.
  static jclass getjclass(jnienv* env) {
    jclass jclazz = env->findclass("org/rocksdb/rocksiterator");
    assert(jclazz != nullptr);
    return jclazz;
  }

  // get the field id of the member variable of org.rocksdb.iterator
  // that stores the pointer to rocksdb::iterator.
  static jfieldid gethandlefieldid(jnienv* env) {
    static jfieldid fid = env->getfieldid(
        getjclass(env), "nativehandle_", "j");
    assert(fid != nullptr);
    return fid;
  }

  // get the pointer to rocksdb::iterator.
  static rocksdb::iterator* gethandle(jnienv* env, jobject jobj) {
    return reinterpret_cast<rocksdb::iterator*>(
        env->getlongfield(jobj, gethandlefieldid(env)));
  }

  // pass the rocksdb::iterator pointer to the java side.
  static void sethandle(
      jnienv* env, jobject jobj, rocksdb::iterator* op) {
    env->setlongfield(
        jobj, gethandlefieldid(env),
        reinterpret_cast<jlong>(op));
  }
};

class filterjni {
 public:
  // get the java class id of org.rocksdb.filterpolicy.
  static jclass getjclass(jnienv* env) {
    jclass jclazz = env->findclass("org/rocksdb/filter");
    assert(jclazz != nullptr);
    return jclazz;
  }

  // get the field id of the member variable of org.rocksdb.filter
  // that stores the pointer to rocksdb::filterpolicy.
  static jfieldid gethandlefieldid(jnienv* env) {
    static jfieldid fid = env->getfieldid(
        getjclass(env), "nativehandle_", "j");
    assert(fid != nullptr);
    return fid;
  }

  // get the pointer to rocksdb::filterpolicy.
  static rocksdb::filterpolicy* gethandle(jnienv* env, jobject jobj) {
    return reinterpret_cast<rocksdb::filterpolicy*>(
        env->getlongfield(jobj, gethandlefieldid(env)));
  }

  // pass the rocksdb::filterpolicy pointer to the java side.
  static void sethandle(
      jnienv* env, jobject jobj, const rocksdb::filterpolicy* op) {
    env->setlongfield(
        jobj, gethandlefieldid(env),
        reinterpret_cast<jlong>(op));
  }
};

class listjni {
 public:
  // get the java class id of java.util.list.
  static jclass getlistclass(jnienv* env) {
    jclass jclazz = env->findclass("java/util/list");
    assert(jclazz != nullptr);
    return jclazz;
  }

  // get the java class id of java.util.arraylist.
  static jclass getarraylistclass(jnienv* env) {
    jclass jclazz = env->findclass("java/util/arraylist");
    assert(jclazz != nullptr);
    return jclazz;
  }

  // get the java class id of java.util.iterator.
  static jclass getiteratorclass(jnienv* env) {
    jclass jclazz = env->findclass("java/util/iterator");
    assert(jclazz != nullptr);
    return jclazz;
  }

  // get the java method id of java.util.list.iterator().
  static jmethodid getiteratormethod(jnienv* env) {
    static jmethodid mid = env->getmethodid(
        getlistclass(env), "iterator", "()ljava/util/iterator;");
    assert(mid != nullptr);
    return mid;
  }

  // get the java method id of java.util.iterator.hasnext().
  static jmethodid gethasnextmethod(jnienv* env) {
    static jmethodid mid = env->getmethodid(
        getiteratorclass(env), "hasnext", "()z");
    assert(mid != nullptr);
    return mid;
  }

  // get the java method id of java.util.iterator.next().
  static jmethodid getnextmethod(jnienv* env) {
    static jmethodid mid = env->getmethodid(
        getiteratorclass(env), "next", "()ljava/lang/object;");
    assert(mid != nullptr);
    return mid;
  }

  // get the java method id of arraylist constructor.
  static jmethodid getarraylistconstructormethodid(jnienv* env, jclass jclazz) {
    static jmethodid mid = env->getmethodid(
        jclazz, "<init>", "(i)v");
    assert(mid != nullptr);
    return mid;
  }

  // get the java method id of java.util.list.add().
  static jmethodid getlistaddmethodid(jnienv* env) {
    static jmethodid mid = env->getmethodid(
        getlistclass(env), "add", "(ljava/lang/object;)z");
    assert(mid != nullptr);
    return mid;
  }
};
}  // namespace rocksdb
#endif  // java_rocksjni_portal_h_
