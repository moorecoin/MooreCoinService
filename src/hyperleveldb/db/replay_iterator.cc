// copyright (c) 2013 the hyperleveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "replay_iterator.h"

#include "filename.h"
#include "db_impl.h"
#include "dbformat.h"
#include "../hyperleveldb/env.h"
#include "../hyperleveldb/iterator.h"
#include "../port/port.h"
#include "../util/logging.h"
#include "../util/mutexlock.h"
#include "../util/random.h"

namespace hyperleveldb {

replayiterator::replayiterator() {
}

replayiterator::~replayiterator() {
}

replaystate::replaystate(iterator* i, sequencenumber s, sequencenumber l)
  : mem_(null),
    iter_(i),
    seq_start_(s),
    seq_limit_(l) {
}

replaystate::replaystate(memtable* m, sequencenumber s)
  : mem_(m),
    iter_(null),
    seq_start_(s),
    seq_limit_(0) {
}

replayiteratorimpl::replayiteratorimpl(dbimpl* db, port::mutex* mutex, const comparator* cmp,
    iterator* iter, memtable* m, sequencenumber s)
  : replayiterator(),
    db_(db),
    mutex_(mutex),
    user_comparator_(cmp),
    start_at_(s),
    valid_(),
    status_(),
    has_current_user_key_(false),
    current_user_key_(),
    current_user_sequence_(),
    rs_(iter, s, kmaxsequencenumber),
    mems_() {
  m->ref();
  mems_.push_back(replaystate(m, s));
}

replayiteratorimpl::~replayiteratorimpl() {
}

bool replayiteratorimpl::valid() {
  prime();
  return valid_;
}

void replayiteratorimpl::next() {
  rs_.iter_->next();
}

bool replayiteratorimpl::hasvalue() {
  parsedinternalkey ikey;
  return parsekey(&ikey) && ikey.type == ktypevalue;
}

slice replayiteratorimpl::key() const {
  assert(valid_);
  return extractuserkey(rs_.iter_->key());
}

slice replayiteratorimpl::value() const {
  assert(valid_);
  return rs_.iter_->value();
}

status replayiteratorimpl::status() const {
  if (!status_.ok()) {
    return status_;
  } else {
    return rs_.iter_->status();
  }
}

void replayiteratorimpl::enqueue(memtable* m, sequencenumber s) {
  m->ref();
  mems_.push_back(replaystate(m, s));
}

void replayiteratorimpl::cleanup() {
  mutex_->unlock();
  if (rs_.iter_) {
    delete rs_.iter_;
  }
  if (rs_.mem_) {
    rs_.mem_->unref();
  }
  mutex_->lock();
  rs_.iter_ = null;
  rs_.mem_ = null;

  while (!mems_.empty()) {
    memtable* mem = mems_.front().mem_;
    iterator* iter = mems_.front().iter_;
    mutex_->unlock();
    if (iter) {
      delete iter;
    }
    if (mem) {
      mem->unref();
    }
    mutex_->lock();
    mems_.pop_front();
  }

  delete this;
}

bool replayiteratorimpl::parsekey(parsedinternalkey* ikey) {
  return parsekey(rs_.iter_->key(), ikey);
}

bool replayiteratorimpl::parsekey(const slice& k, parsedinternalkey* ikey) {
  if (!parseinternalkey(k, ikey)) {
    status_ = status::corruption("corrupted internal key in replayiteratorimpl");
    return false;
  } else {
    return true;
  }
}

void replayiteratorimpl::prime() {
  valid_ = false;
  if (!status_.ok()) {
    return;
  }
  while (true) {
    assert(rs_.iter_);
    while (rs_.iter_->valid()) {
      parsedinternalkey ikey;
      if (!parsekey(rs_.iter_->key(), &ikey)) {
        return;
      }
      // if we can consider this key, and it's recent enough and of the right
      // type
      if ((!has_current_user_key_ ||
           user_comparator_->compare(ikey.user_key,
                                     slice(current_user_key_)) != 0 ||
           ikey.sequence >= current_user_sequence_) &&
          (ikey.sequence >= rs_.seq_start_ &&
            (ikey.type == ktypedeletion || ikey.type == ktypevalue))) {
        has_current_user_key_ = true;
        current_user_key_.assign(ikey.user_key.data(), ikey.user_key.size());
        current_user_sequence_ = ikey.sequence;
        valid_ = true;
        return;
      }
      rs_.iter_->next();
    }
    if (!rs_.iter_->status().ok()) {
      status_ = rs_.iter_->status();
      valid_ = false;
      return;
    }
    // we're done with rs_.iter_
    has_current_user_key_ = false;
    current_user_key_.assign("", 0);
    current_user_sequence_ = kmaxsequencenumber;
    delete rs_.iter_;
    rs_.iter_ = null;
    {
      mutexlock l(mutex_);
      if (mems_.empty() ||
          rs_.seq_limit_ < mems_.front().seq_start_) {
        rs_.seq_start_ = rs_.seq_limit_;
      } else {
        if (rs_.mem_) {
          rs_.mem_->unref();
          rs_.mem_ = null;
        }
        rs_.mem_ = mems_.front().mem_;
        rs_.seq_start_ = mems_.front().seq_start_;
        mems_.pop_front();
      }
    }
    rs_.seq_limit_ = db_->lastsequence();
    rs_.iter_ = rs_.mem_->newiterator();
    rs_.iter_->seektofirst();
    assert(rs_.seq_start_ <= rs_.seq_limit_);
    if (rs_.seq_start_ == rs_.seq_limit_) {
      valid_ = false;
      return;
    }
  }
}

}  // namespace leveldb
