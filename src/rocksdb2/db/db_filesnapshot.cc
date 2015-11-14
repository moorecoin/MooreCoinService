//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2012 facebook.
// use of this source code is governed by a bsd-style license that can be
// found in the license file.

#ifndef rocksdb_lite

#define __stdc_format_macros
#include <inttypes.h>
#include <algorithm>
#include <string>
#include <stdint.h>
#include "db/db_impl.h"
#include "db/filename.h"
#include "db/version_set.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "port/port.h"
#include "util/mutexlock.h"
#include "util/sync_point.h"

namespace rocksdb {

status dbimpl::disablefiledeletions() {
  mutexlock l(&mutex_);
  ++disable_delete_obsolete_files_;
  if (disable_delete_obsolete_files_ == 1) {
    log(options_.info_log, "file deletions disabled");
  } else {
    log(options_.info_log,
        "file deletions disabled, but already disabled. counter: %d",
        disable_delete_obsolete_files_);
  }
  return status::ok();
}

status dbimpl::enablefiledeletions(bool force) {
  deletionstate deletion_state;
  bool should_purge_files = false;
  {
    mutexlock l(&mutex_);
    if (force) {
      // if force, we need to enable file deletions right away
      disable_delete_obsolete_files_ = 0;
    } else if (disable_delete_obsolete_files_ > 0) {
      --disable_delete_obsolete_files_;
    }
    if (disable_delete_obsolete_files_ == 0)  {
      log(options_.info_log, "file deletions enabled");
      should_purge_files = true;
      findobsoletefiles(deletion_state, true);
    } else {
      log(options_.info_log,
          "file deletions enable, but not really enabled. counter: %d",
          disable_delete_obsolete_files_);
    }
  }
  if (should_purge_files)  {
    purgeobsoletefiles(deletion_state);
  }
  logflush(options_.info_log);
  return status::ok();
}

int dbimpl::isfiledeletionsenabled() const {
  return disable_delete_obsolete_files_;
}

status dbimpl::getlivefiles(std::vector<std::string>& ret,
                            uint64_t* manifest_file_size,
                            bool flush_memtable) {

  *manifest_file_size = 0;

  mutex_.lock();

  if (flush_memtable) {
    // flush all dirty data to disk.
    status status;
    for (auto cfd : *versions_->getcolumnfamilyset()) {
      cfd->ref();
      mutex_.unlock();
      status = flushmemtable(cfd, flushoptions());
      mutex_.lock();
      cfd->unref();
      if (!status.ok()) {
        break;
      }
    }
    versions_->getcolumnfamilyset()->freedeadcolumnfamilies();

    if (!status.ok()) {
      mutex_.unlock();
      log(options_.info_log, "cannot flush data %s\n",
          status.tostring().c_str());
      return status;
    }
  }

  // make a set of all of the live *.sst files
  std::vector<filedescriptor> live;
  for (auto cfd : *versions_->getcolumnfamilyset()) {
    cfd->current()->addlivefiles(&live);
  }

  ret.clear();
  ret.reserve(live.size() + 2); //*.sst + current + manifest

  // create names of the live files. the names are not absolute
  // paths, instead they are relative to dbname_;
  for (auto live_file : live) {
    ret.push_back(maketablefilename("", live_file.getnumber()));
  }

  ret.push_back(currentfilename(""));
  ret.push_back(descriptorfilename("", versions_->manifestfilenumber()));

  // find length of manifest file while holding the mutex lock
  *manifest_file_size = versions_->manifestfilesize();

  mutex_.unlock();
  return status::ok();
}

status dbimpl::getsortedwalfiles(vectorlogptr& files) {
  // first get sorted files in db dir, then get sorted files from archived
  // dir, to avoid a race condition where a log file is moved to archived
  // dir in between.
  status s;
  // list wal files in main db dir.
  vectorlogptr logs;
  s = getsortedwalsoftype(options_.wal_dir, logs, kalivelogfile);
  if (!s.ok()) {
    return s;
  }

  // reproduce the race condition where a log file is moved
  // to archived dir, between these two sync points, used in
  // (dbtest,transactionlogiteratorrace)
  test_sync_point("dbimpl::getsortedwalfiles:1");
  test_sync_point("dbimpl::getsortedwalfiles:2");

  files.clear();
  // list wal files in archive dir.
  std::string archivedir = archivaldirectory(options_.wal_dir);
  if (env_->fileexists(archivedir)) {
    s = getsortedwalsoftype(archivedir, files, karchivedlogfile);
    if (!s.ok()) {
      return s;
    }
  }

  uint64_t latest_archived_log_number = 0;
  if (!files.empty()) {
    latest_archived_log_number = files.back()->lognumber();
    log(options_.info_log, "latest archived log: %" priu64,
        latest_archived_log_number);
  }

  files.reserve(files.size() + logs.size());
  for (auto& log : logs) {
    if (log->lognumber() > latest_archived_log_number) {
      files.push_back(std::move(log));
    } else {
      // when the race condition happens, we could see the
      // same log in both db dir and archived dir. simply
      // ignore the one in db dir. note that, if we read
      // archived dir first, we would have missed the log file.
      log(options_.info_log, "%s already moved to archive",
          log->pathname().c_str());
    }
  }

  return s;
}

}

#endif  // rocksdb_lite
