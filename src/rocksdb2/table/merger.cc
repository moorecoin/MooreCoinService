//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "table/merger.h"

#include <vector>
#include <queue>

#include "rocksdb/comparator.h"
#include "rocksdb/iterator.h"
#include "rocksdb/options.h"
#include "table/iter_heap.h"
#include "table/iterator_wrapper.h"
#include "util/arena.h"
#include "util/stop_watch.h"
#include "util/perf_context_imp.h"
#include "util/autovector.h"

namespace rocksdb {
namespace merger {
typedef std::priority_queue<
          iteratorwrapper*,
          std::vector<iteratorwrapper*>,
          maxiteratorcomparator> maxiterheap;

typedef std::priority_queue<
          iteratorwrapper*,
          std::vector<iteratorwrapper*>,
          miniteratorcomparator> miniterheap;

// return's a new maxheap of iteratorwrapper's using the provided comparator.
maxiterheap newmaxiterheap(const comparator* comparator) {
  return maxiterheap(maxiteratorcomparator(comparator));
}

// return's a new minheap of iteratorwrapper's using the provided comparator.
miniterheap newminiterheap(const comparator* comparator) {
  return miniterheap(miniteratorcomparator(comparator));
}
}  // namespace merger

const size_t knumiterreserve = 4;

class mergingiterator : public iterator {
 public:
  mergingiterator(const comparator* comparator, iterator** children, int n,
                  bool is_arena_mode)
      : is_arena_mode_(is_arena_mode),
        comparator_(comparator),
        current_(nullptr),
        use_heap_(true),
        direction_(kforward),
        maxheap_(merger::newmaxiterheap(comparator_)),
        minheap_(merger::newminiterheap(comparator_)) {
    children_.resize(n);
    for (int i = 0; i < n; i++) {
      children_[i].set(children[i]);
    }
    for (auto& child : children_) {
      if (child.valid()) {
        minheap_.push(&child);
      }
    }
  }

  virtual void additerator(iterator* iter) {
    assert(direction_ == kforward);
    children_.emplace_back(iter);
    auto new_wrapper = children_.back();
    if (new_wrapper.valid()) {
      minheap_.push(&new_wrapper);
    }
  }

  virtual ~mergingiterator() {
    for (auto& child : children_) {
      child.deleteiter(is_arena_mode_);
    }
  }

  virtual bool valid() const {
    return (current_ != nullptr);
  }

  virtual void seektofirst() {
    clearheaps();
    for (auto& child : children_) {
      child.seektofirst();
      if (child.valid()) {
        minheap_.push(&child);
      }
    }
    findsmallest();
    direction_ = kforward;
  }

  virtual void seektolast() {
    clearheaps();
    for (auto& child : children_) {
      child.seektolast();
      if (child.valid()) {
        maxheap_.push(&child);
      }
    }
    findlargest();
    direction_ = kreverse;
  }

  virtual void seek(const slice& target) {
    // invalidate the heap.
    use_heap_ = false;
    iteratorwrapper* first_child = nullptr;

    for (auto& child : children_) {
      {
        perf_timer_guard(seek_child_seek_time);
        child.seek(target);
      }
      perf_counter_add(seek_child_seek_count, 1);

      if (child.valid()) {
        // this child has valid key
        if (!use_heap_) {
          if (first_child == nullptr) {
            // it's the first child has valid key. only put it int
            // current_. now the values in the heap should be invalid.
            first_child = &child;
          } else {
            // we have more than one children with valid keys. initialize
            // the heap and put the first child into the heap.
            perf_timer_guard(seek_min_heap_time);
            clearheaps();
            minheap_.push(first_child);
          }
        }
        if (use_heap_) {
          perf_timer_guard(seek_min_heap_time);
          minheap_.push(&child);
        }
      }
    }
    if (use_heap_) {
      // if heap is valid, need to put the smallest key to curent_.
      perf_timer_guard(seek_min_heap_time);
      findsmallest();
    } else {
      // the heap is not valid, then the current_ iterator is the first
      // one, or null if there is no first child.
      current_ = first_child;
    }
    direction_ = kforward;
  }

  virtual void next() {
    assert(valid());

    // ensure that all children are positioned after key().
    // if we are moving in the forward direction, it is already
    // true for all of the non-current_ children since current_ is
    // the smallest child and key() == current_->key().  otherwise,
    // we explicitly position the non-current_ children.
    if (direction_ != kforward) {
      clearheaps();
      for (auto& child : children_) {
        if (&child != current_) {
          child.seek(key());
          if (child.valid() &&
              comparator_->compare(key(), child.key()) == 0) {
            child.next();
          }
          if (child.valid()) {
            minheap_.push(&child);
          }
        }
      }
      direction_ = kforward;
    }

    // as the current points to the current record. move the iterator forward.
    // and if it is valid add it to the heap.
    current_->next();
    if (use_heap_) {
      if (current_->valid()) {
        minheap_.push(current_);
      }
      findsmallest();
    } else if (!current_->valid()) {
      current_ = nullptr;
    }
  }

  virtual void prev() {
    assert(valid());
    // ensure that all children are positioned before key().
    // if we are moving in the reverse direction, it is already
    // true for all of the non-current_ children since current_ is
    // the largest child and key() == current_->key().  otherwise,
    // we explicitly position the non-current_ children.
    if (direction_ != kreverse) {
      clearheaps();
      for (auto& child : children_) {
        if (&child != current_) {
          child.seek(key());
          if (child.valid()) {
            // child is at first entry >= key().  step back one to be < key()
            child.prev();
          } else {
            // child has no entries >= key().  position at last entry.
            child.seektolast();
          }
          if (child.valid()) {
            maxheap_.push(&child);
          }
        }
      }
      direction_ = kreverse;
    }

    current_->prev();
    if (current_->valid()) {
      maxheap_.push(current_);
    }
    findlargest();
  }

  virtual slice key() const {
    assert(valid());
    return current_->key();
  }

  virtual slice value() const {
    assert(valid());
    return current_->value();
  }

  virtual status status() const {
    status status;
    for (auto& child : children_) {
      status = child.status();
      if (!status.ok()) {
        break;
      }
    }
    return status;
  }

 private:
  void findsmallest();
  void findlargest();
  void clearheaps();

  bool is_arena_mode_;
  const comparator* comparator_;
  autovector<iteratorwrapper, knumiterreserve> children_;
  iteratorwrapper* current_;
  // if the value is true, both of iterators in the heap and current_
  // contain valid rows. if it is false, only current_ can possibly contain
  // valid rows.
  // this flag is always true for reverse direction, as we always use heap for
  // the reverse iterating case.
  bool use_heap_;
  // which direction is the iterator moving?
  enum direction {
    kforward,
    kreverse
  };
  direction direction_;
  merger::maxiterheap maxheap_;
  merger::miniterheap minheap_;
};

void mergingiterator::findsmallest() {
  assert(use_heap_);
  if (minheap_.empty()) {
    current_ = nullptr;
  } else {
    current_ = minheap_.top();
    assert(current_->valid());
    minheap_.pop();
  }
}

void mergingiterator::findlargest() {
  assert(use_heap_);
  if (maxheap_.empty()) {
    current_ = nullptr;
  } else {
    current_ = maxheap_.top();
    assert(current_->valid());
    maxheap_.pop();
  }
}

void mergingiterator::clearheaps() {
  use_heap_ = true;
  maxheap_ = merger::newmaxiterheap(comparator_);
  minheap_ = merger::newminiterheap(comparator_);
}

iterator* newmergingiterator(const comparator* cmp, iterator** list, int n,
                             arena* arena) {
  assert(n >= 0);
  if (n == 0) {
    return newemptyiterator(arena);
  } else if (n == 1) {
    return list[0];
  } else {
    if (arena == nullptr) {
      return new mergingiterator(cmp, list, n, false);
    } else {
      auto mem = arena->allocatealigned(sizeof(mergingiterator));
      return new (mem) mergingiterator(cmp, list, n, true);
    }
  }
}

mergeiteratorbuilder::mergeiteratorbuilder(const comparator* comparator,
                                           arena* a)
    : first_iter(nullptr), use_merging_iter(false), arena(a) {

  auto mem = arena->allocatealigned(sizeof(mergingiterator));
  merge_iter = new (mem) mergingiterator(comparator, nullptr, 0, true);
}

void mergeiteratorbuilder::additerator(iterator* iter) {
  if (!use_merging_iter && first_iter != nullptr) {
    merge_iter->additerator(first_iter);
    use_merging_iter = true;
  }
  if (use_merging_iter) {
    merge_iter->additerator(iter);
  } else {
    first_iter = iter;
  }
}

iterator* mergeiteratorbuilder::finish() {
  if (!use_merging_iter) {
    return first_iter;
  } else {
    auto ret = merge_iter;
    merge_iter = nullptr;
    return ret;
  }
}

}  // namespace rocksdb
