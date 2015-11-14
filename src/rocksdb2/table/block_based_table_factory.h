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
#include <string>

#include "rocksdb/flush_block_policy.h"
#include "rocksdb/options.h"
#include "rocksdb/table.h"
#include "db/dbformat.h"

namespace rocksdb {

struct options;
struct envoptions;

using std::unique_ptr;
class blockbasedtablebuilder;

class blockbasedtablefactory : public tablefactory {
 public:
  explicit blockbasedtablefactory(
      const blockbasedtableoptions& table_options = blockbasedtableoptions());

  ~blockbasedtablefactory() {}

  const char* name() const override { return "blockbasedtable"; }

  status newtablereader(const options& options, const envoptions& soptions,
                        const internalkeycomparator& internal_comparator,
                        unique_ptr<randomaccessfile>&& file, uint64_t file_size,
                        unique_ptr<tablereader>* table_reader) const override;

  tablebuilder* newtablebuilder(
      const options& options, const internalkeycomparator& internal_comparator,
      writablefile* file, compressiontype compression_type) const override;

  // sanitizes the specified db options.
  status sanitizedboptions(const dboptions* db_opts) const override {
    return status::ok();
  }

  std::string getprintabletableoptions() const override;

 private:
  blockbasedtableoptions table_options_;
};

extern const std::string khashindexprefixesblock;
extern const std::string khashindexprefixesmetadatablock;

}  // namespace rocksdb
