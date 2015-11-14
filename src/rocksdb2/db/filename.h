//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// file names used by db code

#pragma once
#include <stdint.h>
#include <unordered_map>
#include <string>
#include <vector>
#include "rocksdb/slice.h"
#include "rocksdb/status.h"
#include "rocksdb/transaction_log.h"
#include "port/port.h"

namespace rocksdb {

class env;
class directory;

enum filetype {
  klogfile,
  kdblockfile,
  ktablefile,
  kdescriptorfile,
  kcurrentfile,
  ktempfile,
  kinfologfile,  // either the current one, or an old one
  kmetadatabase,
  kidentityfile
};

// map from file number to path id.
typedef std::unordered_map<uint64_t, uint32_t> filenumtopathidmap;

// return the name of the log file with the specified number
// in the db named by "dbname".  the result will be prefixed with
// "dbname".
extern std::string logfilename(const std::string& dbname, uint64_t number);

static const std::string archival_dir = "archive";

extern std::string archivaldirectory(const std::string& dbname);

//  return the name of the archived log file with the specified number
//  in the db named by "dbname". the result will be prefixed with "dbname".
extern std::string archivedlogfilename(const std::string& dbname,
                                       uint64_t num);

extern std::string maketablefilename(const std::string& name, uint64_t number);

// return the name of the sstable with the specified number
// in the db named by "dbname".  the result will be prefixed with
// "dbname".
extern std::string tablefilename(const std::vector<dbpath>& db_paths,
                                 uint64_t number, uint32_t path_id);

// sufficient buffer size for formatfilenumber.
extern const size_t kformatfilenumberbufsize;

extern void formatfilenumber(uint64_t number, uint32_t path_id, char* out_buf,
                             size_t out_buf_size);

// return the name of the descriptor file for the db named by
// "dbname" and the specified incarnation number.  the result will be
// prefixed with "dbname".
extern std::string descriptorfilename(const std::string& dbname,
                                      uint64_t number);

// return the name of the current file.  this file contains the name
// of the current manifest file.  the result will be prefixed with
// "dbname".
extern std::string currentfilename(const std::string& dbname);

// return the name of the lock file for the db named by
// "dbname".  the result will be prefixed with "dbname".
extern std::string lockfilename(const std::string& dbname);

// return the name of a temporary file owned by the db named "dbname".
// the result will be prefixed with "dbname".
extern std::string tempfilename(const std::string& dbname, uint64_t number);

// a helper structure for prefix of info log names.
struct infologprefix {
  char buf[260];
  slice prefix;
  // prefix with db absolute path encoded
  explicit infologprefix(bool has_log_dir, const std::string& db_absolute_path);
  // default prefix
  explicit infologprefix();
};

// return the name of the info log file for "dbname".
extern std::string infologfilename(const std::string& dbname,
    const std::string& db_path="", const std::string& log_dir="");

// return the name of the old info log file for "dbname".
extern std::string oldinfologfilename(const std::string& dbname, uint64_t ts,
    const std::string& db_path="", const std::string& log_dir="");

// return the name to use for a metadatabase. the result will be prefixed with
// "dbname".
extern std::string metadatabasename(const std::string& dbname,
                                    uint64_t number);

// return the name of the identity file which stores a unique number for the db
// that will get regenerated if the db loses all its data and is recreated fresh
// either from a backup-image or empty
extern std::string identityfilename(const std::string& dbname);

// if filename is a rocksdb file, store the type of the file in *type.
// the number encoded in the filename is stored in *number.  if the
// filename was successfully parsed, returns true.  else return false.
// info_log_name_prefix is the path of info logs.
extern bool parsefilename(const std::string& filename, uint64_t* number,
                          const slice& info_log_name_prefix, filetype* type,
                          walfiletype* log_type = nullptr);
// same as previous function, but skip info log files.
extern bool parsefilename(const std::string& filename, uint64_t* number,
                          filetype* type, walfiletype* log_type = nullptr);

// make the current file point to the descriptor file with the
// specified number.
extern status setcurrentfile(env* env, const std::string& dbname,
                             uint64_t descriptor_number,
                             directory* directory_to_fsync);

// make the identity file for the db
extern status setidentityfile(env* env, const std::string& dbname);

}  // namespace rocksdb
