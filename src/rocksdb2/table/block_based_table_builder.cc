//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "table/block_based_table_builder.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "db/dbformat.h"

#include "rocksdb/cache.h"
#include "rocksdb/comparator.h"
#include "rocksdb/env.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/flush_block_policy.h"
#include "rocksdb/options.h"
#include "rocksdb/table.h"

#include "table/block.h"
#include "table/block_based_table_reader.h"
#include "table/block_builder.h"
#include "table/filter_block.h"
#include "table/format.h"
#include "table/meta_blocks.h"
#include "table/table_builder.h"

#include "util/coding.h"
#include "util/crc32c.h"
#include "util/stop_watch.h"
#include "util/xxhash.h"

namespace rocksdb {

extern const std::string khashindexprefixesblock;
extern const std::string khashindexprefixesmetadatablock;

typedef blockbasedtableoptions::indextype indextype;

// the interface for building index.
// instruction for adding a new concrete indexbuilder:
//  1. create a subclass instantiated from indexbuilder.
//  2. add a new entry associated with that subclass in tableoptions::indextype.
//  3. add a create function for the new subclass in createindexbuilder.
// note: we can devise more advanced design to simplify the process for adding
// new subclass, which will, on the other hand, increase the code complexity and
// catch unwanted attention from readers. given that we won't add/change
// indexes frequently, it makes sense to just embrace a more straightforward
// design that just works.
class indexbuilder {
 public:
  // index builder will construct a set of blocks which contain:
  //  1. one primary index block.
  //  2. (optional) a set of metablocks that contains the metadata of the
  //     primary index.
  struct indexblocks {
    slice index_block_contents;
    std::unordered_map<std::string, slice> meta_blocks;
  };
  explicit indexbuilder(const comparator* comparator)
      : comparator_(comparator) {}

  virtual ~indexbuilder() {}

  // add a new index entry to index block.
  // to allow further optimization, we provide `last_key_in_current_block` and
  // `first_key_in_next_block`, based on which the specific implementation can
  // determine the best index key to be used for the index block.
  // @last_key_in_current_block: this parameter maybe overridden with the value
  //                             "substitute key".
  // @first_key_in_next_block: it will be nullptr if the entry being added is
  //                           the last one in the table
  //
  // requires: finish() has not yet been called.
  virtual void addindexentry(std::string* last_key_in_current_block,
                             const slice* first_key_in_next_block,
                             const blockhandle& block_handle) = 0;

  // this method will be called whenever a key is added. the subclasses may
  // override onkeyadded() if they need to collect additional information.
  virtual void onkeyadded(const slice& key) {}

  // inform the index builder that all entries has been written. block builder
  // may therefore perform any operation required for block finalization.
  //
  // requires: finish() has not yet been called.
  virtual status finish(indexblocks* index_blocks) = 0;

  // get the estimated size for index block.
  virtual size_t estimatedsize() const = 0;

 protected:
  const comparator* comparator_;
};

// this index builder builds space-efficient index block.
//
// optimizations:
//  1. made block's `block_restart_interval` to be 1, which will avoid linear
//     search when doing index lookup.
//  2. shorten the key length for index block. other than honestly using the
//     last key in the data block as the index key, we instead find a shortest
//     substitute key that serves the same function.
class shortenedindexbuilder : public indexbuilder {
 public:
  explicit shortenedindexbuilder(const comparator* comparator)
      : indexbuilder(comparator),
        index_block_builder_(1 /* block_restart_interval == 1 */) {}

  virtual void addindexentry(std::string* last_key_in_current_block,
                             const slice* first_key_in_next_block,
                             const blockhandle& block_handle) override {
    if (first_key_in_next_block != nullptr) {
      comparator_->findshortestseparator(last_key_in_current_block,
                                         *first_key_in_next_block);
    } else {
      comparator_->findshortsuccessor(last_key_in_current_block);
    }

    std::string handle_encoding;
    block_handle.encodeto(&handle_encoding);
    index_block_builder_.add(*last_key_in_current_block, handle_encoding);
  }

  virtual status finish(indexblocks* index_blocks) {
    index_blocks->index_block_contents = index_block_builder_.finish();
    return status::ok();
  }

  virtual size_t estimatedsize() const {
    return index_block_builder_.currentsizeestimate();
  }

 private:
  blockbuilder index_block_builder_;
};

// hashindexbuilder contains a binary-searchable primary index and the
// metadata for secondary hash index construction.
// the metadata for hash index consists two parts:
//  - a metablock that compactly contains a sequence of prefixes. all prefixes
//    are stored consectively without any metadata (like, prefix sizes) being
//    stored, which is kept in the other metablock.
//  - a metablock contains the metadata of the prefixes, including prefix size,
//    restart index and number of block it spans. the format looks like:
//
// +-----------------+---------------------------+---------------------+ <=prefix 1
// | length: 4 bytes | restart interval: 4 bytes | num-blocks: 4 bytes |
// +-----------------+---------------------------+---------------------+ <=prefix 2
// | length: 4 bytes | restart interval: 4 bytes | num-blocks: 4 bytes |
// +-----------------+---------------------------+---------------------+
// |                                                                   |
// | ....                                                              |
// |                                                                   |
// +-----------------+---------------------------+---------------------+ <=prefix n
// | length: 4 bytes | restart interval: 4 bytes | num-blocks: 4 bytes |
// +-----------------+---------------------------+---------------------+
//
// the reason of separating these two metablocks is to enable the efficiently
// reuse the first metablock during hash index construction without unnecessary
// data copy or small heap allocations for prefixes.
class hashindexbuilder : public indexbuilder {
 public:
  explicit hashindexbuilder(const comparator* comparator,
                            const slicetransform* hash_key_extractor)
      : indexbuilder(comparator),
        primary_index_builder(comparator),
        hash_key_extractor_(hash_key_extractor) {}

  virtual void addindexentry(std::string* last_key_in_current_block,
                             const slice* first_key_in_next_block,
                             const blockhandle& block_handle) override {
    ++current_restart_index_;
    primary_index_builder.addindexentry(last_key_in_current_block,
                                        first_key_in_next_block, block_handle);
  }

  virtual void onkeyadded(const slice& key) override {
    auto key_prefix = hash_key_extractor_->transform(key);
    bool is_first_entry = pending_block_num_ == 0;

    // keys may share the prefix
    if (is_first_entry || pending_entry_prefix_ != key_prefix) {
      if (!is_first_entry) {
        flushpendingprefix();
      }

      // need a hard copy otherwise the underlying data changes all the time.
      // todo(kailiu) tostring() is expensive. we may speed up can avoid data
      // copy.
      pending_entry_prefix_ = key_prefix.tostring();
      pending_block_num_ = 1;
      pending_entry_index_ = current_restart_index_;
    } else {
      // entry number increments when keys share the prefix reside in
      // differnt data blocks.
      auto last_restart_index = pending_entry_index_ + pending_block_num_ - 1;
      assert(last_restart_index <= current_restart_index_);
      if (last_restart_index != current_restart_index_) {
        ++pending_block_num_;
      }
    }
  }

  virtual status finish(indexblocks* index_blocks) {
    flushpendingprefix();
    primary_index_builder.finish(index_blocks);
    index_blocks->meta_blocks.insert(
        {khashindexprefixesblock.c_str(), prefix_block_});
    index_blocks->meta_blocks.insert(
        {khashindexprefixesmetadatablock.c_str(), prefix_meta_block_});
    return status::ok();
  }

  virtual size_t estimatedsize() const {
    return primary_index_builder.estimatedsize() + prefix_block_.size() +
           prefix_meta_block_.size();
  }

 private:
  void flushpendingprefix() {
    prefix_block_.append(pending_entry_prefix_.data(),
                         pending_entry_prefix_.size());
    putvarint32(&prefix_meta_block_, pending_entry_prefix_.size());
    putvarint32(&prefix_meta_block_, pending_entry_index_);
    putvarint32(&prefix_meta_block_, pending_block_num_);
  }

  shortenedindexbuilder primary_index_builder;
  const slicetransform* hash_key_extractor_;

  // stores a sequence of prefixes
  std::string prefix_block_;
  // stores the metadata of prefixes
  std::string prefix_meta_block_;

  // the following 3 variables keeps unflushed prefix and its metadata.
  // the details of block_num and entry_index can be found in
  // "block_hash_index.{h,cc}"
  uint32_t pending_block_num_ = 0;
  uint32_t pending_entry_index_ = 0;
  std::string pending_entry_prefix_;

  uint64_t current_restart_index_ = 0;
};

// create a index builder based on its type.
indexbuilder* createindexbuilder(indextype type, const comparator* comparator,
                                 const slicetransform* prefix_extractor) {
  switch (type) {
    case blockbasedtableoptions::kbinarysearch: {
      return new shortenedindexbuilder(comparator);
    }
    case blockbasedtableoptions::khashsearch: {
      return new hashindexbuilder(comparator, prefix_extractor);
    }
    default: {
      assert(!"do not recognize the index type ");
      return nullptr;
    }
  }
  // impossible.
  assert(false);
  return nullptr;
}

bool goodcompressionratio(size_t compressed_size, size_t raw_size) {
  // check to see if compressed less than 12.5%
  return compressed_size < raw_size - (raw_size / 8u);
}

slice compressblock(const slice& raw,
                    const compressionoptions& compression_options,
                    compressiontype* type, std::string* compressed_output) {
  if (*type == knocompression) {
    return raw;
  }

  // will return compressed block contents if (1) the compression method is
  // supported in this platform and (2) the compression rate is "good enough".
  switch (*type) {
    case ksnappycompression:
      if (port::snappy_compress(compression_options, raw.data(), raw.size(),
                                compressed_output) &&
          goodcompressionratio(compressed_output->size(), raw.size())) {
        return *compressed_output;
      }
      break;  // fall back to no compression.
    case kzlibcompression:
      if (port::zlib_compress(compression_options, raw.data(), raw.size(),
                              compressed_output) &&
          goodcompressionratio(compressed_output->size(), raw.size())) {
        return *compressed_output;
      }
      break;  // fall back to no compression.
    case kbzip2compression:
      if (port::bzip2_compress(compression_options, raw.data(), raw.size(),
                               compressed_output) &&
          goodcompressionratio(compressed_output->size(), raw.size())) {
        return *compressed_output;
      }
      break;  // fall back to no compression.
    case klz4compression:
      if (port::lz4_compress(compression_options, raw.data(), raw.size(),
                             compressed_output) &&
          goodcompressionratio(compressed_output->size(), raw.size())) {
        return *compressed_output;
      }
      break;  // fall back to no compression.
    case klz4hccompression:
      if (port::lz4hc_compress(compression_options, raw.data(), raw.size(),
                               compressed_output) &&
          goodcompressionratio(compressed_output->size(), raw.size())) {
        return *compressed_output;
      }
      break;     // fall back to no compression.
    default: {}  // do not recognize this compression type
  }

  // compression method is not supported, or not good compression ratio, so just
  // fall back to uncompressed form.
  *type = knocompression;
  return raw;
}

// kblockbasedtablemagicnumber was picked by running
//    echo rocksdb.table.block_based | sha1sum
// and taking the leading 64 bits.
// please note that kblockbasedtablemagicnumber may also be accessed by
// other .cc files so it have to be explicitly declared with "extern".
extern const uint64_t kblockbasedtablemagicnumber = 0x88e241b785f4cff7ull;
// we also support reading and writing legacy block based table format (for
// backwards compatibility)
extern const uint64_t klegacyblockbasedtablemagicnumber = 0xdb4775248b80fb57ull;

// a collector that collects properties of interest to block-based table.
// for now this class looks heavy-weight since we only write one additional
// property.
// but in the forseeable future, we will add more and more properties that are
// specific to block-based table.
class blockbasedtablebuilder::blockbasedtablepropertiescollector
    : public tablepropertiescollector {
 public:
  explicit blockbasedtablepropertiescollector(
      blockbasedtableoptions::indextype index_type)
      : index_type_(index_type) {}

  virtual status add(const slice& key, const slice& value) {
    // intentionally left blank. have no interest in collecting stats for
    // individual key/value pairs.
    return status::ok();
  }

  virtual status finish(usercollectedproperties* properties) {
    std::string val;
    putfixed32(&val, static_cast<uint32_t>(index_type_));
    properties->insert({blockbasedtablepropertynames::kindextype, val});

    return status::ok();
  }

  // the name of the properties collector can be used for debugging purpose.
  virtual const char* name() const {
    return "blockbasedtablepropertiescollector";
  }

  virtual usercollectedproperties getreadableproperties() const {
    // intentionally left blank.
    return usercollectedproperties();
  }

 private:
  blockbasedtableoptions::indextype index_type_;
};

struct blockbasedtablebuilder::rep {
  const options options;
  const blockbasedtableoptions table_options;
  const internalkeycomparator& internal_comparator;
  writablefile* file;
  uint64_t offset = 0;
  status status;
  blockbuilder data_block;

  internalkeyslicetransform internal_prefix_transform;
  std::unique_ptr<indexbuilder> index_builder;

  std::string last_key;
  compressiontype compression_type;
  tableproperties props;

  bool closed = false;  // either finish() or abandon() has been called.
  filterblockbuilder* filter_block;
  char compressed_cache_key_prefix[blockbasedtable::kmaxcachekeyprefixsize];
  size_t compressed_cache_key_prefix_size;

  blockhandle pending_handle;  // handle to add to index block

  std::string compressed_output;
  std::unique_ptr<flushblockpolicy> flush_block_policy;

  std::vector<std::unique_ptr<tablepropertiescollector>>
      table_properties_collectors;

  rep(const options& opt, const blockbasedtableoptions& table_opt,
      const internalkeycomparator& icomparator,
      writablefile* f, compressiontype compression_type)
      : options(opt),
        table_options(table_opt),
        internal_comparator(icomparator),
        file(f),
        data_block(table_options.block_restart_interval),
        internal_prefix_transform(options.prefix_extractor.get()),
        index_builder(createindexbuilder(
              table_options.index_type, &internal_comparator,
              &this->internal_prefix_transform)),
        compression_type(compression_type),
        filter_block(table_options.filter_policy == nullptr ?
            nullptr :
            new filterblockbuilder(opt, table_options, &internal_comparator)),
        flush_block_policy(
            table_options.flush_block_policy_factory->newflushblockpolicy(
              table_options, data_block)) {
    for (auto& collector_factories :
         options.table_properties_collector_factories) {
      table_properties_collectors.emplace_back(
          collector_factories->createtablepropertiescollector());
    }
    table_properties_collectors.emplace_back(
        new blockbasedtablepropertiescollector(table_options.index_type));
  }
};

blockbasedtablebuilder::blockbasedtablebuilder(
    const options& options, const blockbasedtableoptions& table_options,
    const internalkeycomparator& internal_comparator, writablefile* file,
    compressiontype compression_type)
    : rep_(new rep(options, table_options, internal_comparator,
                   file, compression_type)) {
  if (rep_->filter_block != nullptr) {
    rep_->filter_block->startblock(0);
  }
  if (table_options.block_cache_compressed.get() != nullptr) {
    blockbasedtable::generatecacheprefix(
        table_options.block_cache_compressed.get(), file,
        &rep_->compressed_cache_key_prefix[0],
        &rep_->compressed_cache_key_prefix_size);
  }
}

blockbasedtablebuilder::~blockbasedtablebuilder() {
  assert(rep_->closed);  // catch errors where caller forgot to call finish()
  delete rep_->filter_block;
  delete rep_;
}

void blockbasedtablebuilder::add(const slice& key, const slice& value) {
  rep* r = rep_;
  assert(!r->closed);
  if (!ok()) return;
  if (r->props.num_entries > 0) {
    assert(r->internal_comparator.compare(key, slice(r->last_key)) > 0);
  }

  auto should_flush = r->flush_block_policy->update(key, value);
  if (should_flush) {
    assert(!r->data_block.empty());
    flush();

    // add item to index block.
    // we do not emit the index entry for a block until we have seen the
    // first key for the next data block.  this allows us to use shorter
    // keys in the index block.  for example, consider a block boundary
    // between the keys "the quick brown fox" and "the who".  we can use
    // "the r" as the key for the index block entry since it is >= all
    // entries in the first block and < all entries in subsequent
    // blocks.
    if (ok()) {
      r->index_builder->addindexentry(&r->last_key, &key, r->pending_handle);
    }
  }

  if (r->filter_block != nullptr) {
    r->filter_block->addkey(extractuserkey(key));
  }

  r->last_key.assign(key.data(), key.size());
  r->data_block.add(key, value);
  r->props.num_entries++;
  r->props.raw_key_size += key.size();
  r->props.raw_value_size += value.size();

  r->index_builder->onkeyadded(key);
  notifycollecttablecollectorsonadd(key, value, r->table_properties_collectors,
                                    r->options.info_log.get());
}

void blockbasedtablebuilder::flush() {
  rep* r = rep_;
  assert(!r->closed);
  if (!ok()) return;
  if (r->data_block.empty()) return;
  writeblock(&r->data_block, &r->pending_handle);
  if (ok()) {
    r->status = r->file->flush();
  }
  if (r->filter_block != nullptr) {
    r->filter_block->startblock(r->offset);
  }
  r->props.data_size = r->offset;
  ++r->props.num_data_blocks;
}

void blockbasedtablebuilder::writeblock(blockbuilder* block,
                                        blockhandle* handle) {
  writeblock(block->finish(), handle);
  block->reset();
}

void blockbasedtablebuilder::writeblock(const slice& raw_block_contents,
                                        blockhandle* handle) {
  // file format contains a sequence of blocks where each block has:
  //    block_data: uint8[n]
  //    type: uint8
  //    crc: uint32
  assert(ok());
  rep* r = rep_;

  auto type = r->compression_type;
  slice block_contents;
  if (raw_block_contents.size() < kcompressionsizelimit) {
    block_contents =
        compressblock(raw_block_contents, r->options.compression_opts, &type,
                      &r->compressed_output);
  } else {
    recordtick(r->options.statistics.get(), number_block_not_compressed);
    type = knocompression;
    block_contents = raw_block_contents;
  }
  writerawblock(block_contents, type, handle);
  r->compressed_output.clear();
}

void blockbasedtablebuilder::writerawblock(const slice& block_contents,
                                           compressiontype type,
                                           blockhandle* handle) {
  rep* r = rep_;
  stopwatch sw(r->options.env, r->options.statistics.get(),
               write_raw_block_micros);
  handle->set_offset(r->offset);
  handle->set_size(block_contents.size());
  r->status = r->file->append(block_contents);
  if (r->status.ok()) {
    char trailer[kblocktrailersize];
    trailer[0] = type;
    char* trailer_without_type = trailer + 1;
    switch (r->table_options.checksum) {
      case knochecksum:
        // we don't support no checksum yet
        assert(false);
        // intentional fallthrough in release binary
      case kcrc32c: {
        auto crc = crc32c::value(block_contents.data(), block_contents.size());
        crc = crc32c::extend(crc, trailer, 1);  // extend to cover block type
        encodefixed32(trailer_without_type, crc32c::mask(crc));
        break;
      }
      case kxxhash: {
        void* xxh = xxh32_init(0);
        xxh32_update(xxh, block_contents.data(), block_contents.size());
        xxh32_update(xxh, trailer, 1);  // extend  to cover block type
        encodefixed32(trailer_without_type, xxh32_digest(xxh));
        break;
      }
    }

    r->status = r->file->append(slice(trailer, kblocktrailersize));
    if (r->status.ok()) {
      r->status = insertblockincache(block_contents, type, handle);
    }
    if (r->status.ok()) {
      r->offset += block_contents.size() + kblocktrailersize;
    }
  }
}

status blockbasedtablebuilder::status() const {
  return rep_->status;
}

static void deletecachedblock(const slice& key, void* value) {
  block* block = reinterpret_cast<block*>(value);
  delete block;
}

//
// make a copy of the block contents and insert into compressed block cache
//
status blockbasedtablebuilder::insertblockincache(const slice& block_contents,
                                                  const compressiontype type,
                                                  const blockhandle* handle) {
  rep* r = rep_;
  cache* block_cache_compressed = r->table_options.block_cache_compressed.get();

  if (type != knocompression && block_cache_compressed != nullptr) {

    cache::handle* cache_handle = nullptr;
    size_t size = block_contents.size();

    char* ubuf = new char[size + 1];  // make a new copy
    memcpy(ubuf, block_contents.data(), size);
    ubuf[size] = type;

    blockcontents results;
    slice sl(ubuf, size);
    results.data = sl;
    results.cachable = true; // xxx
    results.heap_allocated = true;
    results.compression_type = type;

    block* block = new block(results);

    // make cache key by appending the file offset to the cache prefix id
    char* end = encodevarint64(
                  r->compressed_cache_key_prefix +
                  r->compressed_cache_key_prefix_size,
                  handle->offset());
    slice key(r->compressed_cache_key_prefix, static_cast<size_t>
              (end - r->compressed_cache_key_prefix));

    // insert into compressed block cache.
    cache_handle = block_cache_compressed->insert(key, block, block->size(),
                                                  &deletecachedblock);
    block_cache_compressed->release(cache_handle);

    // invalidate os cache.
    r->file->invalidatecache(r->offset, size);
  }
  return status::ok();
}

status blockbasedtablebuilder::finish() {
  rep* r = rep_;
  bool empty_data_block = r->data_block.empty();
  flush();
  assert(!r->closed);
  r->closed = true;

  blockhandle filter_block_handle,
              metaindex_block_handle,
              index_block_handle;

  // write filter block
  if (ok() && r->filter_block != nullptr) {
    auto filter_contents = r->filter_block->finish();
    r->props.filter_size = filter_contents.size();
    writerawblock(filter_contents, knocompression, &filter_block_handle);
  }

  // to make sure properties block is able to keep the accurate size of index
  // block, we will finish writing all index entries here and flush them
  // to storage after metaindex block is written.
  if (ok() && !empty_data_block) {
    r->index_builder->addindexentry(
        &r->last_key, nullptr /* no next data block */, r->pending_handle);
  }

  indexbuilder::indexblocks index_blocks;
  auto s = r->index_builder->finish(&index_blocks);
  if (!s.ok()) {
    return s;
  }

  // write meta blocks and metaindex block with the following order.
  //    1. [meta block: filter]
  //    2. [other meta blocks]
  //    3. [meta block: properties]
  //    4. [metaindex block]
  // write meta blocks
  metaindexbuilder meta_index_builder;
  for (const auto& item : index_blocks.meta_blocks) {
    blockhandle block_handle;
    writeblock(item.second, &block_handle);
    meta_index_builder.add(item.first, block_handle);
  }

  if (ok()) {
    if (r->filter_block != nullptr) {
      // add mapping from "<filter_block_prefix>.name" to location
      // of filter data.
      std::string key = blockbasedtable::kfilterblockprefix;
      key.append(r->table_options.filter_policy->name());
      meta_index_builder.add(key, filter_block_handle);
    }

    // write properties block.
    {
      propertyblockbuilder property_block_builder;
      std::vector<std::string> failed_user_prop_collectors;
      r->props.filter_policy_name = r->table_options.filter_policy != nullptr ?
          r->table_options.filter_policy->name() : "";
      r->props.index_size =
          r->index_builder->estimatedsize() + kblocktrailersize;

      // add basic properties
      property_block_builder.addtableproperty(r->props);

      // add use collected properties
      notifycollecttablecollectorsonfinish(r->table_properties_collectors,
                                           r->options.info_log.get(),
                                           &property_block_builder);

      blockhandle properties_block_handle;
      writerawblock(
          property_block_builder.finish(),
          knocompression,
          &properties_block_handle
      );

      meta_index_builder.add(kpropertiesblock, properties_block_handle);
    }  // end of properties block writing
  }    // meta blocks

  // write index block
  if (ok()) {
    // flush the meta index block
    writerawblock(meta_index_builder.finish(), knocompression,
                  &metaindex_block_handle);
    writeblock(index_blocks.index_block_contents, &index_block_handle);
  }

  // write footer
  if (ok()) {
    // no need to write out new footer if we're using default checksum.
    // we're writing legacy magic number because we want old versions of rocksdb
    // be able to read files generated with new release (just in case if
    // somebody wants to roll back after an upgrade)
    // todo(icanadi) at some point in the future, when we're absolutely sure
    // nobody will roll back to rocksdb 2.x versions, retire the legacy magic
    // number and always write new table files with new magic number
    bool legacy = (r->table_options.checksum == kcrc32c);
    footer footer(legacy ? klegacyblockbasedtablemagicnumber
                         : kblockbasedtablemagicnumber);
    footer.set_metaindex_handle(metaindex_block_handle);
    footer.set_index_handle(index_block_handle);
    footer.set_checksum(r->table_options.checksum);
    std::string footer_encoding;
    footer.encodeto(&footer_encoding);
    r->status = r->file->append(footer_encoding);
    if (r->status.ok()) {
      r->offset += footer_encoding.size();
    }
  }

  // print out the table stats
  if (ok()) {
    // user collected properties
    std::string user_collected;
    user_collected.reserve(1024);
    for (const auto& collector : r->table_properties_collectors) {
      for (const auto& prop : collector->getreadableproperties()) {
        user_collected.append(prop.first);
        user_collected.append("=");
        user_collected.append(prop.second);
        user_collected.append("; ");
      }
    }

    log(
        r->options.info_log,
        "table was constructed:\n"
        "  [basic properties]: %s\n"
        "  [user collected properties]: %s",
        r->props.tostring().c_str(),
        user_collected.c_str()
    );
  }

  return r->status;
}

void blockbasedtablebuilder::abandon() {
  rep* r = rep_;
  assert(!r->closed);
  r->closed = true;
}

uint64_t blockbasedtablebuilder::numentries() const {
  return rep_->props.num_entries;
}

uint64_t blockbasedtablebuilder::filesize() const {
  return rep_->offset;
}

const std::string blockbasedtable::kfilterblockprefix = "filter.";

}  // namespace rocksdb
