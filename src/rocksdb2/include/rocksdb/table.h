// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// currently we support two types of tables: plain table and block-based table.
//   1. block-based table: this is the default table type that we inherited from
//      leveldb, which was designed for storing data in hard disk or flash
//      device.
//   2. plain table: it is one of rocksdb's sst file format optimized
//      for low query latency on pure-memory or really low-latency media.
//
// a tutorial of rocksdb table formats is available here:
//   https://github.com/facebook/rocksdb/wiki/a-tutorial-of-rocksdb-sst-formats
//
// example code is also available
//   https://github.com/facebook/rocksdb/wiki/a-tutorial-of-rocksdb-sst-formats#wiki-examples

#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "rocksdb/env.h"
#include "rocksdb/iterator.h"
#include "rocksdb/options.h"
#include "rocksdb/status.h"

namespace rocksdb {

// -- block-based table
class flushblockpolicyfactory;
class randomaccessfile;
class tablebuilder;
class tablereader;
class writablefile;
struct envoptions;
struct options;

using std::unique_ptr;

enum checksumtype : char {
  knochecksum = 0x0,  // not yet supported. will fail
  kcrc32c = 0x1,
  kxxhash = 0x2,
};

// for advanced user only
struct blockbasedtableoptions {
  // @flush_block_policy_factory creates the instances of flush block policy.
  // which provides a configurable way to determine when to flush a block in
  // the block based tables.  if not set, table builder will use the default
  // block flush policy, which cut blocks by block size (please refer to
  // `flushblockbysizepolicy`).
  std::shared_ptr<flushblockpolicyfactory> flush_block_policy_factory;

  // todo(kailiu) temporarily disable this feature by making the default value
  // to be false.
  //
  // indicating if we'd put index/filter blocks to the block cache.
  // if not specified, each "table reader" object will pre-load index/filter
  // block during table initialization.
  bool cache_index_and_filter_blocks = false;

  // the index type that will be used for this table.
  enum indextype : char {
    // a space efficient index block that is optimized for
    // binary-search-based index.
    kbinarysearch,

    // the hash index, if enabled, will do the hash lookup when
    // `options.prefix_extractor` is provided.
    khashsearch,
  };

  indextype index_type = kbinarysearch;

  // influence the behavior when khashsearch is used.
  // if false, stores a precise prefix to block range mapping
  // if true, does not store prefix and allows prefix hash collision
  // (less memory consumption)
  bool hash_index_allow_collision = true;

  // use the specified checksum type. newly created table files will be
  // protected with this checksum type. old table files will still be readable,
  // even though they have different checksum type.
  checksumtype checksum = kcrc32c;

  // disable block cache. if this is set to true,
  // then no block cache should be used, and the block_cache should
  // point to a nullptr object.
  bool no_block_cache = false;

  // if non-null use the specified cache for blocks.
  // if null, rocksdb will automatically create and use an 8mb internal cache.
  std::shared_ptr<cache> block_cache = nullptr;

  // if non-null use the specified cache for compressed blocks.
  // if null, rocksdb will not use a compressed block cache.
  std::shared_ptr<cache> block_cache_compressed = nullptr;

  // approximate size of user data packed per block.  note that the
  // block size specified here corresponds to uncompressed data.  the
  // actual size of the unit read from disk may be smaller if
  // compression is enabled.  this parameter can be changed dynamically.
  size_t block_size = 4 * 1024;

  // this is used to close a block before it reaches the configured
  // 'block_size'. if the percentage of free space in the current block is less
  // than this specified number and adding a new record to the block will
  // exceed the configured block size, then this block will be closed and the
  // new record will be written to the next block.
  int block_size_deviation = 10;

  // number of keys between restart points for delta encoding of keys.
  // this parameter can be changed dynamically.  most clients should
  // leave this parameter alone.
  int block_restart_interval = 16;

  // if non-nullptr, use the specified filter policy to reduce disk reads.
  // many applications will benefit from passing the result of
  // newbloomfilterpolicy() here.
  std::shared_ptr<const filterpolicy> filter_policy = nullptr;

  // if true, place whole keys in the filter (not just prefixes).
  // this must generally be true for gets to be efficient.
  bool whole_key_filtering = true;
};

// table properties that are specific to block-based table properties.
struct blockbasedtablepropertynames {
  // value of this propertis is a fixed int32 number.
  static const std::string kindextype;
};

// create default block based table factory.
extern tablefactory* newblockbasedtablefactory(
    const blockbasedtableoptions& table_options = blockbasedtableoptions());

#ifndef rocksdb_lite

enum encodingtype : char {
  // always write full keys without any special encoding.
  kplain,
  // find opportunity to write the same prefix once for multiple rows.
  // in some cases, when a key follows a previous key with the same prefix,
  // instead of writing out the full key, it just writes out the size of the
  // shared prefix, as well as other bytes, to save some bytes.
  //
  // when using this option, the user is required to use the same prefix
  // extractor to make sure the same prefix will be extracted from the same key.
  // the name() value of the prefix extractor will be stored in the file. when
  // reopening the file, the name of the options.prefix_extractor given will be
  // bitwise compared to the prefix extractors stored in the file. an error
  // will be returned if the two don't match.
  kprefix,
};

// table properties that are specific to plain table properties.
struct plaintablepropertynames {
  static const std::string kprefixextractorname;
  static const std::string kencodingtype;
  static const std::string kbloomversion;
  static const std::string knumbloomblocks;
};

const uint32_t kplaintablevariablelength = 0;

struct plaintableoptions {
  // @user_key_len: plain table has optimization for fix-sized keys, which can
  //                be specified via user_key_len.  alternatively, you can pass
  //                `kplaintablevariablelength` if your keys have variable
  //                lengths.
  uint32_t user_key_len = kplaintablevariablelength;

  // @bloom_bits_per_key: the number of bits used for bloom filer per prefix.
  //                      you may disable it by passing a zero.
  int bloom_bits_per_key = 10;

  // @hash_table_ratio: the desired utilization of the hash table used for
  //                    prefix hashing.
  //                    hash_table_ratio = number of prefixes / #buckets in the
  //                    hash table
  double hash_table_ratio = 0.75;

  // @index_sparseness: inside each prefix, need to build one index record for
  //                    how many keys for binary search inside each hash bucket.
  //                    for encoding type kprefix, the value will be used when
  //                    writing to determine an interval to rewrite the full
  //                    key. it will also be used as a suggestion and satisfied
  //                    when possible.
  size_t index_sparseness = 16;

  // @huge_page_tlb_size: if <=0, allocate hash indexes and blooms from malloc.
  //                      otherwise from huge page tlb. the user needs to
  //                      reserve huge pages for it to be allocated, like:
  //                          sysctl -w vm.nr_hugepages=20
  //                      see linux doc documentation/vm/hugetlbpage.txt
  size_t huge_page_tlb_size = 0;

  // @encoding_type: how to encode the keys. see enum encodingtype above for
  //                 the choices. the value will determine how to encode keys
  //                 when writing to a new sst file. this value will be stored
  //                 inside the sst file which will be used when reading from
  //                 the file, which makes it possible for users to choose
  //                 different encoding type when reopening a db. files with
  //                 different encoding types can co-exist in the same db and
  //                 can be read.
  encodingtype encoding_type = kplain;

  // @full_scan_mode: mode for reading the whole file one record by one without
  //                  using the index.
  bool full_scan_mode = false;

  // @store_index_in_file: compute plain table index and bloom filter during
  //                       file building and store it in file. when reading
  //                       file, index will be mmaped instead of recomputation.
  bool store_index_in_file = false;
};

// -- plain table with prefix-only seek
// for this factory, you need to set options.prefix_extrator properly to make it
// work. look-up will starts with prefix hash lookup for key prefix. inside the
// hash bucket found, a binary search is executed for hash conflicts. finally,
// a linear search is used.

extern tablefactory* newplaintablefactory(const plaintableoptions& options =
                                              plaintableoptions());

struct cuckootablepropertynames {
  // the key that is used to fill empty buckets.
  static const std::string kemptykey;
  // fixed length of value.
  static const std::string kvaluelength;
  // number of hash functions used in cuckoo hash.
  static const std::string knumhashfunc;
  // it denotes the number of buckets in a cuckoo block. given a key and a
  // particular hash function, a cuckoo block is a set of consecutive buckets,
  // where starting bucket id is given by the hash function on the key. in case
  // of a collision during inserting the key, the builder tries to insert the
  // key in other locations of the cuckoo block before using the next hash
  // function. this reduces cache miss during read operation in case of
  // collision.
  static const std::string kcuckooblocksize;
  // size of the hash table. use this number to compute the modulo of hash
  // function. the actual number of buckets will be kmaxhashtablesize +
  // kcuckooblocksize - 1. the last kcuckooblocksize-1 buckets are used to
  // accommodate the cuckoo block from end of hash table, due to cache friendly
  // implementation.
  static const std::string khashtablesize;
  // denotes if the key sorted in the file is internal key (if false)
  // or user key only (if true).
  static const std::string kislastlevel;
};

// cuckoo table factory for sst table format using cache friendly cuckoo hashing
// @hash_table_ratio: determines the utilization of hash tables. smaller values
//                    result in larger hash tables with fewer collisions.
// @max_search_depth: a property used by builder to determine the depth to go to
//                    to search for a path to displace elements in case of
//                    collision. see builder.makespaceforkey method.  higher
//                    values result in more efficient hash tables with fewer
//                    lookups but take more time to build.
// @cuckoo_block_size: in case of collision while inserting, the builder
//                     attempts to insert in the next cuckoo_block_size
//                     locations before skipping over to the next cuckoo hash
//                     function. this makes lookups more cache friendly in case
//                     of collisions.
extern tablefactory* newcuckootablefactory(double hash_table_ratio = 0.9,
    uint32_t max_search_depth = 100, uint32_t cuckoo_block_size = 5);

#endif  // rocksdb_lite

// a base class for table factories.
class tablefactory {
 public:
  virtual ~tablefactory() {}

  // the type of the table.
  //
  // the client of this package should switch to a new name whenever
  // the table format implementation changes.
  //
  // names starting with "rocksdb." are reserved and should not be used
  // by any clients of this package.
  virtual const char* name() const = 0;

  // returns a table object table that can fetch data from file specified
  // in parameter file. it's the caller's responsibility to make sure
  // file is in the correct format.
  //
  // newtablereader() is called in two places:
  // (1) tablecache::findtable() calls the function when table cache miss
  //     and cache the table object returned.
  // (1) sstfilereader (for sst dump) opens the table and dump the table
  //     contents using the interator of the table.
  // options and soptions are options. options is the general options.
  // multiple configured can be accessed from there, including and not
  // limited to block cache and key comparators.
  // file is a file handler to handle the file for the table
  // file_size is the physical file size of the file
  // table_reader is the output table reader
  virtual status newtablereader(
      const options& options, const envoptions& soptions,
      const internalkeycomparator& internal_comparator,
      unique_ptr<randomaccessfile>&& file, uint64_t file_size,
      unique_ptr<tablereader>* table_reader) const = 0;

  // return a table builder to write to a file for this table type.
  //
  // it is called in several places:
  // (1) when flushing memtable to a level-0 output file, it creates a table
  //     builder (in dbimpl::writelevel0table(), by calling buildtable())
  // (2) during compaction, it gets the builder for writing compaction output
  //     files in dbimpl::opencompactionoutputfile().
  // (3) when recovering from transaction logs, it creates a table builder to
  //     write to a level-0 output file (in dbimpl::writelevel0tableforrecovery,
  //     by calling buildtable())
  // (4) when running repairer, it creates a table builder to convert logs to
  //     sst files (in repairer::convertlogtotable() by calling buildtable())
  //
  // options is the general options. multiple configured can be acceseed from
  // there, including and not limited to compression options.
  // file is a handle of a writable file. it is the caller's responsibility to
  // keep the file open and close the file after closing the table builder.
  // compression_type is the compression type to use in this table.
  virtual tablebuilder* newtablebuilder(
      const options& options, const internalkeycomparator& internal_comparator,
      writablefile* file, compressiontype compression_type) const = 0;

  // sanitizes the specified db options.
  //
  // if the function cannot find a way to sanitize the input db options,
  // a non-ok status will be returned.
  virtual status sanitizedboptions(const dboptions* db_opts) const = 0;

  // return a string that contains printable format of table configurations.
  // rocksdb prints configurations at db open().
  virtual std::string getprintabletableoptions() const = 0;
};

#ifndef rocksdb_lite
// create a special table factory that can open both of block based table format
// and plain table, based on setting inside the sst files. it should be used to
// convert a db from one table format to another.
// @table_factory_to_write: the table factory used when writing to new files.
// @block_based_table_factory:  block based table factory to use. if null, use
//                              a default one.
// @plain_table_factory: plain table factory to use. if null, use a default one.
extern tablefactory* newadaptivetablefactory(
    std::shared_ptr<tablefactory> table_factory_to_write = nullptr,
    std::shared_ptr<tablefactory> block_based_table_factory = nullptr,
    std::shared_ptr<tablefactory> plain_table_factory = nullptr,
    std::shared_ptr<tablefactory> cuckoo_table_factory = nullptr);

#endif  // rocksdb_lite

}  // namespace rocksdb
