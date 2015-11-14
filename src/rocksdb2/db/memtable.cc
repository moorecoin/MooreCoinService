//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/memtable.h"

#include <memory>
#include <algorithm>
#include <limits>

#include "db/dbformat.h"
#include "db/merge_context.h"
#include "rocksdb/comparator.h"
#include "rocksdb/env.h"
#include "rocksdb/iterator.h"
#include "rocksdb/merge_operator.h"
#include "rocksdb/slice_transform.h"
#include "table/merger.h"
#include "util/arena.h"
#include "util/coding.h"
#include "util/murmurhash.h"
#include "util/mutexlock.h"
#include "util/perf_context_imp.h"
#include "util/statistics.h"
#include "util/stop_watch.h"

namespace rocksdb {

memtable::memtable(const internalkeycomparator& cmp, const options& options)
    : comparator_(cmp),
      refs_(0),
      karenablocksize(optimizeblocksize(options.arena_block_size)),
      kwritebuffersize(options.write_buffer_size),
      arena_(options.arena_block_size),
      table_(options.memtable_factory->creatememtablerep(
          comparator_, &arena_, options.prefix_extractor.get(),
          options.info_log.get())),
      num_entries_(0),
      flush_in_progress_(false),
      flush_completed_(false),
      file_number_(0),
      first_seqno_(0),
      mem_next_logfile_number_(0),
      locks_(options.inplace_update_support ? options.inplace_update_num_locks
                                            : 0),
      prefix_extractor_(options.prefix_extractor.get()),
      should_flush_(shouldflushnow()) {
  // if should_flush_ == true without an entry inserted, something must have
  // gone wrong already.
  assert(!should_flush_);
  if (prefix_extractor_ && options.memtable_prefix_bloom_bits > 0) {
    prefix_bloom_.reset(new dynamicbloom(
        &arena_,
        options.memtable_prefix_bloom_bits, options.bloom_locality,
        options.memtable_prefix_bloom_probes, nullptr,
        options.memtable_prefix_bloom_huge_page_tlb_size,
        options.info_log.get()));
  }
}

memtable::~memtable() {
  assert(refs_ == 0);
}

size_t memtable::approximatememoryusage() {
  size_t arena_usage = arena_.approximatememoryusage();
  size_t table_usage = table_->approximatememoryusage();
  // let max_usage =  std::numeric_limits<size_t>::max()
  // then if arena_usage + total_usage >= max_usage, return max_usage.
  // the following variation is to avoid numeric overflow.
  if (arena_usage >= std::numeric_limits<size_t>::max() - table_usage) {
    return std::numeric_limits<size_t>::max();
  }
  // otherwise, return the actual usage
  return arena_usage + table_usage;
}

bool memtable::shouldflushnow() const {
  // in a lot of times, we cannot allocate arena blocks that exactly matches the
  // buffer size. thus we have to decide if we should over-allocate or
  // under-allocate.
  // this constant avariable can be interpreted as: if we still have more than
  // "kallowoverallocationratio * karenablocksize" space left, we'd try to over
  // allocate one more block.
  const double kallowoverallocationratio = 0.6;

  // if arena still have room for new block allocation, we can safely say it
  // shouldn't flush.
  auto allocated_memory =
      table_->approximatememoryusage() + arena_.memoryallocatedbytes();

  // if we can still allocate one more block without exceeding the
  // over-allocation ratio, then we should not flush.
  if (allocated_memory + karenablocksize <
      kwritebuffersize + karenablocksize * kallowoverallocationratio) {
    return false;
  }

  // if user keeps adding entries that exceeds kwritebuffersize, we need to
  // flush earlier even though we still have much available memory left.
  if (allocated_memory >
      kwritebuffersize + karenablocksize * kallowoverallocationratio) {
    return true;
  }

  // in this code path, arena has already allocated its "last block", which
  // means the total allocatedmemory size is either:
  //  (1) "moderately" over allocated the memory (no more than `0.6 * arena
  // block size`. or,
  //  (2) the allocated memory is less than write buffer size, but we'll stop
  // here since if we allocate a new arena block, we'll over allocate too much
  // more (half of the arena block size) memory.
  //
  // in either case, to avoid over-allocate, the last block will stop allocation
  // when its usage reaches a certain ratio, which we carefully choose "0.75
  // full" as the stop condition because it addresses the following issue with
  // great simplicity: what if the next inserted entry's size is
  // bigger than allocatedandunused()?
  //
  // the answer is: if the entry size is also bigger than 0.25 *
  // karenablocksize, a dedicated block will be allocated for it; otherwise
  // arena will anyway skip the allocatedandunused() and allocate a new, empty
  // and regular block. in either case, we *overly* over-allocated.
  //
  // therefore, setting the last block to be at most "0.75 full" avoids both
  // cases.
  //
  // note: the average percentage of waste space of this approach can be counted
  // as: "arena block size * 0.25 / write buffer size". user who specify a small
  // write buffer size and/or big arena block size may suffer.
  return arena_.allocatedandunused() < karenablocksize / 4;
}

int memtable::keycomparator::operator()(const char* prefix_len_key1,
                                        const char* prefix_len_key2) const {
  // internal keys are encoded as length-prefixed strings.
  slice k1 = getlengthprefixedslice(prefix_len_key1);
  slice k2 = getlengthprefixedslice(prefix_len_key2);
  return comparator.compare(k1, k2);
}

int memtable::keycomparator::operator()(const char* prefix_len_key,
                                        const slice& key)
    const {
  // internal keys are encoded as length-prefixed strings.
  slice a = getlengthprefixedslice(prefix_len_key);
  return comparator.compare(a, key);
}

slice memtablerep::userkey(const char* key) const {
  slice slice = getlengthprefixedslice(key);
  return slice(slice.data(), slice.size() - 8);
}

keyhandle memtablerep::allocate(const size_t len, char** buf) {
  *buf = arena_->allocate(len);
  return static_cast<keyhandle>(*buf);
}

// encode a suitable internal key target for "target" and return it.
// uses *scratch as scratch space, and the returned pointer will point
// into this scratch space.
const char* encodekey(std::string* scratch, const slice& target) {
  scratch->clear();
  putvarint32(scratch, target.size());
  scratch->append(target.data(), target.size());
  return scratch->data();
}

class memtableiterator: public iterator {
 public:
  memtableiterator(
      const memtable& mem, const readoptions& options, arena* arena)
      : bloom_(nullptr),
        prefix_extractor_(mem.prefix_extractor_),
        valid_(false),
        arena_mode_(arena != nullptr) {
    if (prefix_extractor_ != nullptr && !options.total_order_seek) {
      bloom_ = mem.prefix_bloom_.get();
      iter_ = mem.table_->getdynamicprefixiterator(arena);
    } else {
      iter_ = mem.table_->getiterator(arena);
    }
  }

  ~memtableiterator() {
    if (arena_mode_) {
      iter_->~iterator();
    } else {
      delete iter_;
    }
  }

  virtual bool valid() const { return valid_; }
  virtual void seek(const slice& k) {
    if (bloom_ != nullptr &&
        !bloom_->maycontain(prefix_extractor_->transform(extractuserkey(k)))) {
      valid_ = false;
      return;
    }
    iter_->seek(k, nullptr);
    valid_ = iter_->valid();
  }
  virtual void seektofirst() {
    iter_->seektofirst();
    valid_ = iter_->valid();
  }
  virtual void seektolast() {
    iter_->seektolast();
    valid_ = iter_->valid();
  }
  virtual void next() {
    assert(valid());
    iter_->next();
    valid_ = iter_->valid();
  }
  virtual void prev() {
    assert(valid());
    iter_->prev();
    valid_ = iter_->valid();
  }
  virtual slice key() const {
    assert(valid());
    return getlengthprefixedslice(iter_->key());
  }
  virtual slice value() const {
    assert(valid());
    slice key_slice = getlengthprefixedslice(iter_->key());
    return getlengthprefixedslice(key_slice.data() + key_slice.size());
  }

  virtual status status() const { return status::ok(); }

 private:
  dynamicbloom* bloom_;
  const slicetransform* const prefix_extractor_;
  memtablerep::iterator* iter_;
  bool valid_;
  bool arena_mode_;

  // no copying allowed
  memtableiterator(const memtableiterator&);
  void operator=(const memtableiterator&);
};

iterator* memtable::newiterator(const readoptions& options, arena* arena) {
  if (arena == nullptr) {
    return new memtableiterator(*this, options, nullptr);
  } else {
    auto mem = arena->allocatealigned(sizeof(memtableiterator));
    return new (mem)
        memtableiterator(*this, options, arena);
  }
}

port::rwmutex* memtable::getlock(const slice& key) {
  static murmur_hash hash;
  return &locks_[hash(key) % locks_.size()];
}

void memtable::add(sequencenumber s, valuetype type,
                   const slice& key, /* user key */
                   const slice& value) {
  // format of an entry is concatenation of:
  //  key_size     : varint32 of internal_key.size()
  //  key bytes    : char[internal_key.size()]
  //  value_size   : varint32 of value.size()
  //  value bytes  : char[value.size()]
  size_t key_size = key.size();
  size_t val_size = value.size();
  size_t internal_key_size = key_size + 8;
  const size_t encoded_len =
      varintlength(internal_key_size) + internal_key_size +
      varintlength(val_size) + val_size;
  char* buf = nullptr;
  keyhandle handle = table_->allocate(encoded_len, &buf);
  assert(buf != nullptr);
  char* p = encodevarint32(buf, internal_key_size);
  memcpy(p, key.data(), key_size);
  p += key_size;
  encodefixed64(p, (s << 8) | type);
  p += 8;
  p = encodevarint32(p, val_size);
  memcpy(p, value.data(), val_size);
  assert((unsigned)(p + val_size - buf) == (unsigned)encoded_len);
  table_->insert(handle);
  num_entries_++;

  if (prefix_bloom_) {
    assert(prefix_extractor_);
    prefix_bloom_->add(prefix_extractor_->transform(key));
  }

  // the first sequence number inserted into the memtable
  assert(first_seqno_ == 0 || s > first_seqno_);
  if (first_seqno_ == 0) {
    first_seqno_ = s;
  }

  should_flush_ = shouldflushnow();
}

// callback from memtable::get()
namespace {

struct saver {
  status* status;
  const lookupkey* key;
  bool* found_final_value;  // is value set correctly? used by keymayexist
  bool* merge_in_progress;
  std::string* value;
  const mergeoperator* merge_operator;
  // the merge operations encountered;
  mergecontext* merge_context;
  memtable* mem;
  logger* logger;
  statistics* statistics;
  bool inplace_update_support;
};
}  // namespace

static bool savevalue(void* arg, const char* entry) {
  saver* s = reinterpret_cast<saver*>(arg);
  mergecontext* merge_context = s->merge_context;
  const mergeoperator* merge_operator = s->merge_operator;

  assert(s != nullptr && merge_context != nullptr);

  // entry format is:
  //    klength  varint32
  //    userkey  char[klength-8]
  //    tag      uint64
  //    vlength  varint32
  //    value    char[vlength]
  // check that it belongs to same user key.  we do not check the
  // sequence number since the seek() call above should have skipped
  // all entries with overly large sequence numbers.
  uint32_t key_length;
  const char* key_ptr = getvarint32ptr(entry, entry + 5, &key_length);
  if (s->mem->getinternalkeycomparator().user_comparator()->compare(
          slice(key_ptr, key_length - 8), s->key->user_key()) == 0) {
    // correct user key
    const uint64_t tag = decodefixed64(key_ptr + key_length - 8);
    switch (static_cast<valuetype>(tag & 0xff)) {
      case ktypevalue: {
        if (s->inplace_update_support) {
          s->mem->getlock(s->key->user_key())->readlock();
        }
        slice v = getlengthprefixedslice(key_ptr + key_length);
        *(s->status) = status::ok();
        if (*(s->merge_in_progress)) {
          assert(merge_operator);
          if (!merge_operator->fullmerge(s->key->user_key(), &v,
                                         merge_context->getoperands(), s->value,
                                         s->logger)) {
            recordtick(s->statistics, number_merge_failures);
            *(s->status) =
                status::corruption("error: could not perform merge.");
          }
        } else {
          s->value->assign(v.data(), v.size());
        }
        if (s->inplace_update_support) {
          s->mem->getlock(s->key->user_key())->readunlock();
        }
        *(s->found_final_value) = true;
        return false;
      }
      case ktypedeletion: {
        if (*(s->merge_in_progress)) {
          assert(merge_operator);
          *(s->status) = status::ok();
          if (!merge_operator->fullmerge(s->key->user_key(), nullptr,
                                         merge_context->getoperands(), s->value,
                                         s->logger)) {
            recordtick(s->statistics, number_merge_failures);
            *(s->status) =
                status::corruption("error: could not perform merge.");
          }
        } else {
          *(s->status) = status::notfound();
        }
        *(s->found_final_value) = true;
        return false;
      }
      case ktypemerge: {
        if (!merge_operator) {
          *(s->status) = status::invalidargument(
              "merge_operator is not properly initialized.");
          // normally we continue the loop (return true) when we see a merge
          // operand.  but in case of an error, we should stop the loop
          // immediately and pretend we have found the value to stop further
          // seek.  otherwise, the later call will override this error status.
          *(s->found_final_value) = true;
          return false;
        }
        std::string merge_result;  // temporary area for merge results later
        slice v = getlengthprefixedslice(key_ptr + key_length);
        *(s->merge_in_progress) = true;
        merge_context->pushoperand(v);
        return true;
      }
      default:
        assert(false);
        return true;
    }
  }

  // s->state could be corrupt, merge or notfound
  return false;
}

bool memtable::get(const lookupkey& key, std::string* value, status* s,
                   mergecontext& merge_context, const options& options) {
  // the sequence number is updated synchronously in version_set.h
  if (first_seqno_ == 0) {
    // avoiding recording stats for speed.
    return false;
  }
  perf_timer_guard(get_from_memtable_time);

  slice user_key = key.user_key();
  bool found_final_value = false;
  bool merge_in_progress = s->ismergeinprogress();

  if (prefix_bloom_ &&
      !prefix_bloom_->maycontain(prefix_extractor_->transform(user_key))) {
    // iter is null if prefix bloom says the key does not exist
  } else {
    saver saver;
    saver.status = s;
    saver.found_final_value = &found_final_value;
    saver.merge_in_progress = &merge_in_progress;
    saver.key = &key;
    saver.value = value;
    saver.status = s;
    saver.mem = this;
    saver.merge_context = &merge_context;
    saver.merge_operator = options.merge_operator.get();
    saver.logger = options.info_log.get();
    saver.inplace_update_support = options.inplace_update_support;
    saver.statistics = options.statistics.get();
    table_->get(key, &saver, savevalue);
  }

  // no change to value, since we have not yet found a put/delete
  if (!found_final_value && merge_in_progress) {
    *s = status::mergeinprogress("");
  }
  perf_counter_add(get_from_memtable_count, 1);
  return found_final_value;
}

void memtable::update(sequencenumber seq,
                      const slice& key,
                      const slice& value) {
  lookupkey lkey(key, seq);
  slice mem_key = lkey.memtable_key();

  std::unique_ptr<memtablerep::iterator> iter(
      table_->getdynamicprefixiterator());
  iter->seek(lkey.internal_key(), mem_key.data());

  if (iter->valid()) {
    // entry format is:
    //    key_length  varint32
    //    userkey  char[klength-8]
    //    tag      uint64
    //    vlength  varint32
    //    value    char[vlength]
    // check that it belongs to same user key.  we do not check the
    // sequence number since the seek() call above should have skipped
    // all entries with overly large sequence numbers.
    const char* entry = iter->key();
    uint32_t key_length = 0;
    const char* key_ptr = getvarint32ptr(entry, entry + 5, &key_length);
    if (comparator_.comparator.user_comparator()->compare(
        slice(key_ptr, key_length - 8), lkey.user_key()) == 0) {
      // correct user key
      const uint64_t tag = decodefixed64(key_ptr + key_length - 8);
      switch (static_cast<valuetype>(tag & 0xff)) {
        case ktypevalue: {
          slice prev_value = getlengthprefixedslice(key_ptr + key_length);
          uint32_t prev_size = prev_value.size();
          uint32_t new_size = value.size();

          // update value, if new value size  <= previous value size
          if (new_size <= prev_size ) {
            char* p = encodevarint32(const_cast<char*>(key_ptr) + key_length,
                                     new_size);
            writelock wl(getlock(lkey.user_key()));
            memcpy(p, value.data(), value.size());
            assert((unsigned)((p + value.size()) - entry) ==
                   (unsigned)(varintlength(key_length) + key_length +
                              varintlength(value.size()) + value.size()));
            return;
          }
        }
        default:
          // if the latest value is ktypedeletion, ktypemerge or ktypelogdata
          // we don't have enough space for update inplace
            add(seq, ktypevalue, key, value);
            return;
      }
    }
  }

  // key doesn't exist
  add(seq, ktypevalue, key, value);
}

bool memtable::updatecallback(sequencenumber seq,
                              const slice& key,
                              const slice& delta,
                              const options& options) {
  lookupkey lkey(key, seq);
  slice memkey = lkey.memtable_key();

  std::unique_ptr<memtablerep::iterator> iter(
      table_->getdynamicprefixiterator());
  iter->seek(lkey.internal_key(), memkey.data());

  if (iter->valid()) {
    // entry format is:
    //    key_length  varint32
    //    userkey  char[klength-8]
    //    tag      uint64
    //    vlength  varint32
    //    value    char[vlength]
    // check that it belongs to same user key.  we do not check the
    // sequence number since the seek() call above should have skipped
    // all entries with overly large sequence numbers.
    const char* entry = iter->key();
    uint32_t key_length = 0;
    const char* key_ptr = getvarint32ptr(entry, entry + 5, &key_length);
    if (comparator_.comparator.user_comparator()->compare(
        slice(key_ptr, key_length - 8), lkey.user_key()) == 0) {
      // correct user key
      const uint64_t tag = decodefixed64(key_ptr + key_length - 8);
      switch (static_cast<valuetype>(tag & 0xff)) {
        case ktypevalue: {
          slice prev_value = getlengthprefixedslice(key_ptr + key_length);
          uint32_t  prev_size = prev_value.size();

          char* prev_buffer = const_cast<char*>(prev_value.data());
          uint32_t  new_prev_size = prev_size;

          std::string str_value;
          writelock wl(getlock(lkey.user_key()));
          auto status = options.inplace_callback(prev_buffer, &new_prev_size,
                                                    delta, &str_value);
          if (status == updatestatus::updated_inplace) {
            // value already updated by callback.
            assert(new_prev_size <= prev_size);
            if (new_prev_size < prev_size) {
              // overwrite the new prev_size
              char* p = encodevarint32(const_cast<char*>(key_ptr) + key_length,
                                       new_prev_size);
              if (varintlength(new_prev_size) < varintlength(prev_size)) {
                // shift the value buffer as well.
                memcpy(p, prev_buffer, new_prev_size);
              }
            }
            recordtick(options.statistics.get(), number_keys_updated);
            should_flush_ = shouldflushnow();
            return true;
          } else if (status == updatestatus::updated) {
            add(seq, ktypevalue, key, slice(str_value));
            recordtick(options.statistics.get(), number_keys_written);
            should_flush_ = shouldflushnow();
            return true;
          } else if (status == updatestatus::update_failed) {
            // no action required. return.
            should_flush_ = shouldflushnow();
            return true;
          }
        }
        default:
          break;
      }
    }
  }
  // if the latest value is not ktypevalue
  // or key doesn't exist
  return false;
}

size_t memtable::countsuccessivemergeentries(const lookupkey& key) {
  slice memkey = key.memtable_key();

  // a total ordered iterator is costly for some memtablerep (prefix aware
  // reps). by passing in the user key, we allow efficient iterator creation.
  // the iterator only needs to be ordered within the same user key.
  std::unique_ptr<memtablerep::iterator> iter(
      table_->getdynamicprefixiterator());
  iter->seek(key.internal_key(), memkey.data());

  size_t num_successive_merges = 0;

  for (; iter->valid(); iter->next()) {
    const char* entry = iter->key();
    uint32_t key_length = 0;
    const char* iter_key_ptr = getvarint32ptr(entry, entry + 5, &key_length);
    if (comparator_.comparator.user_comparator()->compare(
            slice(iter_key_ptr, key_length - 8), key.user_key()) != 0) {
      break;
    }

    const uint64_t tag = decodefixed64(iter_key_ptr + key_length - 8);
    if (static_cast<valuetype>(tag & 0xff) != ktypemerge) {
      break;
    }

    ++num_successive_merges;
  }

  return num_successive_merges;
}

void memtablerep::get(const lookupkey& k, void* callback_args,
                      bool (*callback_func)(void* arg, const char* entry)) {
  auto iter = getdynamicprefixiterator();
  for (iter->seek(k.internal_key(), k.memtable_key().data());
       iter->valid() && callback_func(callback_args, iter->key());
       iter->next()) {
  }
}

}  // namespace rocksdb
