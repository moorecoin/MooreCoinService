//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#pragma once
#include "db/dbformat.h"
#include "rocksdb/slice.h"
#include <string>
#include <deque>

namespace rocksdb {

const std::deque<std::string> empty_operand_list;

// the merge context for merging a user key.
// when doing a get(), db will create such a class and pass it when
// issuing get() operation to memtables and version_set. the operands
// will be fetched from the context when issuing partial of full merge.
class mergecontext {
public:
  // clear all the operands
  void clear() {
    if (operand_list) {
      operand_list->clear();
    }
  }
  // replace all operands with merge_result, which are expected to be the
  // merge result of them.
  void pushpartialmergeresult(std::string& merge_result) {
    assert (operand_list);
    operand_list->clear();
    operand_list->push_front(std::move(merge_result));
  }
  // push a merge operand
  void pushoperand(const slice& operand_slice) {
    initialize();
    operand_list->push_front(operand_slice.tostring());
  }
  // return total number of operands in the list
  size_t getnumoperands() const {
    if (!operand_list) {
      return 0;
    }
    return operand_list->size();
  }
  // get the operand at the index.
  slice getoperand(int index) const {
    assert (operand_list);
    return (*operand_list)[index];
  }
  // return all the operands.
  const std::deque<std::string>& getoperands() const {
    if (!operand_list) {
      return empty_operand_list;
    }
    return *operand_list;
  }
private:
  void initialize() {
    if (!operand_list) {
      operand_list.reset(new std::deque<std::string>());
    }
  }
  std::unique_ptr<std::deque<std::string>> operand_list;
};

} // namespace rocksdb

