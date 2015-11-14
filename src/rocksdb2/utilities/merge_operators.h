//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#ifndef merge_operators_h
#define merge_operators_h

#include <memory>
#include <stdio.h>

#include "rocksdb/merge_operator.h"

namespace rocksdb {

class mergeoperators {
 public:
  static std::shared_ptr<mergeoperator> createputoperator();
  static std::shared_ptr<mergeoperator> createuint64addoperator();
  static std::shared_ptr<mergeoperator> createstringappendoperator();
  static std::shared_ptr<mergeoperator> createstringappendtestoperator();

  // will return a different merge operator depending on the string.
  // todo: hook the "name" up to the actual name() of the mergeoperators?
  static std::shared_ptr<mergeoperator> createfromstringid(
      const std::string& name) {
    if (name == "put") {
      return createputoperator();
    } else if ( name == "uint64add") {
      return createuint64addoperator();
    } else if (name == "stringappend") {
      return createstringappendoperator();
    } else if (name == "stringappendtest") {
      return createstringappendtestoperator();
    } else {
      // empty or unknown, just return nullptr
      return nullptr;
    }
  }

};

} // namespace rocksdb

#endif
