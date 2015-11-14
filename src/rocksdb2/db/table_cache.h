//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// thread-safe (provides internal synchronization)

#pragma once
#include <string>
#include <vector>
#include <stdint.h>

#include "db/dbformat.h"
#include "port/port.h"
#include "rocksdb/cache.h"
#include "rocksdb/env.h"
#include "rocksdb/table.h"
#include "table/table_reader.h"

namespace rocksdb {

class env;
class arena;
struct filedescriptor;

class tablecache {
 public:
  tablecache(const options* options, const envoptions& storage_options,
             cache* cache);
  ~tablecache();

  // return an iterator for the specified file number (the corresponding
  // file length must be exactly "file_size" bytes).  if "tableptr" is
  // non-nullptr, also sets "*tableptr" to point to the table object
  // underlying the returned iterator, or nullptr if no table object underlies
  // the returned iterator.  the returned "*tableptr" object is owned by
  // the cache and should not be deleted, and is valid for as long as the
  // returned iterator is live.
  iterator* newiterator(const readoptions& options, const envoptions& toptions,
                        const internalkeycomparator& internal_comparator,
                        const filedescriptor& file_fd,
                        tablereader** table_reader_ptr = nullptr,
                        bool for_compaction = false, arena* arena = nullptr);

  // if a seek to internal key "k" in specified file finds an entry,
  // call (*handle_result)(arg, found_key, found_value) repeatedly until
  // it returns false.
  status get(const readoptions& options,
             const internalkeycomparator& internal_comparator,
             const filedescriptor& file_fd, const slice& k, void* arg,
             bool (*handle_result)(void*, const parsedinternalkey&,
                                   const slice&),
             void (*mark_key_may_exist)(void*) = nullptr);

  // evict any entry for the specified file number
  static void evict(cache* cache, uint64_t file_number);

  // find table reader
  status findtable(const envoptions& toptions,
                   const internalkeycomparator& internal_comparator,
                   const filedescriptor& file_fd, cache::handle**,
                   const bool no_io = false);

  // get tablereader from a cache handle.
  tablereader* gettablereaderfromhandle(cache::handle* handle);

  // get the table properties of a given table.
  // @no_io: indicates if we should load table to the cache if it is not present
  //         in table cache yet.
  // @returns: `properties` will be reset on success. please note that we will
  //            return status::incomplete() if table is not present in cache and
  //            we set `no_io` to be true.
  status gettableproperties(const envoptions& toptions,
                            const internalkeycomparator& internal_comparator,
                            const filedescriptor& file_meta,
                            std::shared_ptr<const tableproperties>* properties,
                            bool no_io = false);

  // return total memory usage of the table reader of the file.
  // 0 of table reader of the file is not loaded.
  size_t getmemoryusagebytablereader(
      const envoptions& toptions,
      const internalkeycomparator& internal_comparator,
      const filedescriptor& fd);

  // release the handle from a cache
  void releasehandle(cache::handle* handle);

 private:
  env* const env_;
  const std::vector<dbpath> db_paths_;
  const options* options_;
  const envoptions& storage_options_;
  cache* const cache_;
};

}  // namespace rocksdb
