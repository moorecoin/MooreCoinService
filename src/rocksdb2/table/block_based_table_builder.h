//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include <stdint.h>
#include <limits>

#include "rocksdb/flush_block_policy.h"
#include "rocksdb/options.h"
#include "rocksdb/status.h"
#include "table/table_builder.h"

namespace rocksdb {

class blockbuilder;
class blockhandle;
class writablefile;
struct blockbasedtableoptions;

class blockbasedtablebuilder : public tablebuilder {
 public:
  // create a builder that will store the contents of the table it is
  // building in *file.  does not close the file.  it is up to the
  // caller to close the file after calling finish().
  blockbasedtablebuilder(const options& options,
                         const blockbasedtableoptions& table_options,
                         const internalkeycomparator& internal_comparator,
                         writablefile* file, compressiontype compression_type);

  // requires: either finish() or abandon() has been called.
  ~blockbasedtablebuilder();

  // add key,value to the table being constructed.
  // requires: key is after any previously added key according to comparator.
  // requires: finish(), abandon() have not been called
  void add(const slice& key, const slice& value) override;

  // return non-ok iff some error has been detected.
  status status() const override;

  // finish building the table.  stops using the file passed to the
  // constructor after this function returns.
  // requires: finish(), abandon() have not been called
  status finish() override;

  // indicate that the contents of this builder should be abandoned.  stops
  // using the file passed to the constructor after this function returns.
  // if the caller is not going to call finish(), it must call abandon()
  // before destroying this builder.
  // requires: finish(), abandon() have not been called
  void abandon() override;

  // number of calls to add() so far.
  uint64_t numentries() const override;

  // size of the file generated so far.  if invoked after a successful
  // finish() call, returns the size of the final generated file.
  uint64_t filesize() const override;

 private:
  bool ok() const { return status().ok(); }
  // call block's finish() method and then write the finalize block contents to
  // file.
  void writeblock(blockbuilder* block, blockhandle* handle);
  // directly write block content to the file.
  void writeblock(const slice& block_contents, blockhandle* handle);
  void writerawblock(const slice& data, compressiontype, blockhandle* handle);
  status insertblockincache(const slice& block_contents,
                            const compressiontype type,
                            const blockhandle* handle);
  struct rep;
  class blockbasedtablepropertiescollectorfactory;
  class blockbasedtablepropertiescollector;
  rep* rep_;

  // advanced operation: flush any buffered key/value pairs to file.
  // can be used to ensure that two adjacent entries never live in
  // the same data block.  most clients should not need to use this method.
  // requires: finish(), abandon() have not been called
  void flush();

  // some compression libraries fail when the raw size is bigger than int. if
  // uncompressed size is bigger than kcompressionsizelimit, don't compress it
  const uint64_t kcompressionsizelimit = std::numeric_limits<int>::max();

  // no copying allowed
  blockbasedtablebuilder(const blockbasedtablebuilder&) = delete;
  void operator=(const blockbasedtablebuilder&) = delete;
};

}  // namespace rocksdb
