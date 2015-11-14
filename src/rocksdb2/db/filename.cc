//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
#define __stdc_format_macros
#include "db/filename.h"
#include <inttypes.h>

#include <ctype.h>
#include <stdio.h>
#include <vector>
#include "db/dbformat.h"
#include "rocksdb/env.h"
#include "util/logging.h"

namespace rocksdb {

// given a path, flatten the path name by replacing all chars not in
// {[0-9,a-z,a-z,-,_,.]} with _. and append '_log\0' at the end.
// return the number of chars stored in dest not including the trailing '\0'.
static size_t getinfologprefix(const std::string& path, char* dest, int len) {
  const char suffix[] = "_log";

  size_t write_idx = 0;
  size_t i = 0;
  size_t src_len = path.size();

  while (i < src_len && write_idx < len - sizeof(suffix)) {
    if ((path[i] >= 'a' && path[i] <= 'z') ||
        (path[i] >= '0' && path[i] <= '9') ||
        (path[i] >= 'a' && path[i] <= 'z') ||
        path[i] == '-' ||
        path[i] == '.' ||
        path[i] == '_'){
      dest[write_idx++] = path[i];
    } else {
      if (i > 0)
        dest[write_idx++] = '_';
    }
    i++;
  }
  assert(sizeof(suffix) <= len - write_idx);
  // "\0" is automatically added by snprintf
  snprintf(dest + write_idx, len - write_idx, suffix);
  write_idx += sizeof(suffix) - 1;
  return write_idx;
}

static std::string makefilename(const std::string& name, uint64_t number,
                                const char* suffix) {
  char buf[100];
  snprintf(buf, sizeof(buf), "/%06llu.%s",
           static_cast<unsigned long long>(number),
           suffix);
  return name + buf;
}

std::string logfilename(const std::string& name, uint64_t number) {
  assert(number > 0);
  return makefilename(name, number, "log");
}

std::string archivaldirectory(const std::string& dir) {
  return dir + "/" + archival_dir;
}
std::string archivedlogfilename(const std::string& name, uint64_t number) {
  assert(number > 0);
  return makefilename(name + "/" + archival_dir, number, "log");
}

std::string maketablefilename(const std::string& path, uint64_t number) {
  return makefilename(path, number, "sst");
}

std::string tablefilename(const std::vector<dbpath>& db_paths, uint64_t number,
                          uint32_t path_id) {
  assert(number > 0);
  std::string path;
  if (path_id >= db_paths.size()) {
    path = db_paths.back().path;
  } else {
    path = db_paths[path_id].path;
  }
  return maketablefilename(path, number);
}

const size_t kformatfilenumberbufsize = 38;

void formatfilenumber(uint64_t number, uint32_t path_id, char* out_buf,
                      size_t out_buf_size) {
  if (path_id == 0) {
    snprintf(out_buf, out_buf_size, "%" priu64, number);
  } else {
    snprintf(out_buf, out_buf_size, "%" priu64
                                    "(path "
                                    "%" priu32 ")",
             number, path_id);
  }
}

std::string descriptorfilename(const std::string& dbname, uint64_t number) {
  assert(number > 0);
  char buf[100];
  snprintf(buf, sizeof(buf), "/manifest-%06llu",
           static_cast<unsigned long long>(number));
  return dbname + buf;
}

std::string currentfilename(const std::string& dbname) {
  return dbname + "/current";
}

std::string lockfilename(const std::string& dbname) {
  return dbname + "/lock";
}

std::string tempfilename(const std::string& dbname, uint64_t number) {
  return makefilename(dbname, number, "dbtmp");
}

infologprefix::infologprefix(bool has_log_dir,
                             const std::string& db_absolute_path) {
  if (!has_log_dir) {
    const char kinfologprefix[] = "log";
    // "\0" is automatically added to the end
    snprintf(buf, sizeof(buf), kinfologprefix);
    prefix = slice(buf, sizeof(kinfologprefix) - 1);
  } else {
    size_t len = getinfologprefix(db_absolute_path, buf, sizeof(buf));
    prefix = slice(buf, len);
  }
}

std::string infologfilename(const std::string& dbname,
    const std::string& db_path, const std::string& log_dir) {
  if (log_dir.empty())
    return dbname + "/log";

  infologprefix info_log_prefix(true, db_path);
  return log_dir + "/" + info_log_prefix.buf;
}

// return the name of the old info log file for "dbname".
std::string oldinfologfilename(const std::string& dbname, uint64_t ts,
    const std::string& db_path, const std::string& log_dir) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(ts));

  if (log_dir.empty())
    return dbname + "/log.old." + buf;

  infologprefix info_log_prefix(true, db_path);
  return log_dir + "/" + info_log_prefix.buf + ".old." + buf;
}

std::string metadatabasename(const std::string& dbname, uint64_t number) {
  char buf[100];
  snprintf(buf, sizeof(buf), "/metadb-%llu",
           static_cast<unsigned long long>(number));
  return dbname + buf;
}

std::string identityfilename(const std::string& dbname) {
  return dbname + "/identity";
}

// owned filenames have the form:
//    dbname/identity
//    dbname/current
//    dbname/lock
//    dbname/<info_log_name_prefix>
//    dbname/<info_log_name_prefix>.old.[0-9]+
//    dbname/manifest-[0-9]+
//    dbname/[0-9]+.(log|sst)
//    dbname/metadb-[0-9]+
//    disregards / at the beginning
bool parsefilename(const std::string& fname,
                   uint64_t* number,
                   filetype* type,
                   walfiletype* log_type) {
  return parsefilename(fname, number, "", type, log_type);
}

bool parsefilename(const std::string& fname, uint64_t* number,
                   const slice& info_log_name_prefix, filetype* type,
                   walfiletype* log_type) {
  slice rest(fname);
  if (fname.length() > 1 && fname[0] == '/') {
    rest.remove_prefix(1);
  }
  if (rest == "identity") {
    *number = 0;
    *type = kidentityfile;
  } else if (rest == "current") {
    *number = 0;
    *type = kcurrentfile;
  } else if (rest == "lock") {
    *number = 0;
    *type = kdblockfile;
  } else if (info_log_name_prefix.size() > 0 &&
             rest.starts_with(info_log_name_prefix)) {
    rest.remove_prefix(info_log_name_prefix.size());
    if (rest == "" || rest == ".old") {
      *number = 0;
      *type = kinfologfile;
    } else if (rest.starts_with(".old.")) {
      uint64_t ts_suffix;
      // sizeof also counts the trailing '\0'.
      rest.remove_prefix(sizeof(".old.") - 1);
      if (!consumedecimalnumber(&rest, &ts_suffix)) {
        return false;
      }
      *number = ts_suffix;
      *type = kinfologfile;
    }
  } else if (rest.starts_with("manifest-")) {
    rest.remove_prefix(strlen("manifest-"));
    uint64_t num;
    if (!consumedecimalnumber(&rest, &num)) {
      return false;
    }
    if (!rest.empty()) {
      return false;
    }
    *type = kdescriptorfile;
    *number = num;
  } else if (rest.starts_with("metadb-")) {
    rest.remove_prefix(strlen("metadb-"));
    uint64_t num;
    if (!consumedecimalnumber(&rest, &num)) {
      return false;
    }
    if (!rest.empty()) {
      return false;
    }
    *type = kmetadatabase;
    *number = num;
  } else {
    // avoid strtoull() to keep filename format independent of the
    // current locale
    bool archive_dir_found = false;
    if (rest.starts_with(archival_dir)) {
      if (rest.size() <= archival_dir.size()) {
        return false;
      }
      rest.remove_prefix(archival_dir.size() + 1); // add 1 to remove / also
      if (log_type) {
        *log_type = karchivedlogfile;
      }
      archive_dir_found = true;
    }
    uint64_t num;
    if (!consumedecimalnumber(&rest, &num)) {
      return false;
    }
    slice suffix = rest;
    if (suffix == slice(".log")) {
      *type = klogfile;
      if (log_type && !archive_dir_found) {
        *log_type = kalivelogfile;
      }
    } else if (archive_dir_found) {
      return false; // archive dir can contain only log files
    } else if (suffix == slice(".sst")) {
      *type = ktablefile;
    } else if (suffix == slice(".dbtmp")) {
      *type = ktempfile;
    } else {
      return false;
    }
    *number = num;
  }
  return true;
}

status setcurrentfile(env* env, const std::string& dbname,
                      uint64_t descriptor_number,
                      directory* directory_to_fsync) {
  // remove leading "dbname/" and add newline to manifest file name
  std::string manifest = descriptorfilename(dbname, descriptor_number);
  slice contents = manifest;
  assert(contents.starts_with(dbname + "/"));
  contents.remove_prefix(dbname.size() + 1);
  std::string tmp = tempfilename(dbname, descriptor_number);
  status s = writestringtofile(env, contents.tostring() + "\n", tmp, true);
  if (s.ok()) {
    s = env->renamefile(tmp, currentfilename(dbname));
  }
  if (s.ok()) {
    if (directory_to_fsync != nullptr) {
      directory_to_fsync->fsync();
    }
  } else {
    env->deletefile(tmp);
  }
  return s;
}

status setidentityfile(env* env, const std::string& dbname) {
  std::string id = env->generateuniqueid();
  assert(!id.empty());
  // reserve the filename dbname/000000.dbtmp for the temporary identity file
  std::string tmp = tempfilename(dbname, 0);
  status s = writestringtofile(env, id, tmp, true);
  if (s.ok()) {
    s = env->renamefile(tmp, identityfilename(dbname));
  }
  if (!s.ok()) {
    env->deletefile(tmp);
  }
  return s;
}

}  // namespace rocksdb
