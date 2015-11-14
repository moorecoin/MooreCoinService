//  copyright (c) 2014, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//

#ifndef rocksdb_lite
#include "util/hash_cuckoo_rep.h"

#include <algorithm>
#include <atomic>
#include <limits>
#include <queue>
#include <string>
#include <memory>
#include <vector>

#include "rocksdb/memtablerep.h"
#include "util/murmurhash.h"
#include "db/memtable.h"
#include "db/skiplist.h"
#include "util/stl_wrappers.h"

namespace rocksdb {
namespace {

// the default maximum size of the cuckoo path searching queue
static const int kcuckoopathmaxsearchsteps = 100;

struct cuckoostep {
  static const int knullstep = -1;
  // the bucket id in the cuckoo array.
  int bucket_id_;
  // index of cuckoo-step array that points to its previous step,
  // -1 if it the beginning step.
  int prev_step_id_;
  // the depth of the current step.
  unsigned int depth_;

  cuckoostep() : bucket_id_(-1), prev_step_id_(knullstep), depth_(1) {}

  cuckoostep(cuckoostep&&) = default;
  cuckoostep& operator=(cuckoostep&&) = default;

  cuckoostep(const cuckoostep&) = delete;
  cuckoostep& operator=(const cuckoostep&) = delete;

  cuckoostep(int bucket_id, int prev_step_id, int depth)
      : bucket_id_(bucket_id), prev_step_id_(prev_step_id), depth_(depth) {}
};

class hashcuckoorep : public memtablerep {
 public:
  explicit hashcuckoorep(const memtablerep::keycomparator& compare,
                         arena* arena, const size_t bucket_count,
                         const unsigned int hash_func_count)
      : memtablerep(arena),
        compare_(compare),
        arena_(arena),
        bucket_count_(bucket_count),
        cuckoo_path_max_depth_(kdefaultcuckoopathmaxdepth),
        occupied_count_(0),
        hash_function_count_(hash_func_count),
        backup_table_(nullptr) {
    char* mem = reinterpret_cast<char*>(
        arena_->allocate(sizeof(std::atomic<const char*>) * bucket_count_));
    cuckoo_array_ = new (mem) std::atomic<const char*>[bucket_count_];
    for (unsigned int bid = 0; bid < bucket_count_; ++bid) {
      cuckoo_array_[bid].store(nullptr, std::memory_order_relaxed);
    }

    cuckoo_path_ = reinterpret_cast<int*>(
        arena_->allocate(sizeof(int*) * (cuckoo_path_max_depth_ + 1)));
    is_nearly_full_ = false;
  }

  // return false, indicating hashcuckoorep does not support merge operator.
  virtual bool ismergeoperatorsupported() const override { return false; }

  // return false, indicating hashcuckoorep does not support snapshot.
  virtual bool issnapshotsupported() const override { return false; }

  // returns true iff an entry that compares equal to key is in the collection.
  virtual bool contains(const char* internal_key) const override;

  virtual ~hashcuckoorep() override {}

  // insert the specified key (internal_key) into the mem-table.  assertion
  // fails if
  // the current mem-table already contains the specified key.
  virtual void insert(keyhandle handle) override;

  // this function returns std::numeric_limits<size_t>::max() in the following
  // three cases to disallow further write operations:
  // 1. when the fullness reaches kmaxfullnes.
  // 2. when the backup_table_ is used.
  //
  // otherwise, this function will always return 0.
  virtual size_t approximatememoryusage() override {
    if (is_nearly_full_) {
      return std::numeric_limits<size_t>::max();
    }
    return 0;
  }

  virtual void get(const lookupkey& k, void* callback_args,
                   bool (*callback_func)(void* arg,
                                         const char* entry)) override;

  class iterator : public memtablerep::iterator {
    std::shared_ptr<std::vector<const char*>> bucket_;
    typename std::vector<const char*>::const_iterator mutable cit_;
    const keycomparator& compare_;
    std::string tmp_;  // for passing to encodekey
    bool mutable sorted_;
    void dosort() const;

   public:
    explicit iterator(std::shared_ptr<std::vector<const char*>> bucket,
                      const keycomparator& compare);

    // initialize an iterator over the specified collection.
    // the returned iterator is not valid.
    // explicit iterator(const memtablerep* collection);
    virtual ~iterator() override{};

    // returns true iff the iterator is positioned at a valid node.
    virtual bool valid() const override;

    // returns the key at the current position.
    // requires: valid()
    virtual const char* key() const override;

    // advances to the next position.
    // requires: valid()
    virtual void next() override;

    // advances to the previous position.
    // requires: valid()
    virtual void prev() override;

    // advance to the first entry with a key >= target
    virtual void seek(const slice& user_key, const char* memtable_key) override;

    // position at the first entry in collection.
    // final state of iterator is valid() iff collection is not empty.
    virtual void seektofirst() override;

    // position at the last entry in collection.
    // final state of iterator is valid() iff collection is not empty.
    virtual void seektolast() override;
  };

  struct cuckoostepbuffer {
    cuckoostepbuffer() : write_index_(0), read_index_(0) {}
    ~cuckoostepbuffer() {}

    int write_index_;
    int read_index_;
    cuckoostep steps_[kcuckoopathmaxsearchsteps];

    cuckoostep& nextwritebuffer() { return steps_[write_index_++]; }

    inline const cuckoostep& readnext() { return steps_[read_index_++]; }

    inline bool hasnewwrite() { return write_index_ > read_index_; }

    inline void reset() {
      write_index_ = 0;
      read_index_ = 0;
    }

    inline bool isfull() { return write_index_ >= kcuckoopathmaxsearchsteps; }

    // returns the number of steps that has been read
    inline int readcount() { return read_index_; }

    // returns the number of steps that has been written to the buffer.
    inline int writecount() { return write_index_; }
  };

 private:
  const memtablerep::keycomparator& compare_;
  // the pointer to arena to allocate memory, immutable after construction.
  arena* const arena_;
  // the number of hash bucket in the hash table.
  const size_t bucket_count_;
  // the maxinum depth of the cuckoo path.
  const unsigned int cuckoo_path_max_depth_;
  // the current number of entries in cuckoo_array_ which has been occupied.
  size_t occupied_count_;
  // the current number of hash functions used in the cuckoo hash.
  unsigned int hash_function_count_;
  // the backup memtablerep to handle the case where cuckoo hash cannot find
  // a vacant bucket for inserting the key of a put request.
  std::shared_ptr<memtablerep> backup_table_;
  // the array to store pointers, pointing to the actual data.
  std::atomic<const char*>* cuckoo_array_;
  // a buffer to store cuckoo path
  int* cuckoo_path_;
  // a boolean flag indicating whether the fullness of bucket array
  // reaches the point to make the current memtable immutable.
  bool is_nearly_full_;

  // the default maximum depth of the cuckoo path.
  static const unsigned int kdefaultcuckoopathmaxdepth = 10;

  cuckoostepbuffer step_buffer_;

  // returns the bucket id assogied to the input slice based on the
  unsigned int gethash(const slice& slice, const int hash_func_id) const {
    // the seeds used in the murmur hash to produce different hash functions.
    static const int kmurmurhashseeds[hashcuckoorepfactory::kmaxhashcount] = {
        545609244,  1769731426, 763324157,  13099088,   592422103,
        1899789565, 248369300,  1984183468, 1613664382, 1491157517};
    return murmurhash(slice.data(), slice.size(),
                      kmurmurhashseeds[hash_func_id]) %
           bucket_count_;
  }

  // a cuckoo path is a sequence of bucket ids, where each id points to a
  // location of cuckoo_array_.  this path describes the displacement sequence
  // of entries in order to store the desired data specified by the input user
  // key.  the path starts from one of the locations associated with the
  // specified user key and ends at a vacant space in the cuckoo array. this
  // function will update the cuckoo_path.
  //
  // @return true if it found a cuckoo path.
  bool findcuckoopath(const char* internal_key, const slice& user_key,
                      int* cuckoo_path, size_t* cuckoo_path_length,
                      int initial_hash_id = 0);

  // perform quick insert by checking whether there is a vacant bucket in one
  // of the possible locations of the input key.  if so, then the function will
  // return true and the key will be stored in that vacant bucket.
  //
  // this function is a helper function of findcuckoopath that discovers the
  // first possible steps of a cuckoo path.  it begins by first computing
  // the possible locations of the input keys (and stores them in bucket_ids.)
  // then, if one of its possible locations is vacant, then the input key will
  // be stored in that vacant space and the function will return true.
  // otherwise, the function will return false indicating a complete search
  // of cuckoo-path is needed.
  bool quickinsert(const char* internal_key, const slice& user_key,
                   int bucket_ids[], const int initial_hash_id);

  // returns the pointer to the internal iterator to the buckets where buckets
  // are sorted according to the user specified keycomparator.  note that
  // any insert after this function call may affect the sorted nature of
  // the returned iterator.
  virtual memtablerep::iterator* getiterator(arena* arena) override {
    std::vector<const char*> compact_buckets;
    for (unsigned int bid = 0; bid < bucket_count_; ++bid) {
      const char* bucket = cuckoo_array_[bid].load(std::memory_order_relaxed);
      if (bucket != nullptr) {
        compact_buckets.push_back(bucket);
      }
    }
    memtablerep* backup_table = backup_table_.get();
    if (backup_table != nullptr) {
      std::unique_ptr<memtablerep::iterator> iter(backup_table->getiterator());
      for (iter->seektofirst(); iter->valid(); iter->next()) {
        compact_buckets.push_back(iter->key());
      }
    }
    if (arena == nullptr) {
      return new iterator(
          std::shared_ptr<std::vector<const char*>>(
              new std::vector<const char*>(std::move(compact_buckets))),
          compare_);
    } else {
      auto mem = arena->allocatealigned(sizeof(iterator));
      return new (mem) iterator(
          std::shared_ptr<std::vector<const char*>>(
              new std::vector<const char*>(std::move(compact_buckets))),
          compare_);
    }
  }
};

void hashcuckoorep::get(const lookupkey& key, void* callback_args,
                        bool (*callback_func)(void* arg, const char* entry)) {
  slice user_key = key.user_key();
  for (unsigned int hid = 0; hid < hash_function_count_; ++hid) {
    const char* bucket =
        cuckoo_array_[gethash(user_key, hid)].load(std::memory_order_acquire);
    if (bucket != nullptr) {
      auto bucket_user_key = userkey(bucket);
      if (user_key.compare(bucket_user_key) == 0) {
        callback_func(callback_args, bucket);
        break;
      }
    } else {
      // as put() always stores at the vacant bucket located by the
      // hash function with the smallest possible id, when we first
      // find a vacant bucket in get(), that means a miss.
      break;
    }
  }
  memtablerep* backup_table = backup_table_.get();
  if (backup_table != nullptr) {
    backup_table->get(key, callback_args, callback_func);
  }
}

void hashcuckoorep::insert(keyhandle handle) {
  static const float kmaxfullness = 0.90;

  auto* key = static_cast<char*>(handle);
  int initial_hash_id = 0;
  size_t cuckoo_path_length = 0;
  auto user_key = userkey(key);
  // find cuckoo path
  if (findcuckoopath(key, user_key, cuckoo_path_, &cuckoo_path_length,
                     initial_hash_id) == false) {
    // if true, then we can't find a vacant bucket for this key even we
    // have used up all the hash functions.  then use a backup memtable to
    // store such key, which will further make this mem-table become
    // immutable.
    if (backup_table_.get() == nullptr) {
      vectorrepfactory factory(10);
      backup_table_.reset(
          factory.creatememtablerep(compare_, arena_, nullptr, nullptr));
      is_nearly_full_ = true;
    }
    backup_table_->insert(key);
    return;
  }
  // when reaching this point, means the insert can be done successfully.
  occupied_count_++;
  if (occupied_count_ >= bucket_count_ * kmaxfullness) {
    is_nearly_full_ = true;
  }

  // perform kickout process if the length of cuckoo path > 1.
  if (cuckoo_path_length == 0) return;

  // the cuckoo path stores the kickout path in reverse order.
  // so the kickout or displacement is actually performed
  // in reverse order, which avoids false-negatives on read
  // by moving each key involved in the cuckoo path to the new
  // location before replacing it.
  for (size_t i = 1; i < cuckoo_path_length; ++i) {
    int kicked_out_bid = cuckoo_path_[i - 1];
    int current_bid = cuckoo_path_[i];
    // since we only allow one writer at a time, it is safe to do relaxed read.
    cuckoo_array_[kicked_out_bid]
        .store(cuckoo_array_[current_bid].load(std::memory_order_relaxed),
               std::memory_order_release);
  }
  int insert_key_bid = cuckoo_path_[cuckoo_path_length - 1];
  cuckoo_array_[insert_key_bid].store(key, std::memory_order_release);
}

bool hashcuckoorep::contains(const char* internal_key) const {
  auto user_key = userkey(internal_key);
  for (unsigned int hid = 0; hid < hash_function_count_; ++hid) {
    const char* stored_key =
        cuckoo_array_[gethash(user_key, hid)].load(std::memory_order_acquire);
    if (stored_key != nullptr) {
      if (compare_(internal_key, stored_key) == 0) {
        return true;
      }
    }
  }
  return false;
}

bool hashcuckoorep::quickinsert(const char* internal_key, const slice& user_key,
                                int bucket_ids[], const int initial_hash_id) {
  int cuckoo_bucket_id = -1;

  // below does the followings:
  // 0. calculate all possible locations of the input key.
  // 1. check if there is a bucket having same user_key as the input does.
  // 2. if there exists such bucket, then replace this bucket by the newly
  //    insert data and return.  this step also performs duplication check.
  // 3. if no such bucket exists but exists a vacant bucket, then insert the
  //    input data into it.
  // 4. if step 1 to 3 all fail, then return false.
  for (unsigned int hid = initial_hash_id; hid < hash_function_count_; ++hid) {
    bucket_ids[hid] = gethash(user_key, hid);
    // since only one put is allowed at a time, and this is part of the put
    // operation, so we can safely perform relaxed load.
    const char* stored_key =
        cuckoo_array_[bucket_ids[hid]].load(std::memory_order_relaxed);
    if (stored_key == nullptr) {
      if (cuckoo_bucket_id == -1) {
        cuckoo_bucket_id = bucket_ids[hid];
      }
    } else {
      const auto bucket_user_key = userkey(stored_key);
      if (bucket_user_key.compare(user_key) == 0) {
        cuckoo_bucket_id = bucket_ids[hid];
        break;
      }
    }
  }

  if (cuckoo_bucket_id != -1) {
    cuckoo_array_[cuckoo_bucket_id]
        .store(internal_key, std::memory_order_release);
    return true;
  }

  return false;
}

// perform pre-check and find the shortest cuckoo path.  a cuckoo path
// is a displacement sequence for inserting the specified input key.
//
// @return true if it successfully found a vacant space or cuckoo-path.
//     if the return value is true but the length of cuckoo_path is zero,
//     then it indicates that a vacant bucket or an bucket with matched user
//     key with the input is found, and a quick insertion is done.
bool hashcuckoorep::findcuckoopath(const char* internal_key,
                                   const slice& user_key, int* cuckoo_path,
                                   size_t* cuckoo_path_length,
                                   const int initial_hash_id) {
  int bucket_ids[hashcuckoorepfactory::kmaxhashcount];
  *cuckoo_path_length = 0;

  if (quickinsert(internal_key, user_key, bucket_ids, initial_hash_id)) {
    return true;
  }
  // if this step is reached, then it means:
  // 1. no vacant bucket in any of the possible locations of the input key.
  // 2. none of the possible locations of the input key has the same user
  //    key as the input `internal_key`.

  // the front and back indices for the step_queue_
  step_buffer_.reset();

  for (unsigned int hid = initial_hash_id; hid < hash_function_count_; ++hid) {
    /// cuckoostep& current_step = step_queue_[front_pos++];
    cuckoostep& current_step = step_buffer_.nextwritebuffer();
    current_step.bucket_id_ = bucket_ids[hid];
    current_step.prev_step_id_ = cuckoostep::knullstep;
    current_step.depth_ = 1;
  }

  while (step_buffer_.hasnewwrite()) {
    int step_id = step_buffer_.read_index_;
    const cuckoostep& step = step_buffer_.readnext();
    // since it's a bfs process, then the first step with its depth deeper
    // than the maximum allowed depth indicates all the remaining steps
    // in the step buffer queue will all exceed the maximum depth.
    // return false immediately indicating we can't find a vacant bucket
    // for the input key before the maximum allowed depth.
    if (step.depth_ >= cuckoo_path_max_depth_) {
      return false;
    }
    // again, we can perform no barrier load safely here as the current
    // thread is the only writer.
    auto bucket_user_key =
        userkey(cuckoo_array_[step.bucket_id_].load(std::memory_order_relaxed));
    if (step.prev_step_id_ != cuckoostep::knullstep) {
      if (bucket_user_key.compare(user_key) == 0) {
        // then there is a loop in the current path, stop discovering this path.
        continue;
      }
    }
    // if the current bucket stores at its nth location, then we only consider
    // its mth location where m > n.  this property makes sure that all reads
    // will not miss if we do have data associated to the query key.
    //
    // the n and m in the above statement is the start_hid and hid in the code.
    unsigned int start_hid = hash_function_count_;
    for (unsigned int hid = 0; hid < hash_function_count_; ++hid) {
      bucket_ids[hid] = gethash(bucket_user_key, hid);
      if (step.bucket_id_ == bucket_ids[hid]) {
        start_hid = hid;
      }
    }
    // must found a bucket which is its current "home".
    assert(start_hid != hash_function_count_);

    // explore all possible next steps from the current step.
    for (unsigned int hid = start_hid + 1; hid < hash_function_count_; ++hid) {
      cuckoostep& next_step = step_buffer_.nextwritebuffer();
      next_step.bucket_id_ = bucket_ids[hid];
      next_step.prev_step_id_ = step_id;
      next_step.depth_ = step.depth_ + 1;
      // once a vacant bucket is found, trace back all its previous steps
      // to generate a cuckoo path.
      if (cuckoo_array_[next_step.bucket_id_].load(std::memory_order_relaxed) ==
          nullptr) {
        // store the last step in the cuckoo path.  note that cuckoo_path
        // stores steps in reverse order.  this allows us to move keys along
        // the cuckoo path by storing each key to the new place first before
        // removing it from the old place.  this property ensures reads will
        // not missed due to moving keys along the cuckoo path.
        cuckoo_path[(*cuckoo_path_length)++] = next_step.bucket_id_;
        int depth;
        for (depth = step.depth_; depth > 0 && step_id != cuckoostep::knullstep;
             depth--) {
          const cuckoostep& prev_step = step_buffer_.steps_[step_id];
          cuckoo_path[(*cuckoo_path_length)++] = prev_step.bucket_id_;
          step_id = prev_step.prev_step_id_;
        }
        assert(depth == 0 && step_id == cuckoostep::knullstep);
        return true;
      }
      if (step_buffer_.isfull()) {
        // if true, then it reaches maxinum number of cuckoo search steps.
        return false;
      }
    }
  }

  // tried all possible paths but still not unable to find a cuckoo path
  // which path leads to a vacant bucket.
  return false;
}

hashcuckoorep::iterator::iterator(
    std::shared_ptr<std::vector<const char*>> bucket,
    const keycomparator& compare)
    : bucket_(bucket),
      cit_(bucket_->end()),
      compare_(compare),
      sorted_(false) {}

void hashcuckoorep::iterator::dosort() const {
  if (!sorted_) {
    std::sort(bucket_->begin(), bucket_->end(),
              stl_wrappers::compare(compare_));
    cit_ = bucket_->begin();
    sorted_ = true;
  }
}

// returns true iff the iterator is positioned at a valid node.
bool hashcuckoorep::iterator::valid() const {
  dosort();
  return cit_ != bucket_->end();
}

// returns the key at the current position.
// requires: valid()
const char* hashcuckoorep::iterator::key() const {
  assert(valid());
  return *cit_;
}

// advances to the next position.
// requires: valid()
void hashcuckoorep::iterator::next() {
  assert(valid());
  if (cit_ == bucket_->end()) {
    return;
  }
  ++cit_;
}

// advances to the previous position.
// requires: valid()
void hashcuckoorep::iterator::prev() {
  assert(valid());
  if (cit_ == bucket_->begin()) {
    // if you try to go back from the first element, the iterator should be
    // invalidated. so we set it to past-the-end. this means that you can
    // treat the container circularly.
    cit_ = bucket_->end();
  } else {
    --cit_;
  }
}

// advance to the first entry with a key >= target
void hashcuckoorep::iterator::seek(const slice& user_key,
                                   const char* memtable_key) {
  dosort();
  // do binary search to find first value not less than the target
  const char* encoded_key =
      (memtable_key != nullptr) ? memtable_key : encodekey(&tmp_, user_key);
  cit_ = std::equal_range(bucket_->begin(), bucket_->end(), encoded_key,
                          [this](const char* a, const char* b) {
                            return compare_(a, b) < 0;
                          }).first;
}

// position at the first entry in collection.
// final state of iterator is valid() iff collection is not empty.
void hashcuckoorep::iterator::seektofirst() {
  dosort();
  cit_ = bucket_->begin();
}

// position at the last entry in collection.
// final state of iterator is valid() iff collection is not empty.
void hashcuckoorep::iterator::seektolast() {
  dosort();
  cit_ = bucket_->end();
  if (bucket_->size() != 0) {
    --cit_;
  }
}

}  // anom namespace

memtablerep* hashcuckoorepfactory::creatememtablerep(
    const memtablerep::keycomparator& compare, arena* arena,
    const slicetransform* transform, logger* logger) {
  // the estimated average fullness.  the write performance of any close hash
  // degrades as the fullness of the mem-table increases.  setting kfullness
  // to a value around 0.7 can better avoid write performance degradation while
  // keeping efficient memory usage.
  static const float kfullness = 0.7;
  size_t pointer_size = sizeof(std::atomic<const char*>);
  assert(write_buffer_size_ >= (average_data_size_ + pointer_size));
  size_t bucket_count =
      (write_buffer_size_ / (average_data_size_ + pointer_size)) / kfullness +
      1;
  unsigned int hash_function_count = hash_function_count_;
  if (hash_function_count < 2) {
    hash_function_count = 2;
  }
  if (hash_function_count > kmaxhashcount) {
    hash_function_count = kmaxhashcount;
  }
  return new hashcuckoorep(compare, arena, bucket_count, hash_function_count);
}

memtablerepfactory* newhashcuckoorepfactory(size_t write_buffer_size,
                                            size_t average_data_size,
                                            unsigned int hash_function_count) {
  return new hashcuckoorepfactory(write_buffer_size, average_data_size,
                                  hash_function_count);
}

}  // namespace rocksdb
#endif  // rocksdb_lite
