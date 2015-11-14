// copyright (c) 2014, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
//
// this file implements the "bridge" between java and c++ and enables
// calling c++ rocksdb::statistics methods from java side.

#include <stdio.h>
#include <stdlib.h>
#include <jni.h>

#include "include/org_rocksdb_statistics.h"
#include "rocksjni/portal.h"
#include "rocksdb/statistics.h"

/*
 * class:     org_rocksdb_statistics
 * method:    gettickercount0
 * signature: (ij)j
 */
jlong java_org_rocksdb_statistics_gettickercount0(
    jnienv* env, jobject jobj, int tickertype, jlong handle) {
  auto st = reinterpret_cast<rocksdb::statistics*>(handle);
  assert(st != nullptr);

  return st->gettickercount(static_cast<rocksdb::tickers>(tickertype));
}

/*
 * class:     org_rocksdb_statistics
 * method:    gehistogramdata0
 * signature: (ij)lorg/rocksdb/histogramdata;
 */
jobject java_org_rocksdb_statistics_gehistogramdata0(
  jnienv* env, jobject jobj, int histogramtype, jlong handle) {
  auto st = reinterpret_cast<rocksdb::statistics*>(handle);
  assert(st != nullptr);

  rocksdb::histogramdata data;
  st->histogramdata(static_cast<rocksdb::histograms>(histogramtype),
    &data);

  // don't reuse class pointer
  jclass jclazz = env->findclass("org/rocksdb/histogramdata");
  jmethodid mid = rocksdb::histogramdatajni::getconstructormethodid(
      env, jclazz);
  return env->newobject(jclazz, mid, data.median, data.percentile95,
      data.percentile99, data.average, data.standard_deviation);
}
