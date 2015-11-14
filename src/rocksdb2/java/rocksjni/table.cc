// copyright (c) 2014, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
//
// this file implements the "bridge" between java and c++ for rocksdb::options.

#include <jni.h>
#include "include/org_rocksdb_plaintableconfig.h"
#include "include/org_rocksdb_blockbasedtableconfig.h"
#include "rocksdb/table.h"
#include "rocksdb/cache.h"
#include "rocksdb/filter_policy.h"

/*
 * class:     org_rocksdb_plaintableconfig
 * method:    newtablefactoryhandle
 * signature: (iidi)j
 */
jlong java_org_rocksdb_plaintableconfig_newtablefactoryhandle(
    jnienv* env, jobject jobj, jint jkey_size, jint jbloom_bits_per_key,
    jdouble jhash_table_ratio, jint jindex_sparseness) {
  rocksdb::plaintableoptions options = rocksdb::plaintableoptions();
  options.user_key_len = jkey_size;
  options.bloom_bits_per_key = jbloom_bits_per_key;
  options.hash_table_ratio = jhash_table_ratio;
  options.index_sparseness = jindex_sparseness;
  return reinterpret_cast<jlong>(rocksdb::newplaintablefactory(options));
}

/*
 * class:     org_rocksdb_blockbasedtableconfig
 * method:    newtablefactoryhandle
 * signature: (zjijiizi)j
 */
jlong java_org_rocksdb_blockbasedtableconfig_newtablefactoryhandle(
    jnienv* env, jobject jobj, jboolean no_block_cache, jlong block_cache_size,
    jint num_shardbits, jlong block_size, jint block_size_deviation,
    jint block_restart_interval, jboolean whole_key_filtering,
    jint bits_per_key) {
  rocksdb::blockbasedtableoptions options;
  options.no_block_cache = no_block_cache;

  if (!no_block_cache && block_cache_size > 0) {
    if (num_shardbits > 0) {
      options.block_cache =
          rocksdb::newlrucache(block_cache_size, num_shardbits);
    } else {
      options.block_cache = rocksdb::newlrucache(block_cache_size);
    }
  }
  options.block_size = block_size;
  options.block_size_deviation = block_size_deviation;
  options.block_restart_interval = block_restart_interval;
  options.whole_key_filtering = whole_key_filtering;
  if (bits_per_key > 0) {
    options.filter_policy.reset(rocksdb::newbloomfilterpolicy(bits_per_key));
  }
  return reinterpret_cast<jlong>(rocksdb::newblockbasedtablefactory(options));
}
