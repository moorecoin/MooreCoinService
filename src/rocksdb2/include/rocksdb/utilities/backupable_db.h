//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#ifndef rocksdb_lite

#define __stdc_format_macros
#include <inttypes.h>
#include <string>
#include <map>
#include <vector>

#include "rocksdb/utilities/stackable_db.h"

#include "rocksdb/env.h"
#include "rocksdb/status.h"

namespace rocksdb {

struct backupabledboptions {
  // where to keep the backup files. has to be different than dbname_
  // best to set this to dbname_ + "/backups"
  // required
  std::string backup_dir;

  // backup env object. it will be used for backup file i/o. if it's
  // nullptr, backups will be written out using dbs env. if it's
  // non-nullptr, backup's i/o will be performed using this object.
  // if you want to have backups on hdfs, use hdfs env here!
  // default: nullptr
  env* backup_env;

  // if share_table_files == true, backup will assume that table files with
  // same name have the same contents. this enables incremental backups and
  // avoids unnecessary data copies.
  // if share_table_files == false, each backup will be on its own and will
  // not share any data with other backups.
  // default: true
  bool share_table_files;

  // backup info and error messages will be written to info_log
  // if non-nullptr.
  // default: nullptr
  logger* info_log;

  // if sync == true, we can guarantee you'll get consistent backup even
  // on a machine crash/reboot. backup process is slower with sync enabled.
  // if sync == false, we don't guarantee anything on machine reboot. however,
  // chances are some of the backups are consistent.
  // default: true
  bool sync;

  // if true, it will delete whatever backups there are already
  // default: false
  bool destroy_old_data;

  // if false, we won't backup log files. this option can be useful for backing
  // up in-memory databases where log file are persisted, but table files are in
  // memory.
  // default: true
  bool backup_log_files;

  // max bytes that can be transferred in a second during backup.
  // if 0, go as fast as you can
  // default: 0
  uint64_t backup_rate_limit;

  // max bytes that can be transferred in a second during restore.
  // if 0, go as fast as you can
  // default: 0
  uint64_t restore_rate_limit;

  // only used if share_table_files is set to true. if true, will consider that
  // backups can come from different databases, hence a sst is not uniquely
  // identifed by its name, but by the triple (file name, crc32, file length)
  // default: false
  // note: this is an experimental option, and you'll need to set it manually
  // *turn it on only if you know what you're doing*
  bool share_files_with_checksum;

  void dump(logger* logger) const;

  explicit backupabledboptions(const std::string& _backup_dir,
                               env* _backup_env = nullptr,
                               bool _share_table_files = true,
                               logger* _info_log = nullptr, bool _sync = true,
                               bool _destroy_old_data = false,
                               bool _backup_log_files = true,
                               uint64_t _backup_rate_limit = 0,
                               uint64_t _restore_rate_limit = 0)
      : backup_dir(_backup_dir),
        backup_env(_backup_env),
        share_table_files(_share_table_files),
        info_log(_info_log),
        sync(_sync),
        destroy_old_data(_destroy_old_data),
        backup_log_files(_backup_log_files),
        backup_rate_limit(_backup_rate_limit),
        restore_rate_limit(_restore_rate_limit),
        share_files_with_checksum(false) {
    assert(share_table_files || !share_files_with_checksum);
  }
};

struct restoreoptions {
  // if true, restore won't overwrite the existing log files in wal_dir. it will
  // also move all log files from archive directory to wal_dir. use this option
  // in combination with backupabledboptions::backup_log_files = false for
  // persisting in-memory databases.
  // default: false
  bool keep_log_files;

  explicit restoreoptions(bool _keep_log_files = false)
      : keep_log_files(_keep_log_files) {}
};

typedef uint32_t backupid;

struct backupinfo {
  backupid backup_id;
  int64_t timestamp;
  uint64_t size;

  backupinfo() {}
  backupinfo(backupid _backup_id, int64_t _timestamp, uint64_t _size)
      : backup_id(_backup_id), timestamp(_timestamp), size(_size) {}
};

class backupenginereadonly {
 public:
  virtual ~backupenginereadonly() {}

  static backupenginereadonly* newreadonlybackupengine(
      env* db_env, const backupabledboptions& options);

  // you can getbackupinfo safely, even with other backupengine performing
  // backups on the same directory
  virtual void getbackupinfo(std::vector<backupinfo>* backup_info) = 0;

  // restoring db from backup is not safe when there is another backupengine
  // running that might call deletebackup() or purgeoldbackups(). it is caller's
  // responsibility to synchronize the operation, i.e. don't delete the backup
  // when you're restoring from it
  virtual status restoredbfrombackup(
      backupid backup_id, const std::string& db_dir, const std::string& wal_dir,
      const restoreoptions& restore_options = restoreoptions()) = 0;
  virtual status restoredbfromlatestbackup(
      const std::string& db_dir, const std::string& wal_dir,
      const restoreoptions& restore_options = restoreoptions()) = 0;
};

// please see the documentation in backupabledb and restorebackupabledb
class backupengine {
 public:
  virtual ~backupengine() {}

  static backupengine* newbackupengine(env* db_env,
                                       const backupabledboptions& options);

  virtual status createnewbackup(db* db, bool flush_before_backup = false) = 0;
  virtual status purgeoldbackups(uint32_t num_backups_to_keep) = 0;
  virtual status deletebackup(backupid backup_id) = 0;
  virtual void stopbackup() = 0;

  virtual void getbackupinfo(std::vector<backupinfo>* backup_info) = 0;
  virtual status restoredbfrombackup(
      backupid backup_id, const std::string& db_dir, const std::string& wal_dir,
      const restoreoptions& restore_options = restoreoptions()) = 0;
  virtual status restoredbfromlatestbackup(
      const std::string& db_dir, const std::string& wal_dir,
      const restoreoptions& restore_options = restoreoptions()) = 0;
};

// stack your db with backupabledb to be able to backup the db
class backupabledb : public stackabledb {
 public:
  // backupabledboptions have to be the same as the ones used in a previous
  // incarnation of the db
  //
  // backupabledb ownes the pointer `db* db` now. you should not delete it or
  // use it after the invocation of backupabledb
  backupabledb(db* db, const backupabledboptions& options);
  virtual ~backupabledb();

  // captures the state of the database in the latest backup
  // not a thread safe call
  status createnewbackup(bool flush_before_backup = false);
  // returns info about backups in backup_info
  void getbackupinfo(std::vector<backupinfo>* backup_info);
  // deletes old backups, keeping latest num_backups_to_keep alive
  status purgeoldbackups(uint32_t num_backups_to_keep);
  // deletes a specific backup
  status deletebackup(backupid backup_id);
  // call this from another thread if you want to stop the backup
  // that is currently happening. it will return immediatelly, will
  // not wait for the backup to stop.
  // the backup will stop asap and the call to createnewbackup will
  // return status::incomplete(). it will not clean up after itself, but
  // the state will remain consistent. the state will be cleaned up
  // next time you create backupabledb or restorebackupabledb.
  void stopbackup();

 private:
  backupengine* backup_engine_;
};

// use this class to access information about backups and restore from them
class restorebackupabledb {
 public:
  restorebackupabledb(env* db_env, const backupabledboptions& options);
  ~restorebackupabledb();

  // returns info about backups in backup_info
  void getbackupinfo(std::vector<backupinfo>* backup_info);

  // restore from backup with backup_id
  // important -- if options_.share_table_files == true and you restore db
  // from some backup that is not the latest, and you start creating new
  // backups from the new db, they will probably fail
  //
  // example: let's say you have backups 1, 2, 3, 4, 5 and you restore 3.
  // if you add new data to the db and try creating a new backup now, the
  // database will diverge from backups 4 and 5 and the new backup will fail.
  // if you want to create new backup, you will first have to delete backups 4
  // and 5.
  status restoredbfrombackup(backupid backup_id, const std::string& db_dir,
                             const std::string& wal_dir,
                             const restoreoptions& restore_options =
                                 restoreoptions());

  // restore from the latest backup
  status restoredbfromlatestbackup(const std::string& db_dir,
                                   const std::string& wal_dir,
                                   const restoreoptions& restore_options =
                                       restoreoptions());
  // deletes old backups, keeping latest num_backups_to_keep alive
  status purgeoldbackups(uint32_t num_backups_to_keep);
  // deletes a specific backup
  status deletebackup(backupid backup_id);

 private:
  backupengine* backup_engine_;
};

}  // namespace rocksdb
#endif  // rocksdb_lite
