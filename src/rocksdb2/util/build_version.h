//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#pragma once
#if !defined(ios_cross_compile)
// if we compile with xcode, we don't run build_detect_vesion, so we don't
// generate these variables
// these variables tell us about the git config and time
extern const char* rocksdb_build_git_sha;

// these variables tell us when the compilation occurred
extern const char* rocksdb_build_compile_time;
extern const char* rocksdb_build_compile_date;
#endif
