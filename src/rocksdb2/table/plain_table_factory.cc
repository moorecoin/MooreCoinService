// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef rocksdb_lite
#include "table/plain_table_factory.h"

#include <memory>
#include <stdint.h>
#include "db/dbformat.h"
#include "table/plain_table_builder.h"
#include "table/plain_table_reader.h"
#include "port/port.h"

namespace rocksdb {

status plaintablefactory::newtablereader(const options& options,
                                         const envoptions& soptions,
                                         const internalkeycomparator& icomp,
                                         unique_ptr<randomaccessfile>&& file,
                                         uint64_t file_size,
                                         unique_ptr<tablereader>* table) const {
  return plaintablereader::open(options, soptions, icomp, std::move(file),
                                file_size, table, bloom_bits_per_key_,
                                hash_table_ratio_, index_sparseness_,
                                huge_page_tlb_size_, full_scan_mode_);
}

tablebuilder* plaintablefactory::newtablebuilder(
    const options& options, const internalkeycomparator& internal_comparator,
    writablefile* file, compressiontype compression_type) const {
  return new plaintablebuilder(options, file, user_key_len_, encoding_type_,
                               index_sparseness_, bloom_bits_per_key_, 6,
                               huge_page_tlb_size_, hash_table_ratio_,
                               store_index_in_file_);
}

std::string plaintablefactory::getprintabletableoptions() const {
  std::string ret;
  ret.reserve(20000);
  const int kbuffersize = 200;
  char buffer[kbuffersize];

  snprintf(buffer, kbuffersize, "  user_key_len: %u\n",
           user_key_len_);
  ret.append(buffer);
  snprintf(buffer, kbuffersize, "  bloom_bits_per_key: %d\n",
           bloom_bits_per_key_);
  ret.append(buffer);
  snprintf(buffer, kbuffersize, "  hash_table_ratio: %lf\n",
           hash_table_ratio_);
  ret.append(buffer);
  snprintf(buffer, kbuffersize, "  index_sparseness: %zd\n",
           index_sparseness_);
  ret.append(buffer);
  snprintf(buffer, kbuffersize, "  huge_page_tlb_size: %zd\n",
           huge_page_tlb_size_);
  ret.append(buffer);
  snprintf(buffer, kbuffersize, "  encoding_type: %d\n",
           encoding_type_);
  ret.append(buffer);
  snprintf(buffer, kbuffersize, "  full_scan_mode: %d\n",
           full_scan_mode_);
  ret.append(buffer);
  snprintf(buffer, kbuffersize, "  store_index_in_file: %d\n",
           store_index_in_file_);
  ret.append(buffer);
  return ret;
}

extern tablefactory* newplaintablefactory(const plaintableoptions& options) {
  return new plaintablefactory(options);
}

const std::string plaintablepropertynames::kprefixextractorname =
    "rocksdb.prefix.extractor.name";

const std::string plaintablepropertynames::kencodingtype =
    "rocksdb.plain.table.encoding.type";

const std::string plaintablepropertynames::kbloomversion =
    "rocksdb.plain.table.bloom.version";

const std::string plaintablepropertynames::knumbloomblocks =
    "rocksdb.plain.table.bloom.numblocks";

}  // namespace rocksdb
#endif  // rocksdb_lite
