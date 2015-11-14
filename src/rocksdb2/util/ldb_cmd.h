//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#pragma once
#include <string>
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <algorithm>
#include <stdio.h>

#include "db/version_set.h"
#include "rocksdb/env.h"
#include "rocksdb/iterator.h"
#include "rocksdb/ldb_tool.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "rocksdb/utilities/db_ttl.h"
#include "util/logging.h"
#include "util/ldb_cmd_execute_result.h"
#include "util/string_util.h"
#include "utilities/ttl/db_ttl_impl.h"

using std::string;
using std::map;
using std::vector;
using std::ostringstream;

namespace rocksdb {

class ldbcommand {
public:

  // command-line arguments
  static const string arg_db;
  static const string arg_hex;
  static const string arg_key_hex;
  static const string arg_value_hex;
  static const string arg_ttl;
  static const string arg_ttl_start;
  static const string arg_ttl_end;
  static const string arg_timestamp;
  static const string arg_from;
  static const string arg_to;
  static const string arg_max_keys;
  static const string arg_bloom_bits;
  static const string arg_compression_type;
  static const string arg_block_size;
  static const string arg_auto_compaction;
  static const string arg_write_buffer_size;
  static const string arg_file_size;
  static const string arg_create_if_missing;

  static ldbcommand* initfromcmdlineargs(
    const vector<string>& args,
    const options& options,
    const ldboptions& ldb_options
  );

  static ldbcommand* initfromcmdlineargs(
    int argc,
    char** argv,
    const options& options,
    const ldboptions& ldb_options
  );

  bool validatecmdlineoptions();

  virtual options prepareoptionsforopendb();

  virtual void setdboptions(options options) {
    options_ = options;
  }

  void setldboptions(const ldboptions& ldb_options) {
    ldb_options_ = ldb_options;
  }

  virtual bool nodbopen() {
    return false;
  }

  virtual ~ldbcommand() {
    if (db_ != nullptr) {
      delete db_;
      db_ = nullptr;
    }
  }

  /* run the command, and return the execute result. */
  void run() {
    if (!exec_state_.isnotstarted()) {
      return;
    }

    if (db_ == nullptr && !nodbopen()) {
      opendb();
      if (!exec_state_.isnotstarted()) {
        return;
      }
    }

    docommand();
    if (exec_state_.isnotstarted()) {
      exec_state_ = ldbcommandexecuteresult::succeed("");
    }

    if (db_ != nullptr) {
      closedb ();
    }
  }

  virtual void docommand() = 0;

  ldbcommandexecuteresult getexecutestate() {
    return exec_state_;
  }

  void clearpreviousrunstate() {
    exec_state_.reset();
  }

  static string hextostring(const string& str) {
    string parsed;
    if (str[0] != '0' || str[1] != 'x') {
      fprintf(stderr, "invalid hex input %s.  must start with 0x\n",
              str.c_str());
      throw "invalid hex input";
    }

    for (unsigned int i = 2; i < str.length();) {
      int c;
      sscanf(str.c_str() + i, "%2x", &c);
      parsed.push_back(c);
      i += 2;
    }
    return parsed;
  }

  static string stringtohex(const string& str) {
    string result = "0x";
    char buf[10];
    for (size_t i = 0; i < str.length(); i++) {
      snprintf(buf, 10, "%02x", (unsigned char)str[i]);
      result += buf;
    }
    return result;
  }

  static const char* delim;

protected:

  ldbcommandexecuteresult exec_state_;
  string db_path_;
  db* db_;
  dbwithttl* db_ttl_;

  /**
   * true implies that this command can work if the db is opened in read-only
   * mode.
   */
  bool is_read_only_;

  /** if true, the key is input/output as hex in get/put/scan/delete etc. */
  bool is_key_hex_;

  /** if true, the value is input/output as hex in get/put/scan/delete etc. */
  bool is_value_hex_;

  /** if true, the value is treated as timestamp suffixed */
  bool is_db_ttl_;

  // if true, the kvs are output with their insert/modify timestamp in a ttl db
  bool timestamp_;

  /**
   * map of options passed on the command-line.
   */
  const map<string, string> option_map_;

  /**
   * flags passed on the command-line.
   */
  const vector<string> flags_;

  /** list of command-line options valid for this command */
  const vector<string> valid_cmd_line_options_;

  bool parsekeyvalue(const string& line, string* key, string* value,
                      bool is_key_hex, bool is_value_hex);

  ldbcommand(const map<string, string>& options, const vector<string>& flags,
             bool is_read_only, const vector<string>& valid_cmd_line_options) :
      db_(nullptr),
      is_read_only_(is_read_only),
      is_key_hex_(false),
      is_value_hex_(false),
      is_db_ttl_(false),
      timestamp_(false),
      option_map_(options),
      flags_(flags),
      valid_cmd_line_options_(valid_cmd_line_options) {

    map<string, string>::const_iterator itr = options.find(arg_db);
    if (itr != options.end()) {
      db_path_ = itr->second;
    }

    is_key_hex_ = iskeyhex(options, flags);
    is_value_hex_ = isvaluehex(options, flags);
    is_db_ttl_ = isflagpresent(flags, arg_ttl);
    timestamp_ = isflagpresent(flags, arg_timestamp);
  }

  void opendb() {
    options opt = prepareoptionsforopendb();
    if (!exec_state_.isnotstarted()) {
      return;
    }
    // open the db.
    status st;
    if (is_db_ttl_) {
      if (is_read_only_) {
        st = dbwithttl::open(opt, db_path_, &db_ttl_, 0, true);
      } else {
        st = dbwithttl::open(opt, db_path_, &db_ttl_);
      }
      db_ = db_ttl_;
    } else if (is_read_only_) {
      st = db::openforreadonly(opt, db_path_, &db_);
    } else {
      st = db::open(opt, db_path_, &db_);
    }
    if (!st.ok()) {
      string msg = st.tostring();
      exec_state_ = ldbcommandexecuteresult::failed(msg);
    }

    options_ = opt;
  }

  void closedb () {
    if (db_ != nullptr) {
      delete db_;
      db_ = nullptr;
    }
  }

  static string printkeyvalue(const string& key, const string& value,
        bool is_key_hex, bool is_value_hex) {
    string result;
    result.append(is_key_hex ? stringtohex(key) : key);
    result.append(delim);
    result.append(is_value_hex ? stringtohex(value) : value);
    return result;
  }

  static string printkeyvalue(const string& key, const string& value,
        bool is_hex) {
    return printkeyvalue(key, value, is_hex, is_hex);
  }

  /**
   * return true if the specified flag is present in the specified flags vector
   */
  static bool isflagpresent(const vector<string>& flags, const string& flag) {
    return (std::find(flags.begin(), flags.end(), flag) != flags.end());
  }

  static string helprangecmdargs() {
    ostringstream str_stream;
    str_stream << " ";
    str_stream << "[--" << arg_from << "] ";
    str_stream << "[--" << arg_to << "] ";
    return str_stream.str();
  }

  /**
   * a helper function that returns a list of command line options
   * used by this command.  it includes the common options and the ones
   * passed in.
   */
  vector<string> buildcmdlineoptions(vector<string> options) {
    vector<string> ret = {arg_db, arg_bloom_bits, arg_block_size,
                          arg_auto_compaction, arg_compression_type,
                          arg_write_buffer_size, arg_file_size};
    ret.insert(ret.end(), options.begin(), options.end());
    return ret;
  }

  bool parseintoption(const map<string, string>& options, const string& option,
                      int& value, ldbcommandexecuteresult& exec_state);

  bool parsestringoption(const map<string, string>& options,
                         const string& option, string* value);

  options options_;
  ldboptions ldb_options_;

private:

  /**
   * interpret command line options and flags to determine if the key
   * should be input/output in hex.
   */
  bool iskeyhex(const map<string, string>& options,
      const vector<string>& flags) {
    return (isflagpresent(flags, arg_hex) ||
        isflagpresent(flags, arg_key_hex) ||
        parsebooleanoption(options, arg_hex, false) ||
        parsebooleanoption(options, arg_key_hex, false));
  }

  /**
   * interpret command line options and flags to determine if the value
   * should be input/output in hex.
   */
  bool isvaluehex(const map<string, string>& options,
      const vector<string>& flags) {
    return (isflagpresent(flags, arg_hex) ||
          isflagpresent(flags, arg_value_hex) ||
          parsebooleanoption(options, arg_hex, false) ||
          parsebooleanoption(options, arg_value_hex, false));
  }

  /**
   * returns the value of the specified option as a boolean.
   * default_val is used if the option is not found in options.
   * throws an exception if the value of the option is not
   * "true" or "false" (case insensitive).
   */
  bool parsebooleanoption(const map<string, string>& options,
      const string& option, bool default_val) {

    map<string, string>::const_iterator itr = options.find(option);
    if (itr != options.end()) {
      string option_val = itr->second;
      return stringtobool(itr->second);
    }
    return default_val;
  }

  /**
   * converts val to a boolean.
   * val must be either true or false (case insensitive).
   * otherwise an exception is thrown.
   */
  bool stringtobool(string val) {
    std::transform(val.begin(), val.end(), val.begin(), ::tolower);
    if (val == "true") {
      return true;
    } else if (val == "false") {
      return false;
    } else {
      throw "invalid value for boolean argument";
    }
  }

  static ldbcommand* selectcommand(
    const string& cmd,
    const vector<string>& cmdparams,
    const map<string, string>& option_map,
    const vector<string>& flags
  );

};

class compactorcommand: public ldbcommand {
public:
  static string name() { return "compact"; }

  compactorcommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags);

  static void help(string& ret);

  virtual void docommand();

private:
  bool null_from_;
  string from_;
  bool null_to_;
  string to_;
};

class dbdumpercommand: public ldbcommand {
public:
  static string name() { return "dump"; }

  dbdumpercommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags);

  static void help(string& ret);

  virtual void docommand();

private:
  bool null_from_;
  string from_;
  bool null_to_;
  string to_;
  int max_keys_;
  string delim_;
  bool count_only_;
  bool count_delim_;
  bool print_stats_;

  static const string arg_count_only;
  static const string arg_count_delim;
  static const string arg_stats;
  static const string arg_ttl_bucket;
};

class internaldumpcommand: public ldbcommand {
public:
  static string name() { return "idump"; }

  internaldumpcommand(const vector<string>& params,
                      const map<string, string>& options,
                      const vector<string>& flags);

  static void help(string& ret);

  virtual void docommand();

private:
  bool has_from_;
  string from_;
  bool has_to_;
  string to_;
  int max_keys_;
  string delim_;
  bool count_only_;
  bool count_delim_;
  bool print_stats_;
  bool is_input_key_hex_;

  static const string arg_delim;
  static const string arg_count_only;
  static const string arg_count_delim;
  static const string arg_stats;
  static const string arg_input_key_hex;
};

class dbloadercommand: public ldbcommand {
public:
  static string name() { return "load"; }

  dbloadercommand(string& db_name, vector<string>& args);

  dbloadercommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags);

  static void help(string& ret);
  virtual void docommand();

  virtual options prepareoptionsforopendb();

private:
  bool create_if_missing_;
  bool disable_wal_;
  bool bulk_load_;
  bool compact_;

  static const string arg_disable_wal;
  static const string arg_bulk_load;
  static const string arg_compact;
};

class manifestdumpcommand: public ldbcommand {
public:
  static string name() { return "manifest_dump"; }

  manifestdumpcommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags);

  static void help(string& ret);
  virtual void docommand();

  virtual bool nodbopen() {
    return true;
  }

private:
  bool verbose_;
  string path_;

  static const string arg_verbose;
  static const string arg_path;
};

class listcolumnfamiliescommand : public ldbcommand {
 public:
  static string name() { return "list_column_families"; }

  listcolumnfamiliescommand(const vector<string>& params,
                            const map<string, string>& options,
                            const vector<string>& flags);

  static void help(string& ret);
  virtual void docommand();

  virtual bool nodbopen() { return true; }

 private:
  string dbname_;
};

class reducedblevelscommand : public ldbcommand {
public:
  static string name() { return "reduce_levels"; }

  reducedblevelscommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags);

  virtual options prepareoptionsforopendb();

  virtual void docommand();

  virtual bool nodbopen() {
    return true;
  }

  static void help(string& msg);

  static vector<string> prepareargs(const string& db_path, int new_levels,
      bool print_old_level = false);

private:
  int old_levels_;
  int new_levels_;
  bool print_old_levels_;

  static const string arg_new_levels;
  static const string arg_print_old_levels;

  status getoldnumoflevels(options& opt, int* levels);
};

class changecompactionstylecommand : public ldbcommand {
public:
  static string name() { return "change_compaction_style"; }

  changecompactionstylecommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags);

  virtual options prepareoptionsforopendb();

  virtual void docommand();

  static void help(string& msg);

private:
  int old_compaction_style_;
  int new_compaction_style_;

  static const string arg_old_compaction_style;
  static const string arg_new_compaction_style;
};

class waldumpercommand : public ldbcommand {
public:
  static string name() { return "dump_wal"; }

  waldumpercommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags);

  virtual bool  nodbopen() {
    return true;
  }

  static void help(string& ret);
  virtual void docommand();

private:
  bool print_header_;
  string wal_file_;
  bool print_values_;

  static const string arg_wal_file;
  static const string arg_print_header;
  static const string arg_print_value;
};


class getcommand : public ldbcommand {
public:
  static string name() { return "get"; }

  getcommand(const vector<string>& params, const map<string, string>& options,
      const vector<string>& flags);

  virtual void docommand();

  static void help(string& ret);

private:
  string key_;
};

class approxsizecommand : public ldbcommand {
public:
  static string name() { return "approxsize"; }

  approxsizecommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags);

  virtual void docommand();

  static void help(string& ret);

private:
  string start_key_;
  string end_key_;
};

class batchputcommand : public ldbcommand {
public:
  static string name() { return "batchput"; }

  batchputcommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags);

  virtual void docommand();

  static void help(string& ret);

  virtual options prepareoptionsforopendb();

private:
  /**
   * the key-values to be inserted.
   */
  vector<std::pair<string, string>> key_values_;
};

class scancommand : public ldbcommand {
public:
  static string name() { return "scan"; }

  scancommand(const vector<string>& params, const map<string, string>& options,
      const vector<string>& flags);

  virtual void docommand();

  static void help(string& ret);

private:
  string start_key_;
  string end_key_;
  bool start_key_specified_;
  bool end_key_specified_;
  int max_keys_scanned_;
};

class deletecommand : public ldbcommand {
public:
  static string name() { return "delete"; }

  deletecommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags);

  virtual void docommand();

  static void help(string& ret);

private:
  string key_;
};

class putcommand : public ldbcommand {
public:
  static string name() { return "put"; }

  putcommand(const vector<string>& params, const map<string, string>& options,
      const vector<string>& flags);

  virtual void docommand();

  static void help(string& ret);

  virtual options prepareoptionsforopendb();

private:
  string key_;
  string value_;
};

/**
 * command that starts up a repl shell that allows
 * get/put/delete.
 */
class dbqueriercommand: public ldbcommand {
public:
  static string name() { return "query"; }

  dbqueriercommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags);

  static void help(string& ret);

  virtual void docommand();

private:
  static const char* help_cmd;
  static const char* get_cmd;
  static const char* put_cmd;
  static const char* delete_cmd;
};

class checkconsistencycommand : public ldbcommand {
public:
  static string name() { return "checkconsistency"; }

  checkconsistencycommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags);

  virtual void docommand();

  virtual bool nodbopen() {
    return true;
  }

  static void help(string& ret);
};

} // namespace rocksdb
