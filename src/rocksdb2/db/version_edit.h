//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include <set>
#include <utility>
#include <vector>
#include <string>
#include "rocksdb/cache.h"
#include "db/dbformat.h"
#include "util/arena.h"
#include "util/autovector.h"

namespace rocksdb {

class versionset;

const uint64_t kfilenumbermask = 0x3fffffffffffffff;

extern uint64_t packfilenumberandpathid(uint64_t number, uint64_t path_id);

// a copyable structure contains information needed to read data from an sst
// file. it can contains a pointer to a table reader opened for the file, or
// file number and size, which can be used to create a new table reader for it.
// the behavior is undefined when a copied of the structure is used when the
// file is not in any live version any more.
struct filedescriptor {
  // table reader in table_reader_handle
  tablereader* table_reader;
  uint64_t packed_number_and_path_id;
  uint64_t file_size;  // file size in bytes

  filedescriptor() : filedescriptor(0, 0, 0) {}

  filedescriptor(uint64_t number, uint32_t path_id, uint64_t file_size)
      : table_reader(nullptr),
        packed_number_and_path_id(packfilenumberandpathid(number, path_id)),
        file_size(file_size) {}

  filedescriptor& operator=(const filedescriptor& fd) {
    table_reader = fd.table_reader;
    packed_number_and_path_id = fd.packed_number_and_path_id;
    file_size = fd.file_size;
    return *this;
  }

  uint64_t getnumber() const {
    return packed_number_and_path_id & kfilenumbermask;
  }
  uint32_t getpathid() const {
    return packed_number_and_path_id / (kfilenumbermask + 1);
  }
  uint64_t getfilesize() const { return file_size; }
};

struct filemetadata {
  int refs;
  filedescriptor fd;
  internalkey smallest;            // smallest internal key served by table
  internalkey largest;             // largest internal key served by table
  bool being_compacted;            // is this file undergoing compaction?
  sequencenumber smallest_seqno;   // the smallest seqno in this file
  sequencenumber largest_seqno;    // the largest seqno in this file

  // needs to be disposed when refs becomes 0.
  cache::handle* table_reader_handle;

  // stats for compensating deletion entries during compaction

  // file size compensated by deletion entry.
  // this is updated in version::updatetemporarystats() first time when the
  // file is created or loaded.  after it is updated, it is immutable.
  uint64_t compensated_file_size;
  uint64_t num_entries;            // the number of entries.
  uint64_t num_deletions;          // the number of deletion entries.
  uint64_t raw_key_size;           // total uncompressed key size.
  uint64_t raw_value_size;         // total uncompressed value size.
  bool init_stats_from_file;   // true if the data-entry stats of this file
                               // has initialized from file.

  filemetadata()
      : refs(0),
        being_compacted(false),
        table_reader_handle(nullptr),
        compensated_file_size(0),
        num_entries(0),
        num_deletions(0),
        raw_key_size(0),
        raw_value_size(0),
        init_stats_from_file(false) {}
};

// a compressed copy of file meta data that just contain
// smallest and largest key's slice
struct fdwithkeyrange {
  filedescriptor fd;
  slice smallest_key;    // slice that contain smallest key
  slice largest_key;     // slice that contain largest key

  fdwithkeyrange()
      : fd(),
        smallest_key(),
        largest_key() {
  }

  fdwithkeyrange(filedescriptor fd,
      slice smallest_key, slice largest_key)
      : fd(fd),
        smallest_key(smallest_key),
        largest_key(largest_key) {
  }
};

// data structure to store an array of fdwithkeyrange in one level
// actual data is guaranteed to be stored closely
struct filelevel {
  size_t num_files;
  fdwithkeyrange* files;
  filelevel() {
    num_files = 0;
    files = nullptr;
  }
};

class versionedit {
 public:
  versionedit() { clear(); }
  ~versionedit() { }

  void clear();

  void setcomparatorname(const slice& name) {
    has_comparator_ = true;
    comparator_ = name.tostring();
  }
  void setlognumber(uint64_t num) {
    has_log_number_ = true;
    log_number_ = num;
  }
  void setprevlognumber(uint64_t num) {
    has_prev_log_number_ = true;
    prev_log_number_ = num;
  }
  void setnextfile(uint64_t num) {
    has_next_file_number_ = true;
    next_file_number_ = num;
  }
  void setlastsequence(sequencenumber seq) {
    has_last_sequence_ = true;
    last_sequence_ = seq;
  }
  void setmaxcolumnfamily(uint32_t max_column_family) {
    has_max_column_family_ = true;
    max_column_family_ = max_column_family;
  }

  // add the specified file at the specified number.
  // requires: this version has not been saved (see versionset::saveto)
  // requires: "smallest" and "largest" are smallest and largest keys in file
  void addfile(int level, uint64_t file, uint64_t file_size,
               uint64_t file_path_id, const internalkey& smallest,
               const internalkey& largest, const sequencenumber& smallest_seqno,
               const sequencenumber& largest_seqno) {
    assert(smallest_seqno <= largest_seqno);
    filemetadata f;
    f.fd = filedescriptor(file, file_size, file_path_id);
    f.smallest = smallest;
    f.largest = largest;
    f.smallest_seqno = smallest_seqno;
    f.largest_seqno = largest_seqno;
    new_files_.push_back(std::make_pair(level, f));
  }

  // delete the specified "file" from the specified "level".
  void deletefile(int level, uint64_t file) {
    deleted_files_.insert({level, file});
  }

  // number of edits
  int numentries() {
    return new_files_.size() + deleted_files_.size();
  }

  bool iscolumnfamilymanipulation() {
    return is_column_family_add_ || is_column_family_drop_;
  }

  void setcolumnfamily(uint32_t column_family_id) {
    column_family_ = column_family_id;
  }

  // set column family id by calling setcolumnfamily()
  void addcolumnfamily(const std::string& name) {
    assert(!is_column_family_drop_);
    assert(!is_column_family_add_);
    assert(numentries() == 0);
    is_column_family_add_ = true;
    column_family_name_ = name;
  }

  // set column family id by calling setcolumnfamily()
  void dropcolumnfamily() {
    assert(!is_column_family_drop_);
    assert(!is_column_family_add_);
    assert(numentries() == 0);
    is_column_family_drop_ = true;
  }

  void encodeto(std::string* dst) const;
  status decodefrom(const slice& src);

  std::string debugstring(bool hex_key = false) const;

 private:
  friend class versionset;
  friend class version;

  typedef std::set< std::pair<int, uint64_t>> deletedfileset;

  bool getlevel(slice* input, int* level, const char** msg);

  int max_level_;
  std::string comparator_;
  uint64_t log_number_;
  uint64_t prev_log_number_;
  uint64_t next_file_number_;
  uint32_t max_column_family_;
  sequencenumber last_sequence_;
  bool has_comparator_;
  bool has_log_number_;
  bool has_prev_log_number_;
  bool has_next_file_number_;
  bool has_last_sequence_;
  bool has_max_column_family_;

  deletedfileset deleted_files_;
  std::vector<std::pair<int, filemetadata>> new_files_;

  // each version edit record should have column_family_id set
  // if it's not set, it is default (0)
  uint32_t column_family_;
  // a version edit can be either column_family add or
  // column_family drop. if it's column family add,
  // it also includes column family name.
  bool is_column_family_drop_;
  bool is_column_family_add_;
  std::string column_family_name_;
};

}  // namespace rocksdb
