// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once

#ifndef rocksdb_lite

#include <string>
#include "rocksdb/options.h"
#include "rocksdb/table.h"

namespace rocksdb {

struct options;
struct envoptions;

using std::unique_ptr;
class status;
class randomaccessfile;
class writablefile;
class table;
class tablebuilder;

class adaptivetablefactory : public tablefactory {
 public:
  ~adaptivetablefactory() {}

  explicit adaptivetablefactory(
      std::shared_ptr<tablefactory> table_factory_to_write,
      std::shared_ptr<tablefactory> block_based_table_factory,
      std::shared_ptr<tablefactory> plain_table_factory,
      std::shared_ptr<tablefactory> cuckoo_table_factory);
  const char* name() const override { return "adaptivetablefactory"; }
  status newtablereader(const options& options, const envoptions& soptions,
                        const internalkeycomparator& internal_comparator,
                        unique_ptr<randomaccessfile>&& file, uint64_t file_size,
                        unique_ptr<tablereader>* table) const override;
  tablebuilder* newtablebuilder(const options& options,
                                const internalkeycomparator& icomparator,
                                writablefile* file,
                                compressiontype compression_type) const
      override;

  // sanitizes the specified db options.
  status sanitizedboptions(const dboptions* db_opts) const override {
    if (db_opts->allow_mmap_reads == false) {
      return status::notsupported(
          "adaptivetable with allow_mmap_reads == false is not supported.");
    }
    return status::ok();
  }

  std::string getprintabletableoptions() const override;

 private:
  std::shared_ptr<tablefactory> table_factory_to_write_;
  std::shared_ptr<tablefactory> block_based_table_factory_;
  std::shared_ptr<tablefactory> plain_table_factory_;
  std::shared_ptr<tablefactory> cuckoo_table_factory_;
};

}  // namespace rocksdb
#endif  // rocksdb_lite
