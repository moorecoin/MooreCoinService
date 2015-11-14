//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#ifndef rocksdb_lite
#include "db/transaction_log_impl.h"
#include "db/write_batch_internal.h"

namespace rocksdb {

transactionlogiteratorimpl::transactionlogiteratorimpl(
    const std::string& dir, const dboptions* options,
    const transactionlogiterator::readoptions& read_options,
    const envoptions& soptions, const sequencenumber seq,
    std::unique_ptr<vectorlogptr> files, dbimpl const* const dbimpl)
    : dir_(dir),
      options_(options),
      read_options_(read_options),
      soptions_(soptions),
      startingsequencenumber_(seq),
      files_(std::move(files)),
      started_(false),
      isvalid_(false),
      currentfileindex_(0),
      currentbatchseq_(0),
      currentlastseq_(0),
      dbimpl_(dbimpl) {
  assert(files_ != nullptr);
  assert(dbimpl_ != nullptr);

  reporter_.env = options_->env;
  reporter_.info_log = options_->info_log.get();
  seektostartsequence(); // seek till starting sequence
}

status transactionlogiteratorimpl::openlogfile(
    const logfile* logfile,
    unique_ptr<sequentialfile>* file) {
  env* env = options_->env;
  if (logfile->type() == karchivedlogfile) {
    std::string fname = archivedlogfilename(dir_, logfile->lognumber());
    return env->newsequentialfile(fname, file, soptions_);
  } else {
    std::string fname = logfilename(dir_, logfile->lognumber());
    status status = env->newsequentialfile(fname, file, soptions_);
    if (!status.ok()) {
      //  if cannot open file in db directory.
      //  try the archive dir, as it could have moved in the meanwhile.
      fname = archivedlogfilename(dir_, logfile->lognumber());
      status = env->newsequentialfile(fname, file, soptions_);
    }
    return status;
  }
}

batchresult transactionlogiteratorimpl::getbatch()  {
  assert(isvalid_);  //  cannot call in a non valid state.
  batchresult result;
  result.sequence = currentbatchseq_;
  result.writebatchptr = std::move(currentbatch_);
  return result;
}

status transactionlogiteratorimpl::status() {
  return currentstatus_;
}

bool transactionlogiteratorimpl::valid() {
  return started_ && isvalid_;
}

bool transactionlogiteratorimpl::restrictedread(
    slice* record,
    std::string* scratch) {
  // don't read if no more complete entries to read from logs
  if (currentlastseq_ >= dbimpl_->getlatestsequencenumber()) {
    return false;
  }
  return currentlogreader_->readrecord(record, scratch);
}

void transactionlogiteratorimpl::seektostartsequence(
    uint64_t startfileindex,
    bool strict) {
  std::string scratch;
  slice record;
  started_ = false;
  isvalid_ = false;
  if (files_->size() <= startfileindex) {
    return;
  }
  status s = openlogreader(files_->at(startfileindex).get());
  if (!s.ok()) {
    currentstatus_ = s;
    reporter_.info(currentstatus_.tostring().c_str());
    return;
  }
  while (restrictedread(&record, &scratch)) {
    if (record.size() < 12) {
      reporter_.corruption(
        record.size(), status::corruption("very small log record"));
      continue;
    }
    updatecurrentwritebatch(record);
    if (currentlastseq_ >= startingsequencenumber_) {
      if (strict && currentbatchseq_ != startingsequencenumber_) {
        currentstatus_ = status::corruption("gap in sequence number. could not "
                                            "seek to required sequence number");
        reporter_.info(currentstatus_.tostring().c_str());
        return;
      } else if (strict) {
        reporter_.info("could seek required sequence number. iterator will "
                       "continue.");
      }
      isvalid_ = true;
      started_ = true; // set started_ as we could seek till starting sequence
      return;
    } else {
      isvalid_ = false;
    }
  }

  // could not find start sequence in first file. normally this must be the
  // only file. otherwise log the error and let the iterator return next entry
  // if strict is set, we want to seek exactly till the start sequence and it
  // should have been present in the file we scanned above
  if (strict) {
    currentstatus_ = status::corruption("gap in sequence number. could not "
                                        "seek to required sequence number");
    reporter_.info(currentstatus_.tostring().c_str());
  } else if (files_->size() != 1) {
    currentstatus_ = status::corruption("start sequence was not found, "
                                        "skipping to the next available");
    reporter_.info(currentstatus_.tostring().c_str());
    // let nextimpl find the next available entry. started_ remains false
    // because we don't want to check for gaps while moving to start sequence
    nextimpl(true);
  }
}

void transactionlogiteratorimpl::next() {
  return nextimpl(false);
}

void transactionlogiteratorimpl::nextimpl(bool internal) {
  std::string scratch;
  slice record;
  isvalid_ = false;
  if (!internal && !started_) {
    // runs every time until we can seek to the start sequence
    return seektostartsequence();
  }
  while(true) {
    assert(currentlogreader_);
    if (currentlogreader_->iseof()) {
      currentlogreader_->unmarkeof();
    }
    while (restrictedread(&record, &scratch)) {
      if (record.size() < 12) {
        reporter_.corruption(
          record.size(), status::corruption("very small log record"));
        continue;
      } else {
        // started_ should be true if called by application
        assert(internal || started_);
        // started_ should be false if called internally
        assert(!internal || !started_);
        updatecurrentwritebatch(record);
        if (internal && !started_) {
          started_ = true;
        }
        return;
      }
    }

    // open the next file
    if (currentfileindex_ < files_->size() - 1) {
      ++currentfileindex_;
      status status =openlogreader(files_->at(currentfileindex_).get());
      if (!status.ok()) {
        isvalid_ = false;
        currentstatus_ = status;
        return;
      }
    } else {
      isvalid_ = false;
      if (currentlastseq_ == dbimpl_->getlatestsequencenumber()) {
        currentstatus_ = status::ok();
      } else {
        currentstatus_ = status::corruption("no more data left");
      }
      return;
    }
  }
}

bool transactionlogiteratorimpl::isbatchexpected(
    const writebatch* batch,
    const sequencenumber expectedseq) {
  assert(batch);
  sequencenumber batchseq = writebatchinternal::sequence(batch);
  if (batchseq != expectedseq) {
    char buf[200];
    snprintf(buf, sizeof(buf),
             "discontinuity in log records. got seq=%lu, expected seq=%lu, "
             "last flushed seq=%lu.log iterator will reseek the correct "
             "batch.",
             (unsigned long)batchseq,
             (unsigned long)expectedseq,
             (unsigned long)dbimpl_->getlatestsequencenumber());
    reporter_.info(buf);
    return false;
  }
  return true;
}

void transactionlogiteratorimpl::updatecurrentwritebatch(const slice& record) {
  std::unique_ptr<writebatch> batch(new writebatch());
  writebatchinternal::setcontents(batch.get(), record);

  sequencenumber expectedseq = currentlastseq_ + 1;
  // if the iterator has started, then confirm that we get continuous batches
  if (started_ && !isbatchexpected(batch.get(), expectedseq)) {
    // seek to the batch having expected sequence number
    if (expectedseq < files_->at(currentfileindex_)->startsequence()) {
      // expected batch must lie in the previous log file
      // avoid underflow.
      if (currentfileindex_ != 0) {
        currentfileindex_--;
      }
    }
    startingsequencenumber_ = expectedseq;
    // currentstatus_ will be set to ok if reseek succeeds
    currentstatus_ = status::notfound("gap in sequence numbers");
    return seektostartsequence(currentfileindex_, true);
  }

  currentbatchseq_ = writebatchinternal::sequence(batch.get());
  currentlastseq_ = currentbatchseq_ +
                    writebatchinternal::count(batch.get()) - 1;
  // currentbatchseq_ can only change here
  assert(currentlastseq_ <= dbimpl_->getlatestsequencenumber());

  currentbatch_ = move(batch);
  isvalid_ = true;
  currentstatus_ = status::ok();
}

status transactionlogiteratorimpl::openlogreader(const logfile* logfile) {
  unique_ptr<sequentialfile> file;
  status status = openlogfile(logfile, &file);
  if (!status.ok()) {
    return status;
  }
  assert(file);
  currentlogreader_.reset(new log::reader(std::move(file), &reporter_,
                                          read_options_.verify_checksums_, 0));
  return status::ok();
}
}  //  namespace rocksdb
#endif  // rocksdb_lite
