// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

#ifndef storage_rocksdb_include_types_h_
#define storage_rocksdb_include_types_h_

#include <stdint.h>

namespace rocksdb {

// define all public custom types here.

// represents a sequence number in a wal file.
typedef uint64_t sequencenumber;

}  //  namespace rocksdb

#endif //  storage_rocksdb_include_types_h_
