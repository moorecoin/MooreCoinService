//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#include <cstdio>
#include <vector>
#include <atomic>

#include "rocksdb/env.h"
#include "util/blob_store.h"
#include "util/testutil.h"

#define kb 1024ll
#define mb 1024*1024ll
// blobstore does costly asserts to make sure it's running correctly, which
// significantly impacts benchmark runtime.
// ndebug will compile out those asserts.
#ifndef ndebug
#define ndebug
#endif

using namespace rocksdb;
using namespace std;

// used by all threads
uint64_t timeout_sec;
env *env;
blobstore* bs;

namespace {
std::string randomstring(random* rnd, uint64_t len) {
  std::string r;
  test::randomstring(rnd, len, &r);
  return r;
}
}  // namespace

struct result {
  uint32_t writes;
  uint32_t reads;
  uint32_t deletes;
  uint64_t data_written;
  uint64_t data_read;

  void print() {
    printf("total writes = %u\n", writes);
    printf("total reads = %u\n", reads);
    printf("total deletes = %u\n", deletes);
    printf("write throughput = %lf mb/s\n",
           (double)data_written / (1024*1024.0) / timeout_sec);
    printf("read throughput = %lf mb/s\n",
           (double)data_read / (1024*1024.0) / timeout_sec);
    printf("total throughput = %lf mb/s\n",
           (double)(data_read + data_written) / (1024*1024.0) / timeout_sec);
  }

  result() {
    writes = reads = deletes = data_read = data_written = 0;
  }

  result (uint32_t writes, uint32_t reads, uint32_t deletes,
          uint64_t data_written, uint64_t data_read) :
    writes(writes), reads(reads), deletes(deletes),
    data_written(data_written), data_read(data_read) {}

};

namespace {
result operator + (const result &a, const result &b) {
  return result(a.writes + b.writes, a.reads + b.reads,
                a.deletes + b.deletes, a.data_written + b.data_written,
                a.data_read + b.data_read);
}
}  // namespace

struct workerthread {
  uint64_t data_size_from, data_size_to;
  double read_ratio;
  uint64_t working_set_size; // start deleting once you reach this
  result result;
  atomic<bool> stopped;

  workerthread(uint64_t data_size_from, uint64_t data_size_to,
                double read_ratio, uint64_t working_set_size) :
    data_size_from(data_size_from), data_size_to(data_size_to),
    read_ratio(read_ratio), working_set_size(working_set_size),
    stopped(false) {}

  workerthread(const workerthread& wt) :
    data_size_from(wt.data_size_from), data_size_to(wt.data_size_to),
    read_ratio(wt.read_ratio), working_set_size(wt.working_set_size),
    stopped(false) {}
};

static void workerthreadbody(void* arg) {
  workerthread* t = reinterpret_cast<workerthread*>(arg);
  random rnd(5);
  string buf;
  vector<pair<blob, uint64_t>> blobs;
  vector<string> random_strings;

  for (int i = 0; i < 10; ++i) {
    random_strings.push_back(randomstring(&rnd, t->data_size_to));
  }

  uint64_t total_size = 0;

  uint64_t start_micros = env->nowmicros();
  while (env->nowmicros() - start_micros < timeout_sec * 1000 * 1000) {
    if (blobs.size() && rand() < rand_max * t->read_ratio) {
      // read
      int bi = rand() % blobs.size();
      status s = bs->get(blobs[bi].first, &buf);
      assert(s.ok());
      t->result.data_read += buf.size();
      t->result.reads++;
    } else {
      // write
      uint64_t size = rand() % (t->data_size_to - t->data_size_from) +
        t->data_size_from;
      total_size += size;
      string put_str = random_strings[rand() % random_strings.size()];
      blobs.push_back(make_pair(blob(), size));
      status s = bs->put(slice(put_str.data(), size), &blobs.back().first);
      assert(s.ok());
      t->result.data_written += size;
      t->result.writes++;
    }

    while (total_size >= t->working_set_size) {
      // delete random
      int bi = rand() % blobs.size();
      total_size -= blobs[bi].second;
      bs->delete(blobs[bi].first);
      blobs.erase(blobs.begin() + bi);
      t->result.deletes++;
    }
  }
  t->stopped.store(true);
}

namespace {
result startbenchmark(vector<workerthread*>& config) {
  for (auto w : config) {
    env->startthread(workerthreadbody, w);
  }

  result result;

  for (auto w : config) {
    while (!w->stopped.load());
    result = result + w->result;
  }

  for (auto w : config) {
    delete w;
  }

  delete bs;

  return result;
}

vector<workerthread*> setupbenchmarkbalanced() {
  string test_path;
  env->gettestdirectory(&test_path);
  test_path.append("/blob_store");

  // config start
  uint32_t block_size = 16*kb;
  uint32_t file_size = 1*mb;
  double read_write_ratio = 0.5;
  uint64_t data_read_from = 16*kb;
  uint64_t data_read_to = 32*kb;
  int number_of_threads = 10;
  uint64_t working_set_size = 5*mb;
  timeout_sec = 5;
  // config end

  bs = new blobstore(test_path, block_size, file_size / block_size, 10000, env);

  vector <workerthread*> config;

  for (int i = 0; i < number_of_threads; ++i) {
    config.push_back(new workerthread(data_read_from,
                                      data_read_to,
                                      read_write_ratio,
                                      working_set_size));
  };

  return config;
}

vector<workerthread*> setupbenchmarkwriteheavy() {
  string test_path;
  env->gettestdirectory(&test_path);
  test_path.append("/blob_store");

  // config start
  uint32_t block_size = 16*kb;
  uint32_t file_size = 1*mb;
  double read_write_ratio = 0.1;
  uint64_t data_read_from = 16*kb;
  uint64_t data_read_to = 32*kb;
  int number_of_threads = 10;
  uint64_t working_set_size = 5*mb;
  timeout_sec = 5;
  // config end

  bs = new blobstore(test_path, block_size, file_size / block_size, 10000, env);

  vector <workerthread*> config;

  for (int i = 0; i < number_of_threads; ++i) {
    config.push_back(new workerthread(data_read_from,
                                      data_read_to,
                                      read_write_ratio,
                                      working_set_size));
  };

  return config;
}

vector<workerthread*> setupbenchmarkreadheavy() {
  string test_path;
  env->gettestdirectory(&test_path);
  test_path.append("/blob_store");

  // config start
  uint32_t block_size = 16*kb;
  uint32_t file_size = 1*mb;
  double read_write_ratio = 0.9;
  uint64_t data_read_from = 16*kb;
  uint64_t data_read_to = 32*kb;
  int number_of_threads = 10;
  uint64_t working_set_size = 5*mb;
  timeout_sec = 5;
  // config end

  bs = new blobstore(test_path, block_size, file_size / block_size, 10000, env);

  vector <workerthread*> config;

  for (int i = 0; i < number_of_threads; ++i) {
    config.push_back(new workerthread(data_read_from,
                                      data_read_to,
                                      read_write_ratio,
                                      working_set_size));
  };

  return config;
}
}  // namespace

int main(int argc, const char** argv) {
  srand(33);
  env = env::default();

  {
    printf("--- balanced read/write benchmark ---\n");
    vector <workerthread*> config = setupbenchmarkbalanced();
    result r = startbenchmark(config);
    r.print();
  }
  {
    printf("--- write heavy benchmark ---\n");
    vector <workerthread*> config = setupbenchmarkwriteheavy();
    result r = startbenchmark(config);
    r.print();
  }
  {
    printf("--- read heavy benchmark ---\n");
    vector <workerthread*> config = setupbenchmarkreadheavy();
    result r = startbenchmark(config);
    r.print();
  }

  return 0;
}
