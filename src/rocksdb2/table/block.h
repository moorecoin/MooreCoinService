//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include <stddef.h>
#include <stdint.h>

#include "rocksdb/iterator.h"
#include "rocksdb/options.h"
#include "db/dbformat.h"

namespace rocksdb {

struct blockcontents;
class comparator;
class blockiter;
class blockhashindex;
class blockprefixindex;

class block {
 public:
  // initialize the block with the specified contents.
  explicit block(const blockcontents& contents);

  ~block();

  size_t size() const { return size_; }
  const char* data() const { return data_; }
  bool cachable() const { return cachable_; }
  uint32_t numrestarts() const;
  compressiontype compression_type() const { return compression_type_; }

  // if hash index lookup is enabled and `use_hash_index` is true. this block
  // will do hash lookup for the key prefix.
  //
  // note: for the hash based lookup, if a key prefix doesn't match any key,
  // the iterator will simply be set as "invalid", rather than returning
  // the key that is just pass the target key.
  //
  // if iter is null, return new iterator
  // if iter is not null, update this one and return it as iterator*
  //
  // if total_order_seek is true, hash_index_ and prefix_index_ are ignored.
  // this option only applies for index block. for data block, hash_index_
  // and prefix_index_ are null, so this option does not matter.
  iterator* newiterator(const comparator* comparator,
      blockiter* iter = nullptr, bool total_order_seek = true);
  void setblockhashindex(blockhashindex* hash_index);
  void setblockprefixindex(blockprefixindex* prefix_index);

  // report an approximation of how much memory has been used.
  size_t approximatememoryusage() const;

 private:
  const char* data_;
  size_t size_;
  uint32_t restart_offset_;     // offset in data_ of restart array
  bool owned_;                  // block owns data_[]
  bool cachable_;
  compressiontype compression_type_;
  std::unique_ptr<blockhashindex> hash_index_;
  std::unique_ptr<blockprefixindex> prefix_index_;

  // no copying allowed
  block(const block&);
  void operator=(const block&);
};

class blockiter : public iterator {
 public:
  blockiter()
      : comparator_(nullptr),
        data_(nullptr),
        restarts_(0),
        num_restarts_(0),
        current_(0),
        restart_index_(0),
        status_(status::ok()),
        hash_index_(nullptr),
        prefix_index_(nullptr) {}

  blockiter(const comparator* comparator, const char* data, uint32_t restarts,
       uint32_t num_restarts, blockhashindex* hash_index,
       blockprefixindex* prefix_index)
      : blockiter() {
    initialize(comparator, data, restarts, num_restarts,
        hash_index, prefix_index);
  }

  void initialize(const comparator* comparator, const char* data,
      uint32_t restarts, uint32_t num_restarts, blockhashindex* hash_index,
      blockprefixindex* prefix_index) {
    assert(data_ == nullptr);           // ensure it is called only once
    assert(num_restarts > 0);           // ensure the param is valid

    comparator_ = comparator;
    data_ = data;
    restarts_ = restarts;
    num_restarts_ = num_restarts;
    current_ = restarts_;
    restart_index_ = num_restarts_;
    hash_index_ = hash_index;
    prefix_index_ = prefix_index;
  }

  void setstatus(status s) {
    status_ = s;
  }

  virtual bool valid() const override { return current_ < restarts_; }
  virtual status status() const override { return status_; }
  virtual slice key() const override {
    assert(valid());
    return key_.getkey();
  }
  virtual slice value() const override {
    assert(valid());
    return value_;
  }

  virtual void next() override;

  virtual void prev() override;

  virtual void seek(const slice& target) override;

  virtual void seektofirst() override;

  virtual void seektolast() override;

 private:
  const comparator* comparator_;
  const char* data_;       // underlying block contents
  uint32_t restarts_;      // offset of restart array (list of fixed32)
  uint32_t num_restarts_;  // number of uint32_t entries in restart array

  // current_ is offset in data_ of current entry.  >= restarts_ if !valid
  uint32_t current_;
  uint32_t restart_index_;  // index of restart block in which current_ falls
  iterkey key_;
  slice value_;
  status status_;
  blockhashindex* hash_index_;
  blockprefixindex* prefix_index_;

  inline int compare(const slice& a, const slice& b) const {
    return comparator_->compare(a, b);
  }

  // return the offset in data_ just past the end of the current entry.
  inline uint32_t nextentryoffset() const {
    return (value_.data() + value_.size()) - data_;
  }

  uint32_t getrestartpoint(uint32_t index) {
    assert(index < num_restarts_);
    return decodefixed32(data_ + restarts_ + index * sizeof(uint32_t));
  }

  void seektorestartpoint(uint32_t index) {
    key_.clear();
    restart_index_ = index;
    // current_ will be fixed by parsenextkey();

    // parsenextkey() starts at the end of value_, so set value_ accordingly
    uint32_t offset = getrestartpoint(index);
    value_ = slice(data_ + offset, 0);
  }

  void corruptionerror();

  bool parsenextkey();

  bool binaryseek(const slice& target, uint32_t left, uint32_t right,
                  uint32_t* index);

  int compareblockkey(uint32_t block_index, const slice& target);

  bool binaryblockindexseek(const slice& target, uint32_t* block_ids,
                            uint32_t left, uint32_t right,
                            uint32_t* index);

  bool hashseek(const slice& target, uint32_t* index);

  bool prefixseek(const slice& target, uint32_t* index);

};

}  // namespace rocksdb
