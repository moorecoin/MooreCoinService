// copyright (c) 2014, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

#include <sstream>
#include "rocksdb/env.h"
#include "util/iostats_context_imp.h"

namespace rocksdb {

#ifndef ios_cross_compile
__thread iostatscontext iostats_context;
#endif  // ios_cross_compile

void iostatscontext::reset() {
  thread_pool_id = env::priority::total;
  bytes_read = 0;
  bytes_written = 0;
}

#define output(counter) #counter << " = " << counter << ", "

std::string iostatscontext::tostring() const {
  std::ostringstream ss;
  ss << output(thread_pool_id)
     << output(bytes_read)
     << output(bytes_written);
  return ss.str();
}

}  // namespace rocksdb
