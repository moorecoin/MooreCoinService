//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#define __stdc_format_macros
#include <inttypes.h>
#include <gflags/gflags.h>

#include "rocksdb/options.h"
#include "util/testharness.h"

using gflags::parsecommandlineflags;
define_bool(enable_print, false, "print options generated to console.");

namespace rocksdb {

class optionstest {};

class stderrlogger : public logger {
 public:
  virtual void logv(const char* format, va_list ap) override {
    vprintf(format, ap);
    printf("\n");
  }
};

options printandgetoptions(size_t total_write_buffer_limit,
                           int read_amplification_threshold,
                           int write_amplification_threshold,
                           uint64_t target_db_size = 68719476736) {
  stderrlogger logger;

  if (flags_enable_print) {
    printf(
        "---- total_write_buffer_limit: %zu "
        "read_amplification_threshold: %d write_amplification_threshold: %d "
        "target_db_size %" priu64 " ----\n",
        total_write_buffer_limit, read_amplification_threshold,
        write_amplification_threshold, target_db_size);
  }

  options options =
      getoptions(total_write_buffer_limit, read_amplification_threshold,
                 write_amplification_threshold, target_db_size);
  if (flags_enable_print) {
    options.dump(&logger);
    printf("-------------------------------------\n\n\n");
  }
  return options;
}

test(optionstest, loosecondition) {
  options options;
  printandgetoptions(static_cast<size_t>(10) * 1024 * 1024 * 1024, 100, 100);

  // less mem table memory budget
  printandgetoptions(32 * 1024 * 1024, 100, 100);

  // tight read amplification
  options = printandgetoptions(128 * 1024 * 1024, 8, 100);
  assert_eq(options.compaction_style, kcompactionstylelevel);

  // tight write amplification
  options = printandgetoptions(128 * 1024 * 1024, 64, 10);
  assert_eq(options.compaction_style, kcompactionstyleuniversal);

  // both tight amplifications
  printandgetoptions(128 * 1024 * 1024, 4, 8);
}
}  // namespace rocksdb

int main(int argc, char** argv) {
  parsecommandlineflags(&argc, &argv, true);
  return rocksdb::test::runalltests();
}
