//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//

#include <map>
#include <string>
#include <vector>
#include <inttypes.h>

#include "db/dbformat.h"
#include "db/memtable.h"
#include "db/write_batch_internal.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "rocksdb/iterator.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/table.h"
#include "rocksdb/table_properties.h"
#include "table/block_based_table_factory.h"
#include "table/plain_table_factory.h"
#include "table/meta_blocks.h"
#include "table/block.h"
#include "table/block_builder.h"
#include "table/format.h"
#include "util/ldb_cmd.h"
#include "util/random.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace rocksdb {

class sstfilereader {
 public:
  explicit sstfilereader(const std::string& file_name,
                         bool verify_checksum,
                         bool output_hex);

  status readsequential(bool print_kv,
                        uint64_t read_num,
                        bool has_from,
                        const std::string& from_key,
                        bool has_to,
                        const std::string& to_key);

  status readtableproperties(
      std::shared_ptr<const tableproperties>* table_properties);
  uint64_t getreadnumber() { return read_num_; }
  tableproperties* getinittableproperties() { return table_properties_.get(); }

 private:
  status newtablereader(const std::string& file_path);
  status readtableproperties(uint64_t table_magic_number,
                             randomaccessfile* file, uint64_t file_size);
  status settableoptionsbymagicnumber(uint64_t table_magic_number);
  status setoldtableoptions();

  std::string file_name_;
  uint64_t read_num_;
  bool verify_checksum_;
  bool output_hex_;
  envoptions soptions_;

  status init_result_;
  unique_ptr<tablereader> table_reader_;
  unique_ptr<randomaccessfile> file_;
  // options_ and internal_comparator_ will also be used in
  // readsequential internally (specifically, seek-related operations)
  options options_;
  internalkeycomparator internal_comparator_;
  unique_ptr<tableproperties> table_properties_;
};

sstfilereader::sstfilereader(const std::string& file_path,
                             bool verify_checksum,
                             bool output_hex)
    :file_name_(file_path), read_num_(0), verify_checksum_(verify_checksum),
    output_hex_(output_hex), internal_comparator_(bytewisecomparator()) {
  fprintf(stdout, "process %s\n", file_path.c_str());

  init_result_ = newtablereader(file_name_);
}

extern uint64_t kblockbasedtablemagicnumber;
extern uint64_t klegacyblockbasedtablemagicnumber;
extern uint64_t kplaintablemagicnumber;
extern uint64_t klegacyplaintablemagicnumber;

status sstfilereader::newtablereader(const std::string& file_path) {
  uint64_t magic_number;

  // read table magic number
  footer footer;

  unique_ptr<randomaccessfile> file;
  uint64_t file_size;
  status s = options_.env->newrandomaccessfile(file_path, &file_, soptions_);
  if (s.ok()) {
    s = options_.env->getfilesize(file_path, &file_size);
  }
  if (s.ok()) {
    s = readfooterfromfile(file_.get(), file_size, &footer);
  }
  if (s.ok()) {
    magic_number = footer.table_magic_number();
  }

  if (s.ok()) {
    if (magic_number == kplaintablemagicnumber ||
        magic_number == klegacyplaintablemagicnumber) {
      soptions_.use_mmap_reads = true;
      options_.env->newrandomaccessfile(file_path, &file_, soptions_);
    }
    options_.comparator = &internal_comparator_;
    // for old sst format, readtableproperties might fail but file can be read
    if (readtableproperties(magic_number, file_.get(), file_size).ok()) {
      settableoptionsbymagicnumber(magic_number);
    } else {
      setoldtableoptions();
    }
  }

  if (s.ok()) {
    s = options_.table_factory->newtablereader(
        options_, soptions_, internal_comparator_, std::move(file_), file_size,
        &table_reader_);
  }
  return s;
}

status sstfilereader::readtableproperties(uint64_t table_magic_number,
                                          randomaccessfile* file,
                                          uint64_t file_size) {
  tableproperties* table_properties = nullptr;
  status s = rocksdb::readtableproperties(file, file_size, table_magic_number,
                                          options_.env, options_.info_log.get(),
                                          &table_properties);
  if (s.ok()) {
    table_properties_.reset(table_properties);
  } else {
    fprintf(stdout, "not able to read table properties\n");
  }
  return s;
}

status sstfilereader::settableoptionsbymagicnumber(
    uint64_t table_magic_number) {
  assert(table_properties_);
  if (table_magic_number == kblockbasedtablemagicnumber ||
      table_magic_number == klegacyblockbasedtablemagicnumber) {
    options_.table_factory = std::make_shared<blockbasedtablefactory>();
    fprintf(stdout, "sst file format: block-based\n");
    auto& props = table_properties_->user_collected_properties;
    auto pos = props.find(blockbasedtablepropertynames::kindextype);
    if (pos != props.end()) {
      auto index_type_on_file = static_cast<blockbasedtableoptions::indextype>(
          decodefixed32(pos->second.c_str()));
      if (index_type_on_file ==
          blockbasedtableoptions::indextype::khashsearch) {
        options_.prefix_extractor.reset(newnooptransform());
      }
    }
  } else if (table_magic_number == kplaintablemagicnumber ||
             table_magic_number == klegacyplaintablemagicnumber) {
    options_.allow_mmap_reads = true;

    plaintableoptions plain_table_options;
    plain_table_options.user_key_len = kplaintablevariablelength;
    plain_table_options.bloom_bits_per_key = 0;
    plain_table_options.hash_table_ratio = 0;
    plain_table_options.index_sparseness = 1;
    plain_table_options.huge_page_tlb_size = 0;
    plain_table_options.encoding_type = kplain;
    plain_table_options.full_scan_mode = true;

    options_.table_factory.reset(newplaintablefactory(plain_table_options));
    fprintf(stdout, "sst file format: plain table\n");
  } else {
    char error_msg_buffer[80];
    snprintf(error_msg_buffer, sizeof(error_msg_buffer) - 1,
             "unsupported table magic number --- %lx",
             (long)table_magic_number);
    return status::invalidargument(error_msg_buffer);
  }

  return status::ok();
}

status sstfilereader::setoldtableoptions() {
  assert(table_properties_ == nullptr);
  options_.table_factory = std::make_shared<blockbasedtablefactory>();
  fprintf(stdout, "sst file format: block-based(old version)\n");

  return status::ok();
}

status sstfilereader::readsequential(bool print_kv,
                                     uint64_t read_num,
                                     bool has_from,
                                     const std::string& from_key,
                                     bool has_to,
                                     const std::string& to_key) {
  if (!table_reader_) {
    return init_result_;
  }

  iterator* iter = table_reader_->newiterator(readoptions(verify_checksum_,
                                                         false));
  uint64_t i = 0;
  if (has_from) {
    internalkey ikey(from_key, kmaxsequencenumber, kvaluetypeforseek);
    iter->seek(ikey.encode());
  } else {
    iter->seektofirst();
  }
  for (; iter->valid(); iter->next()) {
    slice key = iter->key();
    slice value = iter->value();
    ++i;
    if (read_num > 0 && i > read_num)
      break;

    parsedinternalkey ikey;
    if (!parseinternalkey(key, &ikey)) {
      std::cerr << "internal key ["
                << key.tostring(true /* in hex*/)
                << "] parse error!\n";
      continue;
    }

    // if end marker was specified, we stop before it
    if (has_to && bytewisecomparator()->compare(ikey.user_key, to_key) >= 0) {
      break;
    }

    if (print_kv) {
      fprintf(stdout, "%s => %s\n",
          ikey.debugstring(output_hex_).c_str(),
          value.tostring(output_hex_).c_str());
    }
  }

  read_num_ += i;

  status ret = iter->status();
  delete iter;
  return ret;
}

status sstfilereader::readtableproperties(
    std::shared_ptr<const tableproperties>* table_properties) {
  if (!table_reader_) {
    return init_result_;
  }

  *table_properties = table_reader_->gettableproperties();
  return init_result_;
}

}  // namespace rocksdb

static void print_help() {
  fprintf(stderr,
          "sst_dump [--command=check|scan|none] [--verify_checksum] "
          "--file=data_dir_or_sst_file"
          " [--output_hex]"
          " [--input_key_hex]"
          " [--from=<user_key>]"
          " [--to=<user_key>]"
          " [--read_num=num]"
          " [--show_properties]\n");
}

namespace {
string hextostring(const string& str) {
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
}  // namespace

int main(int argc, char** argv) {
  const char* dir_or_file = nullptr;
  uint64_t read_num = -1;
  std::string command;

  char junk;
  uint64_t n;
  bool verify_checksum = false;
  bool output_hex = false;
  bool input_key_hex = false;
  bool has_from = false;
  bool has_to = false;
  bool show_properties = false;
  std::string from_key;
  std::string to_key;
  for (int i = 1; i < argc; i++) {
    if (strncmp(argv[i], "--file=", 7) == 0) {
      dir_or_file = argv[i] + 7;
    } else if (strcmp(argv[i], "--output_hex") == 0) {
      output_hex = true;
    } else if (strcmp(argv[i], "--input_key_hex") == 0) {
      input_key_hex = true;
    } else if (sscanf(argv[i],
               "--read_num=%lu%c",
               (unsigned long*)&n, &junk) == 1) {
      read_num = n;
    } else if (strcmp(argv[i], "--verify_checksum") == 0) {
      verify_checksum = true;
    } else if (strncmp(argv[i], "--command=", 10) == 0) {
      command = argv[i] + 10;
    } else if (strncmp(argv[i], "--from=", 7) == 0) {
      from_key = argv[i] + 7;
      has_from = true;
    } else if (strncmp(argv[i], "--to=", 5) == 0) {
      to_key = argv[i] + 5;
      has_to = true;
    } else if (strcmp(argv[i], "--show_properties") == 0) {
      show_properties = true;
    } else {
      print_help();
      exit(1);
    }
  }

  if (input_key_hex) {
    if (has_from) {
      from_key = hextostring(from_key);
    }
    if (has_to) {
      to_key = hextostring(to_key);
    }
  }

  if (dir_or_file == nullptr) {
    print_help();
    exit(1);
  }

  std::vector<std::string> filenames;
  rocksdb::env* env = rocksdb::env::default();
  rocksdb::status st = env->getchildren(dir_or_file, &filenames);
  bool dir = true;
  if (!st.ok()) {
    filenames.clear();
    filenames.push_back(dir_or_file);
    dir = false;
  }

  fprintf(stdout, "from [%s] to [%s]\n",
      rocksdb::slice(from_key).tostring(true).c_str(),
      rocksdb::slice(to_key).tostring(true).c_str());

  uint64_t total_read = 0;
  for (size_t i = 0; i < filenames.size(); i++) {
    std::string filename = filenames.at(i);
    if (filename.length() <= 4 ||
        filename.rfind(".sst") != filename.length() - 4) {
      // ignore
      continue;
    }
    if (dir) {
      filename = std::string(dir_or_file) + "/" + filename;
    }
    rocksdb::sstfilereader reader(filename, verify_checksum,
                                  output_hex);
    rocksdb::status st;
    // scan all files in give file path.
    if (command == "" || command == "scan" || command == "check") {
      st = reader.readsequential(command != "check",
                                 read_num > 0 ? (read_num - total_read) :
                                                read_num,
                                 has_from, from_key, has_to, to_key);
      if (!st.ok()) {
        fprintf(stderr, "%s: %s\n", filename.c_str(),
            st.tostring().c_str());
      }
      total_read += reader.getreadnumber();
      if (read_num > 0 && total_read > read_num) {
        break;
      }
    }
    if (show_properties) {
      const rocksdb::tableproperties* table_properties;

      std::shared_ptr<const rocksdb::tableproperties>
          table_properties_from_reader;
      st = reader.readtableproperties(&table_properties_from_reader);
      if (!st.ok()) {
        fprintf(stderr, "%s: %s\n", filename.c_str(), st.tostring().c_str());
        fprintf(stderr, "try to use initial table properties\n");
        table_properties = reader.getinittableproperties();
      } else {
        table_properties = table_properties_from_reader.get();
      }
      if (table_properties != nullptr) {
        fprintf(stdout,
                "table properties:\n"
                "------------------------------\n"
                "  %s",
                table_properties->tostring("\n  ", ": ").c_str());
        fprintf(stdout, "# deleted keys: %zd\n",
                rocksdb::getdeletedkeys(
                    table_properties->user_collected_properties));
      }
    }
  }
}
