// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_hyperleveldb_db_log_writer_h_
#define storage_hyperleveldb_db_log_writer_h_

#include <stdint.h>
#include "log_format.h"
#include "../hyperleveldb/slice.h"
#include "../hyperleveldb/status.h"
#include "../port/port.h"

namespace hyperleveldb {

class writablefile;

namespace log {

class writer {
 public:
  // create a writer that will append data to "*dest".
  // "*dest" must be initially empty.
  // "*dest" must remain live while this writer is in use.
  explicit writer(writablefile* dest);
  ~writer();

  status addrecord(const slice& slice);

 private:
  writablefile* dest_;
  port::mutex offset_mtx_;
  uint64_t offset_; // current offset in file

  // crc32c values for all supported record types.  these are
  // pre-computed to reduce the overhead of computing the crc of the
  // record type stored in the header.
  uint32_t type_crc_[kmaxrecordtype + 1];

  status emitphysicalrecordat(recordtype type, const char* ptr, uint64_t offset, size_t length);

  // no copying allowed
  writer(const writer&);
  void operator=(const writer&);
};

}  // namespace log
}  // namespace hyperleveldb

#endif  // storage_hyperleveldb_db_log_writer_h_
