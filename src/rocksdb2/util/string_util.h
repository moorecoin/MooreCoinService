//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#include <sstream>
#include <string>
#include <vector>

#pragma once
namespace rocksdb {

extern std::vector<std::string> stringsplit(std::string arg, char delim);

}  // namespace rocksdb
