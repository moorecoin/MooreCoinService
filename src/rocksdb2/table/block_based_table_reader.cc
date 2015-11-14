//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "table/block_based_table_reader.h"

#include <string>
#include <utility>

#include "db/dbformat.h"

#include "rocksdb/cache.h"
#include "rocksdb/comparator.h"
#include "rocksdb/env.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/iterator.h"
#include "rocksdb/options.h"
#include "rocksdb/statistics.h"
#include "rocksdb/table.h"
#include "rocksdb/table_properties.h"

#include "table/block.h"
#include "table/filter_block.h"
#include "table/block_hash_index.h"
#include "table/block_prefix_index.h"
#include "table/format.h"
#include "table/meta_blocks.h"
#include "table/two_level_iterator.h"

#include "util/coding.h"
#include "util/perf_context_imp.h"
#include "util/stop_watch.h"

namespace rocksdb {

extern const uint64_t kblockbasedtablemagicnumber;
extern const std::string khashindexprefixesblock;
extern const std::string khashindexprefixesmetadatablock;
using std::unique_ptr;

typedef blockbasedtable::indexreader indexreader;

namespace {

// the longest the prefix of the cache key used to identify blocks can be.
// we are using the fact that we know for posix files the unique id is three
// varints.
// for some reason, compiling for ios complains that this variable is unused
const size_t kmaxcachekeyprefixsize __attribute__((unused)) =
    kmaxvarint64length * 3 + 1;

// read the block identified by "handle" from "file".
// the only relevant option is options.verify_checksums for now.
// on failure return non-ok.
// on success fill *result and return ok - caller owns *result
status readblockfromfile(randomaccessfile* file, const footer& footer,
                         const readoptions& options, const blockhandle& handle,
                         block** result, env* env, bool do_uncompress = true) {
  blockcontents contents;
  status s = readblockcontents(file, footer, options, handle, &contents, env,
                               do_uncompress);
  if (s.ok()) {
    *result = new block(contents);
  }

  return s;
}

// delete the resource that is held by the iterator.
template <class resourcetype>
void deleteheldresource(void* arg, void* ignored) {
  delete reinterpret_cast<resourcetype*>(arg);
}

// delete the entry resided in the cache.
template <class entry>
void deletecachedentry(const slice& key, void* value) {
  auto entry = reinterpret_cast<entry*>(value);
  delete entry;
}

// release the cached entry and decrement its ref count.
void releasecachedentry(void* arg, void* h) {
  cache* cache = reinterpret_cast<cache*>(arg);
  cache::handle* handle = reinterpret_cast<cache::handle*>(h);
  cache->release(handle);
}

slice getcachekey(const char* cache_key_prefix, size_t cache_key_prefix_size,
                  const blockhandle& handle, char* cache_key) {
  assert(cache_key != nullptr);
  assert(cache_key_prefix_size != 0);
  assert(cache_key_prefix_size <= kmaxcachekeyprefixsize);
  memcpy(cache_key, cache_key_prefix, cache_key_prefix_size);
  char* end =
      encodevarint64(cache_key + cache_key_prefix_size, handle.offset());
  return slice(cache_key, static_cast<size_t>(end - cache_key));
}

cache::handle* getentryfromcache(cache* block_cache, const slice& key,
                                 tickers block_cache_miss_ticker,
                                 tickers block_cache_hit_ticker,
                                 statistics* statistics) {
  auto cache_handle = block_cache->lookup(key);
  if (cache_handle != nullptr) {
    perf_counter_add(block_cache_hit_count, 1);
    // overall cache hit
    recordtick(statistics, block_cache_hit);
    // block-type specific cache hit
    recordtick(statistics, block_cache_hit_ticker);
  } else {
    // overall cache miss
    recordtick(statistics, block_cache_miss);
    // block-type specific cache miss
    recordtick(statistics, block_cache_miss_ticker);
  }

  return cache_handle;
}

}  // namespace

// -- indexreader and its subclasses
// indexreader is the interface that provide the functionality for index access.
class blockbasedtable::indexreader {
 public:
  explicit indexreader(const comparator* comparator)
      : comparator_(comparator) {}

  virtual ~indexreader() {}

  // create an iterator for index access.
  // an iter is passed in, if it is not null, update this one and return it
  // if it is null, create a new iterator
  virtual iterator* newiterator(
      blockiter* iter = nullptr, bool total_order_seek = true) = 0;

  // the size of the index.
  virtual size_t size() const = 0;

  // report an approximation of how much memory has been used other than memory
  // that was allocated in block cache.
  virtual size_t approximatememoryusage() const = 0;

 protected:
  const comparator* comparator_;
};

// index that allows binary search lookup for the first key of each block.
// this class can be viewed as a thin wrapper for `block` class which already
// supports binary search.
class binarysearchindexreader : public indexreader {
 public:
  // read index from the file and create an intance for
  // `binarysearchindexreader`.
  // on success, index_reader will be populated; otherwise it will remain
  // unmodified.
  static status create(randomaccessfile* file, const footer& footer,
                       const blockhandle& index_handle, env* env,
                       const comparator* comparator,
                       indexreader** index_reader) {
    block* index_block = nullptr;
    auto s = readblockfromfile(file, footer, readoptions(), index_handle,
                               &index_block, env);

    if (s.ok()) {
      *index_reader = new binarysearchindexreader(comparator, index_block);
    }

    return s;
  }

  virtual iterator* newiterator(
      blockiter* iter = nullptr, bool dont_care = true) override {
    return index_block_->newiterator(comparator_, iter, true);
  }

  virtual size_t size() const override { return index_block_->size(); }

  virtual size_t approximatememoryusage() const override {
    assert(index_block_);
    return index_block_->approximatememoryusage();
  }

 private:
  binarysearchindexreader(const comparator* comparator, block* index_block)
      : indexreader(comparator), index_block_(index_block) {
    assert(index_block_ != nullptr);
  }
  std::unique_ptr<block> index_block_;
};

// index that leverages an internal hash table to quicken the lookup for a given
// key.
class hashindexreader : public indexreader {
 public:
  static status create(const slicetransform* hash_key_extractor,
                       const footer& footer, randomaccessfile* file, env* env,
                       const comparator* comparator,
                       const blockhandle& index_handle,
                       iterator* meta_index_iter, indexreader** index_reader,
                       bool hash_index_allow_collision) {
    block* index_block = nullptr;
    auto s = readblockfromfile(file, footer, readoptions(), index_handle,
                               &index_block, env);

    if (!s.ok()) {
      return s;
    }

    // note, failure to create prefix hash index does not need to be a
    // hard error. we can still fall back to the original binary search index.
    // so, create will succeed regardless, from this point on.

    auto new_index_reader =
        new hashindexreader(comparator, index_block);
    *index_reader = new_index_reader;

    // get prefixes block
    blockhandle prefixes_handle;
    s = findmetablock(meta_index_iter, khashindexprefixesblock,
                      &prefixes_handle);
    if (!s.ok()) {
      // todo: log error
      return status::ok();
    }

    // get index metadata block
    blockhandle prefixes_meta_handle;
    s = findmetablock(meta_index_iter, khashindexprefixesmetadatablock,
                      &prefixes_meta_handle);
    if (!s.ok()) {
      // todo: log error
      return status::ok();
    }

    // read contents for the blocks
    blockcontents prefixes_contents;
    s = readblockcontents(file, footer, readoptions(), prefixes_handle,
                          &prefixes_contents, env, true /* do decompression */);
    if (!s.ok()) {
      return s;
    }
    blockcontents prefixes_meta_contents;
    s = readblockcontents(file, footer, readoptions(), prefixes_meta_handle,
                          &prefixes_meta_contents, env,
                          true /* do decompression */);
    if (!s.ok()) {
      if (prefixes_contents.heap_allocated) {
        delete[] prefixes_contents.data.data();
      }
      // todo: log error
      return status::ok();
    }

    if (!hash_index_allow_collision) {
      // todo: deprecate once hash_index_allow_collision proves to be stable.
      blockhashindex* hash_index = nullptr;
      s = createblockhashindex(hash_key_extractor,
                               prefixes_contents.data,
                               prefixes_meta_contents.data,
                               &hash_index);
      // todo: log error
      if (s.ok()) {
        new_index_reader->index_block_->setblockhashindex(hash_index);
        new_index_reader->ownprefixescontents(prefixes_contents);
      }
    } else {
      blockprefixindex* prefix_index = nullptr;
      s = blockprefixindex::create(hash_key_extractor,
                                   prefixes_contents.data,
                                   prefixes_meta_contents.data,
                                   &prefix_index);
      // todo: log error
      if (s.ok()) {
        new_index_reader->index_block_->setblockprefixindex(prefix_index);
      }
    }

    // always release prefix meta block
    if (prefixes_meta_contents.heap_allocated) {
      delete[] prefixes_meta_contents.data.data();
    }

    // release prefix content block if we don't own it.
    if (!new_index_reader->own_prefixes_contents_) {
      if (prefixes_contents.heap_allocated) {
        delete[] prefixes_contents.data.data();
      }
    }

    return status::ok();
  }

  virtual iterator* newiterator(
      blockiter* iter = nullptr, bool total_order_seek = true) override {
    return index_block_->newiterator(comparator_, iter, total_order_seek);
  }

  virtual size_t size() const override { return index_block_->size(); }

  virtual size_t approximatememoryusage() const override {
    assert(index_block_);
    return index_block_->approximatememoryusage() +
           prefixes_contents_.data.size();
  }

 private:
  hashindexreader(const comparator* comparator, block* index_block)
      : indexreader(comparator),
        index_block_(index_block),
        own_prefixes_contents_(false) {
    assert(index_block_ != nullptr);
  }

  ~hashindexreader() {
    if (own_prefixes_contents_ && prefixes_contents_.heap_allocated) {
      delete[] prefixes_contents_.data.data();
    }
  }

  void ownprefixescontents(const blockcontents& prefixes_contents) {
    prefixes_contents_ = prefixes_contents;
    own_prefixes_contents_ = true;
  }

  std::unique_ptr<block> index_block_;
  bool own_prefixes_contents_;
  blockcontents prefixes_contents_;
};


struct blockbasedtable::rep {
  rep(const envoptions& storage_options,
      const blockbasedtableoptions& table_opt,
      const internalkeycomparator& internal_comparator)
      : soptions(storage_options), table_options(table_opt),
        filter_policy(table_opt.filter_policy.get()),
        internal_comparator(internal_comparator) {}

  options options;
  const envoptions& soptions;
  const blockbasedtableoptions& table_options;
  const filterpolicy* const filter_policy;
  const internalkeycomparator& internal_comparator;
  status status;
  unique_ptr<randomaccessfile> file;
  char cache_key_prefix[kmaxcachekeyprefixsize];
  size_t cache_key_prefix_size = 0;
  char compressed_cache_key_prefix[kmaxcachekeyprefixsize];
  size_t compressed_cache_key_prefix_size = 0;

  // footer contains the fixed table information
  footer footer;
  // index_reader and filter will be populated and used only when
  // options.block_cache is nullptr; otherwise we will get the index block via
  // the block cache.
  unique_ptr<indexreader> index_reader;
  unique_ptr<filterblockreader> filter;

  std::shared_ptr<const tableproperties> table_properties;
  blockbasedtableoptions::indextype index_type;
  bool hash_index_allow_collision;
  // todo(kailiu) it is very ugly to use internal key in table, since table
  // module should not be relying on db module. however to make things easier
  // and compatible with existing code, we introduce a wrapper that allows
  // block to extract prefix without knowing if a key is internal or not.
  unique_ptr<slicetransform> internal_prefix_transform;
};

blockbasedtable::~blockbasedtable() {
  delete rep_;
}

// cachableentry represents the entries that *may* be fetched from block cache.
//  field `value` is the item we want to get.
//  field `cache_handle` is the cache handle to the block cache. if the value
//    was not read from cache, `cache_handle` will be nullptr.
template <class tvalue>
struct blockbasedtable::cachableentry {
  cachableentry(tvalue* value, cache::handle* cache_handle)
    : value(value)
    , cache_handle(cache_handle) {
  }
  cachableentry(): cachableentry(nullptr, nullptr) { }
  void release(cache* cache) {
    if (cache_handle) {
      cache->release(cache_handle);
      value = nullptr;
      cache_handle = nullptr;
    }
  }

  tvalue* value = nullptr;
  // if the entry is from the cache, cache_handle will be populated.
  cache::handle* cache_handle = nullptr;
};

// helper function to setup the cache key's prefix for the table.
void blockbasedtable::setupcachekeyprefix(rep* rep) {
  assert(kmaxcachekeyprefixsize >= 10);
  rep->cache_key_prefix_size = 0;
  rep->compressed_cache_key_prefix_size = 0;
  if (rep->table_options.block_cache != nullptr) {
    generatecacheprefix(rep->table_options.block_cache.get(), rep->file.get(),
                        &rep->cache_key_prefix[0],
                        &rep->cache_key_prefix_size);
  }
  if (rep->table_options.block_cache_compressed != nullptr) {
    generatecacheprefix(rep->table_options.block_cache_compressed.get(),
                        rep->file.get(), &rep->compressed_cache_key_prefix[0],
                        &rep->compressed_cache_key_prefix_size);
  }
}

void blockbasedtable::generatecacheprefix(cache* cc,
    randomaccessfile* file, char* buffer, size_t* size) {

  // generate an id from the file
  *size = file->getuniqueid(buffer, kmaxcachekeyprefixsize);

  // if the prefix wasn't generated or was too long,
  // create one from the cache.
  if (*size == 0) {
    char* end = encodevarint64(buffer, cc->newid());
    *size = static_cast<size_t>(end - buffer);
  }
}

void blockbasedtable::generatecacheprefix(cache* cc,
    writablefile* file, char* buffer, size_t* size) {

  // generate an id from the file
  *size = file->getuniqueid(buffer, kmaxcachekeyprefixsize);

  // if the prefix wasn't generated or was too long,
  // create one from the cache.
  if (*size == 0) {
    char* end = encodevarint64(buffer, cc->newid());
    *size = static_cast<size_t>(end - buffer);
  }
}

status blockbasedtable::open(const options& options, const envoptions& soptions,
                             const blockbasedtableoptions& table_options,
                             const internalkeycomparator& internal_comparator,
                             unique_ptr<randomaccessfile>&& file,
                             uint64_t file_size,
                             unique_ptr<tablereader>* table_reader) {
  table_reader->reset();

  footer footer(kblockbasedtablemagicnumber);
  auto s = readfooterfromfile(file.get(), file_size, &footer);
  if (!s.ok()) return s;

  // we've successfully read the footer and the index block: we're
  // ready to serve requests.
  rep* rep = new blockbasedtable::rep(
      soptions, table_options, internal_comparator);
  rep->options = options;
  rep->file = std::move(file);
  rep->footer = footer;
  rep->index_type = table_options.index_type;
  rep->hash_index_allow_collision = table_options.hash_index_allow_collision;
  setupcachekeyprefix(rep);
  unique_ptr<blockbasedtable> new_table(new blockbasedtable(rep));

  // read meta index
  std::unique_ptr<block> meta;
  std::unique_ptr<iterator> meta_iter;
  s = readmetablock(rep, &meta, &meta_iter);

  // read the properties
  bool found_properties_block = true;
  s = seektopropertiesblock(meta_iter.get(), &found_properties_block);

  if (found_properties_block) {
    s = meta_iter->status();
    tableproperties* table_properties = nullptr;
    if (s.ok()) {
      s = readproperties(meta_iter->value(), rep->file.get(), rep->footer,
                         rep->options.env, rep->options.info_log.get(),
                         &table_properties);
    }

    if (!s.ok()) {
      auto err_msg =
        "[warning] encountered error while reading data from properties "
        "block " + s.tostring();
      log(rep->options.info_log, "%s", err_msg.c_str());
    } else {
      rep->table_properties.reset(table_properties);
    }
  } else {
    log(warn_level, rep->options.info_log,
        "cannot find properties block from file.");
  }

  // will use block cache for index/filter blocks access?
  if (table_options.block_cache &&
      table_options.cache_index_and_filter_blocks) {
    // hack: call newindexiterator() to implicitly add index to the block_cache
    unique_ptr<iterator> iter(new_table->newindexiterator(readoptions()));
    s = iter->status();

    if (s.ok()) {
      // hack: call getfilter() to implicitly add filter to the block_cache
      auto filter_entry = new_table->getfilter();
      filter_entry.release(table_options.block_cache.get());
    }
  } else {
    // if we don't use block cache for index/filter blocks access, we'll
    // pre-load these blocks, which will kept in member variables in rep
    // and with a same life-time as this table object.
    indexreader* index_reader = nullptr;
    // todo: we never really verify check sum for index block
    s = new_table->createindexreader(&index_reader, meta_iter.get());

    if (s.ok()) {
      rep->index_reader.reset(index_reader);

      // set filter block
      if (rep->filter_policy) {
        std::string key = kfilterblockprefix;
        key.append(rep->filter_policy->name());
        blockhandle handle;
        if (findmetablock(meta_iter.get(), key, &handle).ok()) {
          rep->filter.reset(readfilter(handle, rep));
        }
      }
    } else {
      delete index_reader;
    }
  }

  if (s.ok()) {
    *table_reader = std::move(new_table);
  }

  return s;
}

void blockbasedtable::setupforcompaction() {
  switch (rep_->options.access_hint_on_compaction_start) {
    case options::none:
      break;
    case options::normal:
      rep_->file->hint(randomaccessfile::normal);
      break;
    case options::sequential:
      rep_->file->hint(randomaccessfile::sequential);
      break;
    case options::willneed:
      rep_->file->hint(randomaccessfile::willneed);
      break;
    default:
      assert(false);
  }
  compaction_optimized_ = true;
}

std::shared_ptr<const tableproperties> blockbasedtable::gettableproperties()
    const {
  return rep_->table_properties;
}

size_t blockbasedtable::approximatememoryusage() const {
  size_t usage = 0;
  if (rep_->filter) {
    usage += rep_->filter->approximatememoryusage();
  }
  if (rep_->index_reader) {
    usage += rep_->index_reader->approximatememoryusage();
  }
  return usage;
}

// load the meta-block from the file. on success, return the loaded meta block
// and its iterator.
status blockbasedtable::readmetablock(
    rep* rep,
    std::unique_ptr<block>* meta_block,
    std::unique_ptr<iterator>* iter) {
  // todo(sanjay): skip this if footer.metaindex_handle() size indicates
  // it is an empty block.
  //  todo: we never really verify check sum for meta index block
  block* meta = nullptr;
  status s = readblockfromfile(
      rep->file.get(),
      rep->footer,
      readoptions(),
      rep->footer.metaindex_handle(),
      &meta,
      rep->options.env);

    if (!s.ok()) {
      auto err_msg =
        "[warning] encountered error while reading data from properties"
        "block " + s.tostring();
      log(rep->options.info_log, "%s", err_msg.c_str());
    }
  if (!s.ok()) {
    delete meta;
    return s;
  }

  meta_block->reset(meta);
  // meta block uses bytewise comparator.
  iter->reset(meta->newiterator(bytewisecomparator()));
  return status::ok();
}

status blockbasedtable::getdatablockfromcache(
    const slice& block_cache_key, const slice& compressed_block_cache_key,
    cache* block_cache, cache* block_cache_compressed, statistics* statistics,
    const readoptions& read_options,
    blockbasedtable::cachableentry<block>* block) {
  status s;
  block* compressed_block = nullptr;
  cache::handle* block_cache_compressed_handle = nullptr;

  // lookup uncompressed cache first
  if (block_cache != nullptr) {
    block->cache_handle =
        getentryfromcache(block_cache, block_cache_key, block_cache_data_miss,
                          block_cache_data_hit, statistics);
    if (block->cache_handle != nullptr) {
      block->value =
          reinterpret_cast<block*>(block_cache->value(block->cache_handle));
      return s;
    }
  }

  // if not found, search from the compressed block cache.
  assert(block->cache_handle == nullptr && block->value == nullptr);

  if (block_cache_compressed == nullptr) {
    return s;
  }

  assert(!compressed_block_cache_key.empty());
  block_cache_compressed_handle =
      block_cache_compressed->lookup(compressed_block_cache_key);
  // if we found in the compressed cache, then uncompress and insert into
  // uncompressed cache
  if (block_cache_compressed_handle == nullptr) {
    recordtick(statistics, block_cache_compressed_miss);
    return s;
  }

  // found compressed block
  recordtick(statistics, block_cache_compressed_hit);
  compressed_block = reinterpret_cast<block*>(
      block_cache_compressed->value(block_cache_compressed_handle));
  assert(compressed_block->compression_type() != knocompression);

  // retrieve the uncompressed contents into a new buffer
  blockcontents contents;
  s = uncompressblockcontents(compressed_block->data(),
                              compressed_block->size(), &contents);

  // insert uncompressed block into block cache
  if (s.ok()) {
    block->value = new block(contents);  // uncompressed block
    assert(block->value->compression_type() == knocompression);
    if (block_cache != nullptr && block->value->cachable() &&
        read_options.fill_cache) {
      block->cache_handle =
          block_cache->insert(block_cache_key, block->value,
                              block->value->size(), &deletecachedentry<block>);
      assert(reinterpret_cast<block*>(
                 block_cache->value(block->cache_handle)) == block->value);
    }
  }

  // release hold on compressed cache entry
  block_cache_compressed->release(block_cache_compressed_handle);
  return s;
}

status blockbasedtable::putdatablocktocache(
    const slice& block_cache_key, const slice& compressed_block_cache_key,
    cache* block_cache, cache* block_cache_compressed,
    const readoptions& read_options, statistics* statistics,
    cachableentry<block>* block, block* raw_block) {
  assert(raw_block->compression_type() == knocompression ||
         block_cache_compressed != nullptr);

  status s;
  // retrieve the uncompressed contents into a new buffer
  blockcontents contents;
  if (raw_block->compression_type() != knocompression) {
    s = uncompressblockcontents(raw_block->data(), raw_block->size(),
                                &contents);
  }
  if (!s.ok()) {
    delete raw_block;
    return s;
  }

  if (raw_block->compression_type() != knocompression) {
    block->value = new block(contents);  // uncompressed block
  } else {
    block->value = raw_block;
    raw_block = nullptr;
  }

  // insert compressed block into compressed block cache.
  // release the hold on the compressed cache entry immediately.
  if (block_cache_compressed != nullptr && raw_block != nullptr &&
      raw_block->cachable()) {
    auto cache_handle = block_cache_compressed->insert(
        compressed_block_cache_key, raw_block, raw_block->size(),
        &deletecachedentry<block>);
    block_cache_compressed->release(cache_handle);
    recordtick(statistics, block_cache_compressed_miss);
    // avoid the following code to delete this cached block.
    raw_block = nullptr;
  }
  delete raw_block;

  // insert into uncompressed block cache
  assert((block->value->compression_type() == knocompression));
  if (block_cache != nullptr && block->value->cachable()) {
    block->cache_handle =
        block_cache->insert(block_cache_key, block->value, block->value->size(),
                            &deletecachedentry<block>);
    recordtick(statistics, block_cache_add);
    assert(reinterpret_cast<block*>(block_cache->value(block->cache_handle)) ==
           block->value);
  }

  return s;
}

filterblockreader* blockbasedtable::readfilter(const blockhandle& filter_handle,
                                               blockbasedtable::rep* rep,
                                               size_t* filter_size) {
  // todo: we might want to unify with readblockfromfile() if we start
  // requiring checksum verification in table::open.
  readoptions opt;
  blockcontents block;
  if (!readblockcontents(rep->file.get(), rep->footer, opt, filter_handle,
                         &block, rep->options.env, false).ok()) {
    return nullptr;
  }

  if (filter_size) {
    *filter_size = block.data.size();
  }

  return new filterblockreader(
       rep->options, rep->table_options, block.data, block.heap_allocated);
}

blockbasedtable::cachableentry<filterblockreader> blockbasedtable::getfilter(
    bool no_io) const {
  // filter pre-populated
  if (rep_->filter != nullptr) {
    return {rep_->filter.get(), nullptr /* cache handle */};
  }

  cache* block_cache = rep_->table_options.block_cache.get();
  if (rep_->filter_policy == nullptr /* do not use filter */ ||
      block_cache == nullptr /* no block cache at all */) {
    return {nullptr /* filter */, nullptr /* cache handle */};
  }

  // fetching from the cache
  char cache_key[kmaxcachekeyprefixsize + kmaxvarint64length];
  auto key = getcachekey(
      rep_->cache_key_prefix,
      rep_->cache_key_prefix_size,
      rep_->footer.metaindex_handle(),
      cache_key
  );

  statistics* statistics = rep_->options.statistics.get();
  auto cache_handle =
      getentryfromcache(block_cache, key, block_cache_filter_miss,
                        block_cache_filter_hit, statistics);

  filterblockreader* filter = nullptr;
  if (cache_handle != nullptr) {
     filter = reinterpret_cast<filterblockreader*>(
         block_cache->value(cache_handle));
  } else if (no_io) {
    // do not invoke any io.
    return cachableentry<filterblockreader>();
  } else {
    size_t filter_size = 0;
    std::unique_ptr<block> meta;
    std::unique_ptr<iterator> iter;
    auto s = readmetablock(rep_, &meta, &iter);

    if (s.ok()) {
      std::string filter_block_key = kfilterblockprefix;
      filter_block_key.append(rep_->filter_policy->name());
      blockhandle handle;
      if (findmetablock(iter.get(), filter_block_key, &handle).ok()) {
        filter = readfilter(handle, rep_, &filter_size);
        assert(filter);
        assert(filter_size > 0);

        cache_handle = block_cache->insert(
            key, filter, filter_size, &deletecachedentry<filterblockreader>);
        recordtick(statistics, block_cache_add);
      }
    }
  }

  return { filter, cache_handle };
}

iterator* blockbasedtable::newindexiterator(const readoptions& read_options,
        blockiter* input_iter) {
  // index reader has already been pre-populated.
  if (rep_->index_reader) {
    return rep_->index_reader->newiterator(
        input_iter, read_options.total_order_seek);
  }

  bool no_io = read_options.read_tier == kblockcachetier;
  cache* block_cache = rep_->table_options.block_cache.get();
  char cache_key[kmaxcachekeyprefixsize + kmaxvarint64length];
  auto key = getcachekey(rep_->cache_key_prefix, rep_->cache_key_prefix_size,
                         rep_->footer.index_handle(), cache_key);
  statistics* statistics = rep_->options.statistics.get();
  auto cache_handle =
      getentryfromcache(block_cache, key, block_cache_index_miss,
                        block_cache_index_hit, statistics);

  if (cache_handle == nullptr && no_io) {
    if (input_iter != nullptr) {
      input_iter->setstatus(status::incomplete("no blocking io"));
      return input_iter;
    } else {
      return newerroriterator(status::incomplete("no blocking io"));
    }
  }

  indexreader* index_reader = nullptr;
  if (cache_handle != nullptr) {
    index_reader =
        reinterpret_cast<indexreader*>(block_cache->value(cache_handle));
  } else {
    // create index reader and put it in the cache.
    status s;
    s = createindexreader(&index_reader);

    if (!s.ok()) {
      // make sure if something goes wrong, index_reader shall remain intact.
      assert(index_reader == nullptr);
      if (input_iter != nullptr) {
        input_iter->setstatus(s);
        return input_iter;
      } else {
        return newerroriterator(s);
      }
    }

    cache_handle = block_cache->insert(key, index_reader, index_reader->size(),
                                       &deletecachedentry<indexreader>);
    recordtick(statistics, block_cache_add);
  }

  assert(cache_handle);
  auto* iter = index_reader->newiterator(
      input_iter, read_options.total_order_seek);
  iter->registercleanup(&releasecachedentry, block_cache, cache_handle);
  return iter;
}

// convert an index iterator value (i.e., an encoded blockhandle)
// into an iterator over the contents of the corresponding block.
// if input_iter is null, new a iterator
// if input_iter is not null, update this iter and return it
iterator* blockbasedtable::newdatablockiterator(rep* rep,
    const readoptions& ro, const slice& index_value,
    blockiter* input_iter) {
  const bool no_io = (ro.read_tier == kblockcachetier);
  cache* block_cache = rep->table_options.block_cache.get();
  cache* block_cache_compressed =
      rep->table_options.block_cache_compressed.get();
  cachableentry<block> block;

  blockhandle handle;
  slice input = index_value;
  // we intentionally allow extra stuff in index_value so that we
  // can add more features in the future.
  status s = handle.decodefrom(&input);

  if (!s.ok()) {
    if (input_iter != nullptr) {
      input_iter->setstatus(s);
      return input_iter;
    } else {
      return newerroriterator(s);
    }
  }

  // if either block cache is enabled, we'll try to read from it.
  if (block_cache != nullptr || block_cache_compressed != nullptr) {
    statistics* statistics = rep->options.statistics.get();
    char cache_key[kmaxcachekeyprefixsize + kmaxvarint64length];
    char compressed_cache_key[kmaxcachekeyprefixsize + kmaxvarint64length];
    slice key, /* key to the block cache */
        ckey /* key to the compressed block cache */;

    // create key for block cache
    if (block_cache != nullptr) {
      key = getcachekey(rep->cache_key_prefix,
                        rep->cache_key_prefix_size, handle, cache_key);
    }

    if (block_cache_compressed != nullptr) {
      ckey = getcachekey(rep->compressed_cache_key_prefix,
                         rep->compressed_cache_key_prefix_size, handle,
                         compressed_cache_key);
    }

    s = getdatablockfromcache(key, ckey, block_cache, block_cache_compressed,
                              statistics, ro, &block);

    if (block.value == nullptr && !no_io && ro.fill_cache) {
      block* raw_block = nullptr;
      {
        stopwatch sw(rep->options.env, statistics, read_block_get_micros);
        s = readblockfromfile(rep->file.get(), rep->footer, ro, handle,
                              &raw_block, rep->options.env,
                              block_cache_compressed == nullptr);
      }

      if (s.ok()) {
        s = putdatablocktocache(key, ckey, block_cache, block_cache_compressed,
                                ro, statistics, &block, raw_block);
      }
    }
  }

  // didn't get any data from block caches.
  if (block.value == nullptr) {
    if (no_io) {
      // could not read from block_cache and can't do io
      if (input_iter != nullptr) {
        input_iter->setstatus(status::incomplete("no blocking io"));
        return input_iter;
      } else {
        return newerroriterator(status::incomplete("no blocking io"));
      }
    }
    s = readblockfromfile(rep->file.get(), rep->footer, ro, handle,
                          &block.value, rep->options.env);
  }

  iterator* iter;
  if (block.value != nullptr) {
    iter = block.value->newiterator(&rep->internal_comparator, input_iter);
    if (block.cache_handle != nullptr) {
      iter->registercleanup(&releasecachedentry, block_cache,
          block.cache_handle);
    } else {
      iter->registercleanup(&deleteheldresource<block>, block.value, nullptr);
    }
  } else {
    if (input_iter != nullptr) {
      input_iter->setstatus(s);
      iter = input_iter;
    } else {
      iter = newerroriterator(s);
    }
  }
  return iter;
}

class blockbasedtable::blockentryiteratorstate : public twoleveliteratorstate {
 public:
  blockentryiteratorstate(blockbasedtable* table,
                          const readoptions& read_options)
      : twoleveliteratorstate(table->rep_->options.prefix_extractor != nullptr),
        table_(table),
        read_options_(read_options) {}

  iterator* newsecondaryiterator(const slice& index_value) override {
    return newdatablockiterator(table_->rep_, read_options_, index_value);
  }

  bool prefixmaymatch(const slice& internal_key) override {
    if (read_options_.total_order_seek) {
      return true;
    }
    return table_->prefixmaymatch(internal_key);
  }

 private:
  // don't own table_
  blockbasedtable* table_;
  const readoptions read_options_;
};

// this will be broken if the user specifies an unusual implementation
// of options.comparator, or if the user specifies an unusual
// definition of prefixes in blockbasedtableoptions.filter_policy.
// in particular, we require the following three properties:
//
// 1) key.starts_with(prefix(key))
// 2) compare(prefix(key), key) <= 0.
// 3) if compare(key1, key2) <= 0, then compare(prefix(key1), prefix(key2)) <= 0
//
// otherwise, this method guarantees no i/o will be incurred.
//
// requires: this method shouldn't be called while the db lock is held.
bool blockbasedtable::prefixmaymatch(const slice& internal_key) {
  if (!rep_->filter_policy) {
    return true;
  }

  assert(rep_->options.prefix_extractor != nullptr);
  auto prefix = rep_->options.prefix_extractor->transform(
      extractuserkey(internal_key));
  internalkey internal_key_prefix(prefix, 0, ktypevalue);
  auto internal_prefix = internal_key_prefix.encode();

  bool may_match = true;
  status s;

  // to prevent any io operation in this method, we set `read_tier` to make
  // sure we always read index or filter only when they have already been
  // loaded to memory.
  readoptions no_io_read_options;
  no_io_read_options.read_tier = kblockcachetier;
  unique_ptr<iterator> iiter(newindexiterator(no_io_read_options));
  iiter->seek(internal_prefix);

  if (!iiter->valid()) {
    // we're past end of file
    // if it's incomplete, it means that we avoided i/o
    // and we're not really sure that we're past the end
    // of the file
    may_match = iiter->status().isincomplete();
  } else if (extractuserkey(iiter->key()).starts_with(
              extractuserkey(internal_prefix))) {
    // we need to check for this subtle case because our only
    // guarantee is that "the key is a string >= last key in that data
    // block" according to the doc/table_format.txt spec.
    //
    // suppose iiter->key() starts with the desired prefix; it is not
    // necessarily the case that the corresponding data block will
    // contain the prefix, since iiter->key() need not be in the
    // block.  however, the next data block may contain the prefix, so
    // we return true to play it safe.
    may_match = true;
  } else {
    // iiter->key() does not start with the desired prefix.  because
    // seek() finds the first key that is >= the seek target, this
    // means that iiter->key() > prefix.  thus, any data blocks coming
    // after the data block corresponding to iiter->key() cannot
    // possibly contain the key.  thus, the corresponding data block
    // is the only one which could potentially contain the prefix.
    slice handle_value = iiter->value();
    blockhandle handle;
    s = handle.decodefrom(&handle_value);
    assert(s.ok());
    auto filter_entry = getfilter(true /* no io */);
    may_match = filter_entry.value == nullptr ||
                filter_entry.value->prefixmaymatch(handle.offset(), prefix);
    filter_entry.release(rep_->table_options.block_cache.get());
  }

  statistics* statistics = rep_->options.statistics.get();
  recordtick(statistics, bloom_filter_prefix_checked);
  if (!may_match) {
    recordtick(statistics, bloom_filter_prefix_useful);
  }

  return may_match;
}

iterator* blockbasedtable::newiterator(const readoptions& read_options,
                                       arena* arena) {
  return newtwoleveliterator(new blockentryiteratorstate(this, read_options),
                             newindexiterator(read_options), arena);
}

status blockbasedtable::get(
    const readoptions& read_options, const slice& key, void* handle_context,
    bool (*result_handler)(void* handle_context, const parsedinternalkey& k,
                           const slice& v),
    void (*mark_key_may_exist_handler)(void* handle_context)) {
  status s;
  blockiter iiter;
  newindexiterator(read_options, &iiter);

  auto filter_entry = getfilter(read_options.read_tier == kblockcachetier);
  filterblockreader* filter = filter_entry.value;
  bool done = false;
  for (iiter.seek(key); iiter.valid() && !done; iiter.next()) {
    slice handle_value = iiter.value();

    blockhandle handle;
    bool may_not_exist_in_filter =
        filter != nullptr && handle.decodefrom(&handle_value).ok() &&
        !filter->keymaymatch(handle.offset(), extractuserkey(key));

    if (may_not_exist_in_filter) {
      // not found
      // todo: think about interaction with merge. if a user key cannot
      // cross one data block, we should be fine.
      recordtick(rep_->options.statistics.get(), bloom_filter_useful);
      break;
    } else {
      blockiter biter;
      newdatablockiterator(rep_, read_options, iiter.value(), &biter);

      if (read_options.read_tier && biter.status().isincomplete()) {
        // couldn't get block from block_cache
        // update saver.state to found because we are only looking for whether
        // we can guarantee the key is not there when "no_io" is set
        (*mark_key_may_exist_handler)(handle_context);
        break;
      }
      if (!biter.status().ok()) {
        s = biter.status();
        break;
      }

      // call the *saver function on each entry/block until it returns false
      for (biter.seek(key); biter.valid(); biter.next()) {
        parsedinternalkey parsed_key;
        if (!parseinternalkey(biter.key(), &parsed_key)) {
          s = status::corruption(slice());
        }

        if (!(*result_handler)(handle_context, parsed_key,
                               biter.value())) {
          done = true;
          break;
        }
      }
      s = biter.status();
    }
  }

  filter_entry.release(rep_->table_options.block_cache.get());
  if (s.ok()) {
    s = iiter.status();
  }

  return s;
}

bool blockbasedtable::test_keyincache(const readoptions& options,
                                      const slice& key) {
  std::unique_ptr<iterator> iiter(newindexiterator(options));
  iiter->seek(key);
  assert(iiter->valid());
  cachableentry<block> block;

  blockhandle handle;
  slice input = iiter->value();
  status s = handle.decodefrom(&input);
  assert(s.ok());
  cache* block_cache = rep_->table_options.block_cache.get();
  assert(block_cache != nullptr);

  char cache_key_storage[kmaxcachekeyprefixsize + kmaxvarint64length];
  slice cache_key =
      getcachekey(rep_->cache_key_prefix, rep_->cache_key_prefix_size, handle,
                  cache_key_storage);
  slice ckey;

  s = getdatablockfromcache(cache_key, ckey, block_cache, nullptr, nullptr,
                            options, &block);
  assert(s.ok());
  bool in_cache = block.value != nullptr;
  if (in_cache) {
    releasecachedentry(block_cache, block.cache_handle);
  }
  return in_cache;
}

// requires: the following fields of rep_ should have already been populated:
//  1. file
//  2. index_handle,
//  3. options
//  4. internal_comparator
//  5. index_type
status blockbasedtable::createindexreader(indexreader** index_reader,
                                          iterator* preloaded_meta_index_iter) {
  // some old version of block-based tables don't have index type present in
  // table properties. if that's the case we can safely use the kbinarysearch.
  auto index_type_on_file = blockbasedtableoptions::kbinarysearch;
  if (rep_->table_properties) {
    auto& props = rep_->table_properties->user_collected_properties;
    auto pos = props.find(blockbasedtablepropertynames::kindextype);
    if (pos != props.end()) {
      index_type_on_file = static_cast<blockbasedtableoptions::indextype>(
          decodefixed32(pos->second.c_str()));
    }
  }

  auto file = rep_->file.get();
  auto env = rep_->options.env;
  auto comparator = &rep_->internal_comparator;
  const footer& footer = rep_->footer;

  if (index_type_on_file == blockbasedtableoptions::khashsearch &&
      rep_->options.prefix_extractor == nullptr) {
    log(rep_->options.info_log,
        "blockbasedtableoptions::khashsearch requires "
        "options.prefix_extractor to be set."
        " fall back to binary seach index.");
    index_type_on_file = blockbasedtableoptions::kbinarysearch;
  }

  switch (index_type_on_file) {
    case blockbasedtableoptions::kbinarysearch: {
      return binarysearchindexreader::create(
          file, footer, footer.index_handle(), env, comparator, index_reader);
    }
    case blockbasedtableoptions::khashsearch: {
      std::unique_ptr<block> meta_guard;
      std::unique_ptr<iterator> meta_iter_guard;
      auto meta_index_iter = preloaded_meta_index_iter;
      if (meta_index_iter == nullptr) {
        auto s = readmetablock(rep_, &meta_guard, &meta_iter_guard);
        if (!s.ok()) {
          // we simply fall back to binary search in case there is any
          // problem with prefix hash index loading.
          log(rep_->options.info_log,
              "unable to read the metaindex block."
              " fall back to binary seach index.");
          return binarysearchindexreader::create(
            file, footer, footer.index_handle(), env, comparator, index_reader);
        }
        meta_index_iter = meta_iter_guard.get();
      }

      // we need to wrap data with internal_prefix_transform to make sure it can
      // handle prefix correctly.
      rep_->internal_prefix_transform.reset(
          new internalkeyslicetransform(rep_->options.prefix_extractor.get()));
      return hashindexreader::create(
          rep_->internal_prefix_transform.get(), footer, file, env, comparator,
          footer.index_handle(), meta_index_iter, index_reader,
          rep_->hash_index_allow_collision);
    }
    default: {
      std::string error_message =
          "unrecognized index type: " + std::to_string(rep_->index_type);
      return status::invalidargument(error_message.c_str());
    }
  }
}

uint64_t blockbasedtable::approximateoffsetof(const slice& key) {
  unique_ptr<iterator> index_iter(newindexiterator(readoptions()));

  index_iter->seek(key);
  uint64_t result;
  if (index_iter->valid()) {
    blockhandle handle;
    slice input = index_iter->value();
    status s = handle.decodefrom(&input);
    if (s.ok()) {
      result = handle.offset();
    } else {
      // strange: we can't decode the block handle in the index block.
      // we'll just return the offset of the metaindex block, which is
      // close to the whole file size for this case.
      result = rep_->footer.metaindex_handle().offset();
    }
  } else {
    // key is past the last key in the file. if table_properties is not
    // available, approximate the offset by returning the offset of the
    // metaindex block (which is right near the end of the file).
    result = 0;
    if (rep_->table_properties) {
      result = rep_->table_properties->data_size;
    }
    // table_properties is not present in the table.
    if (result == 0) {
      result = rep_->footer.metaindex_handle().offset();
    }
  }
  return result;
}

bool blockbasedtable::test_filter_block_preloaded() const {
  return rep_->filter != nullptr;
}

bool blockbasedtable::test_index_reader_preloaded() const {
  return rep_->index_reader != nullptr;
}

}  // namespace rocksdb
