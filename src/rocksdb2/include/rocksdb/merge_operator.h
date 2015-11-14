// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

#ifndef storage_rocksdb_include_merge_operator_h_
#define storage_rocksdb_include_merge_operator_h_

#include <memory>
#include <string>
#include <deque>
#include "rocksdb/slice.h"

namespace rocksdb {

class slice;
class logger;

// the merge operator
//
// essentially, a mergeoperator specifies the semantics of a merge, which only
// client knows. it could be numeric addition, list append, string
// concatenation, edit data structure, ... , anything.
// the library, on the other hand, is concerned with the exercise of this
// interface, at the right time (during get, iteration, compaction...)
//
// to use merge, the client needs to provide an object implementing one of
// the following interfaces:
//  a) associativemergeoperator - for most simple semantics (always take
//    two values, and merge them into one value, which is then put back
//    into rocksdb); numeric addition and string concatenation are examples;
//
//  b) mergeoperator - the generic class for all the more abstract / complex
//    operations; one method (fullmerge) to merge a put/delete value with a
//    merge operand; and another method (partialmerge) that merges multiple
//    operands together. this is especially useful if your key values have
//    complex structures but you would still like to support client-specific
//    incremental updates.
//
// associativemergeoperator is simpler to implement. mergeoperator is simply
// more powerful.
//
// refer to rocksdb-merge wiki for more details and example implementations.
//
class mergeoperator {
 public:
  virtual ~mergeoperator() {}

  // gives the client a way to express the read -> modify -> write semantics
  // key:      (in)    the key that's associated with this merge operation.
  //                   client could multiplex the merge operator based on it
  //                   if the key space is partitioned and different subspaces
  //                   refer to different types of data which have different
  //                   merge operation semantics
  // existing: (in)    null indicates that the key does not exist before this op
  // operand_list:(in) the sequence of merge operations to apply, front() first.
  // new_value:(out)   client is responsible for filling the merge result here
  // logger:   (in)    client could use this to log errors during merge.
  //
  // return true on success.
  // all values passed in will be client-specific values. so if this method
  // returns false, it is because client specified bad data or there was
  // internal corruption. this will be treated as an error by the library.
  //
  // also make use of the *logger for error messages.
  virtual bool fullmerge(const slice& key,
                         const slice* existing_value,
                         const std::deque<std::string>& operand_list,
                         std::string* new_value,
                         logger* logger) const = 0;

  // this function performs merge(left_op, right_op)
  // when both the operands are themselves merge operation types
  // that you would have passed to a db::merge() call in the same order
  // (i.e.: db::merge(key,left_op), followed by db::merge(key,right_op)).
  //
  // partialmerge should combine them into a single merge operation that is
  // saved into *new_value, and then it should return true.
  // *new_value should be constructed such that a call to
  // db::merge(key, *new_value) would yield the same result as a call
  // to db::merge(key, left_op) followed by db::merge(key, right_op).
  //
  // the default implementation of partialmergemulti will use this function
  // as a helper, for backward compatibility.  any successor class of
  // mergeoperator should either implement partialmerge or partialmergemulti,
  // although implementing partialmergemulti is suggested as it is in general
  // more effective to merge multiple operands at a time instead of two
  // operands at a time.
  //
  // if it is impossible or infeasible to combine the two operations,
  // leave new_value unchanged and return false. the library will
  // internally keep track of the operations, and apply them in the
  // correct order once a base-value (a put/delete/end-of-database) is seen.
  //
  // todo: presently there is no way to differentiate between error/corruption
  // and simply "return false". for now, the client should simply return
  // false in any case it cannot perform partial-merge, regardless of reason.
  // if there is corruption in the data, handle it in the fullmerge() function,
  // and return false there.  the default implementation of partialmerge will
  // always return false.
  virtual bool partialmerge(const slice& key, const slice& left_operand,
                            const slice& right_operand, std::string* new_value,
                            logger* logger) const {
    return false;
  }

  // this function performs merge when all the operands are themselves merge
  // operation types that you would have passed to a db::merge() call in the
  // same order (front() first)
  // (i.e. db::merge(key, operand_list[0]), followed by
  //  db::merge(key, operand_list[1]), ...)
  //
  // partialmergemulti should combine them into a single merge operation that is
  // saved into *new_value, and then it should return true.  *new_value should
  // be constructed such that a call to db::merge(key, *new_value) would yield
  // the same result as subquential individual calls to db::merge(key, operand)
  // for each operand in operand_list from front() to back().
  //
  // the partialmergemulti function will be called only when the list of
  // operands are long enough. the minimum amount of operands that will be
  // passed to the function are specified by the "min_partial_merge_operands"
  // option.
  //
  // in the default implementation, partialmergemulti will invoke partialmerge
  // multiple times, where each time it only merges two operands.  developers
  // should either implement partialmergemulti, or implement partialmerge which
  // is served as the helper function of the default partialmergemulti.
  virtual bool partialmergemulti(const slice& key,
                                 const std::deque<slice>& operand_list,
                                 std::string* new_value, logger* logger) const;

  // the name of the mergeoperator. used to check for mergeoperator
  // mismatches (i.e., a db created with one mergeoperator is
  // accessed using a different mergeoperator)
  // todo: the name is currently not stored persistently and thus
  //       no checking is enforced. client is responsible for providing
  //       consistent mergeoperator between db opens.
  virtual const char* name() const = 0;
};

// the simpler, associative merge operator.
class associativemergeoperator : public mergeoperator {
 public:
  virtual ~associativemergeoperator() {}

  // gives the client a way to express the read -> modify -> write semantics
  // key:           (in) the key that's associated with this merge operation.
  // existing_value:(in) null indicates the key does not exist before this op
  // value:         (in) the value to update/merge the existing_value with
  // new_value:    (out) client is responsible for filling the merge result here
  // logger:        (in) client could use this to log errors during merge.
  //
  // return true on success.
  // all values passed in will be client-specific values. so if this method
  // returns false, it is because client specified bad data or there was
  // internal corruption. the client should assume that this will be treated
  // as an error by the library.
  virtual bool merge(const slice& key,
                     const slice* existing_value,
                     const slice& value,
                     std::string* new_value,
                     logger* logger) const = 0;


 private:
  // default implementations of the mergeoperator functions
  virtual bool fullmerge(const slice& key,
                         const slice* existing_value,
                         const std::deque<std::string>& operand_list,
                         std::string* new_value,
                         logger* logger) const override;

  virtual bool partialmerge(const slice& key,
                            const slice& left_operand,
                            const slice& right_operand,
                            std::string* new_value,
                            logger* logger) const override;
};

}  // namespace rocksdb

#endif  // storage_rocksdb_include_merge_operator_h_
