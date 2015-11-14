/**
 * a mergeoperator for rocksdb that implements string append.
 * @author deon nicholas (dnicholas@fb.com)
 * copyright 2013 facebook
 */

#pragma once
#include "rocksdb/merge_operator.h"
#include "rocksdb/slice.h"

namespace rocksdb {

class stringappendoperator : public associativemergeoperator {
 public:
  stringappendoperator(char delim_char);    /// constructor: specify delimiter

  virtual bool merge(const slice& key,
                     const slice* existing_value,
                     const slice& value,
                     std::string* new_value,
                     logger* logger) const override;

  virtual const char* name() const override;

 private:
  char delim_;         // the delimiter is inserted between elements

};

} // namespace rocksdb

