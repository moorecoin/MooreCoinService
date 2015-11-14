//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#include "merge_helper.h"
#include "db/dbformat.h"
#include "rocksdb/comparator.h"
#include "rocksdb/db.h"
#include "rocksdb/merge_operator.h"
#include "util/statistics.h"
#include <string>
#include <stdio.h>

namespace rocksdb {

// pre:  iter points to the first merge type entry
// post: iter points to the first entry beyond the merge process (or the end)
//       keys_, operands_ are updated to reflect the merge result.
//       keys_ stores the list of keys encountered while merging.
//       operands_ stores the list of merge operands encountered while merging.
//       keys_[i] corresponds to operands_[i] for each i.
void mergehelper::mergeuntil(iterator* iter, sequencenumber stop_before,
                             bool at_bottom, statistics* stats, int* steps) {
  // get a copy of the internal key, before it's invalidated by iter->next()
  // also maintain the list of merge operands seen.
  assert(hasoperator());
  keys_.clear();
  operands_.clear();
  keys_.push_front(iter->key().tostring());
  operands_.push_front(iter->value().tostring());
  assert(user_merge_operator_);

  success_ = false;   // will become true if we hit put/delete or bottom

  // we need to parse the internal key again as the parsed key is
  // backed by the internal key!
  // assume no internal key corruption as it has been successfully parsed
  // by the caller.
  // invariant: keys_.back() will not change. hence, orig_ikey is always valid.
  parsedinternalkey orig_ikey;
  parseinternalkey(keys_.back(), &orig_ikey);

  bool hit_the_next_user_key = false;
  std::string merge_result;  // temporary value for merge results
  if (steps) {
    ++(*steps);
  }
  for (iter->next(); iter->valid(); iter->next()) {
    parsedinternalkey ikey;
    assert(operands_.size() >= 1);        // should be invariants!
    assert(keys_.size() == operands_.size());

    if (!parseinternalkey(iter->key(), &ikey)) {
      // stop at corrupted key
      if (assert_valid_internal_key_) {
        assert(!"corrupted internal key is not expected");
      }
      break;
    }

    if (user_comparator_->compare(ikey.user_key, orig_ikey.user_key) != 0) {
      // hit a different user key, stop right here
      hit_the_next_user_key = true;
      break;
    }

    if (stop_before && ikey.sequence <= stop_before) {
      // hit an entry that's visible by the previous snapshot, can't touch that
      break;
    }

    // at this point we are guaranteed that we need to process this key.

    if (ktypedeletion == ikey.type) {
      // hit a delete
      //   => merge nullptr with operands_
      //   => store result in operands_.back() (and update keys_.back())
      //   => change the entry type to ktypevalue for keys_.back()
      // we are done! return a success if the merge passes.
      success_ = user_merge_operator_->fullmerge(ikey.user_key, nullptr,
                                                 operands_, &merge_result,
                                                 logger_);

      // we store the result in keys_.back() and operands_.back()
      // if nothing went wrong (i.e.: no operand corruption on disk)
      if (success_) {
        std::string& key = keys_.back();  // the original key encountered
        orig_ikey.type = ktypevalue;
        updateinternalkey(&key[0], key.size(),
                          orig_ikey.sequence, orig_ikey.type);
        swap(operands_.back(), merge_result);
      } else {
        recordtick(stats, number_merge_failures);
      }

      // move iter to the next entry (before doing anything else)
      iter->next();
      if (steps) {
        ++(*steps);
      }
      return;
    }

    if (ktypevalue == ikey.type) {
      // hit a put
      //   => merge the put value with operands_
      //   => store result in operands_.back() (and update keys_.back())
      //   => change the entry type to ktypevalue for keys_.back()
      // we are done! success!
      const slice value = iter->value();
      success_ = user_merge_operator_->fullmerge(ikey.user_key, &value,
                                                 operands_, &merge_result,
                                                 logger_);

      // we store the result in keys_.back() and operands_.back()
      // if nothing went wrong (i.e.: no operand corruption on disk)
      if (success_) {
        std::string& key = keys_.back();  // the original key encountered
        orig_ikey.type = ktypevalue;
        updateinternalkey(&key[0], key.size(),
                          orig_ikey.sequence, orig_ikey.type);
        swap(operands_.back(), merge_result);
      } else {
        recordtick(stats, number_merge_failures);
      }

      // move iter to the next entry
      iter->next();
      if (steps) {
        ++(*steps);
      }
      return;
    }

    if (ktypemerge == ikey.type) {
      // hit a merge
      //   => merge the operand into the front of the operands_ list
      //   => use the user's associative merge function to determine how.
      //   => then continue because we haven't yet seen a put/delete.
      assert(!operands_.empty()); // should have at least one element in it

      // keep queuing keys and operands until we either meet a put / delete
      // request or later did a partial merge.
      keys_.push_front(iter->key().tostring());
      operands_.push_front(iter->value().tostring());
      if (steps) {
        ++(*steps);
      }
    }
  }

  // we are sure we have seen this key's entire history if we are at the
  // last level and exhausted all internal keys of this user key.
  // note: !iter->valid() does not necessarily mean we hit the
  // beginning of a user key, as versions of a user key might be
  // split into multiple files (even files on the same level)
  // and some files might not be included in the compaction/merge.
  //
  // there are also cases where we have seen the root of history of this
  // key without being sure of it. then, we simply miss the opportunity
  // to combine the keys. since versionset::setupotherinputs() always makes
  // sure that all merge-operands on the same level get compacted together,
  // this will simply lead to these merge operands moving to the next level.
  //
  // so, we only perform the following logic (to merge all operands together
  // without a put/delete) if we are certain that we have seen the end of key.
  bool surely_seen_the_beginning = hit_the_next_user_key && at_bottom;
  if (surely_seen_the_beginning) {
    // do a final merge with nullptr as the existing value and say
    // bye to the merge type (it's now converted to a put)
    assert(ktypemerge == orig_ikey.type);
    assert(operands_.size() >= 1);
    assert(operands_.size() == keys_.size());
    success_ = user_merge_operator_->fullmerge(orig_ikey.user_key, nullptr,
                                               operands_, &merge_result,
                                               logger_);

    if (success_) {
      std::string& key = keys_.back();  // the original key encountered
      orig_ikey.type = ktypevalue;
      updateinternalkey(&key[0], key.size(),
                        orig_ikey.sequence, orig_ikey.type);

      // the final value() is always stored in operands_.back()
      swap(operands_.back(),merge_result);
    } else {
      recordtick(stats, number_merge_failures);
      // do nothing if not success_. leave keys() and operands() as they are.
    }
  } else {
    // we haven't seen the beginning of the key nor a put/delete.
    // attempt to use the user's associative merge function to
    // merge the stacked merge operands into a single operand.

    if (operands_.size() >= 2 &&
        operands_.size() >= min_partial_merge_operands_ &&
        user_merge_operator_->partialmergemulti(
            orig_ikey.user_key,
            std::deque<slice>(operands_.begin(), operands_.end()),
            &merge_result, logger_)) {
      // merging of operands (associative merge) was successful.
      // replace operands with the merge result
      operands_.clear();
      operands_.push_front(std::move(merge_result));
      keys_.erase(keys_.begin(), keys_.end() - 1);
    }
  }
}

} // namespace rocksdb
