// copyright (c) 2012 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include <stdio.h>
#include "dbformat.h"
#include "filename.h"
#include "log_reader.h"
#include "version_edit.h"
#include "write_batch_internal.h"
#include "../hyperleveldb/env.h"
#include "../hyperleveldb/iterator.h"
#include "../hyperleveldb/options.h"
#include "../hyperleveldb/status.h"
#include "../hyperleveldb/table.h"
#include "../hyperleveldb/write_batch.h"
#include "../util/logging.h"

namespace hyperleveldb {

namespace {

bool guesstype(const std::string& fname, filetype* type) {
  size_t pos = fname.rfind('/');
  std::string basename;
  if (pos == std::string::npos) {
    basename = fname;
  } else {
    basename = std::string(fname.data() + pos + 1, fname.size() - pos - 1);
  }
  uint64_t ignored;
  return parsefilename(basename, &ignored, type);
}

// notified when log reader encounters corruption.
class corruptionreporter : public log::reader::reporter {
 public:
  virtual void corruption(size_t bytes, const status& status) {
    printf("corruption: %d bytes; %s\n",
            static_cast<int>(bytes),
            status.tostring().c_str());
  }
};

// print contents of a log file. (*func)() is called on every record.
bool printlogcontents(env* env, const std::string& fname,
                      void (*func)(slice)) {
  sequentialfile* file;
  status s = env->newsequentialfile(fname, &file);
  if (!s.ok()) {
    fprintf(stderr, "%s\n", s.tostring().c_str());
    return false;
  }
  corruptionreporter reporter;
  log::reader reader(file, &reporter, true, 0);
  slice record;
  std::string scratch;
  while (reader.readrecord(&record, &scratch)) {
    printf("--- offset %llu; ",
           static_cast<unsigned long long>(reader.lastrecordoffset()));
    (*func)(record);
  }
  delete file;
  return true;
}

// called on every item found in a writebatch.
class writebatchitemprinter : public writebatch::handler {
 public:
  uint64_t offset_;
  uint64_t sequence_;

  virtual void put(const slice& key, const slice& value) {
    printf("  put '%s' '%s'\n",
           escapestring(key).c_str(),
           escapestring(value).c_str());
  }
  virtual void delete(const slice& key) {
    printf("  del '%s'\n",
           escapestring(key).c_str());
  }
};


// called on every log record (each one of which is a writebatch)
// found in a klogfile.
static void writebatchprinter(slice record) {
  if (record.size() < 12) {
    printf("log record length %d is too small\n",
           static_cast<int>(record.size()));
    return;
  }
  writebatch batch;
  writebatchinternal::setcontents(&batch, record);
  printf("sequence %llu\n",
         static_cast<unsigned long long>(writebatchinternal::sequence(&batch)));
  writebatchitemprinter batch_item_printer;
  status s = batch.iterate(&batch_item_printer);
  if (!s.ok()) {
    printf("  error: %s\n", s.tostring().c_str());
  }
}

bool dumplog(env* env, const std::string& fname) {
  return printlogcontents(env, fname, writebatchprinter);
}

// called on every log record (each one of which is a writebatch)
// found in a kdescriptorfile.
static void versioneditprinter(slice record) {
  versionedit edit;
  status s = edit.decodefrom(record);
  if (!s.ok()) {
    printf("%s\n", s.tostring().c_str());
    return;
  }
  printf("%s", edit.debugstring().c_str());
}

bool dumpdescriptor(env* env, const std::string& fname) {
  return printlogcontents(env, fname, versioneditprinter);
}

bool dumptable(env* env, const std::string& fname) {
  uint64_t file_size;
  randomaccessfile* file = null;
  table* table = null;
  status s = env->getfilesize(fname, &file_size);
  if (s.ok()) {
    s = env->newrandomaccessfile(fname, &file);
  }
  if (s.ok()) {
    // we use the default comparator, which may or may not match the
    // comparator used in this database. however this should not cause
    // problems since we only use table operations that do not require
    // any comparisons.  in particular, we do not call seek or prev.
    s = table::open(options(), file, file_size, &table);
  }
  if (!s.ok()) {
    fprintf(stderr, "%s\n", s.tostring().c_str());
    delete table;
    delete file;
    return false;
  }

  readoptions ro;
  ro.fill_cache = false;
  iterator* iter = table->newiterator(ro);
  for (iter->seektofirst(); iter->valid(); iter->next()) {
    parsedinternalkey key;
    if (!parseinternalkey(iter->key(), &key)) {
      printf("badkey '%s' => '%s'\n",
             escapestring(iter->key()).c_str(),
             escapestring(iter->value()).c_str());
    } else {
      char kbuf[20];
      const char* type;
      if (key.type == ktypedeletion) {
        type = "del";
      } else if (key.type == ktypevalue) {
        type = "val";
      } else {
        snprintf(kbuf, sizeof(kbuf), "%d", static_cast<int>(key.type));
        type = kbuf;
      }
      printf("'%s' @ %8llu : %s => '%s'\n",
             escapestring(key.user_key).c_str(),
             static_cast<unsigned long long>(key.sequence),
             type,
             escapestring(iter->value()).c_str());
    }
  }
  s = iter->status();
  if (!s.ok()) {
    printf("iterator error: %s\n", s.tostring().c_str());
  }

  delete iter;
  delete table;
  delete file;
  return true;
}

bool dumpfile(env* env, const std::string& fname) {
  filetype ftype;
  if (!guesstype(fname, &ftype)) {
    fprintf(stderr, "%s: unknown file type\n", fname.c_str());
    return false;
  }
  switch (ftype) {
    case klogfile:         return dumplog(env, fname);
    case kdescriptorfile:  return dumpdescriptor(env, fname);
    case ktablefile:       return dumptable(env, fname);

    default: {
      fprintf(stderr, "%s: not a dump-able file type\n", fname.c_str());
      break;
    }
  }
  return false;
}

bool handledumpcommand(env* env, char** files, int num) {
  bool ok = true;
  for (int i = 0; i < num; i++) {
    ok &= dumpfile(env, files[i]);
  }
  return ok;
}

}
}  // namespace hyperleveldb

static void usage() {
  fprintf(
      stderr,
      "usage: leveldbutil command...\n"
      "   dump files...         -- dump contents of specified files\n"
      );
}

int main(int argc, char** argv) {
  leveldb::env* env = leveldb::env::default();
  bool ok = true;
  if (argc < 2) {
    usage();
    ok = false;
  } else {
    std::string command = argv[1];
    if (command == "dump") {
      ok = leveldb::handledumpcommand(env, argv+2, argc-2);
    } else {
      usage();
      ok = false;
    }
  }
  return (ok ? 0 : 1);
}
