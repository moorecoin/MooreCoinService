//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#ifndef rocksdb_lite
#include "rocksdb/ldb_tool.h"
#include "util/ldb_cmd.h"

namespace rocksdb {

class defaultsliceformatter : public sliceformatter {
 public:
  virtual std::string format(const slice& s) const override {
    return s.tostring();
  }
};

ldboptions::ldboptions()
    : key_formatter(new defaultsliceformatter()) {
}

class ldbcommandrunner {
public:

  static void printhelp(const char* exec_name) {
    string ret;

    ret.append("ldb - leveldb tool");
    ret.append("\n\n");
    ret.append("commands must specify --" + ldbcommand::arg_db +
        "=<full_path_to_db_directory> when necessary\n");
    ret.append("\n");
    ret.append("the following optional parameters control if keys/values are "
        "input/output as hex or as plain strings:\n");
    ret.append("  --" + ldbcommand::arg_key_hex +
        " : keys are input/output as hex\n");
    ret.append("  --" + ldbcommand::arg_value_hex +
        " : values are input/output as hex\n");
    ret.append("  --" + ldbcommand::arg_hex +
        " : both keys and values are input/output as hex\n");
    ret.append("\n");

    ret.append("the following optional parameters control the database "
        "internals:\n");
    ret.append("  --" + ldbcommand::arg_ttl +
        " with 'put','get','scan','dump','query','batchput'"
        " : db supports ttl and value is internally timestamp-suffixed\n");
    ret.append("  --" + ldbcommand::arg_bloom_bits + "=<int,e.g.:14>\n");
    ret.append("  --" + ldbcommand::arg_compression_type +
        "=<no|snappy|zlib|bzip2>\n");
    ret.append("  --" + ldbcommand::arg_block_size +
        "=<block_size_in_bytes>\n");
    ret.append("  --" + ldbcommand::arg_auto_compaction + "=<true|false>\n");
    ret.append("  --" + ldbcommand::arg_write_buffer_size +
        "=<int,e.g.:4194304>\n");
    ret.append("  --" + ldbcommand::arg_file_size + "=<int,e.g.:2097152>\n");

    ret.append("\n\n");
    ret.append("data access commands:\n");
    putcommand::help(ret);
    getcommand::help(ret);
    batchputcommand::help(ret);
    scancommand::help(ret);
    deletecommand::help(ret);
    dbqueriercommand::help(ret);
    approxsizecommand::help(ret);
    checkconsistencycommand::help(ret);

    ret.append("\n\n");
    ret.append("admin commands:\n");
    waldumpercommand::help(ret);
    compactorcommand::help(ret);
    reducedblevelscommand::help(ret);
    changecompactionstylecommand::help(ret);
    dbdumpercommand::help(ret);
    dbloadercommand::help(ret);
    manifestdumpcommand::help(ret);
    listcolumnfamiliescommand::help(ret);
    internaldumpcommand::help(ret);

    fprintf(stderr, "%s\n", ret.c_str());
  }

  static void runcommand(int argc, char** argv, options options,
                         const ldboptions& ldb_options) {
    if (argc <= 2) {
      printhelp(argv[0]);
      exit(1);
    }

    ldbcommand* cmdobj = ldbcommand::initfromcmdlineargs(argc, argv, options,
                                                         ldb_options);
    if (cmdobj == nullptr) {
      fprintf(stderr, "unknown command\n");
      printhelp(argv[0]);
      exit(1);
    }

    if (!cmdobj->validatecmdlineoptions()) {
      exit(1);
    }

    cmdobj->run();
    ldbcommandexecuteresult ret = cmdobj->getexecutestate();
    fprintf(stderr, "%s\n", ret.tostring().c_str());
    delete cmdobj;

    exit(ret.isfailed());
  }

};


void ldbtool::run(int argc, char** argv, options options,
                  const ldboptions& ldb_options) {
  ldbcommandrunner::runcommand(argc, argv, options, ldb_options);
}
} // namespace rocksdb

#endif  // rocksdb_lite
