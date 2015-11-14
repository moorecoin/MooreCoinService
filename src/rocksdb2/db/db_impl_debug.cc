//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef rocksdb_lite

#include "db/db_impl.h"

namespace rocksdb {

void dbimpl::test_purgeobsoletetewal() { purgeobsoletewalfiles(); }

uint64_t dbimpl::test_getlevel0totalsize() {
  mutexlock l(&mutex_);
  return default_cf_handle_->cfd()->current()->numlevelbytes(0);
}

iterator* dbimpl::test_newinternaliterator(columnfamilyhandle* column_family) {
  columnfamilydata* cfd;
  if (column_family == nullptr) {
    cfd = default_cf_handle_->cfd();
  } else {
    auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
    cfd = cfh->cfd();
  }

  mutex_.lock();
  superversion* super_version = cfd->getsuperversion()->ref();
  mutex_.unlock();
  readoptions roptions;
  return newinternaliterator(roptions, cfd, super_version);
}

int64_t dbimpl::test_maxnextleveloverlappingbytes(
    columnfamilyhandle* column_family) {
  columnfamilydata* cfd;
  if (column_family == nullptr) {
    cfd = default_cf_handle_->cfd();
  } else {
    auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
    cfd = cfh->cfd();
  }
  mutexlock l(&mutex_);
  return cfd->current()->maxnextleveloverlappingbytes();
}

void dbimpl::test_getfilesmetadata(
    columnfamilyhandle* column_family,
    std::vector<std::vector<filemetadata>>* metadata) {
  auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
  auto cfd = cfh->cfd();
  mutexlock l(&mutex_);
  metadata->resize(numberlevels());
  for (int level = 0; level < numberlevels(); level++) {
    const std::vector<filemetadata*>& files = cfd->current()->files_[level];

    (*metadata)[level].clear();
    for (const auto& f : files) {
      (*metadata)[level].push_back(*f);
    }
  }
}

uint64_t dbimpl::test_current_manifest_fileno() {
  return versions_->manifestfilenumber();
}

status dbimpl::test_compactrange(int level, const slice* begin,
                                 const slice* end,
                                 columnfamilyhandle* column_family) {
  columnfamilydata* cfd;
  if (column_family == nullptr) {
    cfd = default_cf_handle_->cfd();
  } else {
    auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
    cfd = cfh->cfd();
  }
  int output_level =
      (cfd->options()->compaction_style == kcompactionstyleuniversal ||
       cfd->options()->compaction_style == kcompactionstylefifo)
          ? level
          : level + 1;
  return runmanualcompaction(cfd, level, output_level, 0, begin, end);
}

status dbimpl::test_flushmemtable(bool wait) {
  flushoptions fo;
  fo.wait = wait;
  return flushmemtable(default_cf_handle_->cfd(), fo);
}

status dbimpl::test_waitforflushmemtable(columnfamilyhandle* column_family) {
  columnfamilydata* cfd;
  if (column_family == nullptr) {
    cfd = default_cf_handle_->cfd();
  } else {
    auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
    cfd = cfh->cfd();
  }
  return waitforflushmemtable(cfd);
}

status dbimpl::test_waitforcompact() {
  // wait until the compaction completes

  // todo: a bug here. this function actually does not necessarily
  // wait for compact. it actually waits for scheduled compaction
  // or flush to finish.

  mutexlock l(&mutex_);
  while ((bg_compaction_scheduled_ || bg_flush_scheduled_) && bg_error_.ok()) {
    bg_cv_.wait();
  }
  return bg_error_;
}

status dbimpl::test_readfirstrecord(const walfiletype type,
                                    const uint64_t number,
                                    sequencenumber* sequence) {
  return readfirstrecord(type, number, sequence);
}

status dbimpl::test_readfirstline(const std::string& fname,
                                  sequencenumber* sequence) {
  return readfirstline(fname, sequence);
}
}  // namespace rocksdb
#endif  // rocksdb_lite
