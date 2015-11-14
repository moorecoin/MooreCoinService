//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#include <memory>
#include "rocksdb/slice.h"
#include "rocksdb/merge_operator.h"
#include "utilities/merge_operators.h"

using namespace rocksdb;

namespace { // anonymous namespace

// a merge operator that mimics put semantics
// since this merge-operator will not be used in production,
// it is implemented as a non-associative merge operator to illustrate the
// new interface and for testing purposes. (that is, we inherit from
// the mergeoperator class rather than the associativemergeoperator
// which would be simpler in this case).
//
// from the client-perspective, semantics are the same.
class putoperator : public mergeoperator {
 public:
  virtual bool fullmerge(const slice& key,
                         const slice* existing_value,
                         const std::deque<std::string>& operand_sequence,
                         std::string* new_value,
                         logger* logger) const override {
    // put basically only looks at the current/latest value
    assert(!operand_sequence.empty());
    assert(new_value != nullptr);
    new_value->assign(operand_sequence.back());
    return true;
  }

  virtual bool partialmerge(const slice& key,
                            const slice& left_operand,
                            const slice& right_operand,
                            std::string* new_value,
                            logger* logger) const override {
    new_value->assign(right_operand.data(), right_operand.size());
    return true;
  }

  using mergeoperator::partialmergemulti;
  virtual bool partialmergemulti(const slice& key,
                                 const std::deque<slice>& operand_list,
                                 std::string* new_value, logger* logger) const
      override {
    new_value->assign(operand_list.back().data(), operand_list.back().size());
    return true;
  }

  virtual const char* name() const override {
    return "putoperator";
  }
};

} // end of anonymous namespace

namespace rocksdb {

std::shared_ptr<mergeoperator> mergeoperators::createputoperator() {
  return std::make_shared<putoperator>();
}

}
