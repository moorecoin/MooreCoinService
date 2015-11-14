//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#ifndef merge_helper_h
#define merge_helper_h

#include "db/dbformat.h"
#include "rocksdb/slice.h"
#include <string>
#include <deque>

namespace rocksdb {

class comparator;
class iterator;
class logger;
class mergeoperator;
class statistics;

class mergehelper {
 public:
  mergehelper(const comparator* user_comparator,
              const mergeoperator* user_merge_operator, logger* logger,
              unsigned min_partial_merge_operands,
              bool assert_valid_internal_key)
      : user_comparator_(user_comparator),
        user_merge_operator_(user_merge_operator),
        logger_(logger),
        min_partial_merge_operands_(min_partial_merge_operands),
        assert_valid_internal_key_(assert_valid_internal_key),
        keys_(),
        operands_(),
        success_(false) {}

  // merge entries until we hit
  //     - a corrupted key
  //     - a put/delete,
  //     - a different user key,
  //     - a specific sequence number (snapshot boundary),
  //  or - the end of iteration
  // iter: (in)  points to the first merge type entry
  //       (out) points to the first entry not included in the merge process
  // stop_before: (in) a sequence number that merge should not cross.
  //                   0 means no restriction
  // at_bottom:   (in) true if the iterator covers the bottem level, which means
  //                   we could reach the start of the history of this user key.
  void mergeuntil(iterator* iter, sequencenumber stop_before = 0,
                  bool at_bottom = false, statistics* stats = nullptr,
                  int* steps = nullptr);

  // query the merge result
  // these are valid until the next mergeuntil call
  // if the merging was successful:
  //   - issuccess() will be true
  //   - key() will have the latest sequence number of the merges.
  //           the type will be put or merge. see important 1 note, below.
  //   - value() will be the result of merging all the operands together
  //   - the user should ignore keys() and values().
  //
  //   important 1: the key type could change after the mergeuntil call.
  //        put/delete + merge + ... + merge => put
  //        merge + ... + merge => merge
  //
  // if the merge operator is not associative, and if a put/delete is not found
  // then the merging will be unsuccessful. in this case:
  //   - issuccess() will be false
  //   - keys() contains the list of internal keys seen in order of iteration.
  //   - values() contains the list of values (merges) seen in the same order.
  //              values() is parallel to keys() so that the first entry in
  //              keys() is the key associated with the first entry in values()
  //              and so on. these lists will be the same length.
  //              all of these pairs will be merges over the same user key.
  //              see important 2 note below.
  //   - the user should ignore key() and value().
  //
  //   important 2: the entries were traversed in order from back to front.
  //                so keys().back() was the first key seen by iterator.
  // todo: re-style this comment to be like the first one
  bool issuccess() const { return success_; }
  slice key() const { assert(success_); return slice(keys_.back()); }
  slice value() const { assert(success_); return slice(operands_.back()); }
  const std::deque<std::string>& keys() const {
    assert(!success_); return keys_;
  }
  const std::deque<std::string>& values() const {
    assert(!success_); return operands_;
  }
  bool hasoperator() const { return user_merge_operator_ != nullptr; }

 private:
  const comparator* user_comparator_;
  const mergeoperator* user_merge_operator_;
  logger* logger_;
  unsigned min_partial_merge_operands_;
  bool assert_valid_internal_key_; // enforce no internal key corruption?

  // the scratch area that holds the result of mergeuntil
  // valid up to the next mergeuntil call
  std::deque<std::string> keys_;    // keeps track of the sequence of keys seen
  std::deque<std::string> operands_;  // parallel with keys_; stores the values
  bool success_;
};

} // namespace rocksdb

#endif
