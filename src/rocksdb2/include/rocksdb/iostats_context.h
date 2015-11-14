// copyright (c) 2014, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

#ifndef include_rocksdb_iostats_context_h_
#define include_rocksdb_iostats_context_h_

#include <stdint.h>
#include <string>

// a thread local context for gathering io-stats efficiently and transparently.
namespace rocksdb {

struct iostatscontext {
  // reset all io-stats counter to zero
  void reset();

  std::string tostring() const;

  // the thread pool id
  uint64_t thread_pool_id;

  // number of bytes that has been written.
  uint64_t bytes_written;
  // number of bytes that has been read.
  uint64_t bytes_read;
};

#ifndef ios_cross_compile
extern __thread iostatscontext iostats_context;
#endif  // ios_cross_compile

}  // namespace rocksdb

#endif  // include_rocksdb_iostats_context_h_
