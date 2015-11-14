//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/version_edit.h"

#include "db/version_set.h"
#include "util/coding.h"
#include "rocksdb/slice.h"

namespace rocksdb {

// tag numbers for serialized versionedit.  these numbers are written to
// disk and should not be changed.
enum tag {
  kcomparator = 1,
  klognumber = 2,
  knextfilenumber = 3,
  klastsequence = 4,
  kcompactpointer = 5,
  kdeletedfile = 6,
  knewfile = 7,
  // 8 was used for large value refs
  kprevlognumber = 9,

  // these are new formats divergent from open source leveldb
  knewfile2 = 100,
  knewfile3 = 102,
  kcolumnfamily = 200,  // specify column family for version edit
  kcolumnfamilyadd = 201,
  kcolumnfamilydrop = 202,
  kmaxcolumnfamily = 203,
};

uint64_t packfilenumberandpathid(uint64_t number, uint64_t path_id) {
  assert(number <= kfilenumbermask);
  return number | (path_id * (kfilenumbermask + 1));
}

void versionedit::clear() {
  comparator_.clear();
  max_level_ = 0;
  log_number_ = 0;
  prev_log_number_ = 0;
  last_sequence_ = 0;
  next_file_number_ = 0;
  max_column_family_ = 0;
  has_comparator_ = false;
  has_log_number_ = false;
  has_prev_log_number_ = false;
  has_next_file_number_ = false;
  has_last_sequence_ = false;
  has_max_column_family_ = false;
  deleted_files_.clear();
  new_files_.clear();
  column_family_ = 0;
  is_column_family_add_ = 0;
  is_column_family_drop_ = 0;
  column_family_name_.clear();
}

void versionedit::encodeto(std::string* dst) const {
  if (has_comparator_) {
    putvarint32(dst, kcomparator);
    putlengthprefixedslice(dst, comparator_);
  }
  if (has_log_number_) {
    putvarint32(dst, klognumber);
    putvarint64(dst, log_number_);
  }
  if (has_prev_log_number_) {
    putvarint32(dst, kprevlognumber);
    putvarint64(dst, prev_log_number_);
  }
  if (has_next_file_number_) {
    putvarint32(dst, knextfilenumber);
    putvarint64(dst, next_file_number_);
  }
  if (has_last_sequence_) {
    putvarint32(dst, klastsequence);
    putvarint64(dst, last_sequence_);
  }
  if (has_max_column_family_) {
    putvarint32(dst, kmaxcolumnfamily);
    putvarint32(dst, max_column_family_);
  }

  for (const auto& deleted : deleted_files_) {
    putvarint32(dst, kdeletedfile);
    putvarint32(dst, deleted.first /* level */);
    putvarint64(dst, deleted.second /* file number */);
  }

  for (size_t i = 0; i < new_files_.size(); i++) {
    const filemetadata& f = new_files_[i].second;
    if (f.fd.getpathid() == 0) {
      // use older format to make sure user can roll back the build if they
      // don't config multiple db paths.
      putvarint32(dst, knewfile2);
    } else {
      putvarint32(dst, knewfile3);
    }
    putvarint32(dst, new_files_[i].first);  // level
    putvarint64(dst, f.fd.getnumber());
    if (f.fd.getpathid() != 0) {
      putvarint32(dst, f.fd.getpathid());
    }
    putvarint64(dst, f.fd.getfilesize());
    putlengthprefixedslice(dst, f.smallest.encode());
    putlengthprefixedslice(dst, f.largest.encode());
    putvarint64(dst, f.smallest_seqno);
    putvarint64(dst, f.largest_seqno);
  }

  // 0 is default and does not need to be explicitly written
  if (column_family_ != 0) {
    putvarint32(dst, kcolumnfamily);
    putvarint32(dst, column_family_);
  }

  if (is_column_family_add_) {
    putvarint32(dst, kcolumnfamilyadd);
    putlengthprefixedslice(dst, slice(column_family_name_));
  }

  if (is_column_family_drop_) {
    putvarint32(dst, kcolumnfamilydrop);
  }
}

static bool getinternalkey(slice* input, internalkey* dst) {
  slice str;
  if (getlengthprefixedslice(input, &str)) {
    dst->decodefrom(str);
    return dst->valid();
  } else {
    return false;
  }
}

bool versionedit::getlevel(slice* input, int* level, const char** msg) {
  uint32_t v;
  if (getvarint32(input, &v)) {
    *level = v;
    if (max_level_ < *level) {
      max_level_ = *level;
    }
    return true;
  } else {
    return false;
  }
}

status versionedit::decodefrom(const slice& src) {
  clear();
  slice input = src;
  const char* msg = nullptr;
  uint32_t tag;

  // temporary storage for parsing
  int level;
  uint64_t number;
  filemetadata f;
  slice str;
  internalkey key;

  while (msg == nullptr && getvarint32(&input, &tag)) {
    switch (tag) {
      case kcomparator:
        if (getlengthprefixedslice(&input, &str)) {
          comparator_ = str.tostring();
          has_comparator_ = true;
        } else {
          msg = "comparator name";
        }
        break;

      case klognumber:
        if (getvarint64(&input, &log_number_)) {
          has_log_number_ = true;
        } else {
          msg = "log number";
        }
        break;

      case kprevlognumber:
        if (getvarint64(&input, &prev_log_number_)) {
          has_prev_log_number_ = true;
        } else {
          msg = "previous log number";
        }
        break;

      case knextfilenumber:
        if (getvarint64(&input, &next_file_number_)) {
          has_next_file_number_ = true;
        } else {
          msg = "next file number";
        }
        break;

      case klastsequence:
        if (getvarint64(&input, &last_sequence_)) {
          has_last_sequence_ = true;
        } else {
          msg = "last sequence number";
        }
        break;

      case kmaxcolumnfamily:
        if (getvarint32(&input, &max_column_family_)) {
          has_max_column_family_ = true;
        } else {
          msg = "max column family";
        }
        break;

      case kcompactpointer:
        if (getlevel(&input, &level, &msg) &&
            getinternalkey(&input, &key)) {
          // we don't use compact pointers anymore,
          // but we should not fail if they are still
          // in manifest
        } else {
          if (!msg) {
            msg = "compaction pointer";
          }
        }
        break;

      case kdeletedfile:
        if (getlevel(&input, &level, &msg) &&
            getvarint64(&input, &number)) {
          deleted_files_.insert(std::make_pair(level, number));
        } else {
          if (!msg) {
            msg = "deleted file";
          }
        }
        break;

      case knewfile: {
        uint64_t number;
        uint64_t file_size;
        if (getlevel(&input, &level, &msg) && getvarint64(&input, &number) &&
            getvarint64(&input, &file_size) &&
            getinternalkey(&input, &f.smallest) &&
            getinternalkey(&input, &f.largest)) {
          f.fd = filedescriptor(number, 0, file_size);
          new_files_.push_back(std::make_pair(level, f));
        } else {
          if (!msg) {
            msg = "new-file entry";
          }
        }
        break;
      }
      case knewfile2: {
        uint64_t number;
        uint64_t file_size;
        if (getlevel(&input, &level, &msg) && getvarint64(&input, &number) &&
            getvarint64(&input, &file_size) &&
            getinternalkey(&input, &f.smallest) &&
            getinternalkey(&input, &f.largest) &&
            getvarint64(&input, &f.smallest_seqno) &&
            getvarint64(&input, &f.largest_seqno)) {
          f.fd = filedescriptor(number, 0, file_size);
          new_files_.push_back(std::make_pair(level, f));
        } else {
          if (!msg) {
            msg = "new-file2 entry";
          }
        }
        break;
      }

      case knewfile3: {
        uint64_t number;
        uint32_t path_id;
        uint64_t file_size;
        if (getlevel(&input, &level, &msg) && getvarint64(&input, &number) &&
            getvarint32(&input, &path_id) && getvarint64(&input, &file_size) &&
            getinternalkey(&input, &f.smallest) &&
            getinternalkey(&input, &f.largest) &&
            getvarint64(&input, &f.smallest_seqno) &&
            getvarint64(&input, &f.largest_seqno)) {
          f.fd = filedescriptor(number, path_id, file_size);
          new_files_.push_back(std::make_pair(level, f));
        } else {
          if (!msg) {
            msg = "new-file2 entry";
          }
        }
        break;
      }

      case kcolumnfamily:
        if (!getvarint32(&input, &column_family_)) {
          if (!msg) {
            msg = "set column family id";
          }
        }
        break;

      case kcolumnfamilyadd:
        if (getlengthprefixedslice(&input, &str)) {
          is_column_family_add_ = true;
          column_family_name_ = str.tostring();
        } else {
          if (!msg) {
            msg = "column family add";
          }
        }
        break;

      case kcolumnfamilydrop:
        is_column_family_drop_ = true;
        break;

      default:
        msg = "unknown tag";
        break;
    }
  }

  if (msg == nullptr && !input.empty()) {
    msg = "invalid tag";
  }

  status result;
  if (msg != nullptr) {
    result = status::corruption("versionedit", msg);
  }
  return result;
}

std::string versionedit::debugstring(bool hex_key) const {
  std::string r;
  r.append("versionedit {");
  if (has_comparator_) {
    r.append("\n  comparator: ");
    r.append(comparator_);
  }
  if (has_log_number_) {
    r.append("\n  lognumber: ");
    appendnumberto(&r, log_number_);
  }
  if (has_prev_log_number_) {
    r.append("\n  prevlognumber: ");
    appendnumberto(&r, prev_log_number_);
  }
  if (has_next_file_number_) {
    r.append("\n  nextfile: ");
    appendnumberto(&r, next_file_number_);
  }
  if (has_last_sequence_) {
    r.append("\n  lastseq: ");
    appendnumberto(&r, last_sequence_);
  }
  for (deletedfileset::const_iterator iter = deleted_files_.begin();
       iter != deleted_files_.end();
       ++iter) {
    r.append("\n  deletefile: ");
    appendnumberto(&r, iter->first);
    r.append(" ");
    appendnumberto(&r, iter->second);
  }
  for (size_t i = 0; i < new_files_.size(); i++) {
    const filemetadata& f = new_files_[i].second;
    r.append("\n  addfile: ");
    appendnumberto(&r, new_files_[i].first);
    r.append(" ");
    appendnumberto(&r, f.fd.getnumber());
    r.append(" ");
    appendnumberto(&r, f.fd.getfilesize());
    r.append(" ");
    r.append(f.smallest.debugstring(hex_key));
    r.append(" .. ");
    r.append(f.largest.debugstring(hex_key));
  }
  r.append("\n  columnfamily: ");
  appendnumberto(&r, column_family_);
  if (is_column_family_add_) {
    r.append("\n  columnfamilyadd: ");
    r.append(column_family_name_);
  }
  if (is_column_family_drop_) {
    r.append("\n  columnfamilydrop");
  }
  if (has_max_column_family_) {
    r.append("\n  maxcolumnfamily: ");
    appendnumberto(&r, max_column_family_);
  }
  r.append("\n}\n");
  return r;
}

}  // namespace rocksdb
