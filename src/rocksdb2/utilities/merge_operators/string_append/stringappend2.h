/**
 * a test mergeoperator for rocksdb that implements string append.
 * it is built using the mergeoperator interface rather than the simpler
 * associativemergeoperator interface. this is useful for testing/benchmarking.
 * while the two operators are semantically the same, all production code
 * should use the stringappendoperator defined in stringappend.{h,cc}. the
 * operator defined in the present file is primarily for testing.
 *
 * @author deon nicholas (dnicholas@fb.com)
 * copyright 2013 facebook
 */

#pragma once
#include <deque>
#include <string>

#include "rocksdb/merge_operator.h"
#include "rocksdb/slice.h"

namespace rocksdb {

class stringappendtestoperator : public mergeoperator {
 public:
  // constructor with delimiter
  explicit stringappendtestoperator(char delim_char);

  virtual bool fullmerge(const slice& key,
                         const slice* existing_value,
                         const std::deque<std::string>& operand_sequence,
                         std::string* new_value,
                         logger* logger) const override;

  virtual bool partialmergemulti(const slice& key,
                                 const std::deque<slice>& operand_list,
                                 std::string* new_value, logger* logger) const
      override;

  virtual const char* name() const override;

 private:
  // a version of partialmerge that actually performs "partial merging".
  // use this to simulate the exact behaviour of the stringappendoperator.
  bool _assocpartialmergemulti(const slice& key,
                               const std::deque<slice>& operand_list,
                               std::string* new_value, logger* logger) const;

  char delim_;         // the delimiter is inserted between elements

};

} // namespace rocksdb
