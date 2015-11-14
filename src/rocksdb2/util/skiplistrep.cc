//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#include "rocksdb/memtablerep.h"
#include "db/memtable.h"
#include "db/skiplist.h"
#include "util/arena.h"

namespace rocksdb {
namespace {
class skiplistrep : public memtablerep {
  skiplist<const char*, const memtablerep::keycomparator&> skip_list_;
public:
  explicit skiplistrep(const memtablerep::keycomparator& compare, arena* arena)
    : memtablerep(arena), skip_list_(compare, arena) {
  }

  // insert key into the list.
  // requires: nothing that compares equal to key is currently in the list.
  virtual void insert(keyhandle handle) override {
    skip_list_.insert(static_cast<char*>(handle));
  }

  // returns true iff an entry that compares equal to key is in the list.
  virtual bool contains(const char* key) const override {
    return skip_list_.contains(key);
  }

  virtual size_t approximatememoryusage() override {
    // all memory is allocated through arena; nothing to report here
    return 0;
  }

  virtual void get(const lookupkey& k, void* callback_args,
                   bool (*callback_func)(void* arg,
                                         const char* entry)) override {
    skiplistrep::iterator iter(&skip_list_);
    slice dummy_slice;
    for (iter.seek(dummy_slice, k.memtable_key().data());
         iter.valid() && callback_func(callback_args, iter.key());
         iter.next()) {
    }
  }

  virtual ~skiplistrep() override { }

  // iteration over the contents of a skip list
  class iterator : public memtablerep::iterator {
    skiplist<const char*, const memtablerep::keycomparator&>::iterator iter_;
   public:
    // initialize an iterator over the specified list.
    // the returned iterator is not valid.
    explicit iterator(
      const skiplist<const char*, const memtablerep::keycomparator&>* list
    ) : iter_(list) { }

    virtual ~iterator() override { }

    // returns true iff the iterator is positioned at a valid node.
    virtual bool valid() const override {
      return iter_.valid();
    }

    // returns the key at the current position.
    // requires: valid()
    virtual const char* key() const override {
      return iter_.key();
    }

    // advances to the next position.
    // requires: valid()
    virtual void next() override {
      iter_.next();
    }

    // advances to the previous position.
    // requires: valid()
    virtual void prev() override {
      iter_.prev();
    }

    // advance to the first entry with a key >= target
    virtual void seek(const slice& user_key, const char* memtable_key)
        override {
      if (memtable_key != nullptr) {
        iter_.seek(memtable_key);
      } else {
        iter_.seek(encodekey(&tmp_, user_key));
      }
    }

    // position at the first entry in list.
    // final state of iterator is valid() iff list is not empty.
    virtual void seektofirst() override {
      iter_.seektofirst();
    }

    // position at the last entry in list.
    // final state of iterator is valid() iff list is not empty.
    virtual void seektolast() override {
      iter_.seektolast();
    }
   protected:
    std::string tmp_;       // for passing to encodekey
  };

  virtual memtablerep::iterator* getiterator(arena* arena = nullptr) override {
    if (arena == nullptr) {
      return new skiplistrep::iterator(&skip_list_);
    } else {
      auto mem = arena->allocatealigned(sizeof(skiplistrep::iterator));
      return new (mem) skiplistrep::iterator(&skip_list_);
    }
  }
};
}

memtablerep* skiplistfactory::creatememtablerep(
    const memtablerep::keycomparator& compare, arena* arena,
    const slicetransform*, logger* logger) {
  return new skiplistrep(compare, arena);
}

} // namespace rocksdb
