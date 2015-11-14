//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#include <string>
#include <cmath>
#include <iostream>
#include <fstream>
#include <iterator>
#include <algorithm>
#include "util/testharness.h"
#include "util/auto_roll_logger.h"
#include "rocksdb/db.h"
#include <sys/stat.h>
#include <errno.h>

using namespace std;

namespace rocksdb {

class autorollloggertest {
 public:
  static void inittestdb() {
    string deletecmd = "rm -rf " + ktestdir;
    assert_true(system(deletecmd.c_str()) == 0);
    env::default()->createdir(ktestdir);
  }

  void rolllogfilebysizetest(autorolllogger* logger,
                             size_t log_max_size,
                             const string& log_message);
  uint64_t rolllogfilebytimetest(autorolllogger* logger,
                                 size_t time,
                                 const string& log_message);

  static const string ksamplemessage;
  static const string ktestdir;
  static const string klogfile;
  static env* env;
};

const string autorollloggertest::ksamplemessage(
    "this is the message to be written to the log file!!");
const string autorollloggertest::ktestdir(test::tmpdir() + "/db_log_test");
const string autorollloggertest::klogfile(test::tmpdir() + "/db_log_test/log");
env* autorollloggertest::env = env::default();

// in this test we only want to log some simple log message with
// no format. logmessage() provides such a simple interface and
// avoids the [format-security] warning which occurs when you
// call log(logger, log_message) directly.
namespace {
void logmessage(logger* logger, const char* message) {
  log(logger, "%s", message);
}

void logmessage(const infologlevel log_level, logger* logger,
                const char* message) {
  log(log_level, logger, "%s", message);
}
}  // namespace

namespace {
void getfilecreatetime(const std::string& fname, uint64_t* file_ctime) {
  struct stat s;
  if (stat(fname.c_str(), &s) != 0) {
    *file_ctime = (uint64_t)0;
  }
  *file_ctime = static_cast<uint64_t>(s.st_ctime);
}
}  // namespace

void autorollloggertest::rolllogfilebysizetest(autorolllogger* logger,
                                               size_t log_max_size,
                                               const string& log_message) {
  logger->setinfologlevel(infologlevel::info_level);
  // measure the size of each message, which is supposed
  // to be equal or greater than log_message.size()
  logmessage(logger, log_message.c_str());
  size_t message_size = logger->getlogfilesize();
  size_t current_log_size = message_size;

  // test the cases when the log file will not be rolled.
  while (current_log_size + message_size < log_max_size) {
    logmessage(logger, log_message.c_str());
    current_log_size += message_size;
    assert_eq(current_log_size, logger->getlogfilesize());
  }

  // now the log file will be rolled
  logmessage(logger, log_message.c_str());
  // since rotation is checked before actual logging, we need to
  // trigger the rotation by logging another message.
  logmessage(logger, log_message.c_str());

  assert_true(message_size == logger->getlogfilesize());
}

uint64_t autorollloggertest::rolllogfilebytimetest(
    autorolllogger* logger, size_t time, const string& log_message) {
  uint64_t expected_create_time;
  uint64_t actual_create_time;
  uint64_t total_log_size;
  assert_ok(env->getfilesize(klogfile, &total_log_size));
  getfilecreatetime(klogfile, &expected_create_time);
  logger->setcallnowmicroseverynrecords(0);

  // -- write to the log for several times, which is supposed
  // to be finished before time.
  for (int i = 0; i < 10; ++i) {
     logmessage(logger, log_message.c_str());
     assert_ok(logger->getstatus());
     // make sure we always write to the same log file (by
     // checking the create time);
     getfilecreatetime(klogfile, &actual_create_time);

     // also make sure the log size is increasing.
     assert_eq(expected_create_time, actual_create_time);
     assert_gt(logger->getlogfilesize(), total_log_size);
     total_log_size = logger->getlogfilesize();
  }

  // -- make the log file expire
  sleep(time);
  logmessage(logger, log_message.c_str());

  // at this time, the new log file should be created.
  getfilecreatetime(klogfile, &actual_create_time);
  assert_gt(actual_create_time, expected_create_time);
  assert_lt(logger->getlogfilesize(), total_log_size);
  expected_create_time = actual_create_time;

  return expected_create_time;
}

test(autorollloggertest, rolllogfilebysize) {
    inittestdb();
    size_t log_max_size = 1024 * 5;

    autorolllogger logger(env::default(), ktestdir, "", log_max_size, 0);

    rolllogfilebysizetest(&logger, log_max_size,
                          ksamplemessage + ":rolllogfilebysize");
}

test(autorollloggertest, rolllogfilebytime) {
    size_t time = 2;
    size_t log_size = 1024 * 5;

    inittestdb();
    // -- test the existence of file during the server restart.
    assert_true(!env->fileexists(klogfile));
    autorolllogger logger(env::default(), ktestdir, "", log_size, time);
    assert_true(env->fileexists(klogfile));

    rolllogfilebytimetest(&logger, time, ksamplemessage + ":rolllogfilebytime");
}

test(autorollloggertest,
     openlogfilesmultipletimeswithoptionlog_max_size) {
  // if only 'log_max_size' options is specified, then every time
  // when rocksdb is restarted, a new empty log file will be created.
  inittestdb();
  // workaround:
  // avoid complier's complaint of "comparison between signed
  // and unsigned integer expressions" because literal 0 is
  // treated as "singed".
  size_t kzero = 0;
  size_t log_size = 1024;

  autorolllogger* logger = new autorolllogger(
    env::default(), ktestdir, "", log_size, 0);

  logmessage(logger, ksamplemessage.c_str());
  assert_gt(logger->getlogfilesize(), kzero);
  delete logger;

  // reopens the log file and an empty log file will be created.
  logger = new autorolllogger(
    env::default(), ktestdir, "", log_size, 0);
  assert_eq(logger->getlogfilesize(), kzero);
  delete logger;
}

test(autorollloggertest, compositerollbytimeandsizelogger) {
  size_t time = 2, log_max_size = 1024 * 5;

  inittestdb();

  autorolllogger logger(env::default(), ktestdir, "", log_max_size, time);

  // test the ability to roll by size
  rolllogfilebysizetest(
      &logger, log_max_size,
      ksamplemessage + ":compositerollbytimeandsizelogger");

  // test the ability to roll by time
  rolllogfilebytimetest( &logger, time,
      ksamplemessage + ":compositerollbytimeandsizelogger");
}

test(autorollloggertest, createloggerfromoptions) {
  dboptions options;
  shared_ptr<logger> logger;

  // normal logger
  assert_ok(createloggerfromoptions(ktestdir, "", env, options, &logger));
  assert_true(dynamic_cast<posixlogger*>(logger.get()));

  // only roll by size
  inittestdb();
  options.max_log_file_size = 1024;
  assert_ok(createloggerfromoptions(ktestdir, "", env, options, &logger));
  autorolllogger* auto_roll_logger =
    dynamic_cast<autorolllogger*>(logger.get());
  assert_true(auto_roll_logger);
  rolllogfilebysizetest(
      auto_roll_logger, options.max_log_file_size,
      ksamplemessage + ":createloggerfromoptions - size");

  // only roll by time
  inittestdb();
  options.max_log_file_size = 0;
  options.log_file_time_to_roll = 2;
  assert_ok(createloggerfromoptions(ktestdir, "", env, options, &logger));
  auto_roll_logger =
    dynamic_cast<autorolllogger*>(logger.get());
  rolllogfilebytimetest(
      auto_roll_logger, options.log_file_time_to_roll,
      ksamplemessage + ":createloggerfromoptions - time");

  // roll by both time and size
  inittestdb();
  options.max_log_file_size = 1024 * 5;
  options.log_file_time_to_roll = 2;
  assert_ok(createloggerfromoptions(ktestdir, "", env, options, &logger));
  auto_roll_logger =
    dynamic_cast<autorolllogger*>(logger.get());
  rolllogfilebysizetest(
      auto_roll_logger, options.max_log_file_size,
      ksamplemessage + ":createloggerfromoptions - both");
  rolllogfilebytimetest(
      auto_roll_logger, options.log_file_time_to_roll,
      ksamplemessage + ":createloggerfromoptions - both");
}

test(autorollloggertest, infologlevel) {
  inittestdb();

  size_t log_size = 8192;
  size_t log_lines = 0;
  // an extra-scope to force the autorolllogger to flush the log file when it
  // becomes out of scope.
  {
    autorolllogger logger(env::default(), ktestdir, "", log_size, 0);
    for (int log_level = infologlevel::fatal_level;
         log_level >= infologlevel::debug_level; log_level--) {
      logger.setinfologlevel((infologlevel)log_level);
      for (int log_type = infologlevel::debug_level;
           log_type <= infologlevel::fatal_level; log_type++) {
        // log messages with log level smaller than log_level will not be
        // logged.
        logmessage((infologlevel)log_type, &logger, ksamplemessage.c_str());
      }
      log_lines += infologlevel::fatal_level - log_level + 1;
    }
    for (int log_level = infologlevel::fatal_level;
         log_level >= infologlevel::debug_level; log_level--) {
      logger.setinfologlevel((infologlevel)log_level);

      // again, messages with level smaller than log_level will not be logged.
      debug(&logger, "%s", ksamplemessage.c_str());
      info(&logger, "%s", ksamplemessage.c_str());
      warn(&logger, "%s", ksamplemessage.c_str());
      error(&logger, "%s", ksamplemessage.c_str());
      fatal(&logger, "%s", ksamplemessage.c_str());
      log_lines += infologlevel::fatal_level - log_level + 1;
    }
  }
  std::ifstream infile(autorollloggertest::klogfile.c_str());
  size_t lines = std::count(std::istreambuf_iterator<char>(infile),
                         std::istreambuf_iterator<char>(), '\n');
  assert_eq(log_lines, lines);
  infile.close();
}

test(autorollloggertest, logfileexistence) {
  rocksdb::db* db;
  rocksdb::options options;
  string deletecmd = "rm -rf " + ktestdir;
  assert_eq(system(deletecmd.c_str()), 0);
  options.max_log_file_size = 100 * 1024 * 1024;
  options.create_if_missing = true;
  assert_ok(rocksdb::db::open(options, ktestdir, &db));
  assert_true(env->fileexists(klogfile));
  delete db;
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
