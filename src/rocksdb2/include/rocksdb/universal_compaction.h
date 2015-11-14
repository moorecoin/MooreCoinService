// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

#ifndef storage_rocksdb_universal_compaction_options_h
#define storage_rocksdb_universal_compaction_options_h

#include <stdint.h>
#include <climits>
#include <vector>

namespace rocksdb {

//
// algorithm used to make a compaction request stop picking new files
// into a single compaction run
//
enum compactionstopstyle {
  kcompactionstopstylesimilarsize, // pick files of similar size
  kcompactionstopstyletotalsize    // total size of picked files > next file
};

class compactionoptionsuniversal {
 public:

  // percentage flexibilty while comparing file size. if the candidate file(s)
  // size is 1% smaller than the next file's size, then include next file into
  // this candidate set. // default: 1
  unsigned int size_ratio;

  // the minimum number of files in a single compaction run. default: 2
  unsigned int min_merge_width;

  // the maximum number of files in a single compaction run. default: uint_max
  unsigned int max_merge_width;

  // the size amplification is defined as the amount (in percentage) of
  // additional storage needed to store a single byte of data in the database.
  // for example, a size amplification of 2% means that a database that
  // contains 100 bytes of user-data may occupy upto 102 bytes of
  // physical storage. by this definition, a fully compacted database has
  // a size amplification of 0%. rocksdb uses the following heuristic
  // to calculate size amplification: it assumes that all files excluding
  // the earliest file contribute to the size amplification.
  // default: 200, which means that a 100 byte database could require upto
  // 300 bytes of storage.
  unsigned int max_size_amplification_percent;

  // if this option is set to be -1 (the default value), all the output files
  // will follow compression type specified.
  //
  // if this option is not negative, we will try to make sure compressed
  // size is just above this value. in normal cases, at least this percentage
  // of data will be compressed.
  // when we are compacting to a new file, here is the criteria whether
  // it needs to be compressed: assuming here are the list of files sorted
  // by generation time:
  //    a1...an b1...bm c1...ct
  // where a1 is the newest and ct is the oldest, and we are going to compact
  // b1...bm, we calculate the total size of all the files as total_size, as
  // well as  the total size of c1...ct as total_c, the compaction output file
  // will be compressed iff
  //   total_c / total_size < this percentage
  // default: -1
  int compression_size_percent;

  // the algorithm used to stop picking files into a single compaction run
  // default: kcompactionstopstyletotalsize
  compactionstopstyle stop_style;

  // default set of parameters
  compactionoptionsuniversal()
      : size_ratio(1),
        min_merge_width(2),
        max_merge_width(uint_max),
        max_size_amplification_percent(200),
        compression_size_percent(-1),
        stop_style(kcompactionstopstyletotalsize) {}
};

}  // namespace rocksdb

#endif  // storage_rocksdb_universal_compaction_options_h
