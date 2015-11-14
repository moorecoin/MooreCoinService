// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef rocksdb_lite
#include "table/adaptive_table_factory.h"

#include "table/format.h"

namespace rocksdb {

adaptivetablefactory::adaptivetablefactory(
    std::shared_ptr<tablefactory> table_factory_to_write,
    std::shared_ptr<tablefactory> block_based_table_factory,
    std::shared_ptr<tablefactory> plain_table_factory,
    std::shared_ptr<tablefactory> cuckoo_table_factory)
    : table_factory_to_write_(table_factory_to_write),
      block_based_table_factory_(block_based_table_factory),
      plain_table_factory_(plain_table_factory),
      cuckoo_table_factory_(cuckoo_table_factory) {
  if (!table_factory_to_write_) {
    table_factory_to_write_ = block_based_table_factory_;
  }
  if (!plain_table_factory_) {
    plain_table_factory_.reset(newplaintablefactory());
  }
  if (!block_based_table_factory_) {
    block_based_table_factory_.reset(newblockbasedtablefactory());
  }
  if (!cuckoo_table_factory_) {
    cuckoo_table_factory_.reset(newcuckootablefactory());
  }
}

extern const uint64_t kplaintablemagicnumber;
extern const uint64_t klegacyplaintablemagicnumber;
extern const uint64_t kblockbasedtablemagicnumber;
extern const uint64_t klegacyblockbasedtablemagicnumber;
extern const uint64_t kcuckootablemagicnumber;

status adaptivetablefactory::newtablereader(
    const options& options, const envoptions& soptions,
    const internalkeycomparator& icomp, unique_ptr<randomaccessfile>&& file,
    uint64_t file_size, unique_ptr<tablereader>* table) const {
  footer footer;
  auto s = readfooterfromfile(file.get(), file_size, &footer);
  if (!s.ok()) {
    return s;
  }
  if (footer.table_magic_number() == kplaintablemagicnumber ||
      footer.table_magic_number() == klegacyplaintablemagicnumber) {
    return plain_table_factory_->newtablereader(
        options, soptions, icomp, std::move(file), file_size, table);
  } else if (footer.table_magic_number() == kblockbasedtablemagicnumber ||
      footer.table_magic_number() == klegacyblockbasedtablemagicnumber) {
    return block_based_table_factory_->newtablereader(
        options, soptions, icomp, std::move(file), file_size, table);
  } else if (footer.table_magic_number() == kcuckootablemagicnumber) {
    return cuckoo_table_factory_->newtablereader(
        options, soptions, icomp, std::move(file), file_size, table);
  } else {
    return status::notsupported("unidentified table format");
  }
}

tablebuilder* adaptivetablefactory::newtablebuilder(
    const options& options, const internalkeycomparator& internal_comparator,
    writablefile* file, compressiontype compression_type) const {
  return table_factory_to_write_->newtablebuilder(options, internal_comparator,
                                                  file, compression_type);
}

std::string adaptivetablefactory::getprintabletableoptions() const {
  std::string ret;
  ret.reserve(20000);
  const int kbuffersize = 200;
  char buffer[kbuffersize];

  if (!table_factory_to_write_) {
    snprintf(buffer, kbuffersize, "  write factory (%s) options:\n%s\n",
             table_factory_to_write_->name(),
             table_factory_to_write_->getprintabletableoptions().c_str());
    ret.append(buffer);
  }
  if (!plain_table_factory_) {
    snprintf(buffer, kbuffersize, "  %s options:\n%s\n",
             plain_table_factory_->name(),
             plain_table_factory_->getprintabletableoptions().c_str());
    ret.append(buffer);
  }
  if (!block_based_table_factory_) {
    snprintf(buffer, kbuffersize, "  %s options:\n%s\n",
             block_based_table_factory_->name(),
             block_based_table_factory_->getprintabletableoptions().c_str());
    ret.append(buffer);
  }
  if (!cuckoo_table_factory_) {
    snprintf(buffer, kbuffersize, "  %s options:\n%s\n",
             cuckoo_table_factory_->name(),
             cuckoo_table_factory_->getprintabletableoptions().c_str());
    ret.append(buffer);
  }
  return ret;
}

extern tablefactory* newadaptivetablefactory(
    std::shared_ptr<tablefactory> table_factory_to_write,
    std::shared_ptr<tablefactory> block_based_table_factory,
    std::shared_ptr<tablefactory> plain_table_factory,
    std::shared_ptr<tablefactory> cuckoo_table_factory) {
  return new adaptivetablefactory(table_factory_to_write,
      block_based_table_factory, plain_table_factory, cuckoo_table_factory);
}

}  // namespace rocksdb
#endif  // rocksdb_lite
