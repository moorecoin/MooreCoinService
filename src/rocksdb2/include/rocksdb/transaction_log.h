// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

#ifndef storage_rocksdb_include_transaction_log_iterator_h_
#define storage_rocksdb_include_transaction_log_iterator_h_

#include "rocksdb/status.h"
#include "rocksdb/types.h"
#include "rocksdb/write_batch.h"
#include <memory>
#include <vector>

namespace rocksdb {

class logfile;
typedef std::vector<std::unique_ptr<logfile>> vectorlogptr;

enum  walfiletype {
  /* indicates that wal file is in archive directory. wal files are moved from
   * the main db directory to archive directory once they are not live and stay
   * there until cleaned up. files are cleaned depending on archive size
   * (options::wal_size_limit_mb) and time since last cleaning
   * (options::wal_ttl_seconds).
   */
  karchivedlogfile = 0,

  /* indicates that wal file is live and resides in the main db directory */
  kalivelogfile = 1
} ;

class logfile {
 public:
  logfile() {}
  virtual ~logfile() {}

  // returns log file's pathname relative to the main db dir
  // eg. for a live-log-file = /000003.log
  //     for an archived-log-file = /archive/000003.log
  virtual std::string pathname() const = 0;


  // primary identifier for log file.
  // this is directly proportional to creation time of the log file
  virtual uint64_t lognumber() const = 0;

  // log file can be either alive or archived
  virtual walfiletype type() const = 0;

  // starting sequence number of writebatch written in this log file
  virtual sequencenumber startsequence() const = 0;

  // size of log file on disk in bytes
  virtual uint64_t sizefilebytes() const = 0;
};

struct batchresult {
  sequencenumber sequence = 0;
  std::unique_ptr<writebatch> writebatchptr;
};

// a transactionlogiterator is used to iterate over the transactions in a db.
// one run of the iterator is continuous, i.e. the iterator will stop at the
// beginning of any gap in sequences
class transactionlogiterator {
 public:
  transactionlogiterator() {}
  virtual ~transactionlogiterator() {}

  // an iterator is either positioned at a writebatch or not valid.
  // this method returns true if the iterator is valid.
  // can read data from a valid iterator.
  virtual bool valid() = 0;

  // moves the iterator to the next writebatch.
  // requires: valid() to be true.
  virtual void next() = 0;

  // returns ok if the iterator is valid.
  // returns the error when something has gone wrong.
  virtual status status() = 0;

  // if valid return's the current write_batch and the sequence number of the
  // earliest transaction contained in the batch.
  // only use if valid() is true and status() is ok.
  virtual batchresult getbatch() = 0;

  // the read options for transactionlogiterator.
  struct readoptions {
    // if true, all data read from underlying storage will be
    // verified against corresponding checksums.
    // default: true
    bool verify_checksums_;

    readoptions() : verify_checksums_(true) {}

    explicit readoptions(bool verify_checksums)
        : verify_checksums_(verify_checksums) {}
  };
};
} //  namespace rocksdb

#endif  // storage_rocksdb_include_transaction_log_iterator_h_
