//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#ifndef rocksdb_lite
#include "db/forward_iterator.h"

#include <limits>
#include <string>
#include <utility>

#include "db/db_impl.h"
#include "db/db_iter.h"
#include "db/column_family.h"
#include "rocksdb/env.h"
#include "rocksdb/slice.h"
#include "rocksdb/slice_transform.h"
#include "table/merger.h"
#include "db/dbformat.h"

namespace rocksdb {

// usage:
//     leveliterator iter;
//     iter.setfileindex(file_index);
//     iter.seek(target);
//     iter.next()
class leveliterator : public iterator {
 public:
  leveliterator(const columnfamilydata* const cfd,
      const readoptions& read_options,
      const std::vector<filemetadata*>& files)
    : cfd_(cfd), read_options_(read_options), files_(files), valid_(false),
      file_index_(std::numeric_limits<uint32_t>::max()) {}

  void setfileindex(uint32_t file_index) {
    assert(file_index < files_.size());
    if (file_index != file_index_) {
      file_index_ = file_index;
      reset();
    }
    valid_ = false;
  }
  void reset() {
    assert(file_index_ < files_.size());
    file_iter_.reset(cfd_->table_cache()->newiterator(
        read_options_, *(cfd_->soptions()), cfd_->internal_comparator(),
        files_[file_index_]->fd, nullptr /* table_reader_ptr */, false));
  }
  void seektolast() override {
    status_ = status::notsupported("leveliterator::seektolast()");
    valid_ = false;
  }
  void prev() {
    status_ = status::notsupported("leveliterator::prev()");
    valid_ = false;
  }
  bool valid() const override {
    return valid_;
  }
  void seektofirst() override {
    setfileindex(0);
    file_iter_->seektofirst();
    valid_ = file_iter_->valid();
  }
  void seek(const slice& internal_key) override {
    assert(file_iter_ != nullptr);
    file_iter_->seek(internal_key);
    valid_ = file_iter_->valid();
  }
  void next() override {
    assert(valid_);
    file_iter_->next();
    for (;;) {
      if (file_iter_->status().isincomplete() || file_iter_->valid()) {
        valid_ = !file_iter_->status().isincomplete();
        return;
      }
      if (file_index_ + 1 >= files_.size()) {
        valid_ = false;
        return;
      }
      setfileindex(file_index_ + 1);
      file_iter_->seektofirst();
    }
  }
  slice key() const override {
    assert(valid_);
    return file_iter_->key();
  }
  slice value() const override {
    assert(valid_);
    return file_iter_->value();
  }
  status status() const override {
    if (!status_.ok()) {
      return status_;
    } else if (file_iter_ && !file_iter_->status().ok()) {
      return file_iter_->status();
    }
    return status::ok();
  }

 private:
  const columnfamilydata* const cfd_;
  const readoptions& read_options_;
  const std::vector<filemetadata*>& files_;

  bool valid_;
  uint32_t file_index_;
  status status_;
  std::unique_ptr<iterator> file_iter_;
};

forwarditerator::forwarditerator(dbimpl* db, const readoptions& read_options,
                                 columnfamilydata* cfd)
    : db_(db),
      read_options_(read_options),
      cfd_(cfd),
      prefix_extractor_(cfd->options()->prefix_extractor.get()),
      user_comparator_(cfd->user_comparator()),
      immutable_min_heap_(minitercomparator(&cfd_->internal_comparator())),
      sv_(nullptr),
      mutable_iter_(nullptr),
      current_(nullptr),
      valid_(false),
      is_prev_set_(false) {}

forwarditerator::~forwarditerator() {
  cleanup();
}

void forwarditerator::cleanup() {
  delete mutable_iter_;
  for (auto* m : imm_iters_) {
    delete m;
  }
  imm_iters_.clear();
  for (auto* f : l0_iters_) {
    delete f;
  }
  l0_iters_.clear();
  for (auto* l : level_iters_) {
    delete l;
  }
  level_iters_.clear();

  if (sv_ != nullptr && sv_->unref()) {
    dbimpl::deletionstate deletion_state;
    db_->mutex_.lock();
    sv_->cleanup();
    db_->findobsoletefiles(deletion_state, false, true);
    db_->mutex_.unlock();
    delete sv_;
    if (deletion_state.havesomethingtodelete()) {
      db_->purgeobsoletefiles(deletion_state);
    }
  }
}

bool forwarditerator::valid() const {
  return valid_;
}

void forwarditerator::seektofirst() {
  if (sv_ == nullptr ||
      sv_ ->version_number != cfd_->getsuperversionnumber()) {
    rebuilditerators();
  } else if (status_.isincomplete()) {
    resetincompleteiterators();
  }
  seekinternal(slice(), true);
}

void forwarditerator::seek(const slice& internal_key) {
  if (sv_ == nullptr ||
      sv_ ->version_number != cfd_->getsuperversionnumber()) {
    rebuilditerators();
  } else if (status_.isincomplete()) {
    resetincompleteiterators();
  }
  seekinternal(internal_key, false);
}

void forwarditerator::seekinternal(const slice& internal_key,
                                   bool seek_to_first) {
  // mutable
  seek_to_first ? mutable_iter_->seektofirst() :
                  mutable_iter_->seek(internal_key);

  // immutable
  // todo(ljin): needtoseekimmutable has negative impact on performance
  // if it turns to need to seek immutable often. we probably want to have
  // an option to turn it off.
  if (seek_to_first || needtoseekimmutable(internal_key)) {
    {
      auto tmp = miniterheap(minitercomparator(&cfd_->internal_comparator()));
      immutable_min_heap_.swap(tmp);
    }
    for (auto* m : imm_iters_) {
      seek_to_first ? m->seektofirst() : m->seek(internal_key);
      if (m->valid()) {
        immutable_min_heap_.push(m);
      }
    }

    slice user_key;
    if (!seek_to_first) {
      user_key = extractuserkey(internal_key);
    }
    auto* files = sv_->current->files_;
    for (uint32_t i = 0; i < files[0].size(); ++i) {
      if (seek_to_first) {
        l0_iters_[i]->seektofirst();
      } else {
        // if the target key passes over the larget key, we are sure next()
        // won't go over this file.
        if (user_comparator_->compare(user_key,
              files[0][i]->largest.user_key()) > 0) {
          continue;
        }
        l0_iters_[i]->seek(internal_key);
      }

      if (l0_iters_[i]->status().isincomplete()) {
        // if any of the immutable iterators is incomplete (no-io option was
        // used), we are unable to reliably find the smallest key
        assert(read_options_.read_tier == kblockcachetier);
        status_ = l0_iters_[i]->status();
        valid_ = false;
        return;
      } else if (l0_iters_[i]->valid()) {
        immutable_min_heap_.push(l0_iters_[i]);
      }
    }

    int32_t search_left_bound = 0;
    int32_t search_right_bound = fileindexer::klevelmaxindex;
    for (int32_t level = 1; level < sv_->current->numberlevels(); ++level) {
      if (files[level].empty()) {
        search_left_bound = 0;
        search_right_bound = fileindexer::klevelmaxindex;
        continue;
      }
      assert(level_iters_[level - 1] != nullptr);
      uint32_t f_idx = 0;
      if (!seek_to_first) {
        // todo(ljin): remove before committing
        // f_idx = findfileinrange(
        //    files[level], internal_key, 0, files[level].size());

        if (search_left_bound == search_right_bound) {
          f_idx = search_left_bound;
        } else if (search_left_bound < search_right_bound) {
          f_idx = findfileinrange(
              files[level], internal_key, search_left_bound,
              search_right_bound == fileindexer::klevelmaxindex ?
                files[level].size() : search_right_bound);
        } else {
          // search_left_bound > search_right_bound
          // there are only 2 cases this can happen:
          // (1) target key is smaller than left most file
          // (2) target key is larger than right most file
          assert(search_left_bound == (int32_t)files[level].size() ||
                 search_right_bound == -1);
          if (search_right_bound == -1) {
            assert(search_left_bound == 0);
            f_idx = 0;
          } else {
            sv_->current->file_indexer_.getnextlevelindex(
                level, files[level].size() - 1,
                1, 1, &search_left_bound, &search_right_bound);
            continue;
          }
        }

        // prepare hints for the next level
        if (f_idx < files[level].size()) {
          int cmp_smallest = user_comparator_->compare(
              user_key, files[level][f_idx]->smallest.user_key());
          int cmp_largest = -1;
          if (cmp_smallest >= 0) {
            cmp_smallest = user_comparator_->compare(
                user_key, files[level][f_idx]->smallest.user_key());
          }
          sv_->current->file_indexer_.getnextlevelindex(level, f_idx,
              cmp_smallest, cmp_largest,
              &search_left_bound, &search_right_bound);
        } else {
          sv_->current->file_indexer_.getnextlevelindex(
              level, files[level].size() - 1,
              1, 1, &search_left_bound, &search_right_bound);
        }
      }

      // seek
      if (f_idx < files[level].size()) {
        level_iters_[level - 1]->setfileindex(f_idx);
        seek_to_first ? level_iters_[level - 1]->seektofirst() :
                        level_iters_[level - 1]->seek(internal_key);

        if (level_iters_[level - 1]->status().isincomplete()) {
          // see above
          assert(read_options_.read_tier == kblockcachetier);
          status_ = level_iters_[level - 1]->status();
          valid_ = false;
          return;
        } else if (level_iters_[level - 1]->valid()) {
          immutable_min_heap_.push(level_iters_[level - 1]);
        }
      }
    }

    if (seek_to_first || immutable_min_heap_.empty()) {
      is_prev_set_ = false;
    } else {
      prev_key_.setkey(internal_key);
      is_prev_set_ = true;
    }
  } else if (current_ && current_ != mutable_iter_) {
    // current_ is one of immutable iterators, push it back to the heap
    immutable_min_heap_.push(current_);
  }

  updatecurrent();
}

void forwarditerator::next() {
  assert(valid_);

  if (sv_ == nullptr ||
      sv_->version_number != cfd_->getsuperversionnumber()) {
    std::string current_key = key().tostring();
    slice old_key(current_key.data(), current_key.size());

    rebuilditerators();
    seekinternal(old_key, false);
    if (!valid_ || key().compare(old_key) != 0) {
      return;
    }
  } else if (current_ != mutable_iter_) {
    // it is going to advance immutable iterator
    prev_key_.setkey(current_->key());
    is_prev_set_ = true;
  }

  current_->next();
  if (current_ != mutable_iter_) {
    if (current_->status().isincomplete()) {
      assert(read_options_.read_tier == kblockcachetier);
      status_ = current_->status();
      valid_ = false;
      return;
    } else if (current_->valid()) {
      immutable_min_heap_.push(current_);
    }
  }

  updatecurrent();
}

slice forwarditerator::key() const {
  assert(valid_);
  return current_->key();
}

slice forwarditerator::value() const {
  assert(valid_);
  return current_->value();
}

status forwarditerator::status() const {
  if (!status_.ok()) {
    return status_;
  } else if (!mutable_iter_->status().ok()) {
    return mutable_iter_->status();
  }

  for (auto *it : imm_iters_) {
    if (it && !it->status().ok()) {
      return it->status();
    }
  }
  for (auto *it : l0_iters_) {
    if (it && !it->status().ok()) {
      return it->status();
    }
  }
  for (auto *it : level_iters_) {
    if (it && !it->status().ok()) {
      return it->status();
    }
  }

  return status::ok();
}

void forwarditerator::rebuilditerators() {
  // clean up
  cleanup();
  // new
  sv_ = cfd_->getreferencedsuperversion(&(db_->mutex_));
  mutable_iter_ = sv_->mem->newiterator(read_options_);
  sv_->imm->additerators(read_options_, &imm_iters_);
  const auto& l0_files = sv_->current->files_[0];
  l0_iters_.reserve(l0_files.size());
  for (const auto* l0 : l0_files) {
    l0_iters_.push_back(cfd_->table_cache()->newiterator(
        read_options_, *cfd_->soptions(), cfd_->internal_comparator(), l0->fd));
  }
  level_iters_.reserve(sv_->current->numberlevels() - 1);
  for (int32_t level = 1; level < sv_->current->numberlevels(); ++level) {
    if (sv_->current->files_[level].empty()) {
      level_iters_.push_back(nullptr);
    } else {
      level_iters_.push_back(new leveliterator(cfd_, read_options_,
          sv_->current->files_[level]));
    }
  }

  current_ = nullptr;
  is_prev_set_ = false;
}

void forwarditerator::resetincompleteiterators() {
  const auto& l0_files = sv_->current->files_[0];
  for (uint32_t i = 0; i < l0_iters_.size(); ++i) {
    assert(i < l0_files.size());
    if (!l0_iters_[i]->status().isincomplete()) {
      continue;
    }
    delete l0_iters_[i];
    l0_iters_[i] = cfd_->table_cache()->newiterator(
        read_options_, *cfd_->soptions(), cfd_->internal_comparator(),
        l0_files[i]->fd);
  }

  for (auto* level_iter : level_iters_) {
    if (level_iter && level_iter->status().isincomplete()) {
      level_iter->reset();
    }
  }

  current_ = nullptr;
  is_prev_set_ = false;
}

void forwarditerator::updatecurrent() {
  if (immutable_min_heap_.empty() && !mutable_iter_->valid()) {
    current_ = nullptr;
  } else if (immutable_min_heap_.empty()) {
    current_ = mutable_iter_;
  } else if (!mutable_iter_->valid()) {
    current_ = immutable_min_heap_.top();
    immutable_min_heap_.pop();
  } else {
    current_ = immutable_min_heap_.top();
    assert(current_ != nullptr);
    assert(current_->valid());
    int cmp = cfd_->internal_comparator().internalkeycomparator::compare(
        mutable_iter_->key(), current_->key());
    assert(cmp != 0);
    if (cmp > 0) {
      immutable_min_heap_.pop();
    } else {
      current_ = mutable_iter_;
    }
  }
  valid_ = (current_ != nullptr);
  if (!status_.ok()) {
    status_ = status::ok();
  }
}

bool forwarditerator::needtoseekimmutable(const slice& target) {
  if (!valid_ || !is_prev_set_) {
    return true;
  }
  slice prev_key = prev_key_.getkey();
  if (prefix_extractor_ && prefix_extractor_->transform(target).compare(
    prefix_extractor_->transform(prev_key)) != 0) {
    return true;
  }
  if (cfd_->internal_comparator().internalkeycomparator::compare(
        prev_key, target) >= 0) {
    return true;
  }
  if (immutable_min_heap_.empty() ||
      cfd_->internal_comparator().internalkeycomparator::compare(
          target, current_ == mutable_iter_ ? immutable_min_heap_.top()->key()
                                            : current_->key()) > 0) {
    return true;
  }
  return false;
}

uint32_t forwarditerator::findfileinrange(
    const std::vector<filemetadata*>& files, const slice& internal_key,
    uint32_t left, uint32_t right) {
  while (left < right) {
    uint32_t mid = (left + right) / 2;
    const filemetadata* f = files[mid];
    if (cfd_->internal_comparator().internalkeycomparator::compare(
          f->largest.encode(), internal_key) < 0) {
      // key at "mid.largest" is < "target".  therefore all
      // files at or before "mid" are uninteresting.
      left = mid + 1;
    } else {
      // key at "mid.largest" is >= "target".  therefore all files
      // after "mid" are uninteresting.
      right = mid;
    }
  }
  return right;
}

}  // namespace rocksdb

#endif  // rocksdb_lite
