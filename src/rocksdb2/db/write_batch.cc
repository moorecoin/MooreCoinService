//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// writebatch::rep_ :=
//    sequence: fixed64
//    count: fixed32
//    data: record[count]
// record :=
//    ktypevalue varstring varstring
//    ktypemerge varstring varstring
//    ktypedeletion varstring
//    ktypecolumnfamilyvalue varint32 varstring varstring
//    ktypecolumnfamilymerge varint32 varstring varstring
//    ktypecolumnfamilydeletion varint32 varstring varstring
// varstring :=
//    len: varint32
//    data: uint8[len]

#include "rocksdb/write_batch.h"
#include "rocksdb/options.h"
#include "rocksdb/merge_operator.h"
#include "db/dbformat.h"
#include "db/db_impl.h"
#include "db/column_family.h"
#include "db/memtable.h"
#include "db/snapshot.h"
#include "db/write_batch_internal.h"
#include "util/coding.h"
#include "util/statistics.h"
#include <stdexcept>

namespace rocksdb {

// writebatch header has an 8-byte sequence number followed by a 4-byte count.
static const size_t kheader = 12;

writebatch::writebatch(size_t reserved_bytes) {
  rep_.reserve((reserved_bytes > kheader) ? reserved_bytes : kheader);
  clear();
}

writebatch::~writebatch() { }

writebatch::handler::~handler() { }

void writebatch::handler::put(const slice& key, const slice& value) {
  // you need to either implement put or putcf
  throw std::runtime_error("handler::put not implemented!");
}

void writebatch::handler::merge(const slice& key, const slice& value) {
  throw std::runtime_error("handler::merge not implemented!");
}

void writebatch::handler::delete(const slice& key) {
  // you need to either implement delete or deletecf
  throw std::runtime_error("handler::delete not implemented!");
}

void writebatch::handler::logdata(const slice& blob) {
  // if the user has not specified something to do with blobs, then we ignore
  // them.
}

bool writebatch::handler::continue() {
  return true;
}

void writebatch::clear() {
  rep_.clear();
  rep_.resize(kheader);
}

int writebatch::count() const {
  return writebatchinternal::count(this);
}

status readrecordfromwritebatch(slice* input, char* tag,
                                uint32_t* column_family, slice* key,
                                slice* value, slice* blob) {
  assert(key != nullptr && value != nullptr);
  *tag = (*input)[0];
  input->remove_prefix(1);
  *column_family = 0;  // default
  switch (*tag) {
    case ktypecolumnfamilyvalue:
      if (!getvarint32(input, column_family)) {
        return status::corruption("bad writebatch put");
      }
    // intentional fallthrough
    case ktypevalue:
      if (!getlengthprefixedslice(input, key) ||
          !getlengthprefixedslice(input, value)) {
        return status::corruption("bad writebatch put");
      }
      break;
    case ktypecolumnfamilydeletion:
      if (!getvarint32(input, column_family)) {
        return status::corruption("bad writebatch delete");
      }
    // intentional fallthrough
    case ktypedeletion:
      if (!getlengthprefixedslice(input, key)) {
        return status::corruption("bad writebatch delete");
      }
      break;
    case ktypecolumnfamilymerge:
      if (!getvarint32(input, column_family)) {
        return status::corruption("bad writebatch merge");
      }
    // intentional fallthrough
    case ktypemerge:
      if (!getlengthprefixedslice(input, key) ||
          !getlengthprefixedslice(input, value)) {
        return status::corruption("bad writebatch merge");
      }
      break;
    case ktypelogdata:
      assert(blob != nullptr);
      if (!getlengthprefixedslice(input, blob)) {
        return status::corruption("bad writebatch blob");
      }
      break;
    default:
      return status::corruption("unknown writebatch tag");
  }
  return status::ok();
}

status writebatch::iterate(handler* handler) const {
  slice input(rep_);
  if (input.size() < kheader) {
    return status::corruption("malformed writebatch (too small)");
  }

  input.remove_prefix(kheader);
  slice key, value, blob;
  int found = 0;
  status s;
  while (s.ok() && !input.empty() && handler->continue()) {
    char tag = 0;
    uint32_t column_family = 0;  // default

    s = readrecordfromwritebatch(&input, &tag, &column_family, &key, &value,
                                 &blob);
    if (!s.ok()) {
      return s;
    }

    switch (tag) {
      case ktypecolumnfamilyvalue:
      case ktypevalue:
        s = handler->putcf(column_family, key, value);
        found++;
        break;
      case ktypecolumnfamilydeletion:
      case ktypedeletion:
        s = handler->deletecf(column_family, key);
        found++;
        break;
      case ktypecolumnfamilymerge:
      case ktypemerge:
        s = handler->mergecf(column_family, key, value);
        found++;
        break;
      case ktypelogdata:
        handler->logdata(blob);
        break;
      default:
        return status::corruption("unknown writebatch tag");
    }
  }
  if (!s.ok()) {
    return s;
  }
  if (found != writebatchinternal::count(this)) {
    return status::corruption("writebatch has wrong count");
  } else {
    return status::ok();
  }
}

int writebatchinternal::count(const writebatch* b) {
  return decodefixed32(b->rep_.data() + 8);
}

void writebatchinternal::setcount(writebatch* b, int n) {
  encodefixed32(&b->rep_[8], n);
}

sequencenumber writebatchinternal::sequence(const writebatch* b) {
  return sequencenumber(decodefixed64(b->rep_.data()));
}

void writebatchinternal::setsequence(writebatch* b, sequencenumber seq) {
  encodefixed64(&b->rep_[0], seq);
}

void writebatchinternal::put(writebatch* b, uint32_t column_family_id,
                             const slice& key, const slice& value) {
  writebatchinternal::setcount(b, writebatchinternal::count(b) + 1);
  if (column_family_id == 0) {
    b->rep_.push_back(static_cast<char>(ktypevalue));
  } else {
    b->rep_.push_back(static_cast<char>(ktypecolumnfamilyvalue));
    putvarint32(&b->rep_, column_family_id);
  }
  putlengthprefixedslice(&b->rep_, key);
  putlengthprefixedslice(&b->rep_, value);
}

void writebatch::put(columnfamilyhandle* column_family, const slice& key,
                     const slice& value) {
  writebatchinternal::put(this, getcolumnfamilyid(column_family), key, value);
}

void writebatchinternal::put(writebatch* b, uint32_t column_family_id,
                             const sliceparts& key, const sliceparts& value) {
  writebatchinternal::setcount(b, writebatchinternal::count(b) + 1);
  if (column_family_id == 0) {
    b->rep_.push_back(static_cast<char>(ktypevalue));
  } else {
    b->rep_.push_back(static_cast<char>(ktypecolumnfamilyvalue));
    putvarint32(&b->rep_, column_family_id);
  }
  putlengthprefixedsliceparts(&b->rep_, key);
  putlengthprefixedsliceparts(&b->rep_, value);
}

void writebatch::put(columnfamilyhandle* column_family, const sliceparts& key,
                     const sliceparts& value) {
  writebatchinternal::put(this, getcolumnfamilyid(column_family), key, value);
}

void writebatchinternal::delete(writebatch* b, uint32_t column_family_id,
                                const slice& key) {
  writebatchinternal::setcount(b, writebatchinternal::count(b) + 1);
  if (column_family_id == 0) {
    b->rep_.push_back(static_cast<char>(ktypedeletion));
  } else {
    b->rep_.push_back(static_cast<char>(ktypecolumnfamilydeletion));
    putvarint32(&b->rep_, column_family_id);
  }
  putlengthprefixedslice(&b->rep_, key);
}

void writebatch::delete(columnfamilyhandle* column_family, const slice& key) {
  writebatchinternal::delete(this, getcolumnfamilyid(column_family), key);
}

void writebatchinternal::delete(writebatch* b, uint32_t column_family_id,
                                const sliceparts& key) {
  writebatchinternal::setcount(b, writebatchinternal::count(b) + 1);
  if (column_family_id == 0) {
    b->rep_.push_back(static_cast<char>(ktypedeletion));
  } else {
    b->rep_.push_back(static_cast<char>(ktypecolumnfamilydeletion));
    putvarint32(&b->rep_, column_family_id);
  }
  putlengthprefixedsliceparts(&b->rep_, key);
}

void writebatch::delete(columnfamilyhandle* column_family,
                        const sliceparts& key) {
  writebatchinternal::delete(this, getcolumnfamilyid(column_family), key);
}

void writebatchinternal::merge(writebatch* b, uint32_t column_family_id,
                               const slice& key, const slice& value) {
  writebatchinternal::setcount(b, writebatchinternal::count(b) + 1);
  if (column_family_id == 0) {
    b->rep_.push_back(static_cast<char>(ktypemerge));
  } else {
    b->rep_.push_back(static_cast<char>(ktypecolumnfamilymerge));
    putvarint32(&b->rep_, column_family_id);
  }
  putlengthprefixedslice(&b->rep_, key);
  putlengthprefixedslice(&b->rep_, value);
}

void writebatch::merge(columnfamilyhandle* column_family, const slice& key,
                       const slice& value) {
  writebatchinternal::merge(this, getcolumnfamilyid(column_family), key, value);
}

void writebatch::putlogdata(const slice& blob) {
  rep_.push_back(static_cast<char>(ktypelogdata));
  putlengthprefixedslice(&rep_, blob);
}

namespace {
class memtableinserter : public writebatch::handler {
 public:
  sequencenumber sequence_;
  columnfamilymemtables* cf_mems_;
  bool ignore_missing_column_families_;
  uint64_t log_number_;
  dbimpl* db_;
  const bool dont_filter_deletes_;

  memtableinserter(sequencenumber sequence, columnfamilymemtables* cf_mems,
                   bool ignore_missing_column_families, uint64_t log_number,
                   db* db, const bool dont_filter_deletes)
      : sequence_(sequence),
        cf_mems_(cf_mems),
        ignore_missing_column_families_(ignore_missing_column_families),
        log_number_(log_number),
        db_(reinterpret_cast<dbimpl*>(db)),
        dont_filter_deletes_(dont_filter_deletes) {
    assert(cf_mems);
    if (!dont_filter_deletes_) {
      assert(db_);
    }
  }

  bool seektocolumnfamily(uint32_t column_family_id, status* s) {
    bool found = cf_mems_->seek(column_family_id);
    if (!found) {
      if (ignore_missing_column_families_) {
        *s = status::ok();
      } else {
        *s = status::invalidargument(
            "invalid column family specified in write batch");
      }
      return false;
    }
    if (log_number_ != 0 && log_number_ < cf_mems_->getlognumber()) {
      // this is true only in recovery environment (log_number_ is always 0 in
      // non-recovery, regular write code-path)
      // * if log_number_ < cf_mems_->getlognumber(), this means that column
      // family already contains updates from this log. we can't apply updates
      // twice because of update-in-place or merge workloads -- ignore the
      // update
      *s = status::ok();
      return false;
    }
    return true;
  }
  virtual status putcf(uint32_t column_family_id, const slice& key,
                       const slice& value) {
    status seek_status;
    if (!seektocolumnfamily(column_family_id, &seek_status)) {
      ++sequence_;
      return seek_status;
    }
    memtable* mem = cf_mems_->getmemtable();
    const options* options = cf_mems_->getoptions();
    if (!options->inplace_update_support) {
      mem->add(sequence_, ktypevalue, key, value);
    } else if (options->inplace_callback == nullptr) {
      mem->update(sequence_, key, value);
      recordtick(options->statistics.get(), number_keys_updated);
    } else {
      if (mem->updatecallback(sequence_, key, value, *options)) {
      } else {
        // key not found in memtable. do sst get, update, add
        snapshotimpl read_from_snapshot;
        read_from_snapshot.number_ = sequence_;
        readoptions ropts;
        ropts.snapshot = &read_from_snapshot;

        std::string prev_value;
        std::string merged_value;

        auto cf_handle = cf_mems_->getcolumnfamilyhandle();
        if (cf_handle == nullptr) {
          cf_handle = db_->defaultcolumnfamily();
        }
        status s = db_->get(ropts, cf_handle, key, &prev_value);

        char* prev_buffer = const_cast<char*>(prev_value.c_str());
        uint32_t prev_size = prev_value.size();
        auto status = options->inplace_callback(s.ok() ? prev_buffer : nullptr,
                                                s.ok() ? &prev_size : nullptr,
                                                value, &merged_value);
        if (status == updatestatus::updated_inplace) {
          // prev_value is updated in-place with final value.
          mem->add(sequence_, ktypevalue, key, slice(prev_buffer, prev_size));
          recordtick(options->statistics.get(), number_keys_written);
        } else if (status == updatestatus::updated) {
          // merged_value contains the final value.
          mem->add(sequence_, ktypevalue, key, slice(merged_value));
          recordtick(options->statistics.get(), number_keys_written);
        }
      }
    }
    // since all puts are logged in trasaction logs (if enabled), always bump
    // sequence number. even if the update eventually fails and does not result
    // in memtable add/update.
    sequence_++;
    return status::ok();
  }

  virtual status mergecf(uint32_t column_family_id, const slice& key,
                         const slice& value) {
    status seek_status;
    if (!seektocolumnfamily(column_family_id, &seek_status)) {
      ++sequence_;
      return seek_status;
    }
    memtable* mem = cf_mems_->getmemtable();
    const options* options = cf_mems_->getoptions();
    bool perform_merge = false;

    if (options->max_successive_merges > 0 && db_ != nullptr) {
      lookupkey lkey(key, sequence_);

      // count the number of successive merges at the head
      // of the key in the memtable
      size_t num_merges = mem->countsuccessivemergeentries(lkey);

      if (num_merges >= options->max_successive_merges) {
        perform_merge = true;
      }
    }

    if (perform_merge) {
      // 1) get the existing value
      std::string get_value;

      // pass in the sequence number so that we also include previous merge
      // operations in the same batch.
      snapshotimpl read_from_snapshot;
      read_from_snapshot.number_ = sequence_;
      readoptions read_options;
      read_options.snapshot = &read_from_snapshot;

      auto cf_handle = cf_mems_->getcolumnfamilyhandle();
      if (cf_handle == nullptr) {
        cf_handle = db_->defaultcolumnfamily();
      }
      db_->get(read_options, cf_handle, key, &get_value);
      slice get_value_slice = slice(get_value);

      // 2) apply this merge
      auto merge_operator = options->merge_operator.get();
      assert(merge_operator);

      std::deque<std::string> operands;
      operands.push_front(value.tostring());
      std::string new_value;
      if (!merge_operator->fullmerge(key, &get_value_slice, operands,
                                     &new_value, options->info_log.get())) {
          // failed to merge!
        recordtick(options->statistics.get(), number_merge_failures);

        // store the delta in memtable
        perform_merge = false;
      } else {
        // 3) add value to memtable
        mem->add(sequence_, ktypevalue, key, new_value);
      }
    }

    if (!perform_merge) {
      // add merge operator to memtable
      mem->add(sequence_, ktypemerge, key, value);
    }

    sequence_++;
    return status::ok();
  }

  virtual status deletecf(uint32_t column_family_id, const slice& key) {
    status seek_status;
    if (!seektocolumnfamily(column_family_id, &seek_status)) {
      ++sequence_;
      return seek_status;
    }
    memtable* mem = cf_mems_->getmemtable();
    const options* options = cf_mems_->getoptions();
    if (!dont_filter_deletes_ && options->filter_deletes) {
      snapshotimpl read_from_snapshot;
      read_from_snapshot.number_ = sequence_;
      readoptions ropts;
      ropts.snapshot = &read_from_snapshot;
      std::string value;
      auto cf_handle = cf_mems_->getcolumnfamilyhandle();
      if (cf_handle == nullptr) {
        cf_handle = db_->defaultcolumnfamily();
      }
      if (!db_->keymayexist(ropts, cf_handle, key, &value)) {
        recordtick(options->statistics.get(), number_filtered_deletes);
        return status::ok();
      }
    }
    mem->add(sequence_, ktypedeletion, key, slice());
    sequence_++;
    return status::ok();
  }
};
}  // namespace

status writebatchinternal::insertinto(const writebatch* b,
                                      columnfamilymemtables* memtables,
                                      bool ignore_missing_column_families,
                                      uint64_t log_number, db* db,
                                      const bool dont_filter_deletes) {
  memtableinserter inserter(writebatchinternal::sequence(b), memtables,
                            ignore_missing_column_families, log_number, db,
                            dont_filter_deletes);
  return b->iterate(&inserter);
}

void writebatchinternal::setcontents(writebatch* b, const slice& contents) {
  assert(contents.size() >= kheader);
  b->rep_.assign(contents.data(), contents.size());
}

void writebatchinternal::append(writebatch* dst, const writebatch* src) {
  setcount(dst, count(dst) + count(src));
  assert(src->rep_.size() >= kheader);
  dst->rep_.append(src->rep_.data() + kheader, src->rep_.size() - kheader);
}

}  // namespace rocksdb
