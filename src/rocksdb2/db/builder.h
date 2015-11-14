//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
#pragma once
#include "rocksdb/comparator.h"
#include "rocksdb/env.h"
#include "rocksdb/status.h"
#include "rocksdb/types.h"
#include "rocksdb/options.h"

namespace rocksdb {

struct options;
struct filemetadata;

class env;
struct envoptions;
class iterator;
class tablecache;
class versionedit;
class tablebuilder;
class writablefile;

extern tablebuilder* newtablebuilder(
    const options& options, const internalkeycomparator& internal_comparator,
    writablefile* file, compressiontype compression_type);

// build a table file from the contents of *iter.  the generated file
// will be named according to number specified in meta. on success, the rest of
// *meta will be filled with metadata about the generated table.
// if no data is present in *iter, meta->file_size will be set to
// zero, and no table file will be produced.
extern status buildtable(const std::string& dbname, env* env,
                         const options& options, const envoptions& soptions,
                         tablecache* table_cache, iterator* iter,
                         filemetadata* meta,
                         const internalkeycomparator& internal_comparator,
                         const sequencenumber newest_snapshot,
                         const sequencenumber earliest_seqno_in_memtable,
                         const compressiontype compression,
                         const env::iopriority io_priority = env::io_high);

}  // namespace rocksdb
