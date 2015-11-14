//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2012 facebook. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file.

#include "db/db_impl_readonly.h"
#include "db/db_impl.h"

#include <algorithm>
#include <set>
#include <string>
#include <stdint.h>
#include <stdio.h>
#include <vector>
#include "db/db_iter.h"
#include "db/dbformat.h"
#include "db/filename.h"
#include "db/log_reader.h"
#include "db/log_writer.h"
#include "db/memtable.h"
#include "db/merge_context.h"
#include "db/table_cache.h"
#include "db/version_set.h"
#include "db/write_batch_internal.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "rocksdb/status.h"
#include "rocksdb/table.h"
#include "rocksdb/merge_operator.h"
#include "port/port.h"
#include "table/block.h"
#include "table/merger.h"
#include "table/two_level_iterator.h"
#include "util/coding.h"
#include "util/logging.h"
#include "util/build_version.h"

namespace rocksdb {

dbimplreadonly::dbimplreadonly(const dboptions& options,
                               const std::string& dbname)
    : dbimpl(options, dbname) {
  log(options_.info_log, "opening the db in read only mode");
}

dbimplreadonly::~dbimplreadonly() {
}

// implementations of the db interface
status dbimplreadonly::get(const readoptions& options,
                           columnfamilyhandle* column_family, const slice& key,
                           std::string* value) {
  status s;
  sequencenumber snapshot = versions_->lastsequence();
  auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
  auto cfd = cfh->cfd();
  superversion* super_version = cfd->getsuperversion();
  mergecontext merge_context;
  lookupkey lkey(key, snapshot);
  if (super_version->mem->get(lkey, value, &s, merge_context,
                              *cfd->options())) {
  } else {
    super_version->current->get(options, lkey, value, &s, &merge_context);
  }
  return s;
}

iterator* dbimplreadonly::newiterator(const readoptions& options,
                                      columnfamilyhandle* column_family) {
  auto cfh = reinterpret_cast<columnfamilyhandleimpl*>(column_family);
  auto cfd = cfh->cfd();
  superversion* super_version = cfd->getsuperversion()->ref();
  sequencenumber latest_snapshot = versions_->lastsequence();
  auto db_iter = newarenawrappeddbiterator(
      env_, *cfd->options(), cfd->user_comparator(),
      (options.snapshot != nullptr
           ? reinterpret_cast<const snapshotimpl*>(options.snapshot)->number_
           : latest_snapshot));
  auto internal_iter =
      newinternaliterator(options, cfd, super_version, db_iter->getarena());
  db_iter->setiterunderdbiter(internal_iter);
  return db_iter;
}

status dbimplreadonly::newiterators(
    const readoptions& options,
    const std::vector<columnfamilyhandle*>& column_families,
    std::vector<iterator*>* iterators) {
  if (iterators == nullptr) {
    return status::invalidargument("iterators not allowed to be nullptr");
  }
  iterators->clear();
  iterators->reserve(column_families.size());
  sequencenumber latest_snapshot = versions_->lastsequence();

  for (auto cfh : column_families) {
    auto cfd = reinterpret_cast<columnfamilyhandleimpl*>(cfh)->cfd();
    auto db_iter = newarenawrappeddbiterator(
        env_, *cfd->options(), cfd->user_comparator(),
        options.snapshot != nullptr
            ? reinterpret_cast<const snapshotimpl*>(options.snapshot)->number_
            : latest_snapshot);
    auto internal_iter = newinternaliterator(
        options, cfd, cfd->getsuperversion()->ref(), db_iter->getarena());
    db_iter->setiterunderdbiter(internal_iter);
    iterators->push_back(db_iter);
  }

  return status::ok();
}

status db::openforreadonly(const options& options, const std::string& dbname,
                           db** dbptr, bool error_if_log_file_exist) {
  *dbptr = nullptr;

  dboptions db_options(options);
  columnfamilyoptions cf_options(options);
  std::vector<columnfamilydescriptor> column_families;
  column_families.push_back(
      columnfamilydescriptor(kdefaultcolumnfamilyname, cf_options));
  std::vector<columnfamilyhandle*> handles;

  status s =
      db::openforreadonly(db_options, dbname, column_families, &handles, dbptr);
  if (s.ok()) {
    assert(handles.size() == 1);
    // i can delete the handle since dbimpl is always holding a
    // reference to default column family
    delete handles[0];
  }
  return s;
}

status db::openforreadonly(
    const dboptions& db_options, const std::string& dbname,
    const std::vector<columnfamilydescriptor>& column_families,
    std::vector<columnfamilyhandle*>* handles, db** dbptr,
    bool error_if_log_file_exist) {
  *dbptr = nullptr;
  handles->clear();

  dbimplreadonly* impl = new dbimplreadonly(db_options, dbname);
  impl->mutex_.lock();
  status s = impl->recover(column_families, true /* read only */,
                           error_if_log_file_exist);
  if (s.ok()) {
    // set column family handles
    for (auto cf : column_families) {
      auto cfd =
          impl->versions_->getcolumnfamilyset()->getcolumnfamily(cf.name);
      if (cfd == nullptr) {
        s = status::invalidargument("column family not found: ", cf.name);
        break;
      }
      handles->push_back(new columnfamilyhandleimpl(cfd, impl, &impl->mutex_));
    }
  }
  if (s.ok()) {
    for (auto cfd : *impl->versions_->getcolumnfamilyset()) {
      delete cfd->installsuperversion(new superversion(), &impl->mutex_);
    }
  }
  impl->mutex_.unlock();
  if (s.ok()) {
    *dbptr = impl;
  } else {
    for (auto h : *handles) {
      delete h;
    }
    handles->clear();
    delete impl;
  }
  return s;
}


}   // namespace rocksdb
