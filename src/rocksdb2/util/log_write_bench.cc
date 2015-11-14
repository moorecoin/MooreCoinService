//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#ifndef gflags
#include <cstdio>
int main() {
  fprintf(stderr, "please install gflags to run rocksdb tools\n");
  return 1;
}
#else

#include <gflags/gflags.h>

#include "rocksdb/env.h"
#include "util/histogram.h"
#include "util/testharness.h"
#include "util/testutil.h"

using gflags::parsecommandlineflags;
using gflags::setusagemessage;

// a simple benchmark to simulate transactional logs

define_int32(num_records, 6000, "number of records.");
define_int32(record_size, 249, "size of each record.");
define_int32(record_interval, 10000, "interval between records (microsec)");
define_int32(bytes_per_sync, 0, "bytes_per_sync parameter in envoptions");
define_bool(enable_sync, false, "sync after each write.");

namespace rocksdb {
void runbenchmark() {
  std::string file_name = test::tmpdir() + "/log_write_benchmark.log";
  env* env = env::default();
  envoptions env_options;
  env_options.use_mmap_writes = false;
  env_options.bytes_per_sync = flags_bytes_per_sync;
  unique_ptr<writablefile> file;
  env->newwritablefile(file_name, &file, env_options);

  std::string record;
  record.assign('x', flags_record_size);

  histogramimpl hist;

  uint64_t start_time = env->nowmicros();
  for (int i = 0; i < flags_num_records; i++) {
    uint64_t start_nanos = env->nownanos();
    file->append(record);
    file->flush();
    if (flags_enable_sync) {
      file->sync();
    }
    hist.add(env->nownanos() - start_nanos);

    if (i % 1000 == 1) {
      fprintf(stderr, "wrote %d records...\n", i);
    }

    int time_to_sleep =
        (i + 1) * flags_record_interval - (env->nowmicros() - start_time);
    if (time_to_sleep > 0) {
      env->sleepformicroseconds(time_to_sleep);
    }
  }

  fprintf(stderr, "distribution of latency of append+flush: \n%s",
          hist.tostring().c_str());
}
}  // namespace rocksdb

int main(int argc, char** argv) {
  setusagemessage(std::string("\nusage:\n") + std::string(argv[0]) +
                  " [options]...");
  parsecommandlineflags(&argc, &argv, true);

  rocksdb::runbenchmark();
  return 0;
}

#endif  // gflags
