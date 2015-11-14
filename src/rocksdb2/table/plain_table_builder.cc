//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#ifndef rocksdb_lite
#include "table/plain_table_builder.h"

#include <string>
#include <assert.h>
#include <map>

#include "rocksdb/comparator.h"
#include "rocksdb/env.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/options.h"
#include "rocksdb/table.h"
#include "table/plain_table_factory.h"
#include "db/dbformat.h"
#include "table/block_builder.h"
#include "table/bloom_block.h"
#include "table/plain_table_index.h"
#include "table/filter_block.h"
#include "table/format.h"
#include "table/meta_blocks.h"
#include "util/coding.h"
#include "util/crc32c.h"
#include "util/stop_watch.h"

namespace rocksdb {

namespace {

// a utility that helps writing block content to the file
//   @offset will advance if @block_contents was successfully written.
//   @block_handle the block handle this particular block.
status writeblock(
    const slice& block_contents,
    writablefile* file,
    uint64_t* offset,
    blockhandle* block_handle) {
  block_handle->set_offset(*offset);
  block_handle->set_size(block_contents.size());
  status s = file->append(block_contents);

  if (s.ok()) {
    *offset += block_contents.size();
  }
  return s;
}

}  // namespace

// kplaintablemagicnumber was picked by running
//    echo rocksdb.table.plain | sha1sum
// and taking the leading 64 bits.
extern const uint64_t kplaintablemagicnumber = 0x8242229663bf9564ull;
extern const uint64_t klegacyplaintablemagicnumber = 0x4f3418eb7a8f13b8ull;

plaintablebuilder::plaintablebuilder(
    const options& options, writablefile* file, uint32_t user_key_len,
    encodingtype encoding_type, size_t index_sparseness,
    uint32_t bloom_bits_per_key, uint32_t num_probes, size_t huge_page_tlb_size,
    double hash_table_ratio, bool store_index_in_file)
    : options_(options),
      bloom_block_(num_probes),
      file_(file),
      bloom_bits_per_key_(bloom_bits_per_key),
      huge_page_tlb_size_(huge_page_tlb_size),
      encoder_(encoding_type, user_key_len, options.prefix_extractor.get(),
               index_sparseness),
      store_index_in_file_(store_index_in_file),
      prefix_extractor_(options.prefix_extractor.get()) {
  // build index block and save it in the file if hash_table_ratio > 0
  if (store_index_in_file_) {
    assert(hash_table_ratio > 0 || istotalordermode());
    index_builder_.reset(
        new plaintableindexbuilder(&arena_, options, index_sparseness,
                                   hash_table_ratio, huge_page_tlb_size_));
    assert(bloom_bits_per_key_ > 0);
    properties_.user_collected_properties
        [plaintablepropertynames::kbloomversion] = "1";  // for future use
  }

  properties_.fixed_key_len = user_key_len;

  // for plain table, we put all the data in a big chuck.
  properties_.num_data_blocks = 1;
  // fill it later if store_index_in_file_ == true
  properties_.index_size = 0;
  properties_.filter_size = 0;
  // to support roll-back to previous version, now still use version 0 for
  // plain encoding.
  properties_.format_version = (encoding_type == kplain) ? 0 : 1;

  if (options_.prefix_extractor) {
    properties_.user_collected_properties
        [plaintablepropertynames::kprefixextractorname] =
        options_.prefix_extractor->name();
  }

  std::string val;
  putfixed32(&val, static_cast<uint32_t>(encoder_.getencodingtype()));
  properties_.user_collected_properties
      [plaintablepropertynames::kencodingtype] = val;

  for (auto& collector_factories :
       options.table_properties_collector_factories) {
    table_properties_collectors_.emplace_back(
        collector_factories->createtablepropertiescollector());
  }
}

plaintablebuilder::~plaintablebuilder() {
}

void plaintablebuilder::add(const slice& key, const slice& value) {
  // temp buffer for metadata bytes between key and value.
  char meta_bytes_buf[6];
  size_t meta_bytes_buf_size = 0;

  parsedinternalkey internal_key;
  parseinternalkey(key, &internal_key);

  // store key hash
  if (store_index_in_file_) {
    if (options_.prefix_extractor.get() == nullptr) {
      keys_or_prefixes_hashes_.push_back(getslicehash(internal_key.user_key));
    } else {
      slice prefix =
          options_.prefix_extractor->transform(internal_key.user_key);
      keys_or_prefixes_hashes_.push_back(getslicehash(prefix));
    }
  }

  // write value
  auto prev_offset = offset_;
  // write out the key
  encoder_.appendkey(key, file_, &offset_, meta_bytes_buf,
                     &meta_bytes_buf_size);
  if (saveindexinfile()) {
    index_builder_->addkeyprefix(getprefix(internal_key), prev_offset);
  }

  // write value length
  int value_size = value.size();
  char* end_ptr =
      encodevarint32(meta_bytes_buf + meta_bytes_buf_size, value_size);
  assert(end_ptr <= meta_bytes_buf + sizeof(meta_bytes_buf));
  meta_bytes_buf_size = end_ptr - meta_bytes_buf;
  file_->append(slice(meta_bytes_buf, meta_bytes_buf_size));

  // write value
  file_->append(value);
  offset_ += value_size + meta_bytes_buf_size;

  properties_.num_entries++;
  properties_.raw_key_size += key.size();
  properties_.raw_value_size += value.size();

  // notify property collectors
  notifycollecttablecollectorsonadd(key, value, table_properties_collectors_,
                                    options_.info_log.get());
}

status plaintablebuilder::status() const { return status_; }

status plaintablebuilder::finish() {
  assert(!closed_);
  closed_ = true;

  properties_.data_size = offset_;

  //  write the following blocks
  //  1. [meta block: bloom] - optional
  //  2. [meta block: index] - optional
  //  3. [meta block: properties]
  //  4. [metaindex block]
  //  5. [footer]

  metaindexbuilder meta_index_builer;

  if (store_index_in_file_ && (properties_.num_entries > 0)) {
    bloom_block_.settotalbits(
        &arena_, properties_.num_entries * bloom_bits_per_key_,
        options_.bloom_locality, huge_page_tlb_size_, options_.info_log.get());

    putvarint32(&properties_.user_collected_properties
                     [plaintablepropertynames::knumbloomblocks],
                bloom_block_.getnumblocks());

    bloom_block_.addkeyshashes(keys_or_prefixes_hashes_);
    blockhandle bloom_block_handle;
    auto finish_result = bloom_block_.finish();

    properties_.filter_size = finish_result.size();
    auto s = writeblock(finish_result, file_, &offset_, &bloom_block_handle);

    if (!s.ok()) {
      return s;
    }

    blockhandle index_block_handle;
    finish_result = index_builder_->finish();

    properties_.index_size = finish_result.size();
    s = writeblock(finish_result, file_, &offset_, &index_block_handle);

    if (!s.ok()) {
      return s;
    }

    meta_index_builer.add(bloomblockbuilder::kbloomblock, bloom_block_handle);
    meta_index_builer.add(plaintableindexbuilder::kplaintableindexblock,
                          index_block_handle);
  }

  // calculate bloom block size and index block size
  propertyblockbuilder property_block_builder;
  // -- add basic properties
  property_block_builder.addtableproperty(properties_);

  property_block_builder.add(properties_.user_collected_properties);

  // -- add user collected properties
  notifycollecttablecollectorsonfinish(table_properties_collectors_,
                                       options_.info_log.get(),
                                       &property_block_builder);

  // -- write property block
  blockhandle property_block_handle;
  auto s = writeblock(
      property_block_builder.finish(),
      file_,
      &offset_,
      &property_block_handle
  );
  if (!s.ok()) {
    return s;
  }
  meta_index_builer.add(kpropertiesblock, property_block_handle);

  // -- write metaindex block
  blockhandle metaindex_block_handle;
  s = writeblock(
      meta_index_builer.finish(),
      file_,
      &offset_,
      &metaindex_block_handle
  );
  if (!s.ok()) {
    return s;
  }

  // write footer
  // no need to write out new footer if we're using default checksum
  footer footer(klegacyplaintablemagicnumber);
  footer.set_metaindex_handle(metaindex_block_handle);
  footer.set_index_handle(blockhandle::nullblockhandle());
  std::string footer_encoding;
  footer.encodeto(&footer_encoding);
  s = file_->append(footer_encoding);
  if (s.ok()) {
    offset_ += footer_encoding.size();
  }

  return s;
}

void plaintablebuilder::abandon() {
  closed_ = true;
}

uint64_t plaintablebuilder::numentries() const {
  return properties_.num_entries;
}

uint64_t plaintablebuilder::filesize() const {
  return offset_;
}

}  // namespace rocksdb
#endif  // rocksdb_lite
