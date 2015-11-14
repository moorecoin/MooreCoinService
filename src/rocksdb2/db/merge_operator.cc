//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
/**
 * back-end implementation details specific to the merge operator.
 */

#include "rocksdb/merge_operator.h"

namespace rocksdb {

// the default implementation of partialmergemulti, which invokes
// partialmerge multiple times internally and merges two operands at
// a time.
bool mergeoperator::partialmergemulti(const slice& key,
                                      const std::deque<slice>& operand_list,
                                      std::string* new_value,
                                      logger* logger) const {
  assert(operand_list.size() >= 2);
  // simply loop through the operands
  std::string temp_value;
  slice temp_slice(operand_list[0]);

  for (size_t i = 1; i < operand_list.size(); ++i) {
    auto& operand = operand_list[i];
    if (!partialmerge(key, temp_slice, operand, &temp_value, logger)) {
      return false;
    }
    swap(temp_value, *new_value);
    temp_slice = slice(*new_value);
  }

  // the result will be in *new_value. all merges succeeded.
  return true;
}

// given a "real" merge from the library, call the user's
// associative merge function one-by-one on each of the operands.
// note: it is assumed that the client's merge-operator will handle any errors.
bool associativemergeoperator::fullmerge(
    const slice& key,
    const slice* existing_value,
    const std::deque<std::string>& operand_list,
    std::string* new_value,
    logger* logger) const {

  // simply loop through the operands
  slice temp_existing;
  std::string temp_value;
  for (const auto& operand : operand_list) {
    slice value(operand);
    if (!merge(key, existing_value, value, &temp_value, logger)) {
      return false;
    }
    swap(temp_value, *new_value);
    temp_existing = slice(*new_value);
    existing_value = &temp_existing;
  }

  // the result will be in *new_value. all merges succeeded.
  return true;
}

// call the user defined simple merge on the operands;
// note: it is assumed that the client's merge-operator will handle any errors.
bool associativemergeoperator::partialmerge(
    const slice& key,
    const slice& left_operand,
    const slice& right_operand,
    std::string* new_value,
    logger* logger) const {
  return merge(key, &left_operand, right_operand, new_value, logger);
}

} // namespace rocksdb
