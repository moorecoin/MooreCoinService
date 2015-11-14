//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// log format information shared by reader and writer.
// see ../doc/log_format.txt for more detail.

#pragma once
namespace rocksdb {
namespace log {

enum recordtype {
  // zero is reserved for preallocated files
  kzerotype = 0,
  kfulltype = 1,

  // for fragments
  kfirsttype = 2,
  kmiddletype = 3,
  klasttype = 4
};
static const int kmaxrecordtype = klasttype;

static const unsigned int kblocksize = 32768;

// header is checksum (4 bytes), type (1 byte), length (2 bytes).
static const int kheadersize = 4 + 1 + 2;

}  // namespace log
}  // namespace rocksdb
