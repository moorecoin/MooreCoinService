//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/db_iter.h"
#include <stdexcept>
#include <deque>
#include <string>
#include <limits>

#include "db/filename.h"
#include "db/dbformat.h"
#include "rocksdb/env.h"
#include "rocksdb/options.h"
#include "rocksdb/iterator.h"
#include "rocksdb/merge_operator.h"
#include "port/port.h"
#include "util/arena.h"
#include "util/logging.h"
#include "util/mutexlock.h"
#include "util/perf_context_imp.h"

namespace rocksdb {

#if 0
static void dumpinternaliter(iterator* iter) {
  for (iter->seektofirst(); iter->valid(); iter->next()) {
    parsedinternalkey k;
    if (!parseinternalkey(iter->key(), &k)) {
      fprintf(stderr, "corrupt '%s'\n", escapestring(iter->key()).c_str());
    } else {
      fprintf(stderr, "@ '%s'\n", k.debugstring().c_str());
    }
  }
}
#endif

// memtables and sstables that make the db representation contain
// (userkey,seq,type) => uservalue entries.  dbiter
// combines multiple entries for the same userkey found in the db
// representation into a single entry while accounting for sequence
// numbers, deletion markers, overwrites, etc.
class dbiter: public iterator {
 public:
  // the following is grossly complicated. todo: clean it up
  // which direction is the iterator currently moving?
  // (1) when moving forward, the internal iterator is positioned at
  //     the exact entry that yields this->key(), this->value()
  // (2) when moving backwards, the internal iterator is positioned
  //     just before all entries whose user key == this->key().
  enum direction {
    kforward,
    kreverse
  };

  dbiter(env* env, const options& options, const comparator* cmp,
         iterator* iter, sequencenumber s, bool arena_mode)
      : arena_mode_(arena_mode),
        env_(env),
        logger_(options.info_log.get()),
        user_comparator_(cmp),
        user_merge_operator_(options.merge_operator.get()),
        iter_(iter),
        sequence_(s),
        direction_(kforward),
        valid_(false),
        current_entry_is_merged_(false),
        statistics_(options.statistics.get()) {
    recordtick(statistics_, no_iterators);
    has_prefix_extractor_ = (options.prefix_extractor.get() != nullptr);
    max_skip_ = options.max_sequential_skip_in_iterations;
  }
  virtual ~dbiter() {
    recordtick(statistics_, no_iterators, -1);
    if (!arena_mode_) {
      delete iter_;
    } else {
      iter_->~iterator();
    }
  }
  virtual void setiter(iterator* iter) {
    assert(iter_ == nullptr);
    iter_ = iter;
  }
  virtual bool valid() const { return valid_; }
  virtual slice key() const {
    assert(valid_);
    return saved_key_.getkey();
  }
  virtual slice value() const {
    assert(valid_);
    return (direction_ == kforward && !current_entry_is_merged_) ?
      iter_->value() : saved_value_;
  }
  virtual status status() const {
    if (status_.ok()) {
      return iter_->status();
    } else {
      return status_;
    }
  }

  virtual void next();
  virtual void prev();
  virtual void seek(const slice& target);
  virtual void seektofirst();
  virtual void seektolast();

 private:
  void previnternal();
  void findparseablekey(parsedinternalkey* ikey, direction direction);
  bool findvalueforcurrentkey();
  bool findvalueforcurrentkeyusingseek();
  void findprevuserkey();
  void findnextuserkey();
  inline void findnextuserentry(bool skipping);
  void findnextuserentryinternal(bool skipping);
  bool parsekey(parsedinternalkey* key);
  void mergevaluesnewtoold();

  inline void clearsavedvalue() {
    if (saved_value_.capacity() > 1048576) {
      std::string empty;
      swap(empty, saved_value_);
    } else {
      saved_value_.clear();
    }
  }

  bool has_prefix_extractor_;
  bool arena_mode_;
  env* const env_;
  logger* logger_;
  const comparator* const user_comparator_;
  const mergeoperator* const user_merge_operator_;
  iterator* iter_;
  sequencenumber const sequence_;

  status status_;
  iterkey saved_key_;
  std::string saved_value_;
  direction direction_;
  bool valid_;
  bool current_entry_is_merged_;
  statistics* statistics_;
  uint64_t max_skip_;

  // no copying allowed
  dbiter(const dbiter&);
  void operator=(const dbiter&);
};

inline bool dbiter::parsekey(parsedinternalkey* ikey) {
  if (!parseinternalkey(iter_->key(), ikey)) {
    status_ = status::corruption("corrupted internal key in dbiter");
    log(logger_, "corrupted internal key in dbiter: %s",
        iter_->key().tostring(true).c_str());
    return false;
  } else {
    return true;
  }
}

void dbiter::next() {
  assert(valid_);

  if (direction_ == kreverse) {
    findnextuserkey();
    direction_ = kforward;
    if (!iter_->valid()) {
      iter_->seektofirst();
    }
  }

  // if the current value is merged, we might already hit end of iter_
  if (!iter_->valid()) {
    valid_ = false;
    return;
  }
  findnextuserentry(true /* skipping the current user key */);
}

// pre: saved_key_ has the current user key if skipping
// post: saved_key_ should have the next user key if valid_,
//       if the current entry is a result of merge
//           current_entry_is_merged_ => true
//           saved_value_             => the merged value
//
// note: in between, saved_key_ can point to a user key that has
//       a delete marker
inline void dbiter::findnextuserentry(bool skipping) {
  perf_timer_guard(find_next_user_entry_time);
  findnextuserentryinternal(skipping);
}

// actual implementation of dbiter::findnextuserentry()
void dbiter::findnextuserentryinternal(bool skipping) {
  // loop until we hit an acceptable entry to yield
  assert(iter_->valid());
  assert(direction_ == kforward);
  current_entry_is_merged_ = false;
  uint64_t num_skipped = 0;
  do {
    parsedinternalkey ikey;
    if (parsekey(&ikey) && ikey.sequence <= sequence_) {
      if (skipping &&
          user_comparator_->compare(ikey.user_key, saved_key_.getkey()) <= 0) {
        num_skipped++; // skip this entry
        perf_counter_add(internal_key_skipped_count, 1);
      } else {
        skipping = false;
        switch (ikey.type) {
          case ktypedeletion:
            // arrange to skip all upcoming entries for this key since
            // they are hidden by this deletion.
            saved_key_.setkey(ikey.user_key);
            skipping = true;
            num_skipped = 0;
            perf_counter_add(internal_delete_skipped_count, 1);
            break;
          case ktypevalue:
            valid_ = true;
            saved_key_.setkey(ikey.user_key);
            return;
          case ktypemerge:
            // by now, we are sure the current ikey is going to yield a value
            saved_key_.setkey(ikey.user_key);
            current_entry_is_merged_ = true;
            valid_ = true;
            mergevaluesnewtoold();  // go to a different state machine
            return;
          default:
            assert(false);
            break;
        }
      }
    }
    // if we have sequentially iterated via numerous keys and still not
    // found the next user-key, then it is better to seek so that we can
    // avoid too many key comparisons. we seek to the last occurence of
    // our current key by looking for sequence number 0.
    if (skipping && num_skipped > max_skip_) {
      num_skipped = 0;
      std::string last_key;
      appendinternalkey(&last_key, parsedinternalkey(saved_key_.getkey(), 0,
                                                     kvaluetypeforseek));
      iter_->seek(last_key);
      recordtick(statistics_, number_of_reseeks_in_iteration);
    } else {
      iter_->next();
    }
  } while (iter_->valid());
  valid_ = false;
}

// merge values of the same user key starting from the current iter_ position
// scan from the newer entries to older entries.
// pre: iter_->key() points to the first merge type entry
//      saved_key_ stores the user key
// post: saved_value_ has the merged value for the user key
//       iter_ points to the next entry (or invalid)
void dbiter::mergevaluesnewtoold() {
  if (!user_merge_operator_) {
    log(logger_, "options::merge_operator is null.");
    throw std::logic_error("dbiter::mergevaluesnewtoold() with"
                           " options::merge_operator null");
  }

  // start the merge process by pushing the first operand
  std::deque<std::string> operands;
  operands.push_front(iter_->value().tostring());

  std::string merge_result;   // temporary string to hold merge result later
  parsedinternalkey ikey;
  for (iter_->next(); iter_->valid(); iter_->next()) {
    if (!parsekey(&ikey)) {
      // skip corrupted key
      continue;
    }

    if (user_comparator_->compare(ikey.user_key, saved_key_.getkey()) != 0) {
      // hit the next user key, stop right here
      break;
    }

    if (ktypedeletion == ikey.type) {
      // hit a delete with the same user key, stop right here
      // iter_ is positioned after delete
      iter_->next();
      break;
    }

    if (ktypevalue == ikey.type) {
      // hit a put, merge the put value with operands and store the
      // final result in saved_value_. we are done!
      // ignore corruption if there is any.
      const slice value = iter_->value();
      user_merge_operator_->fullmerge(ikey.user_key, &value, operands,
                                      &saved_value_, logger_);
      // iter_ is positioned after put
      iter_->next();
      return;
    }

    if (ktypemerge == ikey.type) {
      // hit a merge, add the value as an operand and run associative merge.
      // when complete, add result to operands and continue.
      const slice& value = iter_->value();
      operands.push_front(value.tostring());
    }
  }

  // we either exhausted all internal keys under this user key, or hit
  // a deletion marker.
  // feed null as the existing value to the merge operator, such that
  // client can differentiate this scenario and do things accordingly.
  user_merge_operator_->fullmerge(saved_key_.getkey(), nullptr, operands,
                                  &saved_value_, logger_);
}

void dbiter::prev() {
  assert(valid_);
  if (direction_ == kforward) {
    findprevuserkey();
    direction_ = kreverse;
  }
  previnternal();
}

void dbiter::previnternal() {
  if (!iter_->valid()) {
    valid_ = false;
    return;
  }

  parsedinternalkey ikey;

  while (iter_->valid()) {
    saved_key_.setkey(extractuserkey(iter_->key()));
    if (findvalueforcurrentkey()) {
      valid_ = true;
      if (!iter_->valid()) {
        return;
      }
      findparseablekey(&ikey, kreverse);
      if (user_comparator_->compare(ikey.user_key, saved_key_.getkey()) == 0) {
        findprevuserkey();
      }
      return;
    }
    if (!iter_->valid()) {
      break;
    }
    findparseablekey(&ikey, kreverse);
    if (user_comparator_->compare(ikey.user_key, saved_key_.getkey()) == 0) {

      findprevuserkey();
    }
  }
  // we haven't found any key - iterator is not valid
  assert(!iter_->valid());
  valid_ = false;
}

// this function checks, if the entry with biggest sequence_number <= sequence_
// is non ktypedeletion. if it's not, we save value in saved_value_
bool dbiter::findvalueforcurrentkey() {
  assert(iter_->valid());
  // contains operands for merge operator.
  std::deque<std::string> operands;
  // last entry before merge (could be ktypedeletion or ktypevalue)
  valuetype last_not_merge_type = ktypedeletion;
  valuetype last_key_entry_type = ktypedeletion;

  parsedinternalkey ikey;
  findparseablekey(&ikey, kreverse);

  size_t num_skipped = 0;
  while (iter_->valid() && ikey.sequence <= sequence_ &&
         (user_comparator_->compare(ikey.user_key, saved_key_.getkey()) == 0)) {
    // we iterate too much: let's use seek() to avoid too much key comparisons
    if (num_skipped >= max_skip_) {
      return findvalueforcurrentkeyusingseek();
    }

    last_key_entry_type = ikey.type;
    switch (last_key_entry_type) {
      case ktypevalue:
        operands.clear();
        saved_value_ = iter_->value().tostring();
        last_not_merge_type = ktypevalue;
        break;
      case ktypedeletion:
        operands.clear();
        last_not_merge_type = ktypedeletion;
        break;
      case ktypemerge:
        assert(user_merge_operator_ != nullptr);
        operands.push_back(iter_->value().tostring());
        break;
      default:
        assert(false);
    }

    assert(user_comparator_->compare(ikey.user_key, saved_key_.getkey()) == 0);
    iter_->prev();
    ++num_skipped;
    findparseablekey(&ikey, kreverse);
  }

  switch (last_key_entry_type) {
    case ktypedeletion:
      valid_ = false;
      return false;
    case ktypemerge:
      if (last_not_merge_type == ktypedeletion) {
        user_merge_operator_->fullmerge(saved_key_.getkey(), nullptr, operands,
                                        &saved_value_, logger_);
      } else {
        assert(last_not_merge_type == ktypevalue);
        std::string last_put_value = saved_value_;
        slice temp_slice(last_put_value);
        user_merge_operator_->fullmerge(saved_key_.getkey(), &temp_slice,
                                        operands, &saved_value_, logger_);
      }
      break;
    case ktypevalue:
      // do nothing - we've already has value in saved_value_
      break;
    default:
      assert(false);
      break;
  }
  valid_ = true;
  return true;
}

// this function is used in findvalueforcurrentkey.
// we use seek() function instead of prev() to find necessary value
bool dbiter::findvalueforcurrentkeyusingseek() {
  std::string last_key;
  appendinternalkey(&last_key, parsedinternalkey(saved_key_.getkey(), sequence_,
                                                 kvaluetypeforseek));
  iter_->seek(last_key);
  recordtick(statistics_, number_of_reseeks_in_iteration);

  // assume there is at least one parseable key for this user key
  parsedinternalkey ikey;
  findparseablekey(&ikey, kforward);

  if (ikey.type == ktypevalue || ikey.type == ktypedeletion) {
    if (ikey.type == ktypevalue) {
      saved_value_ = iter_->value().tostring();
      valid_ = true;
      return true;
    }
    valid_ = false;
    return false;
  }

  // ktypemerge. we need to collect all ktypemerge values and save them
  // in operands
  std::deque<std::string> operands;
  while (iter_->valid() &&
         (user_comparator_->compare(ikey.user_key, saved_key_.getkey()) == 0) &&
         ikey.type == ktypemerge) {
    operands.push_front(iter_->value().tostring());
    iter_->next();
    findparseablekey(&ikey, kforward);
  }

  if (!iter_->valid() ||
      (user_comparator_->compare(ikey.user_key, saved_key_.getkey()) != 0) ||
      ikey.type == ktypedeletion) {
    user_merge_operator_->fullmerge(saved_key_.getkey(), nullptr, operands,
                                    &saved_value_, logger_);

    // make iter_ valid and point to saved_key_
    if (!iter_->valid() ||
        (user_comparator_->compare(ikey.user_key, saved_key_.getkey()) != 0)) {
      iter_->seek(last_key);
      recordtick(statistics_, number_of_reseeks_in_iteration);
    }
    valid_ = true;
    return true;
  }

  const slice& value = iter_->value();
  user_merge_operator_->fullmerge(saved_key_.getkey(), &value, operands,
                                  &saved_value_, logger_);
  valid_ = true;
  return true;
}

// used in next to change directions
// go to next user key
// don't use seek(),
// because next user key will be very close
void dbiter::findnextuserkey() {
  if (!iter_->valid()) {
    return;
  }
  parsedinternalkey ikey;
  findparseablekey(&ikey, kforward);
  while (iter_->valid() &&
         user_comparator_->compare(ikey.user_key, saved_key_.getkey()) != 0) {
    iter_->next();
    findparseablekey(&ikey, kforward);
  }
}

// go to previous user_key
void dbiter::findprevuserkey() {
  if (!iter_->valid()) {
    return;
  }
  size_t num_skipped = 0;
  parsedinternalkey ikey;
  findparseablekey(&ikey, kreverse);
  while (iter_->valid() &&
         user_comparator_->compare(ikey.user_key, saved_key_.getkey()) == 0) {
    if (num_skipped >= max_skip_) {
      num_skipped = 0;
      iterkey last_key;
      last_key.setinternalkey(parsedinternalkey(
          saved_key_.getkey(), kmaxsequencenumber, kvaluetypeforseek));
      iter_->seek(last_key.getkey());
      recordtick(statistics_, number_of_reseeks_in_iteration);
    }

    iter_->prev();
    ++num_skipped;
    findparseablekey(&ikey, kreverse);
  }
}

// skip all unparseable keys
void dbiter::findparseablekey(parsedinternalkey* ikey, direction direction) {
  while (iter_->valid() && !parsekey(ikey)) {
    if (direction == kreverse) {
      iter_->prev();
    } else {
      iter_->next();
    }
  }
}

void dbiter::seek(const slice& target) {
  stopwatch sw(env_, statistics_, db_seek);

  saved_key_.clear();
  // now savved_key is used to store internal key.
  saved_key_.setinternalkey(target, sequence_);

  {
    perf_timer_guard(seek_internal_seek_time);
    iter_->seek(saved_key_.getkey());
  }

  if (iter_->valid()) {
    direction_ = kforward;
    clearsavedvalue();
    findnextuserentry(false /*not skipping */);
  } else {
    valid_ = false;
  }
}

void dbiter::seektofirst() {
  // don't use iter_::seek() if we set a prefix extractor
  // because prefix seek wiil be used.
  if (has_prefix_extractor_) {
    max_skip_ = std::numeric_limits<uint64_t>::max();
  }
  direction_ = kforward;
  clearsavedvalue();

  {
    perf_timer_guard(seek_internal_seek_time);
    iter_->seektofirst();
  }

  if (iter_->valid()) {
    findnextuserentry(false /* not skipping */);
  } else {
    valid_ = false;
  }
}

void dbiter::seektolast() {
  // don't use iter_::seek() if we set a prefix extractor
  // because prefix seek wiil be used.
  if (has_prefix_extractor_) {
    max_skip_ = std::numeric_limits<uint64_t>::max();
  }
  direction_ = kreverse;
  clearsavedvalue();

  {
    perf_timer_guard(seek_internal_seek_time);
    iter_->seektolast();
  }

  previnternal();
}

iterator* newdbiterator(env* env, const options& options,
                        const comparator* user_key_comparator,
                        iterator* internal_iter,
                        const sequencenumber& sequence) {
  return new dbiter(env, options, user_key_comparator, internal_iter, sequence,
                    false);
}

arenawrappeddbiter::~arenawrappeddbiter() { db_iter_->~dbiter(); }

void arenawrappeddbiter::setdbiter(dbiter* iter) { db_iter_ = iter; }

void arenawrappeddbiter::setiterunderdbiter(iterator* iter) {
  static_cast<dbiter*>(db_iter_)->setiter(iter);
}

inline bool arenawrappeddbiter::valid() const { return db_iter_->valid(); }
inline void arenawrappeddbiter::seektofirst() { db_iter_->seektofirst(); }
inline void arenawrappeddbiter::seektolast() { db_iter_->seektolast(); }
inline void arenawrappeddbiter::seek(const slice& target) {
  db_iter_->seek(target);
}
inline void arenawrappeddbiter::next() { db_iter_->next(); }
inline void arenawrappeddbiter::prev() { db_iter_->prev(); }
inline slice arenawrappeddbiter::key() const { return db_iter_->key(); }
inline slice arenawrappeddbiter::value() const { return db_iter_->value(); }
inline status arenawrappeddbiter::status() const { return db_iter_->status(); }
void arenawrappeddbiter::registercleanup(cleanupfunction function, void* arg1,
                                         void* arg2) {
  db_iter_->registercleanup(function, arg1, arg2);
}

arenawrappeddbiter* newarenawrappeddbiterator(
    env* env, const options& options, const comparator* user_key_comparator,
    const sequencenumber& sequence) {
  arenawrappeddbiter* iter = new arenawrappeddbiter();
  arena* arena = iter->getarena();
  auto mem = arena->allocatealigned(sizeof(dbiter));
  dbiter* db_iter = new (mem)
      dbiter(env, options, user_key_comparator, nullptr, sequence, true);
  iter->setdbiter(db_iter);
  return iter;
}

}  // namespace rocksdb
