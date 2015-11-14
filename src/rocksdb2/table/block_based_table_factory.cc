//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.


#include "table/block_based_table_factory.h"

#include <memory>
#include <string>
#include <stdint.h>

#include "rocksdb/flush_block_policy.h"
#include "rocksdb/cache.h"
#include "table/block_based_table_builder.h"
#include "table/block_based_table_reader.h"
#include "port/port.h"

namespace rocksdb {

blockbasedtablefactory::blockbasedtablefactory(
    const blockbasedtableoptions& table_options)
    : table_options_(table_options) {
  if (table_options_.flush_block_policy_factory == nullptr) {
    table_options_.flush_block_policy_factory.reset(
        new flushblockbysizepolicyfactory());
  }
  if (table_options_.no_block_cache) {
    table_options_.block_cache.reset();
  } else if (table_options_.block_cache == nullptr) {
    table_options_.block_cache = newlrucache(8 << 20);
  }
  if (table_options_.block_size_deviation < 0 ||
      table_options_.block_size_deviation > 100) {
    table_options_.block_size_deviation = 0;
  }
}

status blockbasedtablefactory::newtablereader(
    const options& options, const envoptions& soptions,
    const internalkeycomparator& internal_comparator,
    unique_ptr<randomaccessfile>&& file, uint64_t file_size,
    unique_ptr<tablereader>* table_reader) const {
  return blockbasedtable::open(options, soptions, table_options_,
                               internal_comparator, std::move(file), file_size,
                               table_reader);
}

tablebuilder* blockbasedtablefactory::newtablebuilder(
    const options& options, const internalkeycomparator& internal_comparator,
    writablefile* file, compressiontype compression_type) const {

  auto table_builder = new blockbasedtablebuilder(
      options, table_options_, internal_comparator, file, compression_type);

  return table_builder;
}

std::string blockbasedtablefactory::getprintabletableoptions() const {
  std::string ret;
  ret.reserve(20000);
  const int kbuffersize = 200;
  char buffer[kbuffersize];

  snprintf(buffer, kbuffersize, "  flush_block_policy_factory: %s (%p)\n",
           table_options_.flush_block_policy_factory->name(),
           table_options_.flush_block_policy_factory.get());
  ret.append(buffer);
  snprintf(buffer, kbuffersize, "  cache_index_and_filter_blocks: %d\n",
           table_options_.cache_index_and_filter_blocks);
  ret.append(buffer);
  snprintf(buffer, kbuffersize, "  index_type: %d\n",
           table_options_.index_type);
  ret.append(buffer);
  snprintf(buffer, kbuffersize, "  hash_index_allow_collision: %d\n",
           table_options_.hash_index_allow_collision);
  ret.append(buffer);
  snprintf(buffer, kbuffersize, "  checksum: %d\n",
           table_options_.checksum);
  ret.append(buffer);
  snprintf(buffer, kbuffersize, "  no_block_cache: %d\n",
           table_options_.no_block_cache);
  ret.append(buffer);
  snprintf(buffer, kbuffersize, "  block_cache: %p\n",
           table_options_.block_cache.get());
  ret.append(buffer);
  if (table_options_.block_cache) {
    snprintf(buffer, kbuffersize, "  block_cache_size: %zd\n",
             table_options_.block_cache->getcapacity());
    ret.append(buffer);
  }
  snprintf(buffer, kbuffersize, "  block_cache_compressed: %p\n",
           table_options_.block_cache_compressed.get());
  ret.append(buffer);
  if (table_options_.block_cache_compressed) {
    snprintf(buffer, kbuffersize, "  block_cache_compressed_size: %zd\n",
             table_options_.block_cache_compressed->getcapacity());
    ret.append(buffer);
  }
  snprintf(buffer, kbuffersize, "  block_size: %zd\n",
           table_options_.block_size);
  ret.append(buffer);
  snprintf(buffer, kbuffersize, "  block_size_deviation: %d\n",
           table_options_.block_size_deviation);
  ret.append(buffer);
  snprintf(buffer, kbuffersize, "  block_restart_interval: %d\n",
           table_options_.block_restart_interval);
  ret.append(buffer);
  snprintf(buffer, kbuffersize, "  filter_policy: %s\n",
           table_options_.filter_policy == nullptr ?
             "nullptr" : table_options_.filter_policy->name());
  ret.append(buffer);
  snprintf(buffer, kbuffersize, "  whole_key_filtering: %d\n",
           table_options_.whole_key_filtering);
  ret.append(buffer);
  return ret;
}

tablefactory* newblockbasedtablefactory(
    const blockbasedtableoptions& table_options) {
  return new blockbasedtablefactory(table_options);
}

const std::string blockbasedtablepropertynames::kindextype =
    "rocksdb.block.based.table.index.type";
const std::string khashindexprefixesblock = "rocksdb.hashindex.prefixes";
const std::string khashindexprefixesmetadatablock =
    "rocksdb.hashindex.metadata";

}  // namespace rocksdb
