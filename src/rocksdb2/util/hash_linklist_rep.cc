//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//

#ifndef rocksdb_lite
#include "util/hash_linklist_rep.h"

#include <algorithm>
#include "rocksdb/memtablerep.h"
#include "util/arena.h"
#include "rocksdb/slice.h"
#include "rocksdb/slice_transform.h"
#include "port/port.h"
#include "port/atomic_pointer.h"
#include "util/histogram.h"
#include "util/murmurhash.h"
#include "db/memtable.h"
#include "db/skiplist.h"

namespace rocksdb {
namespace {

typedef const char* key;
typedef skiplist<key, const memtablerep::keycomparator&> memtableskiplist;
typedef port::atomicpointer pointer;

// a data structure used as the header of a link list of a hash bucket.
struct bucketheader {
  pointer next;
  uint32_t num_entries;

  explicit bucketheader(void* n, uint32_t count)
      : next(n), num_entries(count) {}

  bool isskiplistbucket() { return next.nobarrier_load() == this; }
};

// a data structure used as the header of a skip list of a hash bucket.
struct skiplistbucketheader {
  bucketheader counting_header;
  memtableskiplist skip_list;

  explicit skiplistbucketheader(const memtablerep::keycomparator& cmp,
                                arena* arena, uint32_t count)
      : counting_header(this,  // pointing to itself to indicate header type.
                        count),
        skip_list(cmp, arena) {}
};

struct node {
  // accessors/mutators for links.  wrapped in methods so we can
  // add the appropriate barriers as necessary.
  node* next() {
    // use an 'acquire load' so that we observe a fully initialized
    // version of the returned node.
    return reinterpret_cast<node*>(next_.acquire_load());
  }
  void setnext(node* x) {
    // use a 'release store' so that anybody who reads through this
    // pointer observes a fully initialized version of the inserted node.
    next_.release_store(x);
  }
  // no-barrier variants that can be safely used in a few locations.
  node* nobarrier_next() {
    return reinterpret_cast<node*>(next_.nobarrier_load());
  }

  void nobarrier_setnext(node* x) {
    next_.nobarrier_store(x);
  }

 private:
  port::atomicpointer next_;
 public:
  char key[0];
};

// memory structure of the mem table:
// it is a hash table, each bucket points to one entry, a linked list or a
// skip list. in order to track total number of records in a bucket to determine
// whether should switch to skip list, a header is added just to indicate
// number of entries in the bucket.
//
//
//          +-----> null    case 1. empty bucket
//          |
//          |
//          | +---> +-------+
//          | |     | next  +--> null
//          | |     +-------+
//  +-----+ | |     |       |  case 2. one entry in bucket.
//  |     +-+ |     | data  |          next pointer points to
//  +-----+   |     |       |          null. all other cases
//  |     |   |     |       |          next pointer is not null.
//  +-----+   |     +-------+
//  |     +---+
//  +-----+     +-> +-------+  +> +-------+  +-> +-------+
//  |     |     |   | next  +--+  | next  +--+   | next  +-->null
//  +-----+     |   +-------+     +-------+      +-------+
//  |     +-----+   | count |     |       |      |       |
//  +-----+         +-------+     | data  |      | data  |
//  |     |                       |       |      |       |
//  +-----+          case 3.      |       |      |       |
//  |     |          a header     +-------+      +-------+
//  +-----+          points to
//  |     |          a linked list. count indicates total number
//  +-----+          of rows in this bucket.
//  |     |
//  +-----+    +-> +-------+ <--+
//  |     |    |   | next  +----+
//  +-----+    |   +-------+   case 4. a header points to a skip
//  |     +----+   | count |           list and next pointer points to
//  +-----+        +-------+           itself, to distinguish case 3 or 4.
//  |     |        |       |           count still is kept to indicates total
//  +-----+        | skip +-->         of entries in the bucket for debugging
//  |     |        | list  |   data    purpose.
//  |     |        |      +-->
//  +-----+        |       |
//  |     |        +-------+
//  +-----+
//
// we don't have data race when changing cases because:
// (1) when changing from case 2->3, we create a new bucket header, put the
//     single node there first without changing the original node, and do a
//     release store when changing the bucket pointer. in that case, a reader
//     who sees a stale value of the bucket pointer will read this node, while
//     a reader sees the correct value because of the release store.
// (2) when changing case 3->4, a new header is created with skip list points
//     to the data, before doing an acquire store to change the bucket pointer.
//     the old header and nodes are never changed, so any reader sees any
//     of those existing pointers will guarantee to be able to iterate to the
//     end of the linked list.
// (3) header's next pointer in case 3 might change, but they are never equal
//     to itself, so no matter a reader sees any stale or newer value, it will
//     be able to correctly distinguish case 3 and 4.
//
// the reason that we use case 2 is we want to make the format to be efficient
// when the utilization of buckets is relatively low. if we use case 3 for
// single entry bucket, we will need to waste 12 bytes for every entry,
// which can be significant decrease of memory utilization.
class hashlinklistrep : public memtablerep {
 public:
  hashlinklistrep(const memtablerep::keycomparator& compare, arena* arena,
                  const slicetransform* transform, size_t bucket_size,
                  uint32_t threshold_use_skiplist, size_t huge_page_tlb_size,
                  logger* logger, int bucket_entries_logging_threshold,
                  bool if_log_bucket_dist_when_flash);

  virtual keyhandle allocate(const size_t len, char** buf) override;

  virtual void insert(keyhandle handle) override;

  virtual bool contains(const char* key) const override;

  virtual size_t approximatememoryusage() override;

  virtual void get(const lookupkey& k, void* callback_args,
                   bool (*callback_func)(void* arg,
                                         const char* entry)) override;

  virtual ~hashlinklistrep();

  virtual memtablerep::iterator* getiterator(arena* arena = nullptr) override;

  virtual memtablerep::iterator* getdynamicprefixiterator(
      arena* arena = nullptr) override;

 private:
  friend class dynamiciterator;

  size_t bucket_size_;

  // maps slices (which are transformed user keys) to buckets of keys sharing
  // the same transform.
  port::atomicpointer* buckets_;

  const uint32_t threshold_use_skiplist_;

  // the user-supplied transform whose domain is the user keys.
  const slicetransform* transform_;

  const memtablerep::keycomparator& compare_;

  logger* logger_;
  int bucket_entries_logging_threshold_;
  bool if_log_bucket_dist_when_flash_;

  bool linklistcontains(node* head, const slice& key) const;

  skiplistbucketheader* getskiplistbucketheader(pointer* first_next_pointer)
      const;

  node* getlinklistfirstnode(pointer* first_next_pointer) const;

  slice getprefix(const slice& internal_key) const {
    return transform_->transform(extractuserkey(internal_key));
  }

  size_t gethash(const slice& slice) const {
    return murmurhash(slice.data(), slice.size(), 0) % bucket_size_;
  }

  pointer* getbucket(size_t i) const {
    return static_cast<pointer*>(buckets_[i].acquire_load());
  }

  pointer* getbucket(const slice& slice) const {
    return getbucket(gethash(slice));
  }

  bool equal(const slice& a, const key& b) const {
    return (compare_(b, a) == 0);
  }

  bool equal(const key& a, const key& b) const { return (compare_(a, b) == 0); }

  bool keyisafternode(const slice& internal_key, const node* n) const {
    // nullptr n is considered infinite
    return (n != nullptr) && (compare_(n->key, internal_key) < 0);
  }

  bool keyisafternode(const key& key, const node* n) const {
    // nullptr n is considered infinite
    return (n != nullptr) && (compare_(n->key, key) < 0);
  }


  node* findgreaterorequalinbucket(node* head, const slice& key) const;

  class fulllistiterator : public memtablerep::iterator {
   public:
    explicit fulllistiterator(memtableskiplist* list, arena* arena)
        : iter_(list), full_list_(list), arena_(arena) {}

    virtual ~fulllistiterator() {
    }

    // returns true iff the iterator is positioned at a valid node.
    virtual bool valid() const {
      return iter_.valid();
    }

    // returns the key at the current position.
    // requires: valid()
    virtual const char* key() const {
      assert(valid());
      return iter_.key();
    }

    // advances to the next position.
    // requires: valid()
    virtual void next() {
      assert(valid());
      iter_.next();
    }

    // advances to the previous position.
    // requires: valid()
    virtual void prev() {
      assert(valid());
      iter_.prev();
    }

    // advance to the first entry with a key >= target
    virtual void seek(const slice& internal_key, const char* memtable_key) {
      const char* encoded_key =
          (memtable_key != nullptr) ?
              memtable_key : encodekey(&tmp_, internal_key);
      iter_.seek(encoded_key);
    }

    // position at the first entry in collection.
    // final state of iterator is valid() iff collection is not empty.
    virtual void seektofirst() {
      iter_.seektofirst();
    }

    // position at the last entry in collection.
    // final state of iterator is valid() iff collection is not empty.
    virtual void seektolast() {
      iter_.seektolast();
    }
   private:
    memtableskiplist::iterator iter_;
    // to destruct with the iterator.
    std::unique_ptr<memtableskiplist> full_list_;
    std::unique_ptr<arena> arena_;
    std::string tmp_;       // for passing to encodekey
  };

  class linklistiterator : public memtablerep::iterator {
   public:
    explicit linklistiterator(const hashlinklistrep* const hash_link_list_rep,
                              node* head)
        : hash_link_list_rep_(hash_link_list_rep),
          head_(head),
          node_(nullptr) {}

    virtual ~linklistiterator() {}

    // returns true iff the iterator is positioned at a valid node.
    virtual bool valid() const {
      return node_ != nullptr;
    }

    // returns the key at the current position.
    // requires: valid()
    virtual const char* key() const {
      assert(valid());
      return node_->key;
    }

    // advances to the next position.
    // requires: valid()
    virtual void next() {
      assert(valid());
      node_ = node_->next();
    }

    // advances to the previous position.
    // requires: valid()
    virtual void prev() {
      // prefix iterator does not support total order.
      // we simply set the iterator to invalid state
      reset(nullptr);
    }

    // advance to the first entry with a key >= target
    virtual void seek(const slice& internal_key, const char* memtable_key) {
      node_ = hash_link_list_rep_->findgreaterorequalinbucket(head_,
                                                              internal_key);
    }

    // position at the first entry in collection.
    // final state of iterator is valid() iff collection is not empty.
    virtual void seektofirst() {
      // prefix iterator does not support total order.
      // we simply set the iterator to invalid state
      reset(nullptr);
    }

    // position at the last entry in collection.
    // final state of iterator is valid() iff collection is not empty.
    virtual void seektolast() {
      // prefix iterator does not support total order.
      // we simply set the iterator to invalid state
      reset(nullptr);
    }

   protected:
    void reset(node* head) {
      head_ = head;
      node_ = nullptr;
    }
   private:
    friend class hashlinklistrep;
    const hashlinklistrep* const hash_link_list_rep_;
    node* head_;
    node* node_;

    virtual void seektohead() {
      node_ = head_;
    }
  };

  class dynamiciterator : public hashlinklistrep::linklistiterator {
   public:
    explicit dynamiciterator(hashlinklistrep& memtable_rep)
        : hashlinklistrep::linklistiterator(&memtable_rep, nullptr),
          memtable_rep_(memtable_rep) {}

    // advance to the first entry with a key >= target
    virtual void seek(const slice& k, const char* memtable_key) {
      auto transformed = memtable_rep_.getprefix(k);
      auto* bucket = memtable_rep_.getbucket(transformed);

      skiplistbucketheader* skip_list_header =
          memtable_rep_.getskiplistbucketheader(bucket);
      if (skip_list_header != nullptr) {
        // the bucket is organized as a skip list
        if (!skip_list_iter_) {
          skip_list_iter_.reset(
              new memtableskiplist::iterator(&skip_list_header->skip_list));
        } else {
          skip_list_iter_->setlist(&skip_list_header->skip_list);
        }
        if (memtable_key != nullptr) {
          skip_list_iter_->seek(memtable_key);
        } else {
          iterkey encoded_key;
          encoded_key.encodelengthprefixedkey(k);
          skip_list_iter_->seek(encoded_key.getkey().data());
        }
      } else {
        // the bucket is organized as a linked list
        skip_list_iter_.reset();
        reset(memtable_rep_.getlinklistfirstnode(bucket));
        hashlinklistrep::linklistiterator::seek(k, memtable_key);
      }
    }

    virtual bool valid() const {
      if (skip_list_iter_) {
        return skip_list_iter_->valid();
      }
      return hashlinklistrep::linklistiterator::valid();
    }

    virtual const char* key() const {
      if (skip_list_iter_) {
        return skip_list_iter_->key();
      }
      return hashlinklistrep::linklistiterator::key();
    }

    virtual void next() {
      if (skip_list_iter_) {
        skip_list_iter_->next();
      } else {
        hashlinklistrep::linklistiterator::next();
      }
    }

   private:
    // the underlying memtable
    const hashlinklistrep& memtable_rep_;
    std::unique_ptr<memtableskiplist::iterator> skip_list_iter_;
  };

  class emptyiterator : public memtablerep::iterator {
    // this is used when there wasn't a bucket. it is cheaper than
    // instantiating an empty bucket over which to iterate.
   public:
    emptyiterator() { }
    virtual bool valid() const {
      return false;
    }
    virtual const char* key() const {
      assert(false);
      return nullptr;
    }
    virtual void next() { }
    virtual void prev() { }
    virtual void seek(const slice& user_key, const char* memtable_key) { }
    virtual void seektofirst() { }
    virtual void seektolast() { }
   private:
  };
};

hashlinklistrep::hashlinklistrep(const memtablerep::keycomparator& compare,
                                 arena* arena, const slicetransform* transform,
                                 size_t bucket_size,
                                 uint32_t threshold_use_skiplist,
                                 size_t huge_page_tlb_size, logger* logger,
                                 int bucket_entries_logging_threshold,
                                 bool if_log_bucket_dist_when_flash)
    : memtablerep(arena),
      bucket_size_(bucket_size),
      // threshold to use skip list doesn't make sense if less than 3, so we
      // force it to be minimum of 3 to simplify implementation.
      threshold_use_skiplist_(std::max(threshold_use_skiplist, 3u)),
      transform_(transform),
      compare_(compare),
      logger_(logger),
      bucket_entries_logging_threshold_(bucket_entries_logging_threshold),
      if_log_bucket_dist_when_flash_(if_log_bucket_dist_when_flash) {
  char* mem = arena_->allocatealigned(sizeof(port::atomicpointer) * bucket_size,
                                      huge_page_tlb_size, logger);

  buckets_ = new (mem) port::atomicpointer[bucket_size];

  for (size_t i = 0; i < bucket_size_; ++i) {
    buckets_[i].nobarrier_store(nullptr);
  }
}

hashlinklistrep::~hashlinklistrep() {
}

keyhandle hashlinklistrep::allocate(const size_t len, char** buf) {
  char* mem = arena_->allocatealigned(sizeof(node) + len);
  node* x = new (mem) node();
  *buf = x->key;
  return static_cast<void*>(x);
}

skiplistbucketheader* hashlinklistrep::getskiplistbucketheader(
    pointer* first_next_pointer) const {
  if (first_next_pointer == nullptr) {
    return nullptr;
  }
  if (first_next_pointer->nobarrier_load() == nullptr) {
    // single entry bucket
    return nullptr;
  }
  // counting header
  bucketheader* header = reinterpret_cast<bucketheader*>(first_next_pointer);
  if (header->isskiplistbucket()) {
    assert(header->num_entries > threshold_use_skiplist_);
    auto* skip_list_bucket_header =
        reinterpret_cast<skiplistbucketheader*>(header);
    assert(skip_list_bucket_header->counting_header.next.nobarrier_load() ==
           header);
    return skip_list_bucket_header;
  }
  assert(header->num_entries <= threshold_use_skiplist_);
  return nullptr;
}

node* hashlinklistrep::getlinklistfirstnode(pointer* first_next_pointer) const {
  if (first_next_pointer == nullptr) {
    return nullptr;
  }
  if (first_next_pointer->nobarrier_load() == nullptr) {
    // single entry bucket
    return reinterpret_cast<node*>(first_next_pointer);
  }
  // counting header
  bucketheader* header = reinterpret_cast<bucketheader*>(first_next_pointer);
  if (!header->isskiplistbucket()) {
    assert(header->num_entries <= threshold_use_skiplist_);
    return reinterpret_cast<node*>(header->next.nobarrier_load());
  }
  assert(header->num_entries > threshold_use_skiplist_);
  return nullptr;
}

void hashlinklistrep::insert(keyhandle handle) {
  node* x = static_cast<node*>(handle);
  assert(!contains(x->key));
  slice internal_key = getlengthprefixedslice(x->key);
  auto transformed = getprefix(internal_key);
  auto& bucket = buckets_[gethash(transformed)];
  pointer* first_next_pointer = static_cast<pointer*>(bucket.nobarrier_load());

  if (first_next_pointer == nullptr) {
    // case 1. empty bucket
    // nobarrier_setnext() suffices since we will add a barrier when
    // we publish a pointer to "x" in prev[i].
    x->nobarrier_setnext(nullptr);
    bucket.release_store(x);
    return;
  }

  bucketheader* header = nullptr;
  if (first_next_pointer->nobarrier_load() == nullptr) {
    // case 2. only one entry in the bucket
    // need to convert to a counting bucket and turn to case 4.
    node* first = reinterpret_cast<node*>(first_next_pointer);
    // need to add a bucket header.
    // we have to first convert it to a bucket with header before inserting
    // the new node. otherwise, we might need to change next pointer of first.
    // in that case, a reader might sees the next pointer is null and wrongly
    // think the node is a bucket header.
    auto* mem = arena_->allocatealigned(sizeof(bucketheader));
    header = new (mem) bucketheader(first, 1);
    bucket.release_store(header);
  } else {
    header = reinterpret_cast<bucketheader*>(first_next_pointer);
    if (header->isskiplistbucket()) {
      // case 4. bucket is already a skip list
      assert(header->num_entries > threshold_use_skiplist_);
      auto* skip_list_bucket_header =
          reinterpret_cast<skiplistbucketheader*>(header);
      skip_list_bucket_header->counting_header.num_entries++;
      skip_list_bucket_header->skip_list.insert(x->key);
      return;
    }
  }

  if (bucket_entries_logging_threshold_ > 0 &&
      header->num_entries ==
          static_cast<uint32_t>(bucket_entries_logging_threshold_)) {
    info(logger_,
         "hashlinkedlist bucket %zu has more than %d "
         "entries. key to insert: %s",
         gethash(transformed), header->num_entries,
         getlengthprefixedslice(x->key).tostring(true).c_str());
  }

  if (header->num_entries == threshold_use_skiplist_) {
    // case 3. number of entries reaches the threshold so need to convert to
    // skip list.
    linklistiterator bucket_iter(
        this, reinterpret_cast<node*>(first_next_pointer->nobarrier_load()));
    auto mem = arena_->allocatealigned(sizeof(skiplistbucketheader));
    skiplistbucketheader* new_skip_list_header = new (mem)
        skiplistbucketheader(compare_, arena_, header->num_entries + 1);
    auto& skip_list = new_skip_list_header->skip_list;

    // add all current entries to the skip list
    for (bucket_iter.seektohead(); bucket_iter.valid(); bucket_iter.next()) {
      skip_list.insert(bucket_iter.key());
    }

    // insert the new entry
    skip_list.insert(x->key);
    // set the bucket
    bucket.release_store(new_skip_list_header);
  } else {
    // case 5. need to insert to the sorted linked list without changing the
    // header.
    node* first = reinterpret_cast<node*>(header->next.nobarrier_load());
    assert(first != nullptr);
    // advance counter unless the bucket needs to be advanced to skip list.
    // in that case, we need to make sure the previous count never exceeds
    // threshold_use_skiplist_ to avoid readers to cast to wrong format.
    header->num_entries++;

    node* cur = first;
    node* prev = nullptr;
    while (true) {
      if (cur == nullptr) {
        break;
      }
      node* next = cur->next();
      // make sure the lists are sorted.
      // if x points to head_ or next points nullptr, it is trivially satisfied.
      assert((cur == first) || (next == nullptr) ||
             keyisafternode(next->key, cur));
      if (keyisafternode(internal_key, cur)) {
        // keep searching in this list
        prev = cur;
        cur = next;
      } else {
        break;
      }
    }

    // our data structure does not allow duplicate insertion
    assert(cur == nullptr || !equal(x->key, cur->key));

    // nobarrier_setnext() suffices since we will add a barrier when
    // we publish a pointer to "x" in prev[i].
    x->nobarrier_setnext(cur);

    if (prev) {
      prev->setnext(x);
    } else {
      header->next.release_store(static_cast<void*>(x));
    }
  }
}

bool hashlinklistrep::contains(const char* key) const {
  slice internal_key = getlengthprefixedslice(key);

  auto transformed = getprefix(internal_key);
  auto bucket = getbucket(transformed);
  if (bucket == nullptr) {
    return false;
  }

  skiplistbucketheader* skip_list_header = getskiplistbucketheader(bucket);
  if (skip_list_header != nullptr) {
    return skip_list_header->skip_list.contains(key);
  } else {
    return linklistcontains(getlinklistfirstnode(bucket), internal_key);
  }
}

size_t hashlinklistrep::approximatememoryusage() {
  // memory is always allocated from the arena.
  return 0;
}

void hashlinklistrep::get(const lookupkey& k, void* callback_args,
                          bool (*callback_func)(void* arg, const char* entry)) {
  auto transformed = transform_->transform(k.user_key());
  auto bucket = getbucket(transformed);

  auto* skip_list_header = getskiplistbucketheader(bucket);
  if (skip_list_header != nullptr) {
    // is a skip list
    memtableskiplist::iterator iter(&skip_list_header->skip_list);
    for (iter.seek(k.memtable_key().data());
         iter.valid() && callback_func(callback_args, iter.key());
         iter.next()) {
    }
  } else {
    auto* link_list_head = getlinklistfirstnode(bucket);
    if (link_list_head != nullptr) {
      linklistiterator iter(this, link_list_head);
      for (iter.seek(k.internal_key(), nullptr);
           iter.valid() && callback_func(callback_args, iter.key());
           iter.next()) {
      }
    }
  }
}

memtablerep::iterator* hashlinklistrep::getiterator(arena* alloc_arena) {
  // allocate a new arena of similar size to the one currently in use
  arena* new_arena = new arena(arena_->blocksize());
  auto list = new memtableskiplist(compare_, new_arena);
  histogramimpl keys_per_bucket_hist;

  for (size_t i = 0; i < bucket_size_; ++i) {
    int count = 0;
    auto* bucket = getbucket(i);
    if (bucket != nullptr) {
      auto* skip_list_header = getskiplistbucketheader(bucket);
      if (skip_list_header != nullptr) {
        // is a skip list
        memtableskiplist::iterator itr(&skip_list_header->skip_list);
        for (itr.seektofirst(); itr.valid(); itr.next()) {
          list->insert(itr.key());
          count++;
        }
      } else {
        auto* link_list_head = getlinklistfirstnode(bucket);
        if (link_list_head != nullptr) {
          linklistiterator itr(this, link_list_head);
          for (itr.seektohead(); itr.valid(); itr.next()) {
            list->insert(itr.key());
            count++;
          }
        }
      }
    }
    if (if_log_bucket_dist_when_flash_) {
      keys_per_bucket_hist.add(count);
    }
  }
  if (if_log_bucket_dist_when_flash_ && logger_ != nullptr) {
    info(logger_, "hashlinkedlist entry distribution among buckets: %s",
         keys_per_bucket_hist.tostring().c_str());
  }

  if (alloc_arena == nullptr) {
    return new fulllistiterator(list, new_arena);
  } else {
    auto mem = alloc_arena->allocatealigned(sizeof(fulllistiterator));
    return new (mem) fulllistiterator(list, new_arena);
  }
}

memtablerep::iterator* hashlinklistrep::getdynamicprefixiterator(
    arena* alloc_arena) {
  if (alloc_arena == nullptr) {
    return new dynamiciterator(*this);
  } else {
    auto mem = alloc_arena->allocatealigned(sizeof(dynamiciterator));
    return new (mem) dynamiciterator(*this);
  }
}

bool hashlinklistrep::linklistcontains(node* head,
                                       const slice& user_key) const {
  node* x = findgreaterorequalinbucket(head, user_key);
  return (x != nullptr && equal(user_key, x->key));
}

node* hashlinklistrep::findgreaterorequalinbucket(node* head,
                                                  const slice& key) const {
  node* x = head;
  while (true) {
    if (x == nullptr) {
      return x;
    }
    node* next = x->next();
    // make sure the lists are sorted.
    // if x points to head_ or next points nullptr, it is trivially satisfied.
    assert((x == head) || (next == nullptr) || keyisafternode(next->key, x));
    if (keyisafternode(key, x)) {
      // keep searching in this list
      x = next;
    } else {
      break;
    }
  }
  return x;
}

} // anon namespace

memtablerep* hashlinklistrepfactory::creatememtablerep(
    const memtablerep::keycomparator& compare, arena* arena,
    const slicetransform* transform, logger* logger) {
  return new hashlinklistrep(compare, arena, transform, bucket_count_,
                             threshold_use_skiplist_, huge_page_tlb_size_,
                             logger, bucket_entries_logging_threshold_,
                             if_log_bucket_dist_when_flash_);
}

memtablerepfactory* newhashlinklistrepfactory(
    size_t bucket_count, size_t huge_page_tlb_size,
    int bucket_entries_logging_threshold, bool if_log_bucket_dist_when_flash,
    uint32_t threshold_use_skiplist) {
  return new hashlinklistrepfactory(
      bucket_count, threshold_use_skiplist, huge_page_tlb_size,
      bucket_entries_logging_threshold, if_log_bucket_dist_when_flash);
}

} // namespace rocksdb
#endif  // rocksdb_lite
