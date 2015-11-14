//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// thread safety
// -------------
//
// writes require external synchronization, most likely a mutex.
// reads require a guarantee that the skiplist will not be destroyed
// while the read is in progress.  apart from that, reads progress
// without any internal locking or synchronization.
//
// invariants:
//
// (1) allocated nodes are never deleted until the skiplist is
// destroyed.  this is trivially guaranteed by the code since we
// never delete any skip list nodes.
//
// (2) the contents of a node except for the next/prev pointers are
// immutable after the node has been linked into the skiplist.
// only insert() modifies the list, and it is careful to initialize
// a node and use release-stores to publish the nodes in one or
// more lists.
//
// ... prev vs. next pointer ordering ...
//

#pragma once
#include <assert.h>
#include <stdlib.h>
#include "util/arena.h"
#include "port/port.h"
#include "util/arena.h"
#include "util/random.h"

namespace rocksdb {

template<typename key, class comparator>
class skiplist {
 private:
  struct node;

 public:
  // create a new skiplist object that will use "cmp" for comparing keys,
  // and will allocate memory using "*arena".  objects allocated in the arena
  // must remain allocated for the lifetime of the skiplist object.
  explicit skiplist(comparator cmp, arena* arena,
                    int32_t max_height = 12, int32_t branching_factor = 4);

  // insert key into the list.
  // requires: nothing that compares equal to key is currently in the list.
  void insert(const key& key);

  // returns true iff an entry that compares equal to key is in the list.
  bool contains(const key& key) const;

  // iteration over the contents of a skip list
  class iterator {
   public:
    // initialize an iterator over the specified list.
    // the returned iterator is not valid.
    explicit iterator(const skiplist* list);

    // change the underlying skiplist used for this iterator
    // this enables us not changing the iterator without deallocating
    // an old one and then allocating a new one
    void setlist(const skiplist* list);

    // returns true iff the iterator is positioned at a valid node.
    bool valid() const;

    // returns the key at the current position.
    // requires: valid()
    const key& key() const;

    // advances to the next position.
    // requires: valid()
    void next();

    // advances to the previous position.
    // requires: valid()
    void prev();

    // advance to the first entry with a key >= target
    void seek(const key& target);

    // position at the first entry in list.
    // final state of iterator is valid() iff list is not empty.
    void seektofirst();

    // position at the last entry in list.
    // final state of iterator is valid() iff list is not empty.
    void seektolast();

   private:
    const skiplist* list_;
    node* node_;
    // intentionally copyable
  };

 private:
  const int32_t kmaxheight_;
  const int32_t kbranching_;

  // immutable after construction
  comparator const compare_;
  arena* const arena_;    // arena used for allocations of nodes

  node* const head_;

  // modified only by insert().  read racily by readers, but stale
  // values are ok.
  port::atomicpointer max_height_;   // height of the entire list

  // used for optimizing sequential insert patterns
  node** prev_;
  int32_t prev_height_;

  inline int getmaxheight() const {
    return static_cast<int>(
        reinterpret_cast<intptr_t>(max_height_.nobarrier_load()));
  }

  // read/written only by insert().
  random rnd_;

  node* newnode(const key& key, int height);
  int randomheight();
  bool equal(const key& a, const key& b) const { return (compare_(a, b) == 0); }

  // return true if key is greater than the data stored in "n"
  bool keyisafternode(const key& key, node* n) const;

  // return the earliest node that comes at or after key.
  // return nullptr if there is no such node.
  //
  // if prev is non-nullptr, fills prev[level] with pointer to previous
  // node at "level" for every level in [0..max_height_-1].
  node* findgreaterorequal(const key& key, node** prev) const;

  // return the latest node with a key < key.
  // return head_ if there is no such node.
  node* findlessthan(const key& key) const;

  // return the last node in the list.
  // return head_ if list is empty.
  node* findlast() const;

  // no copying allowed
  skiplist(const skiplist&);
  void operator=(const skiplist&);
};

// implementation details follow
template<typename key, class comparator>
struct skiplist<key, comparator>::node {
  explicit node(const key& k) : key(k) { }

  key const key;

  // accessors/mutators for links.  wrapped in methods so we can
  // add the appropriate barriers as necessary.
  node* next(int n) {
    assert(n >= 0);
    // use an 'acquire load' so that we observe a fully initialized
    // version of the returned node.
    return reinterpret_cast<node*>(next_[n].acquire_load());
  }
  void setnext(int n, node* x) {
    assert(n >= 0);
    // use a 'release store' so that anybody who reads through this
    // pointer observes a fully initialized version of the inserted node.
    next_[n].release_store(x);
  }

  // no-barrier variants that can be safely used in a few locations.
  node* nobarrier_next(int n) {
    assert(n >= 0);
    return reinterpret_cast<node*>(next_[n].nobarrier_load());
  }
  void nobarrier_setnext(int n, node* x) {
    assert(n >= 0);
    next_[n].nobarrier_store(x);
  }

 private:
  // array of length equal to the node height.  next_[0] is lowest level link.
  port::atomicpointer next_[1];
};

template<typename key, class comparator>
typename skiplist<key, comparator>::node*
skiplist<key, comparator>::newnode(const key& key, int height) {
  char* mem = arena_->allocatealigned(
      sizeof(node) + sizeof(port::atomicpointer) * (height - 1));
  return new (mem) node(key);
}

template<typename key, class comparator>
inline skiplist<key, comparator>::iterator::iterator(const skiplist* list) {
  setlist(list);
}

template<typename key, class comparator>
inline void skiplist<key, comparator>::iterator::setlist(const skiplist* list) {
  list_ = list;
  node_ = nullptr;
}

template<typename key, class comparator>
inline bool skiplist<key, comparator>::iterator::valid() const {
  return node_ != nullptr;
}

template<typename key, class comparator>
inline const key& skiplist<key, comparator>::iterator::key() const {
  assert(valid());
  return node_->key;
}

template<typename key, class comparator>
inline void skiplist<key, comparator>::iterator::next() {
  assert(valid());
  node_ = node_->next(0);
}

template<typename key, class comparator>
inline void skiplist<key, comparator>::iterator::prev() {
  // instead of using explicit "prev" links, we just search for the
  // last node that falls before key.
  assert(valid());
  node_ = list_->findlessthan(node_->key);
  if (node_ == list_->head_) {
    node_ = nullptr;
  }
}

template<typename key, class comparator>
inline void skiplist<key, comparator>::iterator::seek(const key& target) {
  node_ = list_->findgreaterorequal(target, nullptr);
}

template<typename key, class comparator>
inline void skiplist<key, comparator>::iterator::seektofirst() {
  node_ = list_->head_->next(0);
}

template<typename key, class comparator>
inline void skiplist<key, comparator>::iterator::seektolast() {
  node_ = list_->findlast();
  if (node_ == list_->head_) {
    node_ = nullptr;
  }
}

template<typename key, class comparator>
int skiplist<key, comparator>::randomheight() {
  // increase height with probability 1 in kbranching
  int height = 1;
  while (height < kmaxheight_ && ((rnd_.next() % kbranching_) == 0)) {
    height++;
  }
  assert(height > 0);
  assert(height <= kmaxheight_);
  return height;
}

template<typename key, class comparator>
bool skiplist<key, comparator>::keyisafternode(const key& key, node* n) const {
  // nullptr n is considered infinite
  return (n != nullptr) && (compare_(n->key, key) < 0);
}

template<typename key, class comparator>
typename skiplist<key, comparator>::node* skiplist<key, comparator>::
  findgreaterorequal(const key& key, node** prev) const {
  // use prev as an optimization hint and fallback to slow path
  if (prev && !keyisafternode(key, prev[0]->next(0))) {
    node* x = prev[0];
    node* next = x->next(0);
    if ((x == head_) || keyisafternode(key, x)) {
      // adjust all relevant insertion points to the previous entry
      for (int i = 1; i < prev_height_; i++) {
        prev[i] = x;
      }
      return next;
    }
  }
  // normal lookup
  node* x = head_;
  int level = getmaxheight() - 1;
  while (true) {
    node* next = x->next(level);
    // make sure the lists are sorted.
    // if x points to head_ or next points nullptr, it is trivially satisfied.
    assert((x == head_) || (next == nullptr) || keyisafternode(next->key, x));
    if (keyisafternode(key, next)) {
      // keep searching in this list
      x = next;
    } else {
      if (prev != nullptr) prev[level] = x;
      if (level == 0) {
        return next;
      } else {
        // switch to next list
        level--;
      }
    }
  }
}

template<typename key, class comparator>
typename skiplist<key, comparator>::node*
skiplist<key, comparator>::findlessthan(const key& key) const {
  node* x = head_;
  int level = getmaxheight() - 1;
  while (true) {
    assert(x == head_ || compare_(x->key, key) < 0);
    node* next = x->next(level);
    if (next == nullptr || compare_(next->key, key) >= 0) {
      if (level == 0) {
        return x;
      } else {
        // switch to next list
        level--;
      }
    } else {
      x = next;
    }
  }
}

template<typename key, class comparator>
typename skiplist<key, comparator>::node* skiplist<key, comparator>::findlast()
    const {
  node* x = head_;
  int level = getmaxheight() - 1;
  while (true) {
    node* next = x->next(level);
    if (next == nullptr) {
      if (level == 0) {
        return x;
      } else {
        // switch to next list
        level--;
      }
    } else {
      x = next;
    }
  }
}

template<typename key, class comparator>
skiplist<key, comparator>::skiplist(const comparator cmp, arena* arena,
                                   int32_t max_height,
                                   int32_t branching_factor)
    : kmaxheight_(max_height),
      kbranching_(branching_factor),
      compare_(cmp),
      arena_(arena),
      head_(newnode(0 /* any key will do */, max_height)),
      max_height_(reinterpret_cast<void*>(1)),
      prev_height_(1),
      rnd_(0xdeadbeef) {
  assert(kmaxheight_ > 0);
  assert(kbranching_ > 0);
  // allocate the prev_ node* array, directly from the passed-in arena.
  // prev_ does not need to be freed, as its life cycle is tied up with
  // the arena as a whole.
  prev_ = (node**) arena_->allocatealigned(sizeof(node*) * kmaxheight_);
  for (int i = 0; i < kmaxheight_; i++) {
    head_->setnext(i, nullptr);
    prev_[i] = head_;
  }
}

template<typename key, class comparator>
void skiplist<key, comparator>::insert(const key& key) {
  // todo(opt): we can use a barrier-free variant of findgreaterorequal()
  // here since insert() is externally synchronized.
  node* x = findgreaterorequal(key, prev_);

  // our data structure does not allow duplicate insertion
  assert(x == nullptr || !equal(key, x->key));

  int height = randomheight();
  if (height > getmaxheight()) {
    for (int i = getmaxheight(); i < height; i++) {
      prev_[i] = head_;
    }
    //fprintf(stderr, "change height from %d to %d\n", max_height_, height);

    // it is ok to mutate max_height_ without any synchronization
    // with concurrent readers.  a concurrent reader that observes
    // the new value of max_height_ will see either the old value of
    // new level pointers from head_ (nullptr), or a new value set in
    // the loop below.  in the former case the reader will
    // immediately drop to the next level since nullptr sorts after all
    // keys.  in the latter case the reader will use the new node.
    max_height_.nobarrier_store(reinterpret_cast<void*>(height));
  }

  x = newnode(key, height);
  for (int i = 0; i < height; i++) {
    // nobarrier_setnext() suffices since we will add a barrier when
    // we publish a pointer to "x" in prev[i].
    x->nobarrier_setnext(i, prev_[i]->nobarrier_next(i));
    prev_[i]->setnext(i, x);
  }
  prev_[0] = x;
  prev_height_ = height;
}

template<typename key, class comparator>
bool skiplist<key, comparator>::contains(const key& key) const {
  node* x = findgreaterorequal(key, nullptr);
  if (x != nullptr && equal(key, x->key)) {
    return true;
  } else {
    return false;
  }
}

}  // namespace rocksdb
