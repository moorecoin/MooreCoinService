//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// must not be included from any .h files to avoid polluting the namespace
// with macros.

#define __stdc_format_macros
#include <inttypes.h>
#include <stdio.h>
#include <string>
#include <algorithm>
#include <vector>

#include "rocksdb/options.h"
#include "rocksdb/env.h"
#include "db/filename.h"

namespace rocksdb {

void dumpdbfilesummary(const dboptions& options, const std::string& dbname) {
  if (options.info_log == nullptr) {
    return;
  }

  auto* env = options.env;
  uint64_t number = 0;
  filetype type = kinfologfile;

  std::vector<std::string> files;
  uint64_t file_num = 0;
  uint64_t file_size;
  std::string file_info, wal_info;

  log(options.info_log, "db summary\n");
  // get files in dbname dir
  if (!env->getchildren(dbname, &files).ok()) {
    log(options.info_log, "error when reading %s dir\n", dbname.c_str());
  }
  std::sort(files.begin(), files.end());
  for (std::string file : files) {
    if (!parsefilename(file, &number, &type)) {
      continue;
    }
    switch (type) {
      case kcurrentfile:
        log(options.info_log, "current file:  %s\n", file.c_str());
        break;
      case kidentityfile:
        log(options.info_log, "identity file:  %s\n", file.c_str());
        break;
      case kdescriptorfile:
        env->getfilesize(dbname + "/" + file, &file_size);
        log(options.info_log, "manifest file:  %s size: %" priu64 " bytes\n",
            file.c_str(), file_size);
        break;
      case klogfile:
        env->getfilesize(dbname + "/" + file, &file_size);
        char str[8];
        snprintf(str, sizeof(str), "%" priu64, file_size);
        wal_info.append(file).append(" size: ").
            append(str, sizeof(str)).append(" ;");
        break;
      case ktablefile:
        if (++file_num < 10) {
          file_info.append(file).append(" ");
        }
        break;
      default:
        break;
    }
  }

  // get sst files in db_path dir
  for (auto& db_path : options.db_paths) {
    if (dbname.compare(db_path.path) != 0) {
      if (!env->getchildren(db_path.path, &files).ok()) {
        log(options.info_log, "error when reading %s dir\n",
            db_path.path.c_str());
        continue;
      }
      std::sort(files.begin(), files.end());
      for (std::string file : files) {
        if (parsefilename(file, &number, &type)) {
          if (type == ktablefile && ++file_num < 10) {
            file_info.append(file).append(" ");
          }
        }
      }
    }
    log(options.info_log, "sst files in %s dir, total num: %" priu64 ", files: %s\n",
        db_path.path.c_str(), file_num, file_info.c_str());
    file_num = 0;
    file_info.clear();
  }

  // get wal file in wal_dir
  if (dbname.compare(options.wal_dir) != 0) {
    if (!env->getchildren(options.wal_dir, &files).ok()) {
      log(options.info_log, "error when reading %s dir\n",
          options.wal_dir.c_str());
      return;
    }
    wal_info.clear();
    for (std::string file : files) {
      if (parsefilename(file, &number, &type)) {
        if (type == klogfile) {
          env->getfilesize(options.wal_dir + "/" + file, &file_size);
          char str[8];
          snprintf(str, sizeof(str), "%" priu64, file_size);
          wal_info.append(file).append(" size: ").
              append(str, sizeof(str)).append(" ;");
        }
      }
    }
  }
  log(options.info_log, "write ahead log file in %s: %s\n",
      options.wal_dir.c_str(), wal_info.c_str());
}
}  // namespace rocksdb
