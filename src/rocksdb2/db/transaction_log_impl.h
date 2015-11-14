//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#ifndef rocksdb_lite
#pragma once
#include <vector>

#include "rocksdb/env.h"
#include "rocksdb/options.h"
#include "rocksdb/types.h"
#include "rocksdb/transaction_log.h"
#include "db/db_impl.h"
#include "db/log_reader.h"
#include "db/filename.h"

namespace rocksdb {

struct logreporter : public log::reader::reporter {
  env* env;
  logger* info_log;
  virtual void corruption(size_t bytes, const status& s) {
    log(info_log, "dropping %zu bytes; %s", bytes, s.tostring().c_str());
  }
  virtual void info(const char* s) {
    log(info_log, "%s", s);
  }
};

class logfileimpl : public logfile {
 public:
  logfileimpl(uint64_t lognum, walfiletype logtype, sequencenumber startseq,
              uint64_t sizebytes) :
    lognumber_(lognum),
    type_(logtype),
    startsequence_(startseq),
    sizefilebytes_(sizebytes) {
  }

  std::string pathname() const {
    if (type_ == karchivedlogfile) {
      return archivedlogfilename("", lognumber_);
    }
    return logfilename("", lognumber_);
  }

  uint64_t lognumber() const { return lognumber_; }

  walfiletype type() const { return type_; }

  sequencenumber startsequence() const { return startsequence_; }

  uint64_t sizefilebytes() const { return sizefilebytes_; }

  bool operator < (const logfile& that) const {
    return lognumber() < that.lognumber();
  }

 private:
  uint64_t lognumber_;
  walfiletype type_;
  sequencenumber startsequence_;
  uint64_t sizefilebytes_;

};

class transactionlogiteratorimpl : public transactionlogiterator {
 public:
  transactionlogiteratorimpl(
      const std::string& dir, const dboptions* options,
      const transactionlogiterator::readoptions& read_options,
      const envoptions& soptions, const sequencenumber seqnum,
      std::unique_ptr<vectorlogptr> files, dbimpl const* const dbimpl);

  virtual bool valid();

  virtual void next();

  virtual status status();

  virtual batchresult getbatch();

 private:
  const std::string& dir_;
  const dboptions* options_;
  const transactionlogiterator::readoptions read_options_;
  const envoptions& soptions_;
  sequencenumber startingsequencenumber_;
  std::unique_ptr<vectorlogptr> files_;
  bool started_;
  bool isvalid_;  // not valid when it starts of.
  status currentstatus_;
  size_t currentfileindex_;
  std::unique_ptr<writebatch> currentbatch_;
  unique_ptr<log::reader> currentlogreader_;
  status openlogfile(const logfile* logfile, unique_ptr<sequentialfile>* file);
  logreporter reporter_;
  sequencenumber currentbatchseq_; // sequence number at start of current batch
  sequencenumber currentlastseq_; // last sequence in the current batch
  dbimpl const * const dbimpl_; // the db on whose log files this iterates

  // reads from transaction log only if the writebatch record has been written
  bool restrictedread(slice* record, std::string* scratch);
  // seeks to startingsequencenumber reading from startfileindex in files_.
  // if strict is set,then must get a batch starting with startingsequencenumber
  void seektostartsequence(uint64_t startfileindex = 0, bool strict = false);
  // implementation of next. seektostartsequence calls it internally with
  // internal=true to let it find next entry even if it has to jump gaps because
  // the iterator may start off from the first available entry but promises to
  // be continuous after that
  void nextimpl(bool internal = false);
  // check if batch is expected, else return false
  bool isbatchexpected(const writebatch* batch, sequencenumber expectedseq);
  // update current batch if a continuous batch is found, else return false
  void updatecurrentwritebatch(const slice& record);
  status openlogreader(const logfile* file);
};
}  //  namespace rocksdb
#endif  // rocksdb_lite
