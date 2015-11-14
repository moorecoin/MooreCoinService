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

#include <cstdio>

#include <gflags/gflags.h>

#include "db/write_batch_internal.h"
#include "rocksdb/db.h"
#include "rocksdb/types.h"
#include "port/atomic_pointer.h"
#include "util/testutil.h"

// run a thread to perform put's.
// another thread uses getupdatessince api to keep getting the updates.
// options :
// --num_inserts = the num of inserts the first thread should perform.
// --wal_ttl = the wal ttl for the run.

using namespace rocksdb;

using gflags::parsecommandlineflags;
using gflags::setusagemessage;

struct datapumpthread {
  size_t no_records;
  db* db; // assumption db is open'ed already.
};

static std::string randomstring(random* rnd, int len) {
  std::string r;
  test::randomstring(rnd, len, &r);
  return r;
}

static void datapumpthreadbody(void* arg) {
  datapumpthread* t = reinterpret_cast<datapumpthread*>(arg);
  db* db = t->db;
  random rnd(301);
  size_t i = 0;
  while(i++ < t->no_records) {
    if(!db->put(writeoptions(), slice(randomstring(&rnd, 500)),
                slice(randomstring(&rnd, 500))).ok()) {
      fprintf(stderr, "error in put\n");
      exit(1);
    }
  }
}

struct replicationthread {
  port::atomicpointer stop;
  db* db;
  volatile size_t no_read;
};

static void replicationthreadbody(void* arg) {
  replicationthread* t = reinterpret_cast<replicationthread*>(arg);
  db* db = t->db;
  unique_ptr<transactionlogiterator> iter;
  sequencenumber currentseqnum = 1;
  while (t->stop.acquire_load() != nullptr) {
    iter.reset();
    status s;
    while(!db->getupdatessince(currentseqnum, &iter).ok()) {
      if (t->stop.acquire_load() == nullptr) {
        return;
      }
    }
    fprintf(stderr, "refreshing iterator\n");
    for(;iter->valid(); iter->next(), t->no_read++, currentseqnum++) {
      batchresult res = iter->getbatch();
      if (res.sequence != currentseqnum) {
        fprintf(stderr,
                "missed a seq no. b/w %ld and %ld\n",
                (long)currentseqnum,
                (long)res.sequence);
        exit(1);
      }
    }
  }
}

define_uint64(num_inserts, 1000, "the num of inserts the first thread should"
              " perform.");
define_uint64(wal_ttl_seconds, 1000, "the wal ttl for the run(in seconds)");
define_uint64(wal_size_limit_mb, 10, "the wal size limit for the run"
              "(in mb)");

int main(int argc, const char** argv) {
  setusagemessage(
      std::string("\nusage:\n") + std::string(argv[0]) +
      " --num_inserts=<num_inserts> --wal_ttl_seconds=<wal_ttl_seconds>" +
      " --wal_size_limit_mb=<wal_size_limit_mb>");
  parsecommandlineflags(&argc, const_cast<char***>(&argv), true);

  env* env = env::default();
  std::string default_db_path;
  env->gettestdirectory(&default_db_path);
  default_db_path += "db_repl_stress";
  options options;
  options.create_if_missing = true;
  options.wal_ttl_seconds = flags_wal_ttl_seconds;
  options.wal_size_limit_mb = flags_wal_size_limit_mb;
  db* db;
  destroydb(default_db_path, options);

  status s = db::open(options, default_db_path, &db);

  if (!s.ok()) {
    fprintf(stderr, "could not open db due to %s\n", s.tostring().c_str());
    exit(1);
  }

  datapumpthread datapump;
  datapump.no_records = flags_num_inserts;
  datapump.db = db;
  env->startthread(datapumpthreadbody, &datapump);

  replicationthread replthread;
  replthread.db = db;
  replthread.no_read = 0;
  replthread.stop.release_store(env); // store something to make it non-null.

  env->startthread(replicationthreadbody, &replthread);
  while(replthread.no_read < flags_num_inserts);
  replthread.stop.release_store(nullptr);
  if (replthread.no_read < datapump.no_records) {
    // no. read should be => than inserted.
    fprintf(stderr, "no. of record's written and read not same\nread : %zu"
            " written : %zu\n", replthread.no_read, datapump.no_records);
    exit(1);
  }
  fprintf(stderr, "successful!\n");
  exit(0);
}

#endif  // gflags
