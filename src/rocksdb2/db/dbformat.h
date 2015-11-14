//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include <stdio.h>
#include <string>
#include "rocksdb/comparator.h"
#include "rocksdb/db.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/slice.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/table.h"
#include "rocksdb/types.h"
#include "util/coding.h"
#include "util/logging.h"

namespace rocksdb {

class internalkey;

// value types encoded as the last component of internal keys.
// do not change these enum values: they are embedded in the on-disk
// data structures.
// the highest bit of the value type needs to be reserved to sst tables
// for them to do more flexible encoding.
enum valuetype : unsigned char {
  ktypedeletion = 0x0,
  ktypevalue = 0x1,
  ktypemerge = 0x2,
  // following types are used only in write ahead logs. they are not used in
  // memtables or sst files:
  ktypelogdata = 0x3,
  ktypecolumnfamilydeletion = 0x4,
  ktypecolumnfamilyvalue = 0x5,
  ktypecolumnfamilymerge = 0x6,
  kmaxvalue = 0x7f
};

// kvaluetypeforseek defines the valuetype that should be passed when
// constructing a parsedinternalkey object for seeking to a particular
// sequence number (since we sort sequence numbers in decreasing order
// and the value type is embedded as the low 8 bits in the sequence
// number in internal keys, we need to use the highest-numbered
// valuetype, not the lowest).
static const valuetype kvaluetypeforseek = ktypemerge;

// we leave eight bits empty at the bottom so a type and sequence#
// can be packed together into 64-bits.
static const sequencenumber kmaxsequencenumber =
    ((0x1ull << 56) - 1);

struct parsedinternalkey {
  slice user_key;
  sequencenumber sequence;
  valuetype type;

  parsedinternalkey() { }  // intentionally left uninitialized (for speed)
  parsedinternalkey(const slice& u, const sequencenumber& seq, valuetype t)
      : user_key(u), sequence(seq), type(t) { }
  std::string debugstring(bool hex = false) const;
};

// return the length of the encoding of "key".
inline size_t internalkeyencodinglength(const parsedinternalkey& key) {
  return key.user_key.size() + 8;
}

extern uint64_t packsequenceandtype(uint64_t seq, valuetype t);

// append the serialization of "key" to *result.
extern void appendinternalkey(std::string* result,
                              const parsedinternalkey& key);

// attempt to parse an internal key from "internal_key".  on success,
// stores the parsed data in "*result", and returns true.
//
// on error, returns false, leaves "*result" in an undefined state.
extern bool parseinternalkey(const slice& internal_key,
                             parsedinternalkey* result);

// returns the user key portion of an internal key.
inline slice extractuserkey(const slice& internal_key) {
  assert(internal_key.size() >= 8);
  return slice(internal_key.data(), internal_key.size() - 8);
}

inline valuetype extractvaluetype(const slice& internal_key) {
  assert(internal_key.size() >= 8);
  const size_t n = internal_key.size();
  uint64_t num = decodefixed64(internal_key.data() + n - 8);
  unsigned char c = num & 0xff;
  return static_cast<valuetype>(c);
}

// a comparator for internal keys that uses a specified comparator for
// the user key portion and breaks ties by decreasing sequence number.
class internalkeycomparator : public comparator {
 private:
  const comparator* user_comparator_;
  std::string name_;
 public:
  explicit internalkeycomparator(const comparator* c) : user_comparator_(c),
    name_("rocksdb.internalkeycomparator:" +
          std::string(user_comparator_->name())) {
  }
  virtual ~internalkeycomparator() {}

  virtual const char* name() const;
  virtual int compare(const slice& a, const slice& b) const;
  virtual void findshortestseparator(
      std::string* start,
      const slice& limit) const;
  virtual void findshortsuccessor(std::string* key) const;

  const comparator* user_comparator() const { return user_comparator_; }

  int compare(const internalkey& a, const internalkey& b) const;
  int compare(const parsedinternalkey& a, const parsedinternalkey& b) const;
};

// modules in this directory should keep internal keys wrapped inside
// the following class instead of plain strings so that we do not
// incorrectly use string comparisons instead of an internalkeycomparator.
class internalkey {
 private:
  std::string rep_;
 public:
  internalkey() { }   // leave rep_ as empty to indicate it is invalid
  internalkey(const slice& user_key, sequencenumber s, valuetype t) {
    appendinternalkey(&rep_, parsedinternalkey(user_key, s, t));
  }

  bool valid() const {
    parsedinternalkey parsed;
    return parseinternalkey(slice(rep_), &parsed);
  }

  void decodefrom(const slice& s) { rep_.assign(s.data(), s.size()); }
  slice encode() const {
    assert(!rep_.empty());
    return rep_;
  }

  slice user_key() const { return extractuserkey(rep_); }

  void setfrom(const parsedinternalkey& p) {
    rep_.clear();
    appendinternalkey(&rep_, p);
  }

  void clear() { rep_.clear(); }

  std::string debugstring(bool hex = false) const;
};

inline int internalkeycomparator::compare(
    const internalkey& a, const internalkey& b) const {
  return compare(a.encode(), b.encode());
}

inline bool parseinternalkey(const slice& internal_key,
                             parsedinternalkey* result) {
  const size_t n = internal_key.size();
  if (n < 8) return false;
  uint64_t num = decodefixed64(internal_key.data() + n - 8);
  unsigned char c = num & 0xff;
  result->sequence = num >> 8;
  result->type = static_cast<valuetype>(c);
  assert(result->type <= valuetype::kmaxvalue);
  result->user_key = slice(internal_key.data(), n - 8);
  return (c <= static_cast<unsigned char>(kvaluetypeforseek));
}

// update the sequence number in the internal key
inline void updateinternalkey(char* internal_key,
                              const size_t internal_key_size,
                              uint64_t seq, valuetype t) {
  assert(internal_key_size >= 8);
  char* seqtype = internal_key + internal_key_size - 8;
  uint64_t newval = (seq << 8) | t;
  encodefixed64(seqtype, newval);
}

// get the sequence number from the internal key
inline uint64_t getinternalkeyseqno(const slice& internal_key) {
  const size_t n = internal_key.size();
  assert(n >= 8);
  uint64_t num = decodefixed64(internal_key.data() + n - 8);
  return num >> 8;
}


// a helper class useful for dbimpl::get()
class lookupkey {
 public:
  // initialize *this for looking up user_key at a snapshot with
  // the specified sequence number.
  lookupkey(const slice& user_key, sequencenumber sequence);

  ~lookupkey();

  // return a key suitable for lookup in a memtable.
  slice memtable_key() const { return slice(start_, end_ - start_); }

  // return an internal key (suitable for passing to an internal iterator)
  slice internal_key() const { return slice(kstart_, end_ - kstart_); }

  // return the user key
  slice user_key() const { return slice(kstart_, end_ - kstart_ - 8); }

 private:
  // we construct a char array of the form:
  //    klength  varint32               <-- start_
  //    userkey  char[klength]          <-- kstart_
  //    tag      uint64
  //                                    <-- end_
  // the array is a suitable memtable key.
  // the suffix starting with "userkey" can be used as an internalkey.
  const char* start_;
  const char* kstart_;
  const char* end_;
  char space_[200];      // avoid allocation for short keys

  // no copying allowed
  lookupkey(const lookupkey&);
  void operator=(const lookupkey&);
};

inline lookupkey::~lookupkey() {
  if (start_ != space_) delete[] start_;
}

class iterkey {
 public:
  iterkey() : key_(space_), buf_size_(sizeof(space_)), key_size_(0) {}

  ~iterkey() { resetbuffer(); }

  slice getkey() const { return slice(key_, key_size_); }

  const size_t size() { return key_size_; }

  void clear() { key_size_ = 0; }

  // append "non_shared_data" to its back, from "shared_len"
  // this function is used in block::iter::parsenextkey
  // shared_len: bytes in [0, shard_len-1] would be remained
  // non_shared_data: data to be append, its length must be >= non_shared_len
  void trimappend(const size_t shared_len, const char* non_shared_data,
                  const size_t non_shared_len) {
    assert(shared_len <= key_size_);

    size_t total_size = shared_len + non_shared_len;
    if (total_size <= buf_size_) {
      key_size_ = total_size;
    } else {
      // need to allocate space, delete previous space
      char* p = new char[total_size];
      memcpy(p, key_, shared_len);

      if (key_ != nullptr && key_ != space_) {
        delete[] key_;
      }

      key_ = p;
      key_size_ = total_size;
      buf_size_ = total_size;
    }

    memcpy(key_ + shared_len, non_shared_data, non_shared_len);
  }

  void setkey(const slice& key) {
    size_t size = key.size();
    enlargebufferifneeded(size);
    memcpy(key_, key.data(), size);
    key_size_ = size;
  }

  void setinternalkey(const slice& key_prefix, const slice& user_key,
                      sequencenumber s,
                      valuetype value_type = kvaluetypeforseek) {
    size_t psize = key_prefix.size();
    size_t usize = user_key.size();
    enlargebufferifneeded(psize + usize + sizeof(uint64_t));
    if (psize > 0) {
      memcpy(key_, key_prefix.data(), psize);
    }
    memcpy(key_ + psize, user_key.data(), usize);
    encodefixed64(key_ + usize + psize, packsequenceandtype(s, value_type));
    key_size_ = psize + usize + sizeof(uint64_t);
  }

  void setinternalkey(const slice& user_key, sequencenumber s,
                      valuetype value_type = kvaluetypeforseek) {
    setinternalkey(slice(), user_key, s, value_type);
  }

  void reserve(size_t size) {
    enlargebufferifneeded(size);
    key_size_ = size;
  }

  void setinternalkey(const parsedinternalkey& parsed_key) {
    setinternalkey(slice(), parsed_key);
  }

  void setinternalkey(const slice& key_prefix,
                      const parsedinternalkey& parsed_key_suffix) {
    setinternalkey(key_prefix, parsed_key_suffix.user_key,
                   parsed_key_suffix.sequence, parsed_key_suffix.type);
  }

  void encodelengthprefixedkey(const slice& key) {
    auto size = key.size();
    enlargebufferifneeded(size + varintlength(size));
    char* ptr = encodevarint32(key_, size);
    memcpy(ptr, key.data(), size);
  }

 private:
  char* key_;
  size_t buf_size_;
  size_t key_size_;
  char space_[32];  // avoid allocation for short keys

  void resetbuffer() {
    if (key_ != nullptr && key_ != space_) {
      delete[] key_;
    }
    key_ = space_;
    buf_size_ = sizeof(space_);
    key_size_ = 0;
  }

  // enlarge the buffer size if needed based on key_size.
  // by default, static allocated buffer is used. once there is a key
  // larger than the static allocated buffer, another buffer is dynamically
  // allocated, until a larger key buffer is requested. in that case, we
  // reallocate buffer and delete the old one.
  void enlargebufferifneeded(size_t key_size) {
    // if size is smaller than buffer size, continue using current buffer,
    // or the static allocated one, as default
    if (key_size > buf_size_) {
      // need to enlarge the buffer.
      resetbuffer();
      key_ = new char[key_size];
      buf_size_ = key_size;
    }
  }

  // no copying allowed
  iterkey(const iterkey&) = delete;
  void operator=(const iterkey&) = delete;
};

class internalkeyslicetransform : public slicetransform {
 public:
  explicit internalkeyslicetransform(const slicetransform* transform)
      : transform_(transform) {}

  virtual const char* name() const { return transform_->name(); }

  virtual slice transform(const slice& src) const {
    auto user_key = extractuserkey(src);
    return transform_->transform(user_key);
  }

  virtual bool indomain(const slice& src) const {
    auto user_key = extractuserkey(src);
    return transform_->indomain(user_key);
  }

  virtual bool inrange(const slice& dst) const {
    auto user_key = extractuserkey(dst);
    return transform_->inrange(user_key);
  }

  const slicetransform* user_prefix_extractor() const { return transform_; }

 private:
  // like comparator, internalkeyslicetransform will not take care of the
  // deletion of transform_
  const slicetransform* const transform_;
};

// read record from a write batch piece from input.
// tag, column_family, key, value and blob are return values. callers own the
// slice they point to.
// tag is defined as valuetype.
// input will be advanced to after the record.
extern status readrecordfromwritebatch(slice* input, char* tag,
                                       uint32_t* column_family, slice* key,
                                       slice* value, slice* blob);
}  // namespace rocksdb
