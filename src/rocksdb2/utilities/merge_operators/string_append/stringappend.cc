/**
 * a mergeoperator for rocksdb that implements string append.
 * @author deon nicholas (dnicholas@fb.com)
 * copyright 2013 facebook
 */

#include "stringappend.h"

#include <memory>
#include <assert.h>

#include "rocksdb/slice.h"
#include "rocksdb/merge_operator.h"
#include "utilities/merge_operators.h"

namespace rocksdb {

// constructor: also specify the delimiter character.
stringappendoperator::stringappendoperator(char delim_char)
    : delim_(delim_char) {
}

// implementation for the merge operation (concatenates two strings)
bool stringappendoperator::merge(const slice& key,
                                 const slice* existing_value,
                                 const slice& value,
                                 std::string* new_value,
                                 logger* logger) const {

  // clear the *new_value for writing.
  assert(new_value);
  new_value->clear();

  if (!existing_value) {
    // no existing_value. set *new_value = value
    new_value->assign(value.data(),value.size());
  } else {
    // generic append (existing_value != null).
    // reserve *new_value to correct size, and apply concatenation.
    new_value->reserve(existing_value->size() + 1 + value.size());
    new_value->assign(existing_value->data(),existing_value->size());
    new_value->append(1,delim_);
    new_value->append(value.data(), value.size());
  }

  return true;
}

const char* stringappendoperator::name() const  {
  return "stringappendoperator";
}

std::shared_ptr<mergeoperator> mergeoperators::createstringappendoperator() {
  return std::make_shared<stringappendoperator>(',');
}

} // namespace rocksdb



