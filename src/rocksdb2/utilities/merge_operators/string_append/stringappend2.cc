/**
 * @author deon nicholas (dnicholas@fb.com)
 * copyright 2013 facebook
 */

#include "stringappend2.h"

#include <memory>
#include <string>
#include <assert.h>

#include "rocksdb/slice.h"
#include "rocksdb/merge_operator.h"
#include "utilities/merge_operators.h"

namespace rocksdb {

// constructor: also specify the delimiter character.
stringappendtestoperator::stringappendtestoperator(char delim_char)
    : delim_(delim_char) {
}

// implementation for the merge operation (concatenates two strings)
bool stringappendtestoperator::fullmerge(
    const slice& key,
    const slice* existing_value,
    const std::deque<std::string>& operands,
    std::string* new_value,
    logger* logger) const {

  // clear the *new_value for writing.
  assert(new_value);
  new_value->clear();

  // compute the space needed for the final result.
  int numbytes = 0;
  for(auto it = operands.begin(); it != operands.end(); ++it) {
    numbytes += it->size() + 1;   // plus 1 for the delimiter
  }

  // only print the delimiter after the first entry has been printed
  bool printdelim = false;

  // prepend the *existing_value if one exists.
  if (existing_value) {
    new_value->reserve(numbytes + existing_value->size());
    new_value->append(existing_value->data(), existing_value->size());
    printdelim = true;
  } else if (numbytes) {
    new_value->reserve(numbytes-1); // minus 1 since we have one less delimiter
  }

  // concatenate the sequence of strings (and add a delimiter between each)
  for(auto it = operands.begin(); it != operands.end(); ++it) {
    if (printdelim) {
      new_value->append(1,delim_);
    }
    new_value->append(*it);
    printdelim = true;
  }

  return true;
}

bool stringappendtestoperator::partialmergemulti(
    const slice& key, const std::deque<slice>& operand_list,
    std::string* new_value, logger* logger) const {
  return false;
}

// a version of partialmerge that actually performs "partial merging".
// use this to simulate the exact behaviour of the stringappendoperator.
bool stringappendtestoperator::_assocpartialmergemulti(
    const slice& key, const std::deque<slice>& operand_list,
    std::string* new_value, logger* logger) const {
  // clear the *new_value for writing
  assert(new_value);
  new_value->clear();
  assert(operand_list.size() >= 2);

  // generic append
  // determine and reserve correct size for *new_value.
  size_t size = 0;
  for (const auto& operand : operand_list) {
    size += operand.size();
  }
  size += operand_list.size() - 1;  // delimiters
  new_value->reserve(size);

  // apply concatenation
  new_value->assign(operand_list.front().data(), operand_list.front().size());

  for (std::deque<slice>::const_iterator it = operand_list.begin() + 1;
       it != operand_list.end(); ++it) {
    new_value->append(1, delim_);
    new_value->append(it->data(), it->size());
  }

  return true;
}

const char* stringappendtestoperator::name() const  {
  return "stringappendtestoperator";
}


std::shared_ptr<mergeoperator>
mergeoperators::createstringappendtestoperator() {
  return std::make_shared<stringappendtestoperator>(',');
}

} // namespace rocksdb

