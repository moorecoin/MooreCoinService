//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#include "rocksdb/utilities/write_batch_with_index.h"
#include "rocksdb/comparator.h"
#include "db/column_family.h"
#include "db/skiplist.h"
#include "util/arena.h"

namespace rocksdb {
namespace {
class readablewritebatch : public writebatch {
 public:
  explicit readablewritebatch(size_t reserved_bytes = 0)
      : writebatch(reserved_bytes) {}
  // retrieve some information from a write entry in the write batch, given
  // the start offset of the write entry.
  status getentryfromdataoffset(size_t data_offset, writetype* type, slice* key,
                                slice* value, slice* blob) const;
};
}  // namespace

// key used by skip list, as the binary searchable index of writebatchwithindex.
struct writebatchindexentry {
  writebatchindexentry(size_t o, uint32_t c)
      : offset(o), column_family(c), search_key(nullptr) {}
  writebatchindexentry(const slice* sk, uint32_t c)
      : offset(0), column_family(c), search_key(sk) {}

  size_t offset;           // offset of an entry in write batch's string buffer.
  uint32_t column_family;  // column family of the entry
  const slice* search_key;  // if not null, instead of reading keys from
                            // write batch, use it to compare. this is used
                            // for lookup key.
};

class writebatchentrycomparator {
 public:
  writebatchentrycomparator(const comparator* comparator,
                            const readablewritebatch* write_batch)
      : comparator_(comparator), write_batch_(write_batch) {}
  // compare a and b. return a negative value if a is less than b, 0 if they
  // are equal, and a positive value if a is greater than b
  int operator()(const writebatchindexentry* entry1,
                 const writebatchindexentry* entry2) const;

 private:
  const comparator* comparator_;
  const readablewritebatch* write_batch_;
};

typedef skiplist<writebatchindexentry*, const writebatchentrycomparator&>
    writebatchentryskiplist;

struct writebatchwithindex::rep {
  rep(const comparator* index_comparator, size_t reserved_bytes = 0)
      : write_batch(reserved_bytes),
        comparator(index_comparator, &write_batch),
        skip_list(comparator, &arena) {}
  readablewritebatch write_batch;
  writebatchentrycomparator comparator;
  arena arena;
  writebatchentryskiplist skip_list;

  writebatchindexentry* getentry(columnfamilyhandle* column_family) {
    return getentrywithcfid(getcolumnfamilyid(column_family));
  }

  writebatchindexentry* getentrywithcfid(uint32_t column_family_id) {
    auto* mem = arena.allocate(sizeof(writebatchindexentry));
    auto* index_entry = new (mem)
        writebatchindexentry(write_batch.getdatasize(), column_family_id);
    return index_entry;
  }
};

class wbwiiteratorimpl : public wbwiiterator {
 public:
  wbwiiteratorimpl(uint32_t column_family_id,
                   writebatchentryskiplist* skip_list,
                   const readablewritebatch* write_batch)
      : column_family_id_(column_family_id),
        skip_list_iter_(skip_list),
        write_batch_(write_batch),
        valid_(false) {}

  virtual ~wbwiiteratorimpl() {}

  virtual bool valid() const override { return valid_; }

  virtual void seek(const slice& key) override {
    valid_ = true;
    writebatchindexentry search_entry(&key, column_family_id_);
    skip_list_iter_.seek(&search_entry);
    readentry();
  }

  virtual void next() override {
    skip_list_iter_.next();
    readentry();
  }

  virtual const writeentry& entry() const override { return current_; }

  virtual status status() const override { return status_; }

 private:
  uint32_t column_family_id_;
  writebatchentryskiplist::iterator skip_list_iter_;
  const readablewritebatch* write_batch_;
  status status_;
  bool valid_;
  writeentry current_;

  void readentry() {
    if (!status_.ok() || !skip_list_iter_.valid()) {
      valid_ = false;
      return;
    }
    const writebatchindexentry* iter_entry = skip_list_iter_.key();
    if (iter_entry == nullptr ||
        iter_entry->column_family != column_family_id_) {
      valid_ = false;
      return;
    }
    slice blob;
    status_ = write_batch_->getentryfromdataoffset(
        iter_entry->offset, &current_.type, &current_.key, &current_.value,
        &blob);
    if (!status_.ok()) {
      valid_ = false;
    } else if (current_.type != kputrecord && current_.type != kdeleterecord &&
               current_.type != kmergerecord) {
      valid_ = false;
      status_ = status::corruption("write batch index is corrupted");
    }
  }
};

status readablewritebatch::getentryfromdataoffset(size_t data_offset,
                                                  writetype* type, slice* key,
                                                  slice* value,
                                                  slice* blob) const {
  if (type == nullptr || key == nullptr || value == nullptr ||
      blob == nullptr) {
    return status::invalidargument("output parameters cannot be null");
  }

  if (data_offset >= getdatasize()) {
    return status::invalidargument("data offset exceed write batch size");
  }
  slice input = slice(rep_.data() + data_offset, rep_.size() - data_offset);
  char tag;
  uint32_t column_family;
  status s =
      readrecordfromwritebatch(&input, &tag, &column_family, key, value, blob);

  switch (tag) {
    case ktypecolumnfamilyvalue:
    case ktypevalue:
      *type = kputrecord;
      break;
    case ktypecolumnfamilydeletion:
    case ktypedeletion:
      *type = kdeleterecord;
      break;
    case ktypecolumnfamilymerge:
    case ktypemerge:
      *type = kmergerecord;
      break;
    case ktypelogdata:
      *type = klogdatarecord;
      break;
    default:
      return status::corruption("unknown writebatch tag");
  }
  return status::ok();
}

writebatchwithindex::writebatchwithindex(const comparator* index_comparator,
                                         size_t reserved_bytes)
    : rep(new rep(index_comparator, reserved_bytes)) {}

writebatchwithindex::~writebatchwithindex() { delete rep; }

writebatch* writebatchwithindex::getwritebatch() { return &rep->write_batch; }

wbwiiterator* writebatchwithindex::newiterator() {
  return new wbwiiteratorimpl(0, &(rep->skip_list), &rep->write_batch);
}

wbwiiterator* writebatchwithindex::newiterator(
    columnfamilyhandle* column_family) {
  return new wbwiiteratorimpl(getcolumnfamilyid(column_family),
                              &(rep->skip_list), &rep->write_batch);
}

void writebatchwithindex::put(columnfamilyhandle* column_family,
                              const slice& key, const slice& value) {
  auto* index_entry = rep->getentry(column_family);
  rep->write_batch.put(column_family, key, value);
  rep->skip_list.insert(index_entry);
}

void writebatchwithindex::put(const slice& key, const slice& value) {
  auto* index_entry = rep->getentrywithcfid(0);
  rep->write_batch.put(key, value);
  rep->skip_list.insert(index_entry);
}

void writebatchwithindex::merge(columnfamilyhandle* column_family,
                                const slice& key, const slice& value) {
  auto* index_entry = rep->getentry(column_family);
  rep->write_batch.merge(column_family, key, value);
  rep->skip_list.insert(index_entry);
}

void writebatchwithindex::merge(const slice& key, const slice& value) {
  auto* index_entry = rep->getentrywithcfid(0);
  rep->write_batch.merge(key, value);
  rep->skip_list.insert(index_entry);
}

void writebatchwithindex::putlogdata(const slice& blob) {
  rep->write_batch.putlogdata(blob);
}

void writebatchwithindex::delete(columnfamilyhandle* column_family,
                                 const slice& key) {
  auto* index_entry = rep->getentry(column_family);
  rep->write_batch.delete(column_family, key);
  rep->skip_list.insert(index_entry);
}

void writebatchwithindex::delete(const slice& key) {
  auto* index_entry = rep->getentrywithcfid(0);
  rep->write_batch.delete(key);
  rep->skip_list.insert(index_entry);
}

void writebatchwithindex::delete(columnfamilyhandle* column_family,
                                 const sliceparts& key) {
  auto* index_entry = rep->getentry(column_family);
  rep->write_batch.delete(column_family, key);
  rep->skip_list.insert(index_entry);
}

void writebatchwithindex::delete(const sliceparts& key) {
  auto* index_entry = rep->getentrywithcfid(0);
  rep->write_batch.delete(key);
  rep->skip_list.insert(index_entry);
}

int writebatchentrycomparator::operator()(
    const writebatchindexentry* entry1,
    const writebatchindexentry* entry2) const {
  if (entry1->column_family > entry2->column_family) {
    return 1;
  } else if (entry1->column_family < entry2->column_family) {
    return -1;
  }

  status s;
  slice key1, key2;
  if (entry1->search_key == nullptr) {
    slice value, blob;
    writetype write_type;
    s = write_batch_->getentryfromdataoffset(entry1->offset, &write_type, &key1,
                                             &value, &blob);
    if (!s.ok()) {
      return 1;
    }
  } else {
    key1 = *(entry1->search_key);
  }
  if (entry2->search_key == nullptr) {
    slice value, blob;
    writetype write_type;
    s = write_batch_->getentryfromdataoffset(entry2->offset, &write_type, &key2,
                                             &value, &blob);
    if (!s.ok()) {
      return -1;
    }
  } else {
    key2 = *(entry2->search_key);
  }

  int cmp = comparator_->compare(key1, key2);
  if (cmp != 0) {
    return cmp;
  } else if (entry1->offset > entry2->offset) {
    return 1;
  } else if (entry1->offset < entry2->offset) {
    return -1;
  }
  return 0;
}

}  // namespace rocksdb
