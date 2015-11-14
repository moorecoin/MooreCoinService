// copyright (c) 2014, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
//
// this file implements the "bridge" between java and c++ for rocksdb::options.

#include <stdio.h>
#include <stdlib.h>
#include <jni.h>
#include <string>
#include <memory>

#include "include/org_rocksdb_options.h"
#include "include/org_rocksdb_writeoptions.h"
#include "include/org_rocksdb_readoptions.h"
#include "rocksjni/portal.h"
#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/statistics.h"
#include "rocksdb/memtablerep.h"
#include "rocksdb/table.h"
#include "rocksdb/slice_transform.h"

/*
 * class:     org_rocksdb_options
 * method:    newoptions
 * signature: ()v
 */
void java_org_rocksdb_options_newoptions(jnienv* env, jobject jobj) {
  rocksdb::options* op = new rocksdb::options();
  rocksdb::optionsjni::sethandle(env, jobj, op);
}

/*
 * class:     org_rocksdb_options
 * method:    disposeinternal
 * signature: (j)v
 */
void java_org_rocksdb_options_disposeinternal(
    jnienv* env, jobject jobj, jlong handle) {
  delete reinterpret_cast<rocksdb::options*>(handle);
}

/*
 * class:     org_rocksdb_options
 * method:    setcreateifmissing
 * signature: (jz)v
 */
void java_org_rocksdb_options_setcreateifmissing(
    jnienv* env, jobject jobj, jlong jhandle, jboolean flag) {
  reinterpret_cast<rocksdb::options*>(jhandle)->create_if_missing = flag;
}

/*
 * class:     org_rocksdb_options
 * method:    createifmissing
 * signature: (j)z
 */
jboolean java_org_rocksdb_options_createifmissing(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->create_if_missing;
}

/*
 * class:     org_rocksdb_options
 * method:    setwritebuffersize
 * signature: (jj)i
 */
void java_org_rocksdb_options_setwritebuffersize(
    jnienv* env, jobject jobj, jlong jhandle, jlong jwrite_buffer_size) {
  reinterpret_cast<rocksdb::options*>(jhandle)->write_buffer_size =
          static_cast<size_t>(jwrite_buffer_size);
}


/*
 * class:     org_rocksdb_options
 * method:    writebuffersize
 * signature: (j)j
 */
jlong java_org_rocksdb_options_writebuffersize(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->write_buffer_size;
}

/*
 * class:     org_rocksdb_options
 * method:    setmaxwritebuffernumber
 * signature: (ji)v
 */
void java_org_rocksdb_options_setmaxwritebuffernumber(
    jnienv* env, jobject jobj, jlong jhandle, jint jmax_write_buffer_number) {
  reinterpret_cast<rocksdb::options*>(jhandle)->max_write_buffer_number =
          jmax_write_buffer_number;
}

/*
 * class:     org_rocksdb_options
 * method:    createstatistics
 * signature: (j)v
 */
void java_org_rocksdb_options_createstatistics(
    jnienv* env, jobject jobj, jlong jopthandle) {
  reinterpret_cast<rocksdb::options*>(jopthandle)->statistics =
      rocksdb::createdbstatistics();
}

/*
 * class:     org_rocksdb_options
 * method:    statisticsptr
 * signature: (j)j
 */
jlong java_org_rocksdb_options_statisticsptr(
    jnienv* env, jobject jobj, jlong jopthandle) {
  auto st = reinterpret_cast<rocksdb::options*>(jopthandle)->statistics.get();
  return reinterpret_cast<jlong>(st);
}

/*
 * class:     org_rocksdb_options
 * method:    maxwritebuffernumber
 * signature: (j)i
 */
jint java_org_rocksdb_options_maxwritebuffernumber(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->max_write_buffer_number;
}

/*
 * class:     org_rocksdb_options
 * method:    errorifexists
 * signature: (j)z
 */
jboolean java_org_rocksdb_options_errorifexists(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->error_if_exists;
}

/*
 * class:     org_rocksdb_options
 * method:    seterrorifexists
 * signature: (jz)v
 */
void java_org_rocksdb_options_seterrorifexists(
    jnienv* env, jobject jobj, jlong jhandle, jboolean error_if_exists) {
  reinterpret_cast<rocksdb::options*>(jhandle)->error_if_exists =
      static_cast<bool>(error_if_exists);
}

/*
 * class:     org_rocksdb_options
 * method:    paranoidchecks
 * signature: (j)z
 */
jboolean java_org_rocksdb_options_paranoidchecks(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->paranoid_checks;
}

/*
 * class:     org_rocksdb_options
 * method:    setparanoidchecks
 * signature: (jz)v
 */
void java_org_rocksdb_options_setparanoidchecks(
    jnienv* env, jobject jobj, jlong jhandle, jboolean paranoid_checks) {
  reinterpret_cast<rocksdb::options*>(jhandle)->paranoid_checks =
      static_cast<bool>(paranoid_checks);
}

/*
 * class:     org_rocksdb_options
 * method:    maxopenfiles
 * signature: (j)i
 */
jint java_org_rocksdb_options_maxopenfiles(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->max_open_files;
}

/*
 * class:     org_rocksdb_options
 * method:    setmaxopenfiles
 * signature: (ji)v
 */
void java_org_rocksdb_options_setmaxopenfiles(
    jnienv* env, jobject jobj, jlong jhandle, jint max_open_files) {
  reinterpret_cast<rocksdb::options*>(jhandle)->max_open_files =
      static_cast<int>(max_open_files);
}

/*
 * class:     org_rocksdb_options
 * method:    disabledatasync
 * signature: (j)z
 */
jboolean java_org_rocksdb_options_disabledatasync(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->disabledatasync;
}

/*
 * class:     org_rocksdb_options
 * method:    setdisabledatasync
 * signature: (jz)v
 */
void java_org_rocksdb_options_setdisabledatasync(
    jnienv* env, jobject jobj, jlong jhandle, jboolean disabledatasync) {
  reinterpret_cast<rocksdb::options*>(jhandle)->disabledatasync =
      static_cast<bool>(disabledatasync);
}

/*
 * class:     org_rocksdb_options
 * method:    usefsync
 * signature: (j)z
 */
jboolean java_org_rocksdb_options_usefsync(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->use_fsync;
}

/*
 * class:     org_rocksdb_options
 * method:    setusefsync
 * signature: (jz)v
 */
void java_org_rocksdb_options_setusefsync(
    jnienv* env, jobject jobj, jlong jhandle, jboolean use_fsync) {
  reinterpret_cast<rocksdb::options*>(jhandle)->use_fsync =
      static_cast<bool>(use_fsync);
}

/*
 * class:     org_rocksdb_options
 * method:    dblogdir
 * signature: (j)ljava/lang/string
 */
jstring java_org_rocksdb_options_dblogdir(
    jnienv* env, jobject jobj, jlong jhandle) {
  return env->newstringutf(
      reinterpret_cast<rocksdb::options*>(jhandle)->db_log_dir.c_str());
}

/*
 * class:     org_rocksdb_options
 * method:    setdblogdir
 * signature: (jljava/lang/string)v
 */
void java_org_rocksdb_options_setdblogdir(
    jnienv* env, jobject jobj, jlong jhandle, jstring jdb_log_dir) {
  const char* log_dir = env->getstringutfchars(jdb_log_dir, 0);
  reinterpret_cast<rocksdb::options*>(jhandle)->db_log_dir.assign(log_dir);
  env->releasestringutfchars(jdb_log_dir, log_dir);
}

/*
 * class:     org_rocksdb_options
 * method:    waldir
 * signature: (j)ljava/lang/string
 */
jstring java_org_rocksdb_options_waldir(
    jnienv* env, jobject jobj, jlong jhandle) {
  return env->newstringutf(
      reinterpret_cast<rocksdb::options*>(jhandle)->wal_dir.c_str());
}

/*
 * class:     org_rocksdb_options
 * method:    setwaldir
 * signature: (jljava/lang/string)v
 */
void java_org_rocksdb_options_setwaldir(
    jnienv* env, jobject jobj, jlong jhandle, jstring jwal_dir) {
  const char* wal_dir = env->getstringutfchars(jwal_dir, 0);
  reinterpret_cast<rocksdb::options*>(jhandle)->wal_dir.assign(wal_dir);
  env->releasestringutfchars(jwal_dir, wal_dir);
}

/*
 * class:     org_rocksdb_options
 * method:    deleteobsoletefilesperiodmicros
 * signature: (j)j
 */
jlong java_org_rocksdb_options_deleteobsoletefilesperiodmicros(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)
      ->delete_obsolete_files_period_micros;
}

/*
 * class:     org_rocksdb_options
 * method:    setdeleteobsoletefilesperiodmicros
 * signature: (jj)v
 */
void java_org_rocksdb_options_setdeleteobsoletefilesperiodmicros(
    jnienv* env, jobject jobj, jlong jhandle, jlong micros) {
  reinterpret_cast<rocksdb::options*>(jhandle)
      ->delete_obsolete_files_period_micros =
          static_cast<int64_t>(micros);
}

/*
 * class:     org_rocksdb_options
 * method:    maxbackgroundcompactions
 * signature: (j)i
 */
jint java_org_rocksdb_options_maxbackgroundcompactions(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->max_background_compactions;
}

/*
 * class:     org_rocksdb_options
 * method:    setmaxbackgroundcompactions
 * signature: (ji)v
 */
void java_org_rocksdb_options_setmaxbackgroundcompactions(
    jnienv* env, jobject jobj, jlong jhandle, jint max) {
  reinterpret_cast<rocksdb::options*>(jhandle)
      ->max_background_compactions = static_cast<int>(max);
}

/*
 * class:     org_rocksdb_options
 * method:    maxbackgroundflushes
 * signature: (j)i
 */
jint java_org_rocksdb_options_maxbackgroundflushes(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->max_background_flushes;
}

/*
 * class:     org_rocksdb_options
 * method:    setmaxbackgroundflushes
 * signature: (ji)v
 */
void java_org_rocksdb_options_setmaxbackgroundflushes(
    jnienv* env, jobject jobj, jlong jhandle, jint max_background_flushes) {
  reinterpret_cast<rocksdb::options*>(jhandle)->max_background_flushes =
      static_cast<int>(max_background_flushes);
}

/*
 * class:     org_rocksdb_options
 * method:    maxlogfilesize
 * signature: (j)j
 */
jlong java_org_rocksdb_options_maxlogfilesize(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->max_log_file_size;
}

/*
 * class:     org_rocksdb_options
 * method:    setmaxlogfilesize
 * signature: (jj)v
 */
void java_org_rocksdb_options_setmaxlogfilesize(
    jnienv* env, jobject jobj, jlong jhandle, jlong max_log_file_size) {
  reinterpret_cast<rocksdb::options*>(jhandle)->max_log_file_size =
      static_cast<size_t>(max_log_file_size);
}

/*
 * class:     org_rocksdb_options
 * method:    logfiletimetoroll
 * signature: (j)j
 */
jlong java_org_rocksdb_options_logfiletimetoroll(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->log_file_time_to_roll;
}

/*
 * class:     org_rocksdb_options
 * method:    setlogfiletimetoroll
 * signature: (jj)v
 */
void java_org_rocksdb_options_setlogfiletimetoroll(
    jnienv* env, jobject jobj, jlong jhandle, jlong log_file_time_to_roll) {
  reinterpret_cast<rocksdb::options*>(jhandle)->log_file_time_to_roll =
      static_cast<size_t>(log_file_time_to_roll);
}

/*
 * class:     org_rocksdb_options
 * method:    keeplogfilenum
 * signature: (j)j
 */
jlong java_org_rocksdb_options_keeplogfilenum(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->keep_log_file_num;
}

/*
 * class:     org_rocksdb_options
 * method:    setkeeplogfilenum
 * signature: (jj)v
 */
void java_org_rocksdb_options_setkeeplogfilenum(
    jnienv* env, jobject jobj, jlong jhandle, jlong keep_log_file_num) {
  reinterpret_cast<rocksdb::options*>(jhandle)->keep_log_file_num =
      static_cast<size_t>(keep_log_file_num);
}

/*
 * class:     org_rocksdb_options
 * method:    maxmanifestfilesize
 * signature: (j)j
 */
jlong java_org_rocksdb_options_maxmanifestfilesize(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->max_manifest_file_size;
}

/*
 * method:    memtablefactoryname
 * signature: (j)ljava/lang/string
 */
jstring java_org_rocksdb_options_memtablefactoryname(
    jnienv* env, jobject jobj, jlong jhandle) {
  auto opt = reinterpret_cast<rocksdb::options*>(jhandle);
  rocksdb::memtablerepfactory* tf = opt->memtable_factory.get();

  // should never be nullptr.
  // default memtable factory is skiplistfactory
  assert(tf);

  // temporarly fix for the historical typo
  if (strcmp(tf->name(), "hashlinklistrepfactory") == 0) {
    return env->newstringutf("hashlinkedlistrepfactory");
  }

  return env->newstringutf(tf->name());
}

/*
 * class:     org_rocksdb_options
 * method:    setmaxmanifestfilesize
 * signature: (jj)v
 */
void java_org_rocksdb_options_setmaxmanifestfilesize(
    jnienv* env, jobject jobj, jlong jhandle, jlong max_manifest_file_size) {
  reinterpret_cast<rocksdb::options*>(jhandle)->max_manifest_file_size =
      static_cast<int64_t>(max_manifest_file_size);
}

/*
 * method:    setmemtablefactory
 * signature: (jj)v
 */
void java_org_rocksdb_options_setmemtablefactory(
    jnienv* env, jobject jobj, jlong jhandle, jlong jfactory_handle) {
  reinterpret_cast<rocksdb::options*>(jhandle)->memtable_factory.reset(
      reinterpret_cast<rocksdb::memtablerepfactory*>(jfactory_handle));
}

/*
 * class:     org_rocksdb_options
 * method:    tablecachenumshardbits
 * signature: (j)i
 */
jint java_org_rocksdb_options_tablecachenumshardbits(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->table_cache_numshardbits;
}

/*
 * class:     org_rocksdb_options
 * method:    settablecachenumshardbits
 * signature: (ji)v
 */
void java_org_rocksdb_options_settablecachenumshardbits(
    jnienv* env, jobject jobj, jlong jhandle, jint table_cache_numshardbits) {
  reinterpret_cast<rocksdb::options*>(jhandle)->table_cache_numshardbits =
      static_cast<int>(table_cache_numshardbits);
}

/*
 * class:     org_rocksdb_options
 * method:    tablecacheremovescancountlimit
 * signature: (j)i
 */
jint java_org_rocksdb_options_tablecacheremovescancountlimit(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->table_cache_remove_scan_count_limit;
}

/*
 * class:     org_rocksdb_options
 * method:    settablecacheremovescancountlimit
 * signature: (ji)v
 */
void java_org_rocksdb_options_settablecacheremovescancountlimit(
    jnienv* env, jobject jobj, jlong jhandle, jint limit) {
  reinterpret_cast<rocksdb::options*>(
      jhandle)->table_cache_remove_scan_count_limit = static_cast<int>(limit);
}

/*
 * method:    usefixedlengthprefixextractor
 * signature: (ji)v
 */
void java_org_rocksdb_options_usefixedlengthprefixextractor(
    jnienv* env, jobject jobj, jlong jhandle, jint jprefix_length) {
  reinterpret_cast<rocksdb::options*>(jhandle)->prefix_extractor.reset(
      rocksdb::newfixedprefixtransform(static_cast<size_t>(jprefix_length)));
}

/*
 * class:     org_rocksdb_options
 * method:    walttlseconds
 * signature: (j)j
 */
jlong java_org_rocksdb_options_walttlseconds(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->wal_ttl_seconds;
}

/*
 * class:     org_rocksdb_options
 * method:    setwalttlseconds
 * signature: (jj)v
 */
void java_org_rocksdb_options_setwalttlseconds(
    jnienv* env, jobject jobj, jlong jhandle, jlong wal_ttl_seconds) {
  reinterpret_cast<rocksdb::options*>(jhandle)->wal_ttl_seconds =
      static_cast<int64_t>(wal_ttl_seconds);
}

/*
 * class:     org_rocksdb_options
 * method:    walttlseconds
 * signature: (j)j
 */
jlong java_org_rocksdb_options_walsizelimitmb(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->wal_size_limit_mb;
}

/*
 * class:     org_rocksdb_options
 * method:    setwalsizelimitmb
 * signature: (jj)v
 */
void java_org_rocksdb_options_setwalsizelimitmb(
    jnienv* env, jobject jobj, jlong jhandle, jlong wal_size_limit_mb) {
  reinterpret_cast<rocksdb::options*>(jhandle)->wal_size_limit_mb =
      static_cast<int64_t>(wal_size_limit_mb);
}

/*
 * class:     org_rocksdb_options
 * method:    manifestpreallocationsize
 * signature: (j)j
 */
jlong java_org_rocksdb_options_manifestpreallocationsize(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)
      ->manifest_preallocation_size;
}

/*
 * class:     org_rocksdb_options
 * method:    setmanifestpreallocationsize
 * signature: (jj)v
 */
void java_org_rocksdb_options_setmanifestpreallocationsize(
    jnienv* env, jobject jobj, jlong jhandle, jlong preallocation_size) {
  reinterpret_cast<rocksdb::options*>(jhandle)->manifest_preallocation_size =
      static_cast<size_t>(preallocation_size);
}

/*
 * class:     org_rocksdb_options
 * method:    allowosbuffer
 * signature: (j)z
 */
jboolean java_org_rocksdb_options_allowosbuffer(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->allow_os_buffer;
}

/*
 * class:     org_rocksdb_options
 * method:    setallowosbuffer
 * signature: (jz)v
 */
void java_org_rocksdb_options_setallowosbuffer(
    jnienv* env, jobject jobj, jlong jhandle, jboolean allow_os_buffer) {
  reinterpret_cast<rocksdb::options*>(jhandle)->allow_os_buffer =
      static_cast<bool>(allow_os_buffer);
}

/*
 * method:    settablefactory
 * signature: (jj)v
 */
void java_org_rocksdb_options_settablefactory(
    jnienv* env, jobject jobj, jlong jhandle, jlong jfactory_handle) {
  reinterpret_cast<rocksdb::options*>(jhandle)->table_factory.reset(
      reinterpret_cast<rocksdb::tablefactory*>(jfactory_handle));
}

/*
 * class:     org_rocksdb_options
 * method:    allowmmapreads
 * signature: (j)z
 */
jboolean java_org_rocksdb_options_allowmmapreads(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->allow_mmap_reads;
}

/*
 * class:     org_rocksdb_options
 * method:    setallowmmapreads
 * signature: (jz)v
 */
void java_org_rocksdb_options_setallowmmapreads(
    jnienv* env, jobject jobj, jlong jhandle, jboolean allow_mmap_reads) {
  reinterpret_cast<rocksdb::options*>(jhandle)->allow_mmap_reads =
      static_cast<bool>(allow_mmap_reads);
}

/*
 * class:     org_rocksdb_options
 * method:    allowmmapwrites
 * signature: (j)z
 */
jboolean java_org_rocksdb_options_allowmmapwrites(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->allow_mmap_writes;
}

/*
 * class:     org_rocksdb_options
 * method:    setallowmmapwrites
 * signature: (jz)v
 */
void java_org_rocksdb_options_setallowmmapwrites(
    jnienv* env, jobject jobj, jlong jhandle, jboolean allow_mmap_writes) {
  reinterpret_cast<rocksdb::options*>(jhandle)->allow_mmap_writes =
      static_cast<bool>(allow_mmap_writes);
}

/*
 * class:     org_rocksdb_options
 * method:    isfdcloseonexec
 * signature: (j)z
 */
jboolean java_org_rocksdb_options_isfdcloseonexec(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->is_fd_close_on_exec;
}

/*
 * class:     org_rocksdb_options
 * method:    setisfdcloseonexec
 * signature: (jz)v
 */
void java_org_rocksdb_options_setisfdcloseonexec(
    jnienv* env, jobject jobj, jlong jhandle, jboolean is_fd_close_on_exec) {
  reinterpret_cast<rocksdb::options*>(jhandle)->is_fd_close_on_exec =
      static_cast<bool>(is_fd_close_on_exec);
}

/*
 * class:     org_rocksdb_options
 * method:    skiplogerroronrecovery
 * signature: (j)z
 */
jboolean java_org_rocksdb_options_skiplogerroronrecovery(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)
      ->skip_log_error_on_recovery;
}

/*
 * class:     org_rocksdb_options
 * method:    setskiplogerroronrecovery
 * signature: (jz)v
 */
void java_org_rocksdb_options_setskiplogerroronrecovery(
    jnienv* env, jobject jobj, jlong jhandle, jboolean skip) {
  reinterpret_cast<rocksdb::options*>(jhandle)->skip_log_error_on_recovery =
      static_cast<bool>(skip);
}

/*
 * class:     org_rocksdb_options
 * method:    statsdumpperiodsec
 * signature: (j)i
 */
jint java_org_rocksdb_options_statsdumpperiodsec(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->stats_dump_period_sec;
}

/*
 * class:     org_rocksdb_options
 * method:    setstatsdumpperiodsec
 * signature: (ji)v
 */
void java_org_rocksdb_options_setstatsdumpperiodsec(
    jnienv* env, jobject jobj, jlong jhandle, jint stats_dump_period_sec) {
  reinterpret_cast<rocksdb::options*>(jhandle)->stats_dump_period_sec =
      static_cast<int>(stats_dump_period_sec);
}

/*
 * class:     org_rocksdb_options
 * method:    adviserandomonopen
 * signature: (j)z
 */
jboolean java_org_rocksdb_options_adviserandomonopen(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->advise_random_on_open;
}

/*
 * class:     org_rocksdb_options
 * method:    setadviserandomonopen
 * signature: (jz)v
 */
void java_org_rocksdb_options_setadviserandomonopen(
    jnienv* env, jobject jobj, jlong jhandle, jboolean advise_random_on_open) {
  reinterpret_cast<rocksdb::options*>(jhandle)->advise_random_on_open =
      static_cast<bool>(advise_random_on_open);
}

/*
 * class:     org_rocksdb_options
 * method:    useadaptivemutex
 * signature: (j)z
 */
jboolean java_org_rocksdb_options_useadaptivemutex(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->use_adaptive_mutex;
}

/*
 * class:     org_rocksdb_options
 * method:    setuseadaptivemutex
 * signature: (jz)v
 */
void java_org_rocksdb_options_setuseadaptivemutex(
    jnienv* env, jobject jobj, jlong jhandle, jboolean use_adaptive_mutex) {
  reinterpret_cast<rocksdb::options*>(jhandle)->use_adaptive_mutex =
      static_cast<bool>(use_adaptive_mutex);
}

/*
 * class:     org_rocksdb_options
 * method:    bytespersync
 * signature: (j)j
 */
jlong java_org_rocksdb_options_bytespersync(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->bytes_per_sync;
}

/*
 * class:     org_rocksdb_options
 * method:    setbytespersync
 * signature: (jj)v
 */
void java_org_rocksdb_options_setbytespersync(
    jnienv* env, jobject jobj, jlong jhandle, jlong bytes_per_sync) {
  reinterpret_cast<rocksdb::options*>(jhandle)->bytes_per_sync =
      static_cast<int64_t>(bytes_per_sync);
}

/*
 * class:     org_rocksdb_options
 * method:    allowthreadlocal
 * signature: (j)z
 */
jboolean java_org_rocksdb_options_allowthreadlocal(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->allow_thread_local;
}

/*
 * class:     org_rocksdb_options
 * method:    setallowthreadlocal
 * signature: (jz)v
 */
void java_org_rocksdb_options_setallowthreadlocal(
    jnienv* env, jobject jobj, jlong jhandle, jboolean allow_thread_local) {
  reinterpret_cast<rocksdb::options*>(jhandle)->allow_thread_local =
      static_cast<bool>(allow_thread_local);
}

/*
 * method:    tablefactoryname
 * signature: (j)ljava/lang/string
 */
jstring java_org_rocksdb_options_tablefactoryname(
    jnienv* env, jobject jobj, jlong jhandle) {
  auto opt = reinterpret_cast<rocksdb::options*>(jhandle);
  rocksdb::tablefactory* tf = opt->table_factory.get();

  // should never be nullptr.
  // default memtable factory is skiplistfactory
  assert(tf);

  return env->newstringutf(tf->name());
}


/*
 * class:     org_rocksdb_options
 * method:    minwritebuffernumbertomerge
 * signature: (j)i
 */
jint java_org_rocksdb_options_minwritebuffernumbertomerge(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->min_write_buffer_number_to_merge;
}

/*
 * class:     org_rocksdb_options
 * method:    setminwritebuffernumbertomerge
 * signature: (ji)v
 */
void java_org_rocksdb_options_setminwritebuffernumbertomerge(
    jnienv* env, jobject jobj, jlong jhandle,
    jint jmin_write_buffer_number_to_merge) {
  reinterpret_cast<rocksdb::options*>(
      jhandle)->min_write_buffer_number_to_merge =
          static_cast<int>(jmin_write_buffer_number_to_merge);
}

/*
 * class:     org_rocksdb_options
 * method:    setcompressiontype
 * signature: (jb)v
 */
void java_org_rocksdb_options_setcompressiontype(
    jnienv* env, jobject jobj, jlong jhandle, jbyte compression) {
  reinterpret_cast<rocksdb::options*>(jhandle)->compression =
      static_cast<rocksdb::compressiontype>(compression);
}

/*
 * class:     org_rocksdb_options
 * method:    compressiontype
 * signature: (j)b
 */
jbyte java_org_rocksdb_options_compressiontype(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->compression;
}

/*
 * class:     org_rocksdb_options
 * method:    setcompactionstyle
 * signature: (jb)v
 */
void java_org_rocksdb_options_setcompactionstyle(
    jnienv* env, jobject jobj, jlong jhandle, jbyte compaction_style) {
  reinterpret_cast<rocksdb::options*>(jhandle)->compaction_style =
      static_cast<rocksdb::compactionstyle>(compaction_style);
}

/*
 * class:     org_rocksdb_options
 * method:    compactionstyle
 * signature: (j)b
 */
jbyte java_org_rocksdb_options_compactionstyle(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->compaction_style;
}

/*
 * class:     org_rocksdb_options
 * method:    numlevels
 * signature: (j)i
 */
jint java_org_rocksdb_options_numlevels(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->num_levels;
}

/*
 * class:     org_rocksdb_options
 * method:    setnumlevels
 * signature: (ji)v
 */
void java_org_rocksdb_options_setnumlevels(
    jnienv* env, jobject jobj, jlong jhandle, jint jnum_levels) {
  reinterpret_cast<rocksdb::options*>(jhandle)->num_levels =
      static_cast<int>(jnum_levels);
}

/*
 * class:     org_rocksdb_options
 * method:    levelzerofilenumcompactiontrigger
 * signature: (j)i
 */
jint java_org_rocksdb_options_levelzerofilenumcompactiontrigger(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->level0_file_num_compaction_trigger;
}

/*
 * class:     org_rocksdb_options
 * method:    setlevelzerofilenumcompactiontrigger
 * signature: (ji)v
 */
void java_org_rocksdb_options_setlevelzerofilenumcompactiontrigger(
    jnienv* env, jobject jobj, jlong jhandle,
    jint jlevel0_file_num_compaction_trigger) {
  reinterpret_cast<rocksdb::options*>(
      jhandle)->level0_file_num_compaction_trigger =
          static_cast<int>(jlevel0_file_num_compaction_trigger);
}

/*
 * class:     org_rocksdb_options
 * method:    levelzeroslowdownwritestrigger
 * signature: (j)i
 */
jint java_org_rocksdb_options_levelzeroslowdownwritestrigger(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->level0_slowdown_writes_trigger;
}

/*
 * class:     org_rocksdb_options
 * method:    setlevelslowdownwritestrigger
 * signature: (ji)v
 */
void java_org_rocksdb_options_setlevelzeroslowdownwritestrigger(
    jnienv* env, jobject jobj, jlong jhandle,
    jint jlevel0_slowdown_writes_trigger) {
  reinterpret_cast<rocksdb::options*>(
      jhandle)->level0_slowdown_writes_trigger =
          static_cast<int>(jlevel0_slowdown_writes_trigger);
}

/*
 * class:     org_rocksdb_options
 * method:    levelzerostopwritestrigger
 * signature: (j)i
 */
jint java_org_rocksdb_options_levelzerostopwritestrigger(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->level0_stop_writes_trigger;
}

/*
 * class:     org_rocksdb_options
 * method:    setlevelstopwritestrigger
 * signature: (ji)v
 */
void java_org_rocksdb_options_setlevelzerostopwritestrigger(
    jnienv* env, jobject jobj, jlong jhandle,
    jint jlevel0_stop_writes_trigger) {
  reinterpret_cast<rocksdb::options*>(jhandle)->level0_stop_writes_trigger =
      static_cast<int>(jlevel0_stop_writes_trigger);
}

/*
 * class:     org_rocksdb_options
 * method:    maxmemcompactionlevel
 * signature: (j)i
 */
jint java_org_rocksdb_options_maxmemcompactionlevel(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->max_mem_compaction_level;
}

/*
 * class:     org_rocksdb_options
 * method:    setmaxmemcompactionlevel
 * signature: (ji)v
 */
void java_org_rocksdb_options_setmaxmemcompactionlevel(
    jnienv* env, jobject jobj, jlong jhandle,
    jint jmax_mem_compaction_level) {
  reinterpret_cast<rocksdb::options*>(jhandle)->max_mem_compaction_level =
      static_cast<int>(jmax_mem_compaction_level);
}

/*
 * class:     org_rocksdb_options
 * method:    targetfilesizebase
 * signature: (j)i
 */
jint java_org_rocksdb_options_targetfilesizebase(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->target_file_size_base;
}

/*
 * class:     org_rocksdb_options
 * method:    settargetfilesizebase
 * signature: (ji)v
 */
void java_org_rocksdb_options_settargetfilesizebase(
    jnienv* env, jobject jobj, jlong jhandle,
    jint jtarget_file_size_base) {
  reinterpret_cast<rocksdb::options*>(jhandle)->target_file_size_base =
      static_cast<int>(jtarget_file_size_base);
}

/*
 * class:     org_rocksdb_options
 * method:    targetfilesizemultiplier
 * signature: (j)i
 */
jint java_org_rocksdb_options_targetfilesizemultiplier(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->target_file_size_multiplier;
}

/*
 * class:     org_rocksdb_options
 * method:    settargetfilesizemultiplier
 * signature: (ji)v
 */
void java_org_rocksdb_options_settargetfilesizemultiplier(
    jnienv* env, jobject jobj, jlong jhandle,
    jint jtarget_file_size_multiplier) {
  reinterpret_cast<rocksdb::options*>(
      jhandle)->target_file_size_multiplier =
          static_cast<int>(jtarget_file_size_multiplier);
}

/*
 * class:     org_rocksdb_options
 * method:    maxbytesforlevelbase
 * signature: (j)j
 */
jlong java_org_rocksdb_options_maxbytesforlevelbase(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->max_bytes_for_level_base;
}

/*
 * class:     org_rocksdb_options
 * method:    setmaxbytesforlevelbase
 * signature: (jj)v
 */
void java_org_rocksdb_options_setmaxbytesforlevelbase(
    jnienv* env, jobject jobj, jlong jhandle,
    jlong jmax_bytes_for_level_base) {
  reinterpret_cast<rocksdb::options*>(
      jhandle)->max_bytes_for_level_base =
          static_cast<int64_t>(jmax_bytes_for_level_base);
}

/*
 * class:     org_rocksdb_options
 * method:    maxbytesforlevelmultiplier
 * signature: (j)i
 */
jint java_org_rocksdb_options_maxbytesforlevelmultiplier(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->max_bytes_for_level_multiplier;
}

/*
 * class:     org_rocksdb_options
 * method:    setmaxbytesforlevelmultiplier
 * signature: (ji)v
 */
void java_org_rocksdb_options_setmaxbytesforlevelmultiplier(
    jnienv* env, jobject jobj, jlong jhandle,
    jint jmax_bytes_for_level_multiplier) {
  reinterpret_cast<rocksdb::options*>(
      jhandle)->max_bytes_for_level_multiplier =
          static_cast<int>(jmax_bytes_for_level_multiplier);
}

/*
 * class:     org_rocksdb_options
 * method:    expandedcompactionfactor
 * signature: (j)i
 */
jint java_org_rocksdb_options_expandedcompactionfactor(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->expanded_compaction_factor;
}

/*
 * class:     org_rocksdb_options
 * method:    setexpandedcompactionfactor
 * signature: (ji)v
 */
void java_org_rocksdb_options_setexpandedcompactionfactor(
    jnienv* env, jobject jobj, jlong jhandle,
    jint jexpanded_compaction_factor) {
  reinterpret_cast<rocksdb::options*>(
      jhandle)->expanded_compaction_factor =
          static_cast<int>(jexpanded_compaction_factor);
}

/*
 * class:     org_rocksdb_options
 * method:    sourcecompactionfactor
 * signature: (j)i
 */
jint java_org_rocksdb_options_sourcecompactionfactor(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->source_compaction_factor;
}

/*
 * class:     org_rocksdb_options
 * method:    setsourcecompactionfactor
 * signature: (ji)v
 */
void java_org_rocksdb_options_setsourcecompactionfactor(
    jnienv* env, jobject jobj, jlong jhandle,
        jint jsource_compaction_factor) {
  reinterpret_cast<rocksdb::options*>(
      jhandle)->source_compaction_factor =
          static_cast<int>(jsource_compaction_factor);
}

/*
 * class:     org_rocksdb_options
 * method:    maxgrandparentoverlapfactor
 * signature: (j)i
 */
jint java_org_rocksdb_options_maxgrandparentoverlapfactor(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->max_grandparent_overlap_factor;
}

/*
 * class:     org_rocksdb_options
 * method:    setmaxgrandparentoverlapfactor
 * signature: (ji)v
 */
void java_org_rocksdb_options_setmaxgrandparentoverlapfactor(
    jnienv* env, jobject jobj, jlong jhandle,
    jint jmax_grandparent_overlap_factor) {
  reinterpret_cast<rocksdb::options*>(
      jhandle)->max_grandparent_overlap_factor =
          static_cast<int>(jmax_grandparent_overlap_factor);
}

/*
 * class:     org_rocksdb_options
 * method:    softratelimit
 * signature: (j)d
 */
jdouble java_org_rocksdb_options_softratelimit(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->soft_rate_limit;
}

/*
 * class:     org_rocksdb_options
 * method:    setsoftratelimit
 * signature: (jd)v
 */
void java_org_rocksdb_options_setsoftratelimit(
    jnienv* env, jobject jobj, jlong jhandle, jdouble jsoft_rate_limit) {
  reinterpret_cast<rocksdb::options*>(jhandle)->soft_rate_limit =
      static_cast<double>(jsoft_rate_limit);
}

/*
 * class:     org_rocksdb_options
 * method:    hardratelimit
 * signature: (j)d
 */
jdouble java_org_rocksdb_options_hardratelimit(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->hard_rate_limit;
}

/*
 * class:     org_rocksdb_options
 * method:    sethardratelimit
 * signature: (jd)v
 */
void java_org_rocksdb_options_sethardratelimit(
    jnienv* env, jobject jobj, jlong jhandle, jdouble jhard_rate_limit) {
  reinterpret_cast<rocksdb::options*>(jhandle)->hard_rate_limit =
      static_cast<double>(jhard_rate_limit);
}

/*
 * class:     org_rocksdb_options
 * method:    ratelimitdelaymaxmilliseconds
 * signature: (j)i
 */
jint java_org_rocksdb_options_ratelimitdelaymaxmilliseconds(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->rate_limit_delay_max_milliseconds;
}

/*
 * class:     org_rocksdb_options
 * method:    setratelimitdelaymaxmilliseconds
 * signature: (ji)v
 */
void java_org_rocksdb_options_setratelimitdelaymaxmilliseconds(
    jnienv* env, jobject jobj, jlong jhandle,
    jint jrate_limit_delay_max_milliseconds) {
  reinterpret_cast<rocksdb::options*>(
      jhandle)->rate_limit_delay_max_milliseconds =
          static_cast<int>(jrate_limit_delay_max_milliseconds);
}

/*
 * class:     org_rocksdb_options
 * method:    arenablocksize
 * signature: (j)j
 */
jlong java_org_rocksdb_options_arenablocksize(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->arena_block_size;
}

/*
 * class:     org_rocksdb_options
 * method:    setarenablocksize
 * signature: (jj)v
 */
void java_org_rocksdb_options_setarenablocksize(
    jnienv* env, jobject jobj, jlong jhandle, jlong jarena_block_size) {
  reinterpret_cast<rocksdb::options*>(jhandle)->arena_block_size =
      static_cast<size_t>(jarena_block_size);
}

/*
 * class:     org_rocksdb_options
 * method:    disableautocompactions
 * signature: (j)z
 */
jboolean java_org_rocksdb_options_disableautocompactions(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->disable_auto_compactions;
}

/*
 * class:     org_rocksdb_options
 * method:    setdisableautocompactions
 * signature: (jz)v
 */
void java_org_rocksdb_options_setdisableautocompactions(
    jnienv* env, jobject jobj, jlong jhandle,
    jboolean jdisable_auto_compactions) {
  reinterpret_cast<rocksdb::options*>(
      jhandle)->disable_auto_compactions =
          static_cast<bool>(jdisable_auto_compactions);
}

/*
 * class:     org_rocksdb_options
 * method:    purgeredundantkvswhileflush
 * signature: (j)z
 */
jboolean java_org_rocksdb_options_purgeredundantkvswhileflush(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->purge_redundant_kvs_while_flush;
}

/*
 * class:     org_rocksdb_options
 * method:    setpurgeredundantkvswhileflush
 * signature: (jz)v
 */
void java_org_rocksdb_options_setpurgeredundantkvswhileflush(
    jnienv* env, jobject jobj, jlong jhandle,
    jboolean jpurge_redundant_kvs_while_flush) {
  reinterpret_cast<rocksdb::options*>(
      jhandle)->purge_redundant_kvs_while_flush =
          static_cast<bool>(jpurge_redundant_kvs_while_flush);
}

/*
 * class:     org_rocksdb_options
 * method:    verifychecksumsincompaction
 * signature: (j)z
 */
jboolean java_org_rocksdb_options_verifychecksumsincompaction(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->verify_checksums_in_compaction;
}

/*
 * class:     org_rocksdb_options
 * method:    setverifychecksumsincompaction
 * signature: (jz)v
 */
void java_org_rocksdb_options_setverifychecksumsincompaction(
    jnienv* env, jobject jobj, jlong jhandle,
    jboolean jverify_checksums_in_compaction) {
  reinterpret_cast<rocksdb::options*>(
      jhandle)->verify_checksums_in_compaction =
          static_cast<bool>(jverify_checksums_in_compaction);
}

/*
 * class:     org_rocksdb_options
 * method:    filterdeletes
 * signature: (j)z
 */
jboolean java_org_rocksdb_options_filterdeletes(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->filter_deletes;
}

/*
 * class:     org_rocksdb_options
 * method:    setfilterdeletes
 * signature: (jz)v
 */
void java_org_rocksdb_options_setfilterdeletes(
    jnienv* env, jobject jobj, jlong jhandle, jboolean jfilter_deletes) {
  reinterpret_cast<rocksdb::options*>(jhandle)->filter_deletes =
      static_cast<bool>(jfilter_deletes);
}

/*
 * class:     org_rocksdb_options
 * method:    maxsequentialskipiniterations
 * signature: (j)j
 */
jlong java_org_rocksdb_options_maxsequentialskipiniterations(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->max_sequential_skip_in_iterations;
}

/*
 * class:     org_rocksdb_options
 * method:    setmaxsequentialskipiniterations
 * signature: (jj)v
 */
void java_org_rocksdb_options_setmaxsequentialskipiniterations(
    jnienv* env, jobject jobj, jlong jhandle,
    jlong jmax_sequential_skip_in_iterations) {
  reinterpret_cast<rocksdb::options*>(
      jhandle)->max_sequential_skip_in_iterations =
          static_cast<int64_t>(jmax_sequential_skip_in_iterations);
}

/*
 * class:     org_rocksdb_options
 * method:    inplaceupdatesupport
 * signature: (j)z
 */
jboolean java_org_rocksdb_options_inplaceupdatesupport(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->inplace_update_support;
}

/*
 * class:     org_rocksdb_options
 * method:    setinplaceupdatesupport
 * signature: (jz)v
 */
void java_org_rocksdb_options_setinplaceupdatesupport(
    jnienv* env, jobject jobj, jlong jhandle,
    jboolean jinplace_update_support) {
  reinterpret_cast<rocksdb::options*>(
      jhandle)->inplace_update_support =
          static_cast<bool>(jinplace_update_support);
}

/*
 * class:     org_rocksdb_options
 * method:    inplaceupdatenumlocks
 * signature: (j)j
 */
jlong java_org_rocksdb_options_inplaceupdatenumlocks(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->inplace_update_num_locks;
}

/*
 * class:     org_rocksdb_options
 * method:    setinplaceupdatenumlocks
 * signature: (jj)v
 */
void java_org_rocksdb_options_setinplaceupdatenumlocks(
    jnienv* env, jobject jobj, jlong jhandle,
    jlong jinplace_update_num_locks) {
  reinterpret_cast<rocksdb::options*>(
      jhandle)->inplace_update_num_locks =
          static_cast<size_t>(jinplace_update_num_locks);
}

/*
 * class:     org_rocksdb_options
 * method:    memtableprefixbloombits
 * signature: (j)i
 */
jint java_org_rocksdb_options_memtableprefixbloombits(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->memtable_prefix_bloom_bits;
}

/*
 * class:     org_rocksdb_options
 * method:    setmemtableprefixbloombits
 * signature: (ji)v
 */
void java_org_rocksdb_options_setmemtableprefixbloombits(
    jnienv* env, jobject jobj, jlong jhandle,
    jint jmemtable_prefix_bloom_bits) {
  reinterpret_cast<rocksdb::options*>(
      jhandle)->memtable_prefix_bloom_bits =
          static_cast<int32_t>(jmemtable_prefix_bloom_bits);
}

/*
 * class:     org_rocksdb_options
 * method:    memtableprefixbloomprobes
 * signature: (j)i
 */
jint java_org_rocksdb_options_memtableprefixbloomprobes(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->memtable_prefix_bloom_probes;
}

/*
 * class:     org_rocksdb_options
 * method:    setmemtableprefixbloomprobes
 * signature: (ji)v
 */
void java_org_rocksdb_options_setmemtableprefixbloomprobes(
    jnienv* env, jobject jobj, jlong jhandle,
    jint jmemtable_prefix_bloom_probes) {
  reinterpret_cast<rocksdb::options*>(
      jhandle)->memtable_prefix_bloom_probes =
          static_cast<int32_t>(jmemtable_prefix_bloom_probes);
}

/*
 * class:     org_rocksdb_options
 * method:    bloomlocality
 * signature: (j)i
 */
jint java_org_rocksdb_options_bloomlocality(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->bloom_locality;
}

/*
 * class:     org_rocksdb_options
 * method:    setbloomlocality
 * signature: (ji)v
 */
void java_org_rocksdb_options_setbloomlocality(
    jnienv* env, jobject jobj, jlong jhandle, jint jbloom_locality) {
  reinterpret_cast<rocksdb::options*>(jhandle)->bloom_locality =
      static_cast<int32_t>(jbloom_locality);
}

/*
 * class:     org_rocksdb_options
 * method:    maxsuccessivemerges
 * signature: (j)j
 */
jlong java_org_rocksdb_options_maxsuccessivemerges(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(jhandle)->max_successive_merges;
}

/*
 * class:     org_rocksdb_options
 * method:    setmaxsuccessivemerges
 * signature: (jj)v
 */
void java_org_rocksdb_options_setmaxsuccessivemerges(
    jnienv* env, jobject jobj, jlong jhandle,
    jlong jmax_successive_merges) {
  reinterpret_cast<rocksdb::options*>(jhandle)->max_successive_merges =
      static_cast<size_t>(jmax_successive_merges);
}

/*
 * class:     org_rocksdb_options
 * method:    minpartialmergeoperands
 * signature: (j)i
 */
jint java_org_rocksdb_options_minpartialmergeoperands(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::options*>(
      jhandle)->min_partial_merge_operands;
}

/*
 * class:     org_rocksdb_options
 * method:    setminpartialmergeoperands
 * signature: (ji)v
 */
void java_org_rocksdb_options_setminpartialmergeoperands(
    jnienv* env, jobject jobj, jlong jhandle,
    jint jmin_partial_merge_operands) {
  reinterpret_cast<rocksdb::options*>(
      jhandle)->min_partial_merge_operands =
          static_cast<int32_t>(jmin_partial_merge_operands);
}

//////////////////////////////////////////////////////////////////////////////
// writeoptions

/*
 * class:     org_rocksdb_writeoptions
 * method:    newwriteoptions
 * signature: ()v
 */
void java_org_rocksdb_writeoptions_newwriteoptions(
    jnienv* env, jobject jwrite_options) {
  rocksdb::writeoptions* op = new rocksdb::writeoptions();
  rocksdb::writeoptionsjni::sethandle(env, jwrite_options, op);
}

/*
 * class:     org_rocksdb_writeoptions
 * method:    disposeinternal
 * signature: ()v
 */
void java_org_rocksdb_writeoptions_disposeinternal(
    jnienv* env, jobject jwrite_options, jlong jhandle) {
  auto write_options = reinterpret_cast<rocksdb::writeoptions*>(jhandle);
  delete write_options;

  rocksdb::writeoptionsjni::sethandle(env, jwrite_options, nullptr);
}

/*
 * class:     org_rocksdb_writeoptions
 * method:    setsync
 * signature: (jz)v
 */
void java_org_rocksdb_writeoptions_setsync(
  jnienv* env, jobject jwrite_options, jlong jhandle, jboolean jflag) {
  reinterpret_cast<rocksdb::writeoptions*>(jhandle)->sync = jflag;
}

/*
 * class:     org_rocksdb_writeoptions
 * method:    sync
 * signature: (j)z
 */
jboolean java_org_rocksdb_writeoptions_sync(
    jnienv* env, jobject jwrite_options, jlong jhandle) {
  return reinterpret_cast<rocksdb::writeoptions*>(jhandle)->sync;
}

/*
 * class:     org_rocksdb_writeoptions
 * method:    setdisablewal
 * signature: (jz)v
 */
void java_org_rocksdb_writeoptions_setdisablewal(
    jnienv* env, jobject jwrite_options, jlong jhandle, jboolean jflag) {
  reinterpret_cast<rocksdb::writeoptions*>(jhandle)->disablewal = jflag;
}

/*
 * class:     org_rocksdb_writeoptions
 * method:    disablewal
 * signature: (j)z
 */
jboolean java_org_rocksdb_writeoptions_disablewal(
    jnienv* env, jobject jwrite_options, jlong jhandle) {
  return reinterpret_cast<rocksdb::writeoptions*>(jhandle)->disablewal;
}

/////////////////////////////////////////////////////////////////////
// rocksdb::readoptions

/*
 * class:     org_rocksdb_readoptions
 * method:    newreadoptions
 * signature: ()v
 */
void java_org_rocksdb_readoptions_newreadoptions(
    jnienv* env, jobject jobj) {
  auto read_opt = new rocksdb::readoptions();
  rocksdb::readoptionsjni::sethandle(env, jobj, read_opt);
}

/*
 * class:     org_rocksdb_readoptions
 * method:    disposeinternal
 * signature: (j)v
 */
void java_org_rocksdb_readoptions_disposeinternal(
    jnienv* env, jobject jobj, jlong jhandle) {
  delete reinterpret_cast<rocksdb::readoptions*>(jhandle);
  rocksdb::readoptionsjni::sethandle(env, jobj, nullptr);
}

/*
 * class:     org_rocksdb_readoptions
 * method:    verifychecksums
 * signature: (j)z
 */
jboolean java_org_rocksdb_readoptions_verifychecksums(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::readoptions*>(
      jhandle)->verify_checksums;
}

/*
 * class:     org_rocksdb_readoptions
 * method:    setverifychecksums
 * signature: (jz)v
 */
void java_org_rocksdb_readoptions_setverifychecksums(
    jnienv* env, jobject jobj, jlong jhandle,
    jboolean jverify_checksums) {
  reinterpret_cast<rocksdb::readoptions*>(jhandle)->verify_checksums =
      static_cast<bool>(jverify_checksums);
}

/*
 * class:     org_rocksdb_readoptions
 * method:    fillcache
 * signature: (j)z
 */
jboolean java_org_rocksdb_readoptions_fillcache(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::readoptions*>(jhandle)->fill_cache;
}

/*
 * class:     org_rocksdb_readoptions
 * method:    setfillcache
 * signature: (jz)v
 */
void java_org_rocksdb_readoptions_setfillcache(
    jnienv* env, jobject jobj, jlong jhandle, jboolean jfill_cache) {
  reinterpret_cast<rocksdb::readoptions*>(jhandle)->fill_cache =
      static_cast<bool>(jfill_cache);
}

/*
 * class:     org_rocksdb_readoptions
 * method:    tailing
 * signature: (j)z
 */
jboolean java_org_rocksdb_readoptions_tailing(
    jnienv* env, jobject jobj, jlong jhandle) {
  return reinterpret_cast<rocksdb::readoptions*>(jhandle)->tailing;
}

/*
 * class:     org_rocksdb_readoptions
 * method:    settailing
 * signature: (jz)v
 */
void java_org_rocksdb_readoptions_settailing(
    jnienv* env, jobject jobj, jlong jhandle, jboolean jtailing) {
  reinterpret_cast<rocksdb::readoptions*>(jhandle)->tailing =
      static_cast<bool>(jtailing);
}
