// copyright (c) 2014, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
//
// this file implements the "bridge" between java and c++ and enables
// calling c++ rocksdb::backupabledb and rocksdb::backupabledboptions methods
// from java side.

#include <stdio.h>
#include <stdlib.h>
#include <jni.h>
#include <string>

#include "include/org_rocksdb_backupabledb.h"
#include "include/org_rocksdb_backupabledboptions.h"
#include "rocksjni/portal.h"
#include "rocksdb/utilities/backupable_db.h"

/*
 * class:     org_rocksdb_backupabledb
 * method:    open
 * signature: (jj)v
 */
void java_org_rocksdb_backupabledb_open(
    jnienv* env, jobject jbdb, jlong jdb_handle, jlong jopt_handle) {
  auto db = reinterpret_cast<rocksdb::db*>(jdb_handle);
  auto opt = reinterpret_cast<rocksdb::backupabledboptions*>(jopt_handle);
  auto bdb = new rocksdb::backupabledb(db, *opt);

  // as backupabledb extends rocksdb on the java side, we can reuse
  // the rocksdb portal here.
  rocksdb::rocksdbjni::sethandle(env, jbdb, bdb);
}

/*
 * class:     org_rocksdb_backupabledb
 * method:    createnewbackup
 * signature: (jz)v
 */
void java_org_rocksdb_backupabledb_createnewbackup(
    jnienv* env, jobject jbdb, jlong jhandle, jboolean jflag) {
  rocksdb::status s =
      reinterpret_cast<rocksdb::backupabledb*>(jhandle)->createnewbackup(jflag);
  if (!s.ok()) {
    rocksdb::rocksdbexceptionjni::thrownew(env, s);
  }
}

/*
 * class:     org_rocksdb_backupabledb
 * method:    purgeoldbackups
 * signature: (ji)v
 */
void java_org_rocksdb_backupabledb_purgeoldbackups(
    jnienv* env, jobject jbdb, jlong jhandle, jboolean jnumbackupstokeep) {
  rocksdb::status s =
      reinterpret_cast<rocksdb::backupabledb*>(jhandle)->
      purgeoldbackups(jnumbackupstokeep);
  if (!s.ok()) {
    rocksdb::rocksdbexceptionjni::thrownew(env, s);
  }
}

///////////////////////////////////////////////////////////////////////////
// backupdboptions

/*
 * class:     org_rocksdb_backupabledboptions
 * method:    newbackupabledboptions
 * signature: (ljava/lang/string;)v
 */
void java_org_rocksdb_backupabledboptions_newbackupabledboptions(
    jnienv* env, jobject jobj, jstring jpath, jboolean jshare_table_files,
    jboolean jsync, jboolean jdestroy_old_data, jboolean jbackup_log_files,
    jlong jbackup_rate_limit, jlong jrestore_rate_limit) {
  jbackup_rate_limit = (jbackup_rate_limit <= 0) ? 0 : jbackup_rate_limit;
  jrestore_rate_limit = (jrestore_rate_limit <= 0) ? 0 : jrestore_rate_limit;

  const char* cpath = env->getstringutfchars(jpath, 0);

  auto bopt = new rocksdb::backupabledboptions(cpath, nullptr,
      jshare_table_files, nullptr, jsync, jdestroy_old_data, jbackup_log_files,
      jbackup_rate_limit, jrestore_rate_limit);

  env->releasestringutfchars(jpath, cpath);

  rocksdb::backupabledboptionsjni::sethandle(env, jobj, bopt);
}

/*
 * class:     org_rocksdb_backupabledboptions
 * method:    backupdir
 * signature: (j)ljava/lang/string;
 */
jstring java_org_rocksdb_backupabledboptions_backupdir(
    jnienv* env, jobject jopt, jlong jhandle, jstring jpath) {
  auto bopt = reinterpret_cast<rocksdb::backupabledboptions*>(jhandle);
  return env->newstringutf(bopt->backup_dir.c_str());
}

/*
 * class:     org_rocksdb_backupabledboptions
 * method:    disposeinternal
 * signature: (j)v
 */
void java_org_rocksdb_backupabledboptions_disposeinternal(
    jnienv* env, jobject jopt, jlong jhandle) {
  auto bopt = reinterpret_cast<rocksdb::backupabledboptions*>(jhandle);
  assert(bopt);
  delete bopt;

  rocksdb::backupabledboptionsjni::sethandle(env, jopt, nullptr);
}
