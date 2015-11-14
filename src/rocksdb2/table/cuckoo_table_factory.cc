// copyright (c) 2014, facebook, inc. all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

#ifndef rocksdb_lite
#include "table/cuckoo_table_factory.h"

#include "db/dbformat.h"
#include "table/cuckoo_table_builder.h"
#include "table/cuckoo_table_reader.h"

namespace rocksdb {
status cuckootablefactory::newtablereader(const options& options,
    const envoptions& soptions, const internalkeycomparator& icomp,
    std::unique_ptr<randomaccessfile>&& file, uint64_t file_size,
    std::unique_ptr<tablereader>* table) const {
  std::unique_ptr<cuckootablereader> new_reader(new cuckootablereader(options,
      std::move(file), file_size, icomp.user_comparator(), nullptr));
  status s = new_reader->status();
  if (s.ok()) {
    *table = std::move(new_reader);
  }
  return s;
}

tablebuilder* cuckootablefactory::newtablebuilder(
    const options& options, const internalkeycomparator& internal_comparator,
    writablefile* file, compressiontype compression_type) const {
  return new cuckootablebuilder(file, hash_table_ratio_, 64, max_search_depth_,
      internal_comparator.user_comparator(), cuckoo_block_size_, nullptr);
}

std::string cuckootablefactory::getprintabletableoptions() const {
  std::string ret;
  ret.reserve(2000);
  const int kbuffersize = 200;
  char buffer[kbuffersize];

  snprintf(buffer, kbuffersize, "  hash_table_ratio: %lf\n",
           hash_table_ratio_);
  ret.append(buffer);
  snprintf(buffer, kbuffersize, "  max_search_depth: %u\n",
           max_search_depth_);
  ret.append(buffer);
  snprintf(buffer, kbuffersize, "  cuckoo_block_size: %u\n",
           cuckoo_block_size_);
  ret.append(buffer);
  return ret;
}

tablefactory* newcuckootablefactory(double hash_table_ratio,
    uint32_t max_search_depth, uint32_t cuckoo_block_size) {
  return new cuckootablefactory(
      hash_table_ratio, max_search_depth, cuckoo_block_size);
}

}  // namespace rocksdb
#endif  // rocksdb_lite
