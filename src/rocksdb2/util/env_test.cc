//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <sys/types.h>

#include <iostream>
#include <unordered_set>

#ifdef os_linux
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "rocksdb/env.h"
#include "port/port.h"
#include "util/coding.h"
#include "util/log_buffer.h"
#include "util/mutexlock.h"
#include "util/testharness.h"

namespace rocksdb {

static const int kdelaymicros = 100000;

class envposixtest {
 private:
  port::mutex mu_;
  std::string events_;

 public:
  env* env_;
  envposixtest() : env_(env::default()) { }
};

static void setbool(void* ptr) {
  reinterpret_cast<port::atomicpointer*>(ptr)->nobarrier_store(ptr);
}

test(envposixtest, runimmediately) {
  port::atomicpointer called (nullptr);
  env_->schedule(&setbool, &called);
  env::default()->sleepformicroseconds(kdelaymicros);
  assert_true(called.nobarrier_load() != nullptr);
}

test(envposixtest, runmany) {
  port::atomicpointer last_id (nullptr);

  struct cb {
    port::atomicpointer* last_id_ptr;   // pointer to shared slot
    uintptr_t id;             // order# for the execution of this callback

    cb(port::atomicpointer* p, int i) : last_id_ptr(p), id(i) { }

    static void run(void* v) {
      cb* cb = reinterpret_cast<cb*>(v);
      void* cur = cb->last_id_ptr->nobarrier_load();
      assert_eq(cb->id-1, reinterpret_cast<uintptr_t>(cur));
      cb->last_id_ptr->release_store(reinterpret_cast<void*>(cb->id));
    }
  };

  // schedule in different order than start time
  cb cb1(&last_id, 1);
  cb cb2(&last_id, 2);
  cb cb3(&last_id, 3);
  cb cb4(&last_id, 4);
  env_->schedule(&cb::run, &cb1);
  env_->schedule(&cb::run, &cb2);
  env_->schedule(&cb::run, &cb3);
  env_->schedule(&cb::run, &cb4);

  env::default()->sleepformicroseconds(kdelaymicros);
  void* cur = last_id.acquire_load();
  assert_eq(4u, reinterpret_cast<uintptr_t>(cur));
}

struct state {
  port::mutex mu;
  int val;
  int num_running;
};

static void threadbody(void* arg) {
  state* s = reinterpret_cast<state*>(arg);
  s->mu.lock();
  s->val += 1;
  s->num_running -= 1;
  s->mu.unlock();
}

test(envposixtest, startthread) {
  state state;
  state.val = 0;
  state.num_running = 3;
  for (int i = 0; i < 3; i++) {
    env_->startthread(&threadbody, &state);
  }
  while (true) {
    state.mu.lock();
    int num = state.num_running;
    state.mu.unlock();
    if (num == 0) {
      break;
    }
    env::default()->sleepformicroseconds(kdelaymicros);
  }
  assert_eq(state.val, 3);
}

test(envposixtest, twopools) {

  class cb {
   public:
    cb(const std::string& pool_name, int pool_size)
        : mu_(),
          num_running_(0),
          num_finished_(0),
          pool_size_(pool_size),
          pool_name_(pool_name) { }

    static void run(void* v) {
      cb* cb = reinterpret_cast<cb*>(v);
      cb->run();
    }

    void run() {
      {
        mutexlock l(&mu_);
        num_running_++;
        std::cout << "pool " << pool_name_ << ": "
                  << num_running_ << " running threads.\n";
        // make sure we don't have more than pool_size_ jobs running.
        assert_le(num_running_, pool_size_);
      }

      // sleep for 1 sec
      env::default()->sleepformicroseconds(1000000);

      {
        mutexlock l(&mu_);
        num_running_--;
        num_finished_++;
      }
    }

    int numfinished() {
      mutexlock l(&mu_);
      return num_finished_;
    }

   private:
    port::mutex mu_;
    int num_running_;
    int num_finished_;
    int pool_size_;
    std::string pool_name_;
  };

  const int klowpoolsize = 2;
  const int khighpoolsize = 4;
  const int kjobs = 8;

  cb low_pool_job("low", klowpoolsize);
  cb high_pool_job("high", khighpoolsize);

  env_->setbackgroundthreads(klowpoolsize);
  env_->setbackgroundthreads(khighpoolsize, env::priority::high);

  assert_eq(0u, env_->getthreadpoolqueuelen(env::priority::low));
  assert_eq(0u, env_->getthreadpoolqueuelen(env::priority::high));

  // schedule same number of jobs in each pool
  for (int i = 0; i < kjobs; i++) {
    env_->schedule(&cb::run, &low_pool_job);
    env_->schedule(&cb::run, &high_pool_job, env::priority::high);
  }
  // wait a short while for the jobs to be dispatched.
  env::default()->sleepformicroseconds(kdelaymicros);
  assert_eq((unsigned int)(kjobs - klowpoolsize),
            env_->getthreadpoolqueuelen());
  assert_eq((unsigned int)(kjobs - klowpoolsize),
            env_->getthreadpoolqueuelen(env::priority::low));
  assert_eq((unsigned int)(kjobs - khighpoolsize),
            env_->getthreadpoolqueuelen(env::priority::high));

  // wait for all jobs to finish
  while (low_pool_job.numfinished() < kjobs ||
         high_pool_job.numfinished() < kjobs) {
    env_->sleepformicroseconds(kdelaymicros);
  }

  assert_eq(0u, env_->getthreadpoolqueuelen(env::priority::low));
  assert_eq(0u, env_->getthreadpoolqueuelen(env::priority::high));
}

test(envposixtest, decreasenumbgthreads) {
  class sleepingbackgroundtask {
   public:
    explicit sleepingbackgroundtask()
        : bg_cv_(&mutex_), should_sleep_(true), sleeping_(false) {}
    void dosleep() {
      mutexlock l(&mutex_);
      sleeping_ = true;
      while (should_sleep_) {
        bg_cv_.wait();
      }
      sleeping_ = false;
      bg_cv_.signalall();
    }

    void wakeup() {
      mutexlock l(&mutex_);
      should_sleep_ = false;
      bg_cv_.signalall();

      while (sleeping_) {
        bg_cv_.wait();
      }
    }

    bool issleeping() {
      mutexlock l(&mutex_);
      return sleeping_;
    }

    static void dosleeptask(void* arg) {
      reinterpret_cast<sleepingbackgroundtask*>(arg)->dosleep();
    }

   private:
    port::mutex mutex_;
    port::condvar bg_cv_;  // signalled when background work finishes
    bool should_sleep_;
    bool sleeping_;
  };

  std::vector<sleepingbackgroundtask> tasks(10);

  // set number of thread to 1 first.
  env_->setbackgroundthreads(1, env::priority::high);
  env::default()->sleepformicroseconds(kdelaymicros);

  // schedule 3 tasks. 0 running; task 1, 2 waiting.
  for (size_t i = 0; i < 3; i++) {
    env_->schedule(&sleepingbackgroundtask::dosleeptask, &tasks[i],
                   env::priority::high);
    env::default()->sleepformicroseconds(kdelaymicros);
  }
  assert_eq(2u, env_->getthreadpoolqueuelen(env::priority::high));
  assert_true(tasks[0].issleeping());
  assert_true(!tasks[1].issleeping());
  assert_true(!tasks[2].issleeping());

  // increase to 2 threads. task 0, 1 running; 2 waiting
  env_->setbackgroundthreads(2, env::priority::high);
  env::default()->sleepformicroseconds(kdelaymicros);
  assert_eq(1u, env_->getthreadpoolqueuelen(env::priority::high));
  assert_true(tasks[0].issleeping());
  assert_true(tasks[1].issleeping());
  assert_true(!tasks[2].issleeping());

  // shrink back to 1 thread. still task 0, 1 running, 2 waiting
  env_->setbackgroundthreads(1, env::priority::high);
  env::default()->sleepformicroseconds(kdelaymicros);
  assert_eq(1u, env_->getthreadpoolqueuelen(env::priority::high));
  assert_true(tasks[0].issleeping());
  assert_true(tasks[1].issleeping());
  assert_true(!tasks[2].issleeping());

  // the last task finishes. task 0 running, 2 waiting.
  tasks[1].wakeup();
  env::default()->sleepformicroseconds(kdelaymicros);
  assert_eq(1u, env_->getthreadpoolqueuelen(env::priority::high));
  assert_true(tasks[0].issleeping());
  assert_true(!tasks[1].issleeping());
  assert_true(!tasks[2].issleeping());

  // increase to 5 threads. task 0 and 2 running.
  env_->setbackgroundthreads(5, env::priority::high);
  env::default()->sleepformicroseconds(kdelaymicros);
  assert_eq((unsigned int)0, env_->getthreadpoolqueuelen(env::priority::high));
  assert_true(tasks[0].issleeping());
  assert_true(tasks[2].issleeping());

  // change number of threads a couple of times while there is no sufficient
  // tasks.
  env_->setbackgroundthreads(7, env::priority::high);
  env::default()->sleepformicroseconds(kdelaymicros);
  tasks[2].wakeup();
  assert_eq(0u, env_->getthreadpoolqueuelen(env::priority::high));
  env_->setbackgroundthreads(3, env::priority::high);
  env::default()->sleepformicroseconds(kdelaymicros);
  assert_eq(0u, env_->getthreadpoolqueuelen(env::priority::high));
  env_->setbackgroundthreads(4, env::priority::high);
  env::default()->sleepformicroseconds(kdelaymicros);
  assert_eq(0u, env_->getthreadpoolqueuelen(env::priority::high));
  env_->setbackgroundthreads(5, env::priority::high);
  env::default()->sleepformicroseconds(kdelaymicros);
  assert_eq(0u, env_->getthreadpoolqueuelen(env::priority::high));
  env_->setbackgroundthreads(4, env::priority::high);
  env::default()->sleepformicroseconds(kdelaymicros);
  assert_eq(0u, env_->getthreadpoolqueuelen(env::priority::high));

  env::default()->sleepformicroseconds(kdelaymicros * 50);

  // enqueue 5 more tasks. thread pool size now is 4.
  // task 0, 3, 4, 5 running;6, 7 waiting.
  for (size_t i = 3; i < 8; i++) {
    env_->schedule(&sleepingbackgroundtask::dosleeptask, &tasks[i],
                   env::priority::high);
  }
  env::default()->sleepformicroseconds(kdelaymicros);
  assert_eq(2u, env_->getthreadpoolqueuelen(env::priority::high));
  assert_true(tasks[3].issleeping());
  assert_true(tasks[4].issleeping());
  assert_true(tasks[5].issleeping());
  assert_true(!tasks[6].issleeping());
  assert_true(!tasks[7].issleeping());

  // wake up task 0, 3 and 4. task 5, 6, 7 running.
  tasks[0].wakeup();
  tasks[3].wakeup();
  tasks[4].wakeup();

  env::default()->sleepformicroseconds(kdelaymicros);
  assert_eq((unsigned int)0, env_->getthreadpoolqueuelen(env::priority::high));
  for (size_t i = 5; i < 8; i++) {
    assert_true(tasks[i].issleeping());
  }

  // shrink back to 1 thread. still task 5, 6, 7 running
  env_->setbackgroundthreads(1, env::priority::high);
  env::default()->sleepformicroseconds(kdelaymicros);
  assert_true(tasks[5].issleeping());
  assert_true(tasks[6].issleeping());
  assert_true(tasks[7].issleeping());

  // wake up task  6. task 5, 7 running
  tasks[6].wakeup();
  env::default()->sleepformicroseconds(kdelaymicros);
  assert_true(tasks[5].issleeping());
  assert_true(!tasks[6].issleeping());
  assert_true(tasks[7].issleeping());

  // wake up threads 7. task 5 running
  tasks[7].wakeup();
  env::default()->sleepformicroseconds(kdelaymicros);
  assert_true(!tasks[7].issleeping());

  // enqueue thread 8 and 9. task 5 running; one of 8, 9 might be running.
  env_->schedule(&sleepingbackgroundtask::dosleeptask, &tasks[8],
                 env::priority::high);
  env_->schedule(&sleepingbackgroundtask::dosleeptask, &tasks[9],
                 env::priority::high);
  env::default()->sleepformicroseconds(kdelaymicros);
  assert_gt(env_->getthreadpoolqueuelen(env::priority::high), (unsigned int)0);
  assert_true(!tasks[8].issleeping() || !tasks[9].issleeping());

  // increase to 4 threads. task 5, 8, 9 running.
  env_->setbackgroundthreads(4, env::priority::high);
  env::default()->sleepformicroseconds(kdelaymicros);
  assert_eq((unsigned int)0, env_->getthreadpoolqueuelen(env::priority::high));
  assert_true(tasks[8].issleeping());
  assert_true(tasks[9].issleeping());

  // shrink to 1 thread
  env_->setbackgroundthreads(1, env::priority::high);

  // wake up thread 9.
  tasks[9].wakeup();
  env::default()->sleepformicroseconds(kdelaymicros);
  assert_true(!tasks[9].issleeping());
  assert_true(tasks[8].issleeping());

  // wake up thread 8
  tasks[8].wakeup();
  env::default()->sleepformicroseconds(kdelaymicros);
  assert_true(!tasks[8].issleeping());

  // wake up the last thread
  tasks[5].wakeup();

  env::default()->sleepformicroseconds(kdelaymicros);
  assert_true(!tasks[5].issleeping());
}

#ifdef os_linux
// to make sure the env::getuniqueid() related tests work correctly, the files
// should be stored in regular storage like "hard disk" or "flash device".
// otherwise we cannot get the correct id.
//
// the following function act as the replacement of test::tmpdir() that may be
// customized by user to be on a storage that doesn't work with getuniqueid().
//
// todo(kailiu) this function still assumes /tmp/<test-dir> reside in regular
// storage system.
namespace {
bool issinglevarint(const std::string& s) {
  slice slice(s);

  uint64_t v;
  if (!getvarint64(&slice, &v)) {
    return false;
  }

  return slice.size() == 0;
}

bool isuniqueidvalid(const std::string& s) {
  return !s.empty() && !issinglevarint(s);
}

const size_t max_id_size = 100;
char temp_id[max_id_size];

std::string getondisktestdir() {
  char base[100];
  snprintf(base, sizeof(base), "/tmp/rocksdbtest-%d",
           static_cast<int>(geteuid()));
  // directory may already exist
  env::default()->createdirifmissing(base);

  return base;
}
}  // namespace

// only works in linux platforms
test(envposixtest, randomaccessuniqueid) {
  // create file.
  const envoptions soptions;
  std::string fname = getondisktestdir() + "/" + "testfile";
  unique_ptr<writablefile> wfile;
  assert_ok(env_->newwritablefile(fname, &wfile, soptions));

  unique_ptr<randomaccessfile> file;

  // get unique id
  assert_ok(env_->newrandomaccessfile(fname, &file, soptions));
  size_t id_size = file->getuniqueid(temp_id, max_id_size);
  assert_true(id_size > 0);
  std::string unique_id1(temp_id, id_size);
  assert_true(isuniqueidvalid(unique_id1));

  // get unique id again
  assert_ok(env_->newrandomaccessfile(fname, &file, soptions));
  id_size = file->getuniqueid(temp_id, max_id_size);
  assert_true(id_size > 0);
  std::string unique_id2(temp_id, id_size);
  assert_true(isuniqueidvalid(unique_id2));

  // get unique id again after waiting some time.
  env_->sleepformicroseconds(1000000);
  assert_ok(env_->newrandomaccessfile(fname, &file, soptions));
  id_size = file->getuniqueid(temp_id, max_id_size);
  assert_true(id_size > 0);
  std::string unique_id3(temp_id, id_size);
  assert_true(isuniqueidvalid(unique_id3));

  // check ids are the same.
  assert_eq(unique_id1, unique_id2);
  assert_eq(unique_id2, unique_id3);

  // delete the file
  env_->deletefile(fname);
}

// only works in linux platforms
#ifdef rocksdb_fallocate_present
test(envposixtest, allocatetest) {
  std::string fname = getondisktestdir() + "/preallocate_testfile";
  envoptions soptions;
  soptions.use_mmap_writes = false;
  unique_ptr<writablefile> wfile;
  assert_ok(env_->newwritablefile(fname, &wfile, soptions));

  // allocate 100 mb
  size_t kpreallocatesize = 100 * 1024 * 1024;
  size_t kblocksize = 512;
  std::string data = "test";
  wfile->setpreallocationblocksize(kpreallocatesize);
  assert_ok(wfile->append(slice(data)));
  assert_ok(wfile->flush());

  struct stat f_stat;
  stat(fname.c_str(), &f_stat);
  assert_eq((unsigned int)data.size(), f_stat.st_size);
  // verify that blocks are preallocated
  // note here that we don't check the exact number of blocks preallocated --
  // we only require that number of allocated blocks is at least what we expect.
  // it looks like some fs give us more blocks that we asked for. that's fine.
  // it might be worth investigating further.
  auto st_blocks = f_stat.st_blocks;
  assert_le((unsigned int)(kpreallocatesize / kblocksize), st_blocks);

  // close the file, should deallocate the blocks
  wfile.reset();

  stat(fname.c_str(), &f_stat);
  assert_eq((unsigned int)data.size(), f_stat.st_size);
  // verify that preallocated blocks were deallocated on file close
  assert_gt(st_blocks, f_stat.st_blocks);
}
#endif

// returns true if any of the strings in ss are the prefix of another string.
bool hasprefix(const std::unordered_set<std::string>& ss) {
  for (const std::string& s: ss) {
    if (s.empty()) {
      return true;
    }
    for (size_t i = 1; i < s.size(); ++i) {
      if (ss.count(s.substr(0, i)) != 0) {
        return true;
      }
    }
  }
  return false;
}

// only works in linux platforms
test(envposixtest, randomaccessuniqueidconcurrent) {
  // check whether a bunch of concurrently existing files have unique ids.
  const envoptions soptions;

  // create the files
  std::vector<std::string> fnames;
  for (int i = 0; i < 1000; ++i) {
    fnames.push_back(getondisktestdir() + "/" + "testfile" + std::to_string(i));

    // create file.
    unique_ptr<writablefile> wfile;
    assert_ok(env_->newwritablefile(fnames[i], &wfile, soptions));
  }

  // collect and check whether the ids are unique.
  std::unordered_set<std::string> ids;
  for (const std::string fname: fnames) {
    unique_ptr<randomaccessfile> file;
    std::string unique_id;
    assert_ok(env_->newrandomaccessfile(fname, &file, soptions));
    size_t id_size = file->getuniqueid(temp_id, max_id_size);
    assert_true(id_size > 0);
    unique_id = std::string(temp_id, id_size);
    assert_true(isuniqueidvalid(unique_id));

    assert_true(ids.count(unique_id) == 0);
    ids.insert(unique_id);
  }

  // delete the files
  for (const std::string fname: fnames) {
    assert_ok(env_->deletefile(fname));
  }

  assert_true(!hasprefix(ids));
}

// only works in linux platforms
test(envposixtest, randomaccessuniqueiddeletes) {
  const envoptions soptions;

  std::string fname = getondisktestdir() + "/" + "testfile";

  // check that after file is deleted we don't get same id again in a new file.
  std::unordered_set<std::string> ids;
  for (int i = 0; i < 1000; ++i) {
    // create file.
    {
      unique_ptr<writablefile> wfile;
      assert_ok(env_->newwritablefile(fname, &wfile, soptions));
    }

    // get unique id
    std::string unique_id;
    {
      unique_ptr<randomaccessfile> file;
      assert_ok(env_->newrandomaccessfile(fname, &file, soptions));
      size_t id_size = file->getuniqueid(temp_id, max_id_size);
      assert_true(id_size > 0);
      unique_id = std::string(temp_id, id_size);
    }

    assert_true(isuniqueidvalid(unique_id));
    assert_true(ids.count(unique_id) == 0);
    ids.insert(unique_id);

    // delete the file
    assert_ok(env_->deletefile(fname));
  }

  assert_true(!hasprefix(ids));
}

// only works in linux platforms
test(envposixtest, invalidatecache) {
  const envoptions soptions;
  std::string fname = test::tmpdir() + "/" + "testfile";

  // create file.
  {
    unique_ptr<writablefile> wfile;
    assert_ok(env_->newwritablefile(fname, &wfile, soptions));
    assert_ok(wfile.get()->append(slice("hello world")));
    assert_ok(wfile.get()->invalidatecache(0, 0));
    assert_ok(wfile.get()->close());
  }

  // random read
  {
    unique_ptr<randomaccessfile> file;
    char scratch[100];
    slice result;
    assert_ok(env_->newrandomaccessfile(fname, &file, soptions));
    assert_ok(file.get()->read(0, 11, &result, scratch));
    assert_eq(memcmp(scratch, "hello world", 11), 0);
    assert_ok(file.get()->invalidatecache(0, 11));
    assert_ok(file.get()->invalidatecache(0, 0));
  }

  // sequential read
  {
    unique_ptr<sequentialfile> file;
    char scratch[100];
    slice result;
    assert_ok(env_->newsequentialfile(fname, &file, soptions));
    assert_ok(file.get()->read(11, &result, scratch));
    assert_eq(memcmp(scratch, "hello world", 11), 0);
    assert_ok(file.get()->invalidatecache(0, 11));
    assert_ok(file.get()->invalidatecache(0, 0));
  }
  // delete the file
  assert_ok(env_->deletefile(fname));
}
#endif

test(envposixtest, posixrandomrwfiletest) {
  envoptions soptions;
  soptions.use_mmap_writes = soptions.use_mmap_reads = false;
  std::string fname = test::tmpdir() + "/" + "testfile";

  unique_ptr<randomrwfile> file;
  assert_ok(env_->newrandomrwfile(fname, &file, soptions));
  // if you run the unit test on tmpfs, then tmpfs might not
  // support fallocate. it is still better to trigger that
  // code-path instead of eliminating it completely.
  file.get()->allocate(0, 10*1024*1024);
  assert_ok(file.get()->write(100, slice("hello world")));
  assert_ok(file.get()->write(105, slice("hello world")));
  assert_ok(file.get()->sync());
  assert_ok(file.get()->fsync());
  char scratch[100];
  slice result;
  assert_ok(file.get()->read(100, 16, &result, scratch));
  assert_eq(result.compare("hellohello world"), 0);
  assert_ok(file.get()->close());
}

class testlogger : public logger {
 public:
  virtual void logv(const char* format, va_list ap) override {
    log_count++;

    char new_format[550];
    std::fill_n(new_format, sizeof(new_format), '2');
    {
      va_list backup_ap;
      va_copy(backup_ap, ap);
      int n = vsnprintf(new_format, sizeof(new_format) - 1, format, backup_ap);
      // 48 bytes for extra information + bytes allocated

      if (new_format[0] == '[') {
        // "[debug] "
        assert_true(n <= 56 + (512 - static_cast<int>(sizeof(struct timeval))));
      } else {
        assert_true(n <= 48 + (512 - static_cast<int>(sizeof(struct timeval))));
      }
      va_end(backup_ap);
    }

    for (size_t i = 0; i < sizeof(new_format); i++) {
      if (new_format[i] == 'x') {
        char_x_count++;
      } else if (new_format[i] == '\0') {
        char_0_count++;
      }
    }
  }
  int log_count;
  int char_x_count;
  int char_0_count;
};

test(envposixtest, logbuffertest) {
  testlogger test_logger;
  test_logger.setinfologlevel(infologlevel::info_level);
  test_logger.log_count = 0;
  test_logger.char_x_count = 0;
  test_logger.char_0_count = 0;
  logbuffer log_buffer(infologlevel::info_level, &test_logger);
  logbuffer log_buffer_debug(debug_level, &test_logger);

  char bytes200[200];
  std::fill_n(bytes200, sizeof(bytes200), '1');
  bytes200[sizeof(bytes200) - 1] = '\0';
  char bytes600[600];
  std::fill_n(bytes600, sizeof(bytes600), '1');
  bytes600[sizeof(bytes600) - 1] = '\0';
  char bytes9000[9000];
  std::fill_n(bytes9000, sizeof(bytes9000), '1');
  bytes9000[sizeof(bytes9000) - 1] = '\0';

  logtobuffer(&log_buffer, "x%sx", bytes200);
  logtobuffer(&log_buffer, "x%sx", bytes600);
  logtobuffer(&log_buffer, "x%sx%sx%sx", bytes200, bytes200, bytes200);
  logtobuffer(&log_buffer, "x%sx%sx", bytes200, bytes600);
  logtobuffer(&log_buffer, "x%sx%sx", bytes600, bytes9000);

  logtobuffer(&log_buffer_debug, "x%sx", bytes200);
  test_logger.setinfologlevel(debug_level);
  logtobuffer(&log_buffer_debug, "x%sx%sx%sx", bytes600, bytes9000, bytes200);

  assert_eq(0, test_logger.log_count);
  log_buffer.flushbuffertolog();
  log_buffer_debug.flushbuffertolog();
  assert_eq(6, test_logger.log_count);
  assert_eq(6, test_logger.char_0_count);
  assert_eq(10, test_logger.char_x_count);
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
