//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once

#include <stdint.h>
#include <memory>
#include <utility>
#include <string>

#include "rocksdb/statistics.h"
#include "rocksdb/status.h"
#include "rocksdb/table.h"
#include "table/table_reader.h"
#include "util/coding.h"

namespace rocksdb {

class block;
class blockiter;
class blockhandle;
class cache;
class filterblockreader;
class footer;
class internalkeycomparator;
class iterator;
class randomaccessfile;
class tablecache;
class tablereader;
class writablefile;
struct blockbasedtableoptions;
struct envoptions;
struct options;
struct readoptions;

using std::unique_ptr;

// a table is a sorted map from strings to strings.  tables are
// immutable and persistent.  a table may be safely accessed from
// multiple threads without external synchronization.
class blockbasedtable : public tablereader {
 public:
  static const std::string kfilterblockprefix;

  // attempt to open the table that is stored in bytes [0..file_size)
  // of "file", and read the metadata entries necessary to allow
  // retrieving data from the table.
  //
  // if successful, returns ok and sets "*table_reader" to the newly opened
  // table.  the client should delete "*table_reader" when no longer needed.
  // if there was an error while initializing the table, sets "*table_reader"
  // to nullptr and returns a non-ok status.
  //
  // *file must remain live while this table is in use.
  static status open(const options& db_options, const envoptions& env_options,
                     const blockbasedtableoptions& table_options,
                     const internalkeycomparator& internal_key_comparator,
                     unique_ptr<randomaccessfile>&& file, uint64_t file_size,
                     unique_ptr<tablereader>* table_reader);

  bool prefixmaymatch(const slice& internal_key);

  // returns a new iterator over the table contents.
  // the result of newiterator() is initially invalid (caller must
  // call one of the seek methods on the iterator before using it).
  iterator* newiterator(const readoptions&, arena* arena = nullptr) override;

  status get(const readoptions& readoptions, const slice& key,
             void* handle_context,
             bool (*result_handler)(void* handle_context,
                                    const parsedinternalkey& k, const slice& v),
             void (*mark_key_may_exist_handler)(void* handle_context) =
                 nullptr) override;

  // given a key, return an approximate byte offset in the file where
  // the data for that key begins (or would begin if the key were
  // present in the file).  the returned value is in terms of file
  // bytes, and so includes effects like compression of the underlying data.
  // e.g., the approximate offset of the last key in the table will
  // be close to the file length.
  uint64_t approximateoffsetof(const slice& key) override;

  // returns true if the block for the specified key is in cache.
  // requires: key is in this table && block cache enabled
  bool test_keyincache(const readoptions& options, const slice& key);

  // set up the table for compaction. might change some parameters with
  // posix_fadvise
  void setupforcompaction() override;

  std::shared_ptr<const tableproperties> gettableproperties() const override;

  size_t approximatememoryusage() const override;

  ~blockbasedtable();

  bool test_filter_block_preloaded() const;
  bool test_index_reader_preloaded() const;
  // implementation of indexreader will be exposed to internal cc file only.
  class indexreader;

 private:
  template <class tvalue>
  struct cachableentry;

  struct rep;
  rep* rep_;
  bool compaction_optimized_;

  class blockentryiteratorstate;
  // input_iter: if it is not null, update this one and return it as iterator
  static iterator* newdatablockiterator(rep* rep, const readoptions& ro,
                                        const slice& index_value,
                                        blockiter* input_iter = nullptr);

  // for the following two functions:
  // if `no_io == true`, we will not try to read filter/index from sst file
  // were they not present in cache yet.
  cachableentry<filterblockreader> getfilter(bool no_io = false) const;

  // get the iterator from the index reader.
  // if input_iter is not set, return new iterator
  // if input_iter is set, update it and return it as iterator
  //
  // note: erroriterator with status::incomplete shall be returned if all the
  // following conditions are met:
  //  1. we enabled table_options.cache_index_and_filter_blocks.
  //  2. index is not present in block cache.
  //  3. we disallowed any io to be performed, that is, read_options ==
  //     kblockcachetier
  iterator* newindexiterator(const readoptions& read_options,
                             blockiter* input_iter = nullptr);

  // read block cache from block caches (if set): block_cache and
  // block_cache_compressed.
  // on success, status::ok with be returned and @block will be populated with
  // pointer to the block as well as its block handle.
  static status getdatablockfromcache(
      const slice& block_cache_key, const slice& compressed_block_cache_key,
      cache* block_cache, cache* block_cache_compressed, statistics* statistics,
      const readoptions& read_options,
      blockbasedtable::cachableentry<block>* block);
  // put a raw block (maybe compressed) to the corresponding block caches.
  // this method will perform decompression against raw_block if needed and then
  // populate the block caches.
  // on success, status::ok will be returned; also @block will be populated with
  // uncompressed block and its cache handle.
  //
  // requires: raw_block is heap-allocated. putdatablocktocache() will be
  // responsible for releasing its memory if error occurs.
  static status putdatablocktocache(
      const slice& block_cache_key, const slice& compressed_block_cache_key,
      cache* block_cache, cache* block_cache_compressed,
      const readoptions& read_options, statistics* statistics,
      cachableentry<block>* block, block* raw_block);

  // calls (*handle_result)(arg, ...) repeatedly, starting with the entry found
  // after a call to seek(key), until handle_result returns false.
  // may not make such a call if filter policy says that key is not present.
  friend class tablecache;
  friend class blockbasedtablebuilder;

  void readmeta(const footer& footer);

  // create a index reader based on the index type stored in the table.
  // optionally, user can pass a preloaded meta_index_iter for the index that
  // need to access extra meta blocks for index construction. this parameter
  // helps avoid re-reading meta index block if caller already created one.
  status createindexreader(indexreader** index_reader,
                           iterator* preloaded_meta_index_iter = nullptr);

  // read the meta block from sst.
  static status readmetablock(
      rep* rep,
      std::unique_ptr<block>* meta_block,
      std::unique_ptr<iterator>* iter);

  // create the filter from the filter block.
  static filterblockreader* readfilter(const blockhandle& filter_handle,
                                       rep* rep, size_t* filter_size = nullptr);

  static void setupcachekeyprefix(rep* rep);

  explicit blockbasedtable(rep* rep)
      : rep_(rep), compaction_optimized_(false) {}

  // generate a cache key prefix from the file
  static void generatecacheprefix(cache* cc,
    randomaccessfile* file, char* buffer, size_t* size);
  static void generatecacheprefix(cache* cc,
    writablefile* file, char* buffer, size_t* size);

  // the longest prefix of the cache key used to identify blocks.
  // for posix files the unique id is three varints.
  static const size_t kmaxcachekeyprefixsize = kmaxvarint64length*3+1;

  // no copying allowed
  explicit blockbasedtable(const tablereader&) = delete;
  void operator=(const tablereader&) = delete;
};

}  // namespace rocksdb
