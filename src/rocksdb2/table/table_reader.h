//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include <memory>

namespace rocksdb {

class iterator;
struct parsedinternalkey;
class slice;
class arena;
struct readoptions;
struct tableproperties;

// a table is a sorted map from strings to strings.  tables are
// immutable and persistent.  a table may be safely accessed from
// multiple threads without external synchronization.
class tablereader {
 public:
  virtual ~tablereader() {}

  // returns a new iterator over the table contents.
  // the result of newiterator() is initially invalid (caller must
  // call one of the seek methods on the iterator before using it).
  // arena: if not null, the arena needs to be used to allocate the iterator.
  //        when destroying the iterator, the caller will not call "delete"
  //        but iterator::~iterator() directly. the destructor needs to destroy
  //        all the states but those allocated in arena.
  virtual iterator* newiterator(const readoptions&, arena* arena = nullptr) = 0;

  // given a key, return an approximate byte offset in the file where
  // the data for that key begins (or would begin if the key were
  // present in the file).  the returned value is in terms of file
  // bytes, and so includes effects like compression of the underlying data.
  // e.g., the approximate offset of the last key in the table will
  // be close to the file length.
  virtual uint64_t approximateoffsetof(const slice& key) = 0;

  // set up the table for compaction. might change some parameters with
  // posix_fadvise
  virtual void setupforcompaction() = 0;

  virtual std::shared_ptr<const tableproperties> gettableproperties() const = 0;

  // prepare work that can be done before the real get()
  virtual void prepare(const slice& target) {}

  // report an approximation of how much memory has been used.
  virtual size_t approximatememoryusage() const = 0;

  // calls (*result_handler)(handle_context, ...) repeatedly, starting with
  // the entry found after a call to seek(key), until result_handler returns
  // false, where k is the actual internal key for a row found and v as the
  // value of the key. may not make such a call if filter policy says that key
  // is not present.
  //
  // mark_key_may_exist_handler needs to be called when it is configured to be
  // memory only and the key is not found in the block cache, with
  // the parameter to be handle_context.
  //
  // readoptions is the options for the read
  // key is the key to search for
  virtual status get(
      const readoptions& readoptions, const slice& key, void* handle_context,
      bool (*result_handler)(void* arg, const parsedinternalkey& k,
                             const slice& v),
      void (*mark_key_may_exist_handler)(void* handle_context) = nullptr) = 0;
};

}  // namespace rocksdb
