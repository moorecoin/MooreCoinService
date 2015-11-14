// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

#pragma once

#include <string>
#include "rocksdb/table.h"

namespace rocksdb {

class slice;
class blockbuilder;
struct options;

// flushblockpolicy provides a configurable way to determine when to flush a
// block in the block based tables,
class flushblockpolicy {
 public:
  // keep track of the key/value sequences and return the boolean value to
  // determine if table builder should flush current data block.
  virtual bool update(const slice& key,
                      const slice& value) = 0;

  virtual ~flushblockpolicy() { }
};

class flushblockpolicyfactory {
 public:
  // return the name of the flush block policy.
  virtual const char* name() const = 0;

  // return a new block flush policy that flushes data blocks by data size.
  // flushblockpolicy may need to access the metadata of the data block
  // builder to determine when to flush the blocks.
  //
  // callers must delete the result after any database that is using the
  // result has been closed.
  virtual flushblockpolicy* newflushblockpolicy(
      const blockbasedtableoptions& table_options,
      const blockbuilder& data_block_builder) const = 0;

  virtual ~flushblockpolicyfactory() { }
};

class flushblockbysizepolicyfactory : public flushblockpolicyfactory {
 public:
  flushblockbysizepolicyfactory() {}

  virtual const char* name() const override {
    return "flushblockbysizepolicyfactory";
  }

  virtual flushblockpolicy* newflushblockpolicy(
      const blockbasedtableoptions& table_options,
      const blockbuilder& data_block_builder) const override;
};

}  // rocksdb
