// copyright (c) 2014, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
//
// this file implements the "bridge" between java and c++ and enables
// calling c++ rocksdb::restorebackupabledb and rocksdb::restoreoptions methods
// from java side.

#include <stdio.h>
#include <stdlib.h>
#include <jni.h>
#include <iostream>
#include <string>

#include "include/org_rocksdb_restoreoptions.h"
#include "include/org_rocksdb_restorebackupabledb.h"
#include "rocksjni/portal.h"
#include "rocksdb/utilities/backupable_db.h"
/*
 * class:     org_rocksdb_restoreoptions
 * method:    newrestoreoptions
 * signature: (z)j
 */
jlong java_org_rocksdb_restoreoptions_newrestoreoptions(jnienv* env,
    jobject jobj, jboolean keep_log_files) {
  auto ropt = new rocksdb::restoreoptions(keep_log_files);
  return reinterpret_cast<jlong>(ropt);
}

/*
 * class:     org_rocksdb_restoreoptions
 * method:    dispose
 * signature: (j)v
 */
void java_org_rocksdb_restoreoptions_dispose(jnienv* env, jobject jobj,
    jlong jhandle) {
  auto ropt = reinterpret_cast<rocksdb::restoreoptions*>(jhandle);
  assert(ropt);
  delete ropt;
}

/*
 * class:     org_rocksdb_restorebackupabledb
 * method:    newrestorebackupabledb
 * signature: (j)j
 */
jlong java_org_rocksdb_restorebackupabledb_newrestorebackupabledb(jnienv* env,
    jobject jobj, jlong jopt_handle) {
  auto opt = reinterpret_cast<rocksdb::backupabledboptions*>(jopt_handle);
  auto rdb = new rocksdb::restorebackupabledb(rocksdb::env::default(), *opt);
  return reinterpret_cast<jlong>(rdb);
}

/*
 * class:     org_rocksdb_restorebackupabledb
 * method:    restoredbfrombackup0
 * signature: (jjljava/lang/string;ljava/lang/string;j)v
 */
void java_org_rocksdb_restorebackupabledb_restoredbfrombackup0(jnienv* env,
    jobject jobj, jlong jhandle, jlong jbackup_id, jstring jdb_dir,
    jstring jwal_dir, jlong jopt_handle) {
  auto opt = reinterpret_cast<rocksdb::restoreoptions*>(jopt_handle);

  const char* cdb_dir = env->getstringutfchars(jdb_dir, 0);
  const char* cwal_dir = env->getstringutfchars(jwal_dir, 0);

  auto rdb = reinterpret_cast<rocksdb::restorebackupabledb*>(jhandle);
  rocksdb::status s =
      rdb->restoredbfrombackup(jbackup_id, cdb_dir, cwal_dir, *opt);

  env->releasestringutfchars(jdb_dir, cdb_dir);
  env->releasestringutfchars(jwal_dir, cwal_dir);

  if(!s.ok()) {
    rocksdb::rocksdbexceptionjni::thrownew(env, s);
  }
}

/*
 * class:     org_rocksdb_restorebackupabledb
 * method:    restoredbfromlatestbackup0
 * signature: (jljava/lang/string;ljava/lang/string;j)v
 */
void java_org_rocksdb_restorebackupabledb_restoredbfromlatestbackup0(
    jnienv* env, jobject jobj, jlong jhandle, jstring jdb_dir, jstring jwal_dir,
    jlong jopt_handle) {
  auto opt = reinterpret_cast<rocksdb::restoreoptions*>(jopt_handle);

  const char* cdb_dir = env->getstringutfchars(jdb_dir, 0);
  const char* cwal_dir = env->getstringutfchars(jwal_dir, 0);

  auto rdb = reinterpret_cast<rocksdb::restorebackupabledb*>(jhandle);
  rocksdb::status s =
      rdb->restoredbfromlatestbackup(cdb_dir, cwal_dir, *opt);

  env->releasestringutfchars(jdb_dir, cdb_dir);
  env->releasestringutfchars(jwal_dir, cwal_dir);

  if(!s.ok()) {
    rocksdb::rocksdbexceptionjni::thrownew(env, s);
  }
}

/*
 * class:     org_rocksdb_restorebackupabledb
 * method:    purgeoldbackups0
 * signature: (ji)v
 */
void java_org_rocksdb_restorebackupabledb_purgeoldbackups0(jnienv* env,
    jobject jobj, jlong jhandle, jint jnum_backups_to_keep) {
  auto rdb = reinterpret_cast<rocksdb::restorebackupabledb*>(jhandle);
  rocksdb::status s = rdb->purgeoldbackups(jnum_backups_to_keep);

  if(!s.ok()) {
    rocksdb::rocksdbexceptionjni::thrownew(env, s);
  }
}

/*
 * class:     org_rocksdb_restorebackupabledb
 * method:    deletebackup0
 * signature: (jj)v
 */
void java_org_rocksdb_restorebackupabledb_deletebackup0(jnienv* env,
    jobject jobj, jlong jhandle, jlong jbackup_id) {
  auto rdb = reinterpret_cast<rocksdb::restorebackupabledb*>(jhandle);
  rocksdb::status s = rdb->deletebackup(jbackup_id);

  if(!s.ok()) {
    rocksdb::rocksdbexceptionjni::thrownew(env, s);
  }
}

/*
 * class:     org_rocksdb_restorebackupabledb
 * method:    dispose
 * signature: (j)v
 */
void java_org_rocksdb_restorebackupabledb_dispose(jnienv* env, jobject jobj,
    jlong jhandle) {
  auto ropt = reinterpret_cast<rocksdb::restorebackupabledb*>(jhandle);
  assert(ropt);
  delete ropt;
}
