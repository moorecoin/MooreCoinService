//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include "rocksdb/db.h"

namespace rocksdb {

class snapshotlist;

// snapshots are kept in a doubly-linked list in the db.
// each snapshotimpl corresponds to a particular sequence number.
class snapshotimpl : public snapshot {
 public:
  sequencenumber number_;  // const after creation

 private:
  friend class snapshotlist;

  // snapshotimpl is kept in a doubly-linked circular list
  snapshotimpl* prev_;
  snapshotimpl* next_;

  snapshotlist* list_;                 // just for sanity checks
};

class snapshotlist {
 public:
  snapshotlist() {
    list_.prev_ = &list_;
    list_.next_ = &list_;
    list_.number_ = 0xffffffffl;      // placeholder marker, for debugging
  }

  bool empty() const { return list_.next_ == &list_; }
  snapshotimpl* oldest() const { assert(!empty()); return list_.next_; }
  snapshotimpl* newest() const { assert(!empty()); return list_.prev_; }

  const snapshotimpl* new(sequencenumber seq) {
    snapshotimpl* s = new snapshotimpl;
    s->number_ = seq;
    s->list_ = this;
    s->next_ = &list_;
    s->prev_ = list_.prev_;
    s->prev_->next_ = s;
    s->next_->prev_ = s;
    return s;
  }

  void delete(const snapshotimpl* s) {
    assert(s->list_ == this);
    s->prev_->next_ = s->next_;
    s->next_->prev_ = s->prev_;
    delete s;
  }

  // retrieve all snapshot numbers. they are sorted in ascending order.
  void getall(std::vector<sequencenumber>& ret) {
    if (empty()) return;
    snapshotimpl* s = &list_;
    while (s->next_ != &list_) {
      ret.push_back(s->next_->number_);
      s = s ->next_;
    }
  }

  // get the sequence number of the most recent snapshot
  const sequencenumber getnewest() {
    if (empty()) {
      return 0;
    }
    return newest()->number_;
  }

 private:
  // dummy head of doubly-linked list of snapshots
  snapshotimpl list_;
};

}  // namespace rocksdb
