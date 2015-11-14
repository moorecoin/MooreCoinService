//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#ifndef rocksdb_lite
#include "util/ldb_cmd.h"

#include "db/dbformat.h"
#include "db/db_impl.h"
#include "db/log_reader.h"
#include "db/filename.h"
#include "db/write_batch_internal.h"
#include "rocksdb/write_batch.h"
#include "rocksdb/cache.h"
#include "util/coding.h"
#include "utilities/ttl/db_ttl_impl.h"

#include <ctime>
#include <dirent.h>
#include <limits>
#include <sstream>
#include <string>
#include <stdexcept>

namespace rocksdb {

using namespace std;

const string ldbcommand::arg_db = "db";
const string ldbcommand::arg_hex = "hex";
const string ldbcommand::arg_key_hex = "key_hex";
const string ldbcommand::arg_value_hex = "value_hex";
const string ldbcommand::arg_ttl = "ttl";
const string ldbcommand::arg_ttl_start = "start_time";
const string ldbcommand::arg_ttl_end = "end_time";
const string ldbcommand::arg_timestamp = "timestamp";
const string ldbcommand::arg_from = "from";
const string ldbcommand::arg_to = "to";
const string ldbcommand::arg_max_keys = "max_keys";
const string ldbcommand::arg_bloom_bits = "bloom_bits";
const string ldbcommand::arg_compression_type = "compression_type";
const string ldbcommand::arg_block_size = "block_size";
const string ldbcommand::arg_auto_compaction = "auto_compaction";
const string ldbcommand::arg_write_buffer_size = "write_buffer_size";
const string ldbcommand::arg_file_size = "file_size";
const string ldbcommand::arg_create_if_missing = "create_if_missing";

const char* ldbcommand::delim = " ==> ";

ldbcommand* ldbcommand::initfromcmdlineargs(
  int argc,
  char** argv,
  const options& options,
  const ldboptions& ldb_options
) {
  vector<string> args;
  for (int i = 1; i < argc; i++) {
    args.push_back(argv[i]);
  }
  return initfromcmdlineargs(args, options, ldb_options);
}

/**
 * parse the command-line arguments and create the appropriate ldbcommand2
 * instance.
 * the command line arguments must be in the following format:
 * ./ldb --db=path_to_db [--commonopt1=commonopt1val] ..
 *        command <param1> <param2> ... [-cmdspecificopt1=cmdspecificopt1val] ..
 * this is similar to the command line format used by hbaseclienttool.
 * command name is not included in args.
 * returns nullptr if the command-line cannot be parsed.
 */
ldbcommand* ldbcommand::initfromcmdlineargs(
  const vector<string>& args,
  const options& options,
  const ldboptions& ldb_options
) {
  // --x=y command line arguments are added as x->y map entries.
  map<string, string> option_map;

  // command-line arguments of the form --hex end up in this array as hex
  vector<string> flags;

  // everything other than option_map and flags. represents commands
  // and their parameters.  for eg: put key1 value1 go into this vector.
  vector<string> cmdtokens;

  const string option_prefix = "--";

  for (const auto& arg : args) {
    if (arg[0] == '-' && arg[1] == '-'){
      vector<string> splits = stringsplit(arg, '=');
      if (splits.size() == 2) {
        string optionkey = splits[0].substr(option_prefix.size());
        option_map[optionkey] = splits[1];
      } else {
        string optionkey = splits[0].substr(option_prefix.size());
        flags.push_back(optionkey);
      }
    } else {
      cmdtokens.push_back(arg);
    }
  }

  if (cmdtokens.size() < 1) {
    fprintf(stderr, "command not specified!");
    return nullptr;
  }

  string cmd = cmdtokens[0];
  vector<string> cmdparams(cmdtokens.begin()+1, cmdtokens.end());
  ldbcommand* command = ldbcommand::selectcommand(
    cmd,
    cmdparams,
    option_map,
    flags
  );

  if (command) {
    command->setdboptions(options);
    command->setldboptions(ldb_options);
  }
  return command;
}

ldbcommand* ldbcommand::selectcommand(
    const std::string& cmd,
    const vector<string>& cmdparams,
    const map<string, string>& option_map,
    const vector<string>& flags
  ) {

  if (cmd == getcommand::name()) {
    return new getcommand(cmdparams, option_map, flags);
  } else if (cmd == putcommand::name()) {
    return new putcommand(cmdparams, option_map, flags);
  } else if (cmd == batchputcommand::name()) {
    return new batchputcommand(cmdparams, option_map, flags);
  } else if (cmd == scancommand::name()) {
    return new scancommand(cmdparams, option_map, flags);
  } else if (cmd == deletecommand::name()) {
    return new deletecommand(cmdparams, option_map, flags);
  } else if (cmd == approxsizecommand::name()) {
    return new approxsizecommand(cmdparams, option_map, flags);
  } else if (cmd == dbqueriercommand::name()) {
    return new dbqueriercommand(cmdparams, option_map, flags);
  } else if (cmd == compactorcommand::name()) {
    return new compactorcommand(cmdparams, option_map, flags);
  } else if (cmd == waldumpercommand::name()) {
    return new waldumpercommand(cmdparams, option_map, flags);
  } else if (cmd == reducedblevelscommand::name()) {
    return new reducedblevelscommand(cmdparams, option_map, flags);
  } else if (cmd == changecompactionstylecommand::name()) {
    return new changecompactionstylecommand(cmdparams, option_map, flags);
  } else if (cmd == dbdumpercommand::name()) {
    return new dbdumpercommand(cmdparams, option_map, flags);
  } else if (cmd == dbloadercommand::name()) {
    return new dbloadercommand(cmdparams, option_map, flags);
  } else if (cmd == manifestdumpcommand::name()) {
    return new manifestdumpcommand(cmdparams, option_map, flags);
  } else if (cmd == listcolumnfamiliescommand::name()) {
    return new listcolumnfamiliescommand(cmdparams, option_map, flags);
  } else if (cmd == internaldumpcommand::name()) {
    return new internaldumpcommand(cmdparams, option_map, flags);
  } else if (cmd == checkconsistencycommand::name()) {
    return new checkconsistencycommand(cmdparams, option_map, flags);
  }
  return nullptr;
}


/**
 * parses the specific integer option and fills in the value.
 * returns true if the option is found.
 * returns false if the option is not found or if there is an error parsing the
 * value.  if there is an error, the specified exec_state is also
 * updated.
 */
bool ldbcommand::parseintoption(const map<string, string>& options,
                                const string& option, int& value,
                                ldbcommandexecuteresult& exec_state) {

  map<string, string>::const_iterator itr = option_map_.find(option);
  if (itr != option_map_.end()) {
    try {
      value = stoi(itr->second);
      return true;
    } catch(const invalid_argument&) {
      exec_state = ldbcommandexecuteresult::failed(option +
                      " has an invalid value.");
    } catch(const out_of_range&) {
      exec_state = ldbcommandexecuteresult::failed(option +
                      " has a value out-of-range.");
    }
  }
  return false;
}

/**
 * parses the specified option and fills in the value.
 * returns true if the option is found.
 * returns false otherwise.
 */
bool ldbcommand::parsestringoption(const map<string, string>& options,
                                   const string& option, string* value) {
  auto itr = option_map_.find(option);
  if (itr != option_map_.end()) {
    *value = itr->second;
    return true;
  }
  return false;
}

options ldbcommand::prepareoptionsforopendb() {

  options opt = options_;
  opt.create_if_missing = false;

  map<string, string>::const_iterator itr;

  blockbasedtableoptions table_options;
  int bits;
  if (parseintoption(option_map_, arg_bloom_bits, bits, exec_state_)) {
    if (bits > 0) {
      table_options.filter_policy.reset(newbloomfilterpolicy(bits));
    } else {
      exec_state_ = ldbcommandexecuteresult::failed(arg_bloom_bits +
                      " must be > 0.");
    }
  }

  int block_size;
  if (parseintoption(option_map_, arg_block_size, block_size, exec_state_)) {
    if (block_size > 0) {
      table_options.block_size = block_size;
      opt.table_factory.reset(newblockbasedtablefactory(table_options));
    } else {
      exec_state_ = ldbcommandexecuteresult::failed(arg_block_size +
                      " must be > 0.");
    }
  }

  itr = option_map_.find(arg_auto_compaction);
  if (itr != option_map_.end()) {
    opt.disable_auto_compactions = ! stringtobool(itr->second);
  }

  itr = option_map_.find(arg_compression_type);
  if (itr != option_map_.end()) {
    string comp = itr->second;
    if (comp == "no") {
      opt.compression = knocompression;
    } else if (comp == "snappy") {
      opt.compression = ksnappycompression;
    } else if (comp == "zlib") {
      opt.compression = kzlibcompression;
    } else if (comp == "bzip2") {
      opt.compression = kbzip2compression;
    } else if (comp == "lz4") {
      opt.compression = klz4compression;
    } else if (comp == "lz4hc") {
      opt.compression = klz4hccompression;
    } else {
      // unknown compression.
      exec_state_ = ldbcommandexecuteresult::failed(
                      "unknown compression level: " + comp);
    }
  }

  int write_buffer_size;
  if (parseintoption(option_map_, arg_write_buffer_size, write_buffer_size,
        exec_state_)) {
    if (write_buffer_size > 0) {
      opt.write_buffer_size = write_buffer_size;
    } else {
      exec_state_ = ldbcommandexecuteresult::failed(arg_write_buffer_size +
                      " must be > 0.");
    }
  }

  int file_size;
  if (parseintoption(option_map_, arg_file_size, file_size, exec_state_)) {
    if (file_size > 0) {
      opt.target_file_size_base = file_size;
    } else {
      exec_state_ = ldbcommandexecuteresult::failed(arg_file_size +
                      " must be > 0.");
    }
  }

  if (opt.db_paths.size() == 0) {
    opt.db_paths.emplace_back(db_path_, std::numeric_limits<uint64_t>::max());
  }

  return opt;
}

bool ldbcommand::parsekeyvalue(const string& line, string* key, string* value,
                              bool is_key_hex, bool is_value_hex) {
  size_t pos = line.find(delim);
  if (pos != string::npos) {
    *key = line.substr(0, pos);
    *value = line.substr(pos + strlen(delim));
    if (is_key_hex) {
      *key = hextostring(*key);
    }
    if (is_value_hex) {
      *value = hextostring(*value);
    }
    return true;
  } else {
    return false;
  }
}

/**
 * make sure that only the command-line options and flags expected by this
 * command are specified on the command-line.  extraneous options are usually
 * the result of user error.
 * returns true if all checks pass.  else returns false, and prints an
 * appropriate error msg to stderr.
 */
bool ldbcommand::validatecmdlineoptions() {

  for (map<string, string>::const_iterator itr = option_map_.begin();
        itr != option_map_.end(); itr++) {
    if (find(valid_cmd_line_options_.begin(),
          valid_cmd_line_options_.end(), itr->first) ==
          valid_cmd_line_options_.end()) {
      fprintf(stderr, "invalid command-line option %s\n", itr->first.c_str());
      return false;
    }
  }

  for (vector<string>::const_iterator itr = flags_.begin();
        itr != flags_.end(); itr++) {
    if (find(valid_cmd_line_options_.begin(),
          valid_cmd_line_options_.end(), *itr) ==
          valid_cmd_line_options_.end()) {
      fprintf(stderr, "invalid command-line flag %s\n", itr->c_str());
      return false;
    }
  }

  if (!nodbopen() && option_map_.find(arg_db) == option_map_.end()) {
    fprintf(stderr, "%s must be specified\n", arg_db.c_str());
    return false;
  }

  return true;
}

compactorcommand::compactorcommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags) :
    ldbcommand(options, flags, false,
               buildcmdlineoptions({arg_from, arg_to, arg_hex, arg_key_hex,
                                    arg_value_hex, arg_ttl})),
    null_from_(true), null_to_(true) {

  map<string, string>::const_iterator itr = options.find(arg_from);
  if (itr != options.end()) {
    null_from_ = false;
    from_ = itr->second;
  }

  itr = options.find(arg_to);
  if (itr != options.end()) {
    null_to_ = false;
    to_ = itr->second;
  }

  if (is_key_hex_) {
    if (!null_from_) {
      from_ = hextostring(from_);
    }
    if (!null_to_) {
      to_ = hextostring(to_);
    }
  }
}

void compactorcommand::help(string& ret) {
  ret.append("  ");
  ret.append(compactorcommand::name());
  ret.append(helprangecmdargs());
  ret.append("\n");
}

void compactorcommand::docommand() {

  slice* begin = nullptr;
  slice* end = nullptr;
  if (!null_from_) {
    begin = new slice(from_);
  }
  if (!null_to_) {
    end = new slice(to_);
  }

  db_->compactrange(begin, end);
  exec_state_ = ldbcommandexecuteresult::succeed("");

  delete begin;
  delete end;
}

const string dbloadercommand::arg_disable_wal = "disable_wal";
const string dbloadercommand::arg_bulk_load = "bulk_load";
const string dbloadercommand::arg_compact = "compact";

dbloadercommand::dbloadercommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags) :
    ldbcommand(options, flags, false,
               buildcmdlineoptions({arg_hex, arg_key_hex, arg_value_hex,
                                    arg_from, arg_to, arg_create_if_missing,
                                    arg_disable_wal, arg_bulk_load,
                                    arg_compact})),
    create_if_missing_(false), disable_wal_(false), bulk_load_(false),
    compact_(false) {

  create_if_missing_ = isflagpresent(flags, arg_create_if_missing);
  disable_wal_ = isflagpresent(flags, arg_disable_wal);
  bulk_load_ = isflagpresent(flags, arg_bulk_load);
  compact_ = isflagpresent(flags, arg_compact);
}

void dbloadercommand::help(string& ret) {
  ret.append("  ");
  ret.append(dbloadercommand::name());
  ret.append(" [--" + arg_create_if_missing + "]");
  ret.append(" [--" + arg_disable_wal + "]");
  ret.append(" [--" + arg_bulk_load + "]");
  ret.append(" [--" + arg_compact + "]");
  ret.append("\n");
}

options dbloadercommand::prepareoptionsforopendb() {
  options opt = ldbcommand::prepareoptionsforopendb();
  opt.create_if_missing = create_if_missing_;
  if (bulk_load_) {
    opt.prepareforbulkload();
  }
  return opt;
}

void dbloadercommand::docommand() {
  if (!db_) {
    return;
  }

  writeoptions write_options;
  if (disable_wal_) {
    write_options.disablewal = true;
  }

  int bad_lines = 0;
  string line;
  while (getline(cin, line, '\n')) {
    string key;
    string value;
    if (parsekeyvalue(line, &key, &value, is_key_hex_, is_value_hex_)) {
      db_->put(write_options, slice(key), slice(value));
    } else if (0 == line.find("keys in range:")) {
      // ignore this line
    } else if (0 == line.find("created bg thread 0x")) {
      // ignore this line
    } else {
      bad_lines ++;
    }
  }

  if (bad_lines > 0) {
    cout << "warning: " << bad_lines << " bad lines ignored." << endl;
  }
  if (compact_) {
    db_->compactrange(nullptr, nullptr);
  }
}

// ----------------------------------------------------------------------------

const string manifestdumpcommand::arg_verbose = "verbose";
const string manifestdumpcommand::arg_path    = "path";

void manifestdumpcommand::help(string& ret) {
  ret.append("  ");
  ret.append(manifestdumpcommand::name());
  ret.append(" [--" + arg_verbose + "]");
  ret.append(" [--" + arg_path + "=<path_to_manifest_file>]");
  ret.append("\n");
}

manifestdumpcommand::manifestdumpcommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags) :
    ldbcommand(options, flags, false,
               buildcmdlineoptions({arg_verbose, arg_path, arg_hex})),
    verbose_(false),
    path_("")
{
  verbose_ = isflagpresent(flags, arg_verbose);

  map<string, string>::const_iterator itr = options.find(arg_path);
  if (itr != options.end()) {
    path_ = itr->second;
    if (path_.empty()) {
      exec_state_ = ldbcommandexecuteresult::failed("--path: missing pathname");
    }
  }
}

void manifestdumpcommand::docommand() {

  std::string manifestfile;

  if (!path_.empty()) {
    manifestfile = path_;
  } else {
    bool found = false;
    // we need to find the manifest file by searching the directory
    // containing the db for files of the form manifest_[0-9]+
    dir* d = opendir(db_path_.c_str());
    if (d == nullptr) {
      exec_state_ = ldbcommandexecuteresult::failed(
        db_path_ + " is not a directory");
      return;
    }
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr) {
      unsigned int match;
      unsigned long long num;
      if (sscanf(entry->d_name,
                 "manifest-%ln%ln",
                 (unsigned long*)&num,
                 (unsigned long*)&match)
          && match == strlen(entry->d_name)) {
        if (!found) {
          manifestfile = db_path_ + "/" + std::string(entry->d_name);
          found = true;
        } else {
          exec_state_ = ldbcommandexecuteresult::failed(
            "multiple manifest files found; use --path to select one");
          return;
        }
      }
    }
    closedir(d);
  }

  if (verbose_) {
    printf("processing manifest file %s\n", manifestfile.c_str());
  }

  options options;
  envoptions sopt;
  std::string file(manifestfile);
  std::string dbname("dummy");
  std::shared_ptr<cache> tc(newlrucache(
      options.max_open_files - 10, options.table_cache_numshardbits,
      options.table_cache_remove_scan_count_limit));
  // notice we are using the default options not through sanitizeoptions(),
  // if versionset::dumpmanifest() depends on any option done by
  // sanitizeoptions(), we need to initialize it manually.
  options.db_paths.emplace_back("dummy", 0);
  versionset versions(dbname, &options, sopt, tc.get());
  status s = versions.dumpmanifest(options, file, verbose_, is_key_hex_);
  if (!s.ok()) {
    printf("error in processing file %s %s\n", manifestfile.c_str(),
           s.tostring().c_str());
  }
  if (verbose_) {
    printf("processing manifest file %s done\n", manifestfile.c_str());
  }
}

// ----------------------------------------------------------------------------

void listcolumnfamiliescommand::help(string& ret) {
  ret.append("  ");
  ret.append(listcolumnfamiliescommand::name());
  ret.append(" full_path_to_db_directory ");
  ret.append("\n");
}

listcolumnfamiliescommand::listcolumnfamiliescommand(
    const vector<string>& params, const map<string, string>& options,
    const vector<string>& flags)
    : ldbcommand(options, flags, false, {}) {

  if (params.size() != 1) {
    exec_state_ = ldbcommandexecuteresult::failed(
        "dbname must be specified for the list_column_families command");
  } else {
    dbname_ = params[0];
  }
}

void listcolumnfamiliescommand::docommand() {
  vector<string> column_families;
  status s = db::listcolumnfamilies(dboptions(), dbname_, &column_families);
  if (!s.ok()) {
    printf("error in processing db %s %s\n", dbname_.c_str(),
           s.tostring().c_str());
  } else {
    printf("column families in %s: \n{", dbname_.c_str());
    bool first = true;
    for (auto cf : column_families) {
      if (!first) {
        printf(", ");
      }
      first = false;
      printf("%s", cf.c_str());
    }
    printf("}\n");
  }
}

// ----------------------------------------------------------------------------

namespace {

string readabletime(int unixtime) {
  char time_buffer [80];
  time_t rawtime = unixtime;
  struct tm * timeinfo = localtime(&rawtime);
  strftime(time_buffer, 80, "%c", timeinfo);
  return string(time_buffer);
}

// this function only called when it's the sane case of >1 buckets in time-range
// also called only when timekv falls between ttl_start and ttl_end provided
void incbucketcounts(vector<uint64_t>& bucket_counts, int ttl_start,
      int time_range, int bucket_size, int timekv, int num_buckets) {
  assert(time_range > 0 && timekv >= ttl_start && bucket_size > 0 &&
    timekv < (ttl_start + time_range) && num_buckets > 1);
  int bucket = (timekv - ttl_start) / bucket_size;
  bucket_counts[bucket]++;
}

void printbucketcounts(const vector<uint64_t>& bucket_counts, int ttl_start,
      int ttl_end, int bucket_size, int num_buckets) {
  int time_point = ttl_start;
  for(int i = 0; i < num_buckets - 1; i++, time_point += bucket_size) {
    fprintf(stdout, "keys in range %s to %s : %lu\n",
            readabletime(time_point).c_str(),
            readabletime(time_point + bucket_size).c_str(),
            (unsigned long)bucket_counts[i]);
  }
  fprintf(stdout, "keys in range %s to %s : %lu\n",
          readabletime(time_point).c_str(),
          readabletime(ttl_end).c_str(),
          (unsigned long)bucket_counts[num_buckets - 1]);
}

}  // namespace

const string internaldumpcommand::arg_count_only = "count_only";
const string internaldumpcommand::arg_count_delim = "count_delim";
const string internaldumpcommand::arg_stats = "stats";
const string internaldumpcommand::arg_input_key_hex = "input_key_hex";

internaldumpcommand::internaldumpcommand(const vector<string>& params,
                                         const map<string, string>& options,
                                         const vector<string>& flags) :
    ldbcommand(options, flags, true,
               buildcmdlineoptions({ arg_hex, arg_key_hex, arg_value_hex,
                                     arg_from, arg_to, arg_max_keys,
                                     arg_count_only, arg_count_delim, arg_stats,
                                     arg_input_key_hex})),
    has_from_(false),
    has_to_(false),
    max_keys_(-1),
    delim_("."),
    count_only_(false),
    count_delim_(false),
    print_stats_(false),
    is_input_key_hex_(false) {

  has_from_ = parsestringoption(options, arg_from, &from_);
  has_to_ = parsestringoption(options, arg_to, &to_);

  parseintoption(options, arg_max_keys, max_keys_, exec_state_);
  map<string, string>::const_iterator itr = options.find(arg_count_delim);
  if (itr != options.end()) {
    delim_ = itr->second;
    count_delim_ = true;
   // fprintf(stdout,"delim = %c\n",delim_[0]);
  } else {
    count_delim_ = isflagpresent(flags, arg_count_delim);
    delim_=".";
  }

  print_stats_ = isflagpresent(flags, arg_stats);
  count_only_ = isflagpresent(flags, arg_count_only);
  is_input_key_hex_ = isflagpresent(flags, arg_input_key_hex);

  if (is_input_key_hex_) {
    if (has_from_) {
      from_ = hextostring(from_);
    }
    if (has_to_) {
      to_ = hextostring(to_);
    }
  }
}

void internaldumpcommand::help(string& ret) {
  ret.append("  ");
  ret.append(internaldumpcommand::name());
  ret.append(helprangecmdargs());
  ret.append(" [--" + arg_input_key_hex + "]");
  ret.append(" [--" + arg_max_keys + "=<n>]");
  ret.append(" [--" + arg_count_only + "]");
  ret.append(" [--" + arg_count_delim + "=<char>]");
  ret.append(" [--" + arg_stats + "]");
  ret.append("\n");
}

void internaldumpcommand::docommand() {
  if (!db_) {
    return;
  }

  if (print_stats_) {
    string stats;
    if (db_->getproperty("rocksdb.stats", &stats)) {
      fprintf(stdout, "%s\n", stats.c_str());
    }
  }

  // cast as dbimpl to get internal iterator
  dbimpl* idb = dynamic_cast<dbimpl*>(db_);
  if (!idb) {
    exec_state_ = ldbcommandexecuteresult::failed("db is not dbimpl");
    return;
  }
  string rtype1,rtype2,row,val;
  rtype2 = "";
  uint64_t c=0;
  uint64_t s1=0,s2=0;
  // setup internal key iterator
  auto iter = unique_ptr<iterator>(idb->test_newinternaliterator());
  status st = iter->status();
  if (!st.ok()) {
    exec_state_ = ldbcommandexecuteresult::failed("iterator error:"
                                                  + st.tostring());
  }

  if (has_from_) {
    internalkey ikey(from_, kmaxsequencenumber, kvaluetypeforseek);
    iter->seek(ikey.encode());
  } else {
    iter->seektofirst();
  }

  long long count = 0;
  for (; iter->valid(); iter->next()) {
    parsedinternalkey ikey;
    if (!parseinternalkey(iter->key(), &ikey)) {
      fprintf(stderr, "internal key [%s] parse error!\n",
              iter->key().tostring(true /* in hex*/).data());
      // todo: add error counter
      continue;
    }

    // if end marker was specified, we stop before it
    if (has_to_ && options_.comparator->compare(ikey.user_key, to_) >= 0) {
      break;
    }

    ++count;
    int k;
    if (count_delim_) {
      rtype1 = "";
      s1=0;
      row = iter->key().tostring();
      val = iter->value().tostring();
      for(k=0;row[k]!='\x01' && row[k]!='\0';k++)
        s1++;
      for(k=0;val[k]!='\x01' && val[k]!='\0';k++)
        s1++;
      for(int j=0;row[j]!=delim_[0] && row[j]!='\0' && row[j]!='\x01';j++)
        rtype1+=row[j];
      if(rtype2.compare("") && rtype2.compare(rtype1)!=0) {
        fprintf(stdout,"%s => count:%lld\tsize:%lld\n",rtype2.c_str(),
            (long long)c,(long long)s2);
        c=1;
        s2=s1;
        rtype2 = rtype1;
      } else {
        c++;
        s2+=s1;
        rtype2=rtype1;
    }
  }

    if (!count_only_ && !count_delim_) {
      string key = ikey.debugstring(is_key_hex_);
      string value = iter->value().tostring(is_value_hex_);
      std::cout << key << " => " << value << "\n";
    }

    // terminate if maximum number of keys have been dumped
    if (max_keys_ > 0 && count >= max_keys_) break;
  }
  if(count_delim_) {
    fprintf(stdout,"%s => count:%lld\tsize:%lld\n", rtype2.c_str(),
        (long long)c,(long long)s2);
  } else
  fprintf(stdout, "internal keys in range: %lld\n", (long long) count);
}


const string dbdumpercommand::arg_count_only = "count_only";
const string dbdumpercommand::arg_count_delim = "count_delim";
const string dbdumpercommand::arg_stats = "stats";
const string dbdumpercommand::arg_ttl_bucket = "bucket";

dbdumpercommand::dbdumpercommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags) :
    ldbcommand(options, flags, true,
               buildcmdlineoptions({arg_ttl, arg_hex, arg_key_hex,
                                    arg_value_hex, arg_from, arg_to,
                                    arg_max_keys, arg_count_only,
                                    arg_count_delim, arg_stats, arg_ttl_start,
                                    arg_ttl_end, arg_ttl_bucket,
                                    arg_timestamp})),
    null_from_(true),
    null_to_(true),
    max_keys_(-1),
    count_only_(false),
    count_delim_(false),
    print_stats_(false) {

  map<string, string>::const_iterator itr = options.find(arg_from);
  if (itr != options.end()) {
    null_from_ = false;
    from_ = itr->second;
  }

  itr = options.find(arg_to);
  if (itr != options.end()) {
    null_to_ = false;
    to_ = itr->second;
  }

  itr = options.find(arg_max_keys);
  if (itr != options.end()) {
    try {
      max_keys_ = stoi(itr->second);
    } catch(const invalid_argument&) {
      exec_state_ = ldbcommandexecuteresult::failed(arg_max_keys +
                        " has an invalid value");
    } catch(const out_of_range&) {
      exec_state_ = ldbcommandexecuteresult::failed(arg_max_keys +
                        " has a value out-of-range");
    }
  }
  itr = options.find(arg_count_delim);
  if (itr != options.end()) {
    delim_ = itr->second;
    count_delim_ = true;
  } else {
    count_delim_ = isflagpresent(flags, arg_count_delim);
    delim_=".";
  }

  print_stats_ = isflagpresent(flags, arg_stats);
  count_only_ = isflagpresent(flags, arg_count_only);

  if (is_key_hex_) {
    if (!null_from_) {
      from_ = hextostring(from_);
    }
    if (!null_to_) {
      to_ = hextostring(to_);
    }
  }
}

void dbdumpercommand::help(string& ret) {
  ret.append("  ");
  ret.append(dbdumpercommand::name());
  ret.append(helprangecmdargs());
  ret.append(" [--" + arg_ttl + "]");
  ret.append(" [--" + arg_max_keys + "=<n>]");
  ret.append(" [--" + arg_timestamp + "]");
  ret.append(" [--" + arg_count_only + "]");
  ret.append(" [--" + arg_count_delim + "=<char>]");
  ret.append(" [--" + arg_stats + "]");
  ret.append(" [--" + arg_ttl_bucket + "=<n>]");
  ret.append(" [--" + arg_ttl_start + "=<n>:- is inclusive]");
  ret.append(" [--" + arg_ttl_end + "=<n>:- is exclusive]");
  ret.append("\n");
}

void dbdumpercommand::docommand() {
  if (!db_) {
    return;
  }
  // parse command line args
  uint64_t count = 0;
  if (print_stats_) {
    string stats;
    if (db_->getproperty("rocksdb.stats", &stats)) {
      fprintf(stdout, "%s\n", stats.c_str());
    }
  }

  // setup key iterator
  iterator* iter = db_->newiterator(readoptions());
  status st = iter->status();
  if (!st.ok()) {
    exec_state_ = ldbcommandexecuteresult::failed("iterator error."
        + st.tostring());
  }

  if (!null_from_) {
    iter->seek(from_);
  } else {
    iter->seektofirst();
  }

  int max_keys = max_keys_;
  int ttl_start;
  if (!parseintoption(option_map_, arg_ttl_start, ttl_start, exec_state_)) {
    ttl_start = dbwithttlimpl::kmintimestamp;  // ttl introduction time
  }
  int ttl_end;
  if (!parseintoption(option_map_, arg_ttl_end, ttl_end, exec_state_)) {
    ttl_end = dbwithttlimpl::kmaxtimestamp;  // max time allowed by ttl feature
  }
  if (ttl_end < ttl_start) {
    fprintf(stderr, "error: end time can't be less than start time\n");
    delete iter;
    return;
  }
  int time_range = ttl_end - ttl_start;
  int bucket_size;
  if (!parseintoption(option_map_, arg_ttl_bucket, bucket_size, exec_state_) ||
      bucket_size <= 0) {
    bucket_size = time_range; // will have just 1 bucket by default
  }
  //cretaing variables for row count of each type
  string rtype1,rtype2,row,val;
  rtype2 = "";
  uint64_t c=0;
  uint64_t s1=0,s2=0;

  // at this point, bucket_size=0 => time_range=0
  uint64_t num_buckets = (bucket_size >= time_range) ? 1 :
    ((time_range + bucket_size - 1) / bucket_size);
  vector<uint64_t> bucket_counts(num_buckets, 0);
  if (is_db_ttl_ && !count_only_ && timestamp_ && !count_delim_) {
    fprintf(stdout, "dumping key-values from %s to %s\n",
            readabletime(ttl_start).c_str(), readabletime(ttl_end).c_str());
  }

  for (; iter->valid(); iter->next()) {
    int rawtime = 0;
    // if end marker was specified, we stop before it
    if (!null_to_ && (iter->key().tostring() >= to_))
      break;
    // terminate if maximum number of keys have been dumped
    if (max_keys == 0)
      break;
    if (is_db_ttl_) {
      ttliterator* it_ttl = dynamic_cast<ttliterator*>(iter);
      assert(it_ttl);
      rawtime = it_ttl->timestamp();
      if (rawtime < ttl_start || rawtime >= ttl_end) {
        continue;
      }
    }
    if (max_keys > 0) {
      --max_keys;
    }
    if (is_db_ttl_ && num_buckets > 1) {
      incbucketcounts(bucket_counts, ttl_start, time_range, bucket_size,
                      rawtime, num_buckets);
    }
    ++count;
    if (count_delim_) {
      rtype1 = "";
      row = iter->key().tostring();
      val = iter->value().tostring();
      s1 = row.size()+val.size();
      for(int j=0;row[j]!=delim_[0] && row[j]!='\0';j++)
        rtype1+=row[j];
      if(rtype2.compare("") && rtype2.compare(rtype1)!=0) {
        fprintf(stdout,"%s => count:%lld\tsize:%lld\n",rtype2.c_str(),
            (long long )c,(long long)s2);
        c=1;
        s2=s1;
        rtype2 = rtype1;
      } else {
          c++;
          s2+=s1;
          rtype2=rtype1;
      }

    }



    if (!count_only_ && !count_delim_) {
      if (is_db_ttl_ && timestamp_) {
        fprintf(stdout, "%s ", readabletime(rawtime).c_str());
      }
      string str = printkeyvalue(iter->key().tostring(),
                                 iter->value().tostring(), is_key_hex_,
                                 is_value_hex_);
      fprintf(stdout, "%s\n", str.c_str());
    }
  }

  if (num_buckets > 1 && is_db_ttl_) {
    printbucketcounts(bucket_counts, ttl_start, ttl_end, bucket_size,
                      num_buckets);
  } else if(count_delim_) {
    fprintf(stdout,"%s => count:%lld\tsize:%lld\n",rtype2.c_str(),
        (long long )c,(long long)s2);
  } else {
    fprintf(stdout, "keys in range: %lld\n", (long long) count);
  }
  // clean up
  delete iter;
}

const string reducedblevelscommand::arg_new_levels = "new_levels";
const string  reducedblevelscommand::arg_print_old_levels = "print_old_levels";

reducedblevelscommand::reducedblevelscommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags) :
    ldbcommand(options, flags, false,
               buildcmdlineoptions({arg_new_levels, arg_print_old_levels})),
    old_levels_(1 << 16),
    new_levels_(-1),
    print_old_levels_(false) {


  parseintoption(option_map_, arg_new_levels, new_levels_, exec_state_);
  print_old_levels_ = isflagpresent(flags, arg_print_old_levels);

  if(new_levels_ <= 0) {
    exec_state_ = ldbcommandexecuteresult::failed(
           " use --" + arg_new_levels + " to specify a new level number\n");
  }
}

vector<string> reducedblevelscommand::prepareargs(const string& db_path,
    int new_levels, bool print_old_level) {
  vector<string> ret;
  ret.push_back("reduce_levels");
  ret.push_back("--" + arg_db + "=" + db_path);
  ret.push_back("--" + arg_new_levels + "=" + to_string(new_levels));
  if(print_old_level) {
    ret.push_back("--" + arg_print_old_levels);
  }
  return ret;
}

void reducedblevelscommand::help(string& ret) {
  ret.append("  ");
  ret.append(reducedblevelscommand::name());
  ret.append(" --" + arg_new_levels + "=<new number of levels>");
  ret.append(" [--" + arg_print_old_levels + "]");
  ret.append("\n");
}

options reducedblevelscommand::prepareoptionsforopendb() {
  options opt = ldbcommand::prepareoptionsforopendb();
  opt.num_levels = old_levels_;
  opt.max_bytes_for_level_multiplier_additional.resize(opt.num_levels, 1);
  // disable size compaction
  opt.max_bytes_for_level_base = 1ull << 50;
  opt.max_bytes_for_level_multiplier = 1;
  opt.max_mem_compaction_level = 0;
  return opt;
}

status reducedblevelscommand::getoldnumoflevels(options& opt,
    int* levels) {
  envoptions soptions;
  std::shared_ptr<cache> tc(
      newlrucache(opt.max_open_files - 10, opt.table_cache_numshardbits,
                  opt.table_cache_remove_scan_count_limit));
  const internalkeycomparator cmp(opt.comparator);
  versionset versions(db_path_, &opt, soptions, tc.get());
  std::vector<columnfamilydescriptor> dummy;
  columnfamilydescriptor dummy_descriptor(kdefaultcolumnfamilyname,
                                          columnfamilyoptions(opt));
  dummy.push_back(dummy_descriptor);
  // we rely the versionset::recover to tell us the internal data structures
  // in the db. and the recover() should never do any change
  // (like logandapply) to the manifest file.
  status st = versions.recover(dummy);
  if (!st.ok()) {
    return st;
  }
  int max = -1;
  auto default_cfd = versions.getcolumnfamilyset()->getdefault();
  for (int i = 0; i < default_cfd->numberlevels(); i++) {
    if (default_cfd->current()->numlevelfiles(i)) {
      max = i;
    }
  }

  *levels = max + 1;
  return st;
}

void reducedblevelscommand::docommand() {
  if (new_levels_ <= 1) {
    exec_state_ = ldbcommandexecuteresult::failed(
        "invalid number of levels.\n");
    return;
  }

  status st;
  options opt = prepareoptionsforopendb();
  int old_level_num = -1;
  st = getoldnumoflevels(opt, &old_level_num);
  if (!st.ok()) {
    exec_state_ = ldbcommandexecuteresult::failed(st.tostring());
    return;
  }

  if (print_old_levels_) {
    fprintf(stdout, "the old number of levels in use is %d\n", old_level_num);
  }

  if (old_level_num <= new_levels_) {
    return;
  }

  old_levels_ = old_level_num;

  opendb();
  if (!db_) {
    return;
  }
  // compact the whole db to put all files to the highest level.
  fprintf(stdout, "compacting the db...\n");
  db_->compactrange(nullptr, nullptr);
  closedb();

  envoptions soptions;
  st = versionset::reducenumberoflevels(db_path_, &opt, soptions, new_levels_);
  if (!st.ok()) {
    exec_state_ = ldbcommandexecuteresult::failed(st.tostring());
    return;
  }
}

const string changecompactionstylecommand::arg_old_compaction_style =
  "old_compaction_style";
const string changecompactionstylecommand::arg_new_compaction_style =
  "new_compaction_style";

changecompactionstylecommand::changecompactionstylecommand(
      const vector<string>& params, const map<string, string>& options,
      const vector<string>& flags) :
    ldbcommand(options, flags, false,
               buildcmdlineoptions({arg_old_compaction_style,
                                    arg_new_compaction_style})),
    old_compaction_style_(-1),
    new_compaction_style_(-1) {

  parseintoption(option_map_, arg_old_compaction_style, old_compaction_style_,
    exec_state_);
  if (old_compaction_style_ != kcompactionstylelevel &&
     old_compaction_style_ != kcompactionstyleuniversal) {
    exec_state_ = ldbcommandexecuteresult::failed(
      "use --" + arg_old_compaction_style + " to specify old compaction " +
      "style. check ldb help for proper compaction style value.\n");
    return;
  }

  parseintoption(option_map_, arg_new_compaction_style, new_compaction_style_,
    exec_state_);
  if (new_compaction_style_ != kcompactionstylelevel &&
     new_compaction_style_ != kcompactionstyleuniversal) {
    exec_state_ = ldbcommandexecuteresult::failed(
      "use --" + arg_new_compaction_style + " to specify new compaction " +
      "style. check ldb help for proper compaction style value.\n");
    return;
  }

  if (new_compaction_style_ == old_compaction_style_) {
    exec_state_ = ldbcommandexecuteresult::failed(
      "old compaction style is the same as new compaction style. "
      "nothing to do.\n");
    return;
  }

  if (old_compaction_style_ == kcompactionstyleuniversal &&
      new_compaction_style_ == kcompactionstylelevel) {
    exec_state_ = ldbcommandexecuteresult::failed(
      "convert from universal compaction to level compaction. "
      "nothing to do.\n");
    return;
  }
}

void changecompactionstylecommand::help(string& ret) {
  ret.append("  ");
  ret.append(changecompactionstylecommand::name());
  ret.append(" --" + arg_old_compaction_style + "=<old compaction style: 0 " +
             "for level compaction, 1 for universal compaction>");
  ret.append(" --" + arg_new_compaction_style + "=<new compaction style: 0 " +
             "for level compaction, 1 for universal compaction>");
  ret.append("\n");
}

options changecompactionstylecommand::prepareoptionsforopendb() {
  options opt = ldbcommand::prepareoptionsforopendb();

  if (old_compaction_style_ == kcompactionstylelevel &&
      new_compaction_style_ == kcompactionstyleuniversal) {
    // in order to convert from level compaction to universal compaction, we
    // need to compact all data into a single file and move it to level 0.
    opt.disable_auto_compactions = true;
    opt.target_file_size_base = int_max;
    opt.target_file_size_multiplier = 1;
    opt.max_bytes_for_level_base = int_max;
    opt.max_bytes_for_level_multiplier = 1;
  }

  return opt;
}

void changecompactionstylecommand::docommand() {
  // print db stats before we have made any change
  std::string property;
  std::string files_per_level;
  for (int i = 0; i < db_->numberlevels(); i++) {
    db_->getproperty("rocksdb.num-files-at-level" + numbertostring(i),
                     &property);

    // format print string
    char buf[100];
    snprintf(buf, sizeof(buf), "%s%s", (i ? "," : ""), property.c_str());
    files_per_level += buf;
  }
  fprintf(stdout, "files per level before compaction: %s\n",
          files_per_level.c_str());

  // manual compact into a single file and move the file to level 0
  db_->compactrange(nullptr, nullptr,
                    true /* reduce level */,
                    0    /* reduce to level 0 */);

  // verify compaction result
  files_per_level = "";
  int num_files = 0;
  for (int i = 0; i < db_->numberlevels(); i++) {
    db_->getproperty("rocksdb.num-files-at-level" + numbertostring(i),
                     &property);

    // format print string
    char buf[100];
    snprintf(buf, sizeof(buf), "%s%s", (i ? "," : ""), property.c_str());
    files_per_level += buf;

    num_files = atoi(property.c_str());

    // level 0 should have only 1 file
    if (i == 0 && num_files != 1) {
      exec_state_ = ldbcommandexecuteresult::failed("number of db files at "
        "level 0 after compaction is " + std::to_string(num_files) +
        ", not 1.\n");
      return;
    }
    // other levels should have no file
    if (i > 0 && num_files != 0) {
      exec_state_ = ldbcommandexecuteresult::failed("number of db files at "
        "level " + std::to_string(i) + " after compaction is " +
        std::to_string(num_files) + ", not 0.\n");
      return;
    }
  }

  fprintf(stdout, "files per level after compaction: %s\n",
          files_per_level.c_str());
}

class inmemoryhandler : public writebatch::handler {
 public:
  inmemoryhandler(stringstream& row, bool print_values) : handler(),row_(row) {
    print_values_ = print_values;
  }

  void commonputmerge(const slice& key, const slice& value) {
    string k = ldbcommand::stringtohex(key.tostring());
    if (print_values_) {
      string v = ldbcommand::stringtohex(value.tostring());
      row_ << k << " : ";
      row_ << v << " ";
    } else {
      row_ << k << " ";
    }
  }

  virtual void put(const slice& key, const slice& value) {
    row_ << "put : ";
    commonputmerge(key, value);
  }

  virtual void merge(const slice& key, const slice& value) {
    row_ << "merge : ";
    commonputmerge(key, value);
  }

  virtual void delete(const slice& key) {
    row_ <<",delete : ";
    row_ << ldbcommand::stringtohex(key.tostring()) << " ";
  }

  virtual ~inmemoryhandler() { };

 private:
  stringstream & row_;
  bool print_values_;
};

const string waldumpercommand::arg_wal_file = "walfile";
const string waldumpercommand::arg_print_value = "print_value";
const string waldumpercommand::arg_print_header = "header";

waldumpercommand::waldumpercommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags) :
    ldbcommand(options, flags, true,
               buildcmdlineoptions(
                {arg_wal_file, arg_print_header, arg_print_value})),
    print_header_(false), print_values_(false) {

  wal_file_.clear();

  map<string, string>::const_iterator itr = options.find(arg_wal_file);
  if (itr != options.end()) {
    wal_file_ = itr->second;
  }


  print_header_ = isflagpresent(flags, arg_print_header);
  print_values_ = isflagpresent(flags, arg_print_value);
  if (wal_file_.empty()) {
    exec_state_ = ldbcommandexecuteresult::failed(
                    "argument " + arg_wal_file + " must be specified.");
  }
}

void waldumpercommand::help(string& ret) {
  ret.append("  ");
  ret.append(waldumpercommand::name());
  ret.append(" --" + arg_wal_file + "=<write_ahead_log_file_path>");
  ret.append(" [--" + arg_print_header + "] ");
  ret.append(" [--" + arg_print_value + "] ");
  ret.append("\n");
}

void waldumpercommand::docommand() {
  struct stderrreporter : public log::reader::reporter {
    virtual void corruption(size_t bytes, const status& s) {
      cerr<<"corruption detected in log file "<<s.tostring()<<"\n";
    }
  };

  unique_ptr<sequentialfile> file;
  env* env_ = env::default();
  envoptions soptions;
  status status = env_->newsequentialfile(wal_file_, &file, soptions);
  if (!status.ok()) {
    exec_state_ = ldbcommandexecuteresult::failed("failed to open wal file " +
      status.tostring());
  } else {
    stderrreporter reporter;
    log::reader reader(move(file), &reporter, true, 0);
    string scratch;
    writebatch batch;
    slice record;
    stringstream row;
    if (print_header_) {
      cout<<"sequence,count,bytesize,physical offset,key(s)";
      if (print_values_) {
        cout << " : value ";
      }
      cout << "\n";
    }
    while(reader.readrecord(&record, &scratch)) {
      row.str("");
      if (record.size() < 12) {
        reporter.corruption(
            record.size(), status::corruption("log record too small"));
      } else {
        writebatchinternal::setcontents(&batch, record);
        row<<writebatchinternal::sequence(&batch)<<",";
        row<<writebatchinternal::count(&batch)<<",";
        row<<writebatchinternal::bytesize(&batch)<<",";
        row<<reader.lastrecordoffset()<<",";
        inmemoryhandler handler(row, print_values_);
        batch.iterate(&handler);
        row<<"\n";
      }
      cout<<row.str();
    }
  }
}


getcommand::getcommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags) :
  ldbcommand(options, flags, true, buildcmdlineoptions({arg_ttl, arg_hex,
                                                        arg_key_hex,
                                                        arg_value_hex})) {

  if (params.size() != 1) {
    exec_state_ = ldbcommandexecuteresult::failed(
                    "<key> must be specified for the get command");
  } else {
    key_ = params.at(0);
  }

  if (is_key_hex_) {
    key_ = hextostring(key_);
  }
}

void getcommand::help(string& ret) {
  ret.append("  ");
  ret.append(getcommand::name());
  ret.append(" <key>");
  ret.append(" [--" + arg_ttl + "]");
  ret.append("\n");
}

void getcommand::docommand() {
  string value;
  status st = db_->get(readoptions(), key_, &value);
  if (st.ok()) {
    fprintf(stdout, "%s\n",
              (is_value_hex_ ? stringtohex(value) : value).c_str());
  } else {
    exec_state_ = ldbcommandexecuteresult::failed(st.tostring());
  }
}


approxsizecommand::approxsizecommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags) :
  ldbcommand(options, flags, true,
             buildcmdlineoptions({arg_hex, arg_key_hex, arg_value_hex,
                                  arg_from, arg_to})) {

  if (options.find(arg_from) != options.end()) {
    start_key_ = options.find(arg_from)->second;
  } else {
    exec_state_ = ldbcommandexecuteresult::failed(arg_from +
                    " must be specified for approxsize command");
    return;
  }

  if (options.find(arg_to) != options.end()) {
    end_key_ = options.find(arg_to)->second;
  } else {
    exec_state_ = ldbcommandexecuteresult::failed(arg_to +
                    " must be specified for approxsize command");
    return;
  }

  if (is_key_hex_) {
    start_key_ = hextostring(start_key_);
    end_key_ = hextostring(end_key_);
  }
}

void approxsizecommand::help(string& ret) {
  ret.append("  ");
  ret.append(approxsizecommand::name());
  ret.append(helprangecmdargs());
  ret.append("\n");
}

void approxsizecommand::docommand() {

  range ranges[1];
  ranges[0] = range(start_key_, end_key_);
  uint64_t sizes[1];
  db_->getapproximatesizes(ranges, 1, sizes);
  fprintf(stdout, "%lu\n", (unsigned long)sizes[0]);
  /* weird that getapproximatesizes() returns void, although documentation
   * says that it returns a status object.
  if (!st.ok()) {
    exec_state_ = ldbcommandexecuteresult::failed(st.tostring());
  }
  */
}


batchputcommand::batchputcommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags) :
  ldbcommand(options, flags, false,
             buildcmdlineoptions({arg_ttl, arg_hex, arg_key_hex, arg_value_hex,
                                  arg_create_if_missing})) {

  if (params.size() < 2) {
    exec_state_ = ldbcommandexecuteresult::failed(
        "at least one <key> <value> pair must be specified batchput.");
  } else if (params.size() % 2 != 0) {
    exec_state_ = ldbcommandexecuteresult::failed(
        "equal number of <key>s and <value>s must be specified for batchput.");
  } else {
    for (size_t i = 0; i < params.size(); i += 2) {
      string key = params.at(i);
      string value = params.at(i+1);
      key_values_.push_back(pair<string, string>(
                    is_key_hex_ ? hextostring(key) : key,
                    is_value_hex_ ? hextostring(value) : value));
    }
  }
}

void batchputcommand::help(string& ret) {
  ret.append("  ");
  ret.append(batchputcommand::name());
  ret.append(" <key> <value> [<key> <value>] [..]");
  ret.append(" [--" + arg_ttl + "]");
  ret.append("\n");
}

void batchputcommand::docommand() {
  writebatch batch;

  for (vector<pair<string, string>>::const_iterator itr
        = key_values_.begin(); itr != key_values_.end(); itr++) {
      batch.put(itr->first, itr->second);
  }
  status st = db_->write(writeoptions(), &batch);
  if (st.ok()) {
    fprintf(stdout, "ok\n");
  } else {
    exec_state_ = ldbcommandexecuteresult::failed(st.tostring());
  }
}

options batchputcommand::prepareoptionsforopendb() {
  options opt = ldbcommand::prepareoptionsforopendb();
  opt.create_if_missing = isflagpresent(flags_, arg_create_if_missing);
  return opt;
}


scancommand::scancommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags) :
    ldbcommand(options, flags, true,
               buildcmdlineoptions({arg_ttl, arg_hex, arg_key_hex, arg_to,
                                    arg_value_hex, arg_from, arg_timestamp,
                                    arg_max_keys, arg_ttl_start, arg_ttl_end})),
    start_key_specified_(false),
    end_key_specified_(false),
    max_keys_scanned_(-1) {

  map<string, string>::const_iterator itr = options.find(arg_from);
  if (itr != options.end()) {
    start_key_ = itr->second;
    if (is_key_hex_) {
      start_key_ = hextostring(start_key_);
    }
    start_key_specified_ = true;
  }
  itr = options.find(arg_to);
  if (itr != options.end()) {
    end_key_ = itr->second;
    if (is_key_hex_) {
      end_key_ = hextostring(end_key_);
    }
    end_key_specified_ = true;
  }

  itr = options.find(arg_max_keys);
  if (itr != options.end()) {
    try {
      max_keys_scanned_ = stoi(itr->second);
    } catch(const invalid_argument&) {
      exec_state_ = ldbcommandexecuteresult::failed(arg_max_keys +
                        " has an invalid value");
    } catch(const out_of_range&) {
      exec_state_ = ldbcommandexecuteresult::failed(arg_max_keys +
                        " has a value out-of-range");
    }
  }
}

void scancommand::help(string& ret) {
  ret.append("  ");
  ret.append(scancommand::name());
  ret.append(helprangecmdargs());
  ret.append(" [--" + arg_ttl + "]");
  ret.append(" [--" + arg_timestamp + "]");
  ret.append(" [--" + arg_max_keys + "=<n>q] ");
  ret.append(" [--" + arg_ttl_start + "=<n>:- is inclusive]");
  ret.append(" [--" + arg_ttl_end + "=<n>:- is exclusive]");
  ret.append("\n");
}

void scancommand::docommand() {

  int num_keys_scanned = 0;
  iterator* it = db_->newiterator(readoptions());
  if (start_key_specified_) {
    it->seek(start_key_);
  } else {
    it->seektofirst();
  }
  int ttl_start;
  if (!parseintoption(option_map_, arg_ttl_start, ttl_start, exec_state_)) {
    ttl_start = dbwithttlimpl::kmintimestamp;  // ttl introduction time
  }
  int ttl_end;
  if (!parseintoption(option_map_, arg_ttl_end, ttl_end, exec_state_)) {
    ttl_end = dbwithttlimpl::kmaxtimestamp;  // max time allowed by ttl feature
  }
  if (ttl_end < ttl_start) {
    fprintf(stderr, "error: end time can't be less than start time\n");
    delete it;
    return;
  }
  if (is_db_ttl_ && timestamp_) {
    fprintf(stdout, "scanning key-values from %s to %s\n",
            readabletime(ttl_start).c_str(), readabletime(ttl_end).c_str());
  }
  for ( ;
        it->valid() && (!end_key_specified_ || it->key().tostring() < end_key_);
        it->next()) {
    string key = ldb_options_.key_formatter->format(it->key());
    if (is_db_ttl_) {
      ttliterator* it_ttl = dynamic_cast<ttliterator*>(it);
      assert(it_ttl);
      int rawtime = it_ttl->timestamp();
      if (rawtime < ttl_start || rawtime >= ttl_end) {
        continue;
      }
      if (timestamp_) {
        fprintf(stdout, "%s ", readabletime(rawtime).c_str());
      }
    }
    string value = it->value().tostring();
    fprintf(stdout, "%s : %s\n",
            (is_key_hex_ ? "0x" + it->key().tostring(true) : key).c_str(),
            (is_value_hex_ ? stringtohex(value) : value).c_str()
        );
    num_keys_scanned++;
    if (max_keys_scanned_ >= 0 && num_keys_scanned >= max_keys_scanned_) {
      break;
    }
  }
  if (!it->status().ok()) {  // check for any errors found during the scan
    exec_state_ = ldbcommandexecuteresult::failed(it->status().tostring());
  }
  delete it;
}


deletecommand::deletecommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags) :
  ldbcommand(options, flags, false,
             buildcmdlineoptions({arg_hex, arg_key_hex, arg_value_hex})) {

  if (params.size() != 1) {
    exec_state_ = ldbcommandexecuteresult::failed(
                    "key must be specified for the delete command");
  } else {
    key_ = params.at(0);
    if (is_key_hex_) {
      key_ = hextostring(key_);
    }
  }
}

void deletecommand::help(string& ret) {
  ret.append("  ");
  ret.append(deletecommand::name() + " <key>");
  ret.append("\n");
}

void deletecommand::docommand() {
  status st = db_->delete(writeoptions(), key_);
  if (st.ok()) {
    fprintf(stdout, "ok\n");
  } else {
    exec_state_ = ldbcommandexecuteresult::failed(st.tostring());
  }
}


putcommand::putcommand(const vector<string>& params,
      const map<string, string>& options, const vector<string>& flags) :
  ldbcommand(options, flags, false,
             buildcmdlineoptions({arg_ttl, arg_hex, arg_key_hex, arg_value_hex,
                                  arg_create_if_missing})) {

  if (params.size() != 2) {
    exec_state_ = ldbcommandexecuteresult::failed(
                    "<key> and <value> must be specified for the put command");
  } else {
    key_ = params.at(0);
    value_ = params.at(1);
  }

  if (is_key_hex_) {
    key_ = hextostring(key_);
  }

  if (is_value_hex_) {
    value_ = hextostring(value_);
  }
}

void putcommand::help(string& ret) {
  ret.append("  ");
  ret.append(putcommand::name());
  ret.append(" <key> <value> ");
  ret.append(" [--" + arg_ttl + "]");
  ret.append("\n");
}

void putcommand::docommand() {
  status st = db_->put(writeoptions(), key_, value_);
  if (st.ok()) {
    fprintf(stdout, "ok\n");
  } else {
    exec_state_ = ldbcommandexecuteresult::failed(st.tostring());
  }
}

options putcommand::prepareoptionsforopendb() {
  options opt = ldbcommand::prepareoptionsforopendb();
  opt.create_if_missing = isflagpresent(flags_, arg_create_if_missing);
  return opt;
}


const char* dbqueriercommand::help_cmd = "help";
const char* dbqueriercommand::get_cmd = "get";
const char* dbqueriercommand::put_cmd = "put";
const char* dbqueriercommand::delete_cmd = "delete";

dbqueriercommand::dbqueriercommand(const vector<string>& params,
    const map<string, string>& options, const vector<string>& flags) :
  ldbcommand(options, flags, false,
             buildcmdlineoptions({arg_ttl, arg_hex, arg_key_hex,
                                  arg_value_hex})) {

}

void dbqueriercommand::help(string& ret) {
  ret.append("  ");
  ret.append(dbqueriercommand::name());
  ret.append(" [--" + arg_ttl + "]");
  ret.append("\n");
  ret.append("    starts a repl shell.  type help for list of available "
             "commands.");
  ret.append("\n");
}

void dbqueriercommand::docommand() {
  if (!db_) {
    return;
  }

  readoptions read_options;
  writeoptions write_options;

  string line;
  string key;
  string value;
  while (getline(cin, line, '\n')) {

    // parse line into vector<string>
    vector<string> tokens;
    size_t pos = 0;
    while (true) {
      size_t pos2 = line.find(' ', pos);
      if (pos2 == string::npos) {
        break;
      }
      tokens.push_back(line.substr(pos, pos2-pos));
      pos = pos2 + 1;
    }
    tokens.push_back(line.substr(pos));

    const string& cmd = tokens[0];

    if (cmd == help_cmd) {
      fprintf(stdout,
              "get <key>\n"
              "put <key> <value>\n"
              "delete <key>\n");
    } else if (cmd == delete_cmd && tokens.size() == 2) {
      key = (is_key_hex_ ? hextostring(tokens[1]) : tokens[1]);
      db_->delete(write_options, slice(key));
      fprintf(stdout, "successfully deleted %s\n", tokens[1].c_str());
    } else if (cmd == put_cmd && tokens.size() == 3) {
      key = (is_key_hex_ ? hextostring(tokens[1]) : tokens[1]);
      value = (is_value_hex_ ? hextostring(tokens[2]) : tokens[2]);
      db_->put(write_options, slice(key), slice(value));
      fprintf(stdout, "successfully put %s %s\n",
              tokens[1].c_str(), tokens[2].c_str());
    } else if (cmd == get_cmd && tokens.size() == 2) {
      key = (is_key_hex_ ? hextostring(tokens[1]) : tokens[1]);
      if (db_->get(read_options, slice(key), &value).ok()) {
        fprintf(stdout, "%s\n", printkeyvalue(key, value,
              is_key_hex_, is_value_hex_).c_str());
      } else {
        fprintf(stdout, "not found %s\n", tokens[1].c_str());
      }
    } else {
      fprintf(stdout, "unknown command %s\n", line.c_str());
    }
  }
}

checkconsistencycommand::checkconsistencycommand(const vector<string>& params,
    const map<string, string>& options, const vector<string>& flags) :
  ldbcommand(options, flags, false,
             buildcmdlineoptions({})) {
}

void checkconsistencycommand::help(string& ret) {
  ret.append("  ");
  ret.append(checkconsistencycommand::name());
  ret.append("\n");
}

void checkconsistencycommand::docommand() {
  options opt = prepareoptionsforopendb();
  opt.paranoid_checks = true;
  if (!exec_state_.isnotstarted()) {
    return;
  }
  db* db;
  status st = db::openforreadonly(opt, db_path_, &db, false);
  delete db;
  if (st.ok()) {
    fprintf(stdout, "ok\n");
  } else {
    exec_state_ = ldbcommandexecuteresult::failed(st.tostring());
  }
}

}   // namespace rocksdb
#endif  // rocksdb_lite
