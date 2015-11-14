// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_rocksdb_include_db_h_
#define storage_rocksdb_include_db_h_

#include <stdint.h>
#include <stdio.h>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include "rocksdb/version.h"
#include "rocksdb/iterator.h"
#include "rocksdb/options.h"
#include "rocksdb/types.h"
#include "rocksdb/transaction_log.h"

namespace rocksdb {

using std::unique_ptr;

class columnfamilyhandle {
 public:
  virtual ~columnfamilyhandle() {}
};
extern const std::string kdefaultcolumnfamilyname;

struct columnfamilydescriptor {
  std::string name;
  columnfamilyoptions options;
  columnfamilydescriptor()
      : name(kdefaultcolumnfamilyname), options(columnfamilyoptions()) {}
  columnfamilydescriptor(const std::string& _name,
                         const columnfamilyoptions& _options)
      : name(_name), options(_options) {}
};

static const int kmajorversion = __rocksdb_major__;
static const int kminorversion = __rocksdb_minor__;

struct options;
struct readoptions;
struct writeoptions;
struct flushoptions;
struct tableproperties;
class writebatch;
class env;

// metadata associated with each sst file.
struct livefilemetadata {
  std::string column_family_name;  // name of the column family
  std::string db_path;
  std::string name;                // name of the file
  int level;               // level at which this file resides.
  size_t size;             // file size in bytes.
  std::string smallestkey; // smallest user defined key in the file.
  std::string largestkey;  // largest user defined key in the file.
  sequencenumber smallest_seqno; // smallest seqno in file
  sequencenumber largest_seqno;  // largest seqno in file
};

// abstract handle to particular state of a db.
// a snapshot is an immutable object and can therefore be safely
// accessed from multiple threads without any external synchronization.
class snapshot {
 protected:
  virtual ~snapshot();
};

// a range of keys
struct range {
  slice start;          // included in the range
  slice limit;          // not included in the range

  range() { }
  range(const slice& s, const slice& l) : start(s), limit(l) { }
};

// a collections of table properties objects, where
//  key: is the table's file name.
//  value: the table properties object of the given table.
typedef std::unordered_map<std::string, std::shared_ptr<const tableproperties>>
    tablepropertiescollection;

// a db is a persistent ordered map from keys to values.
// a db is safe for concurrent access from multiple threads without
// any external synchronization.
class db {
 public:
  // open the database with the specified "name".
  // stores a pointer to a heap-allocated database in *dbptr and returns
  // ok on success.
  // stores nullptr in *dbptr and returns a non-ok status on error.
  // caller should delete *dbptr when it is no longer needed.
  static status open(const options& options,
                     const std::string& name,
                     db** dbptr);

  // open the database for read only. all db interfaces
  // that modify data, like put/delete, will return error.
  // if the db is opened in read only mode, then no compactions
  // will happen.
  static status openforreadonly(const options& options,
      const std::string& name, db** dbptr,
      bool error_if_log_file_exist = false);

  // open the database for read only with column families. when opening db with
  // read only, you can specify only a subset of column families in the
  // database that should be opened. however, you always need to specify default
  // column family. the default column family name is 'default' and it's stored
  // in rocksdb::kdefaultcolumnfamilyname
  static status openforreadonly(
      const dboptions& db_options, const std::string& name,
      const std::vector<columnfamilydescriptor>& column_families,
      std::vector<columnfamilyhandle*>* handles, db** dbptr,
      bool error_if_log_file_exist = false);

  // open db with column families.
  // db_options specify database specific options
  // column_families is the vector of all column families in the databse,
  // containing column family name and options. you need to open all column
  // families in the database. to get the list of column families, you can use
  // listcolumnfamilies(). also, you can open only a subset of column families
  // for read-only access.
  // the default column family name is 'default' and it's stored
  // in rocksdb::kdefaultcolumnfamilyname.
  // if everything is ok, handles will on return be the same size
  // as column_families --- handles[i] will be a handle that you
  // will use to operate on column family column_family[i]
  static status open(const dboptions& db_options, const std::string& name,
                     const std::vector<columnfamilydescriptor>& column_families,
                     std::vector<columnfamilyhandle*>* handles, db** dbptr);

  // listcolumnfamilies will open the db specified by argument name
  // and return the list of all column families in that db
  // through column_families argument. the ordering of
  // column families in column_families is unspecified.
  static status listcolumnfamilies(const dboptions& db_options,
                                   const std::string& name,
                                   std::vector<std::string>* column_families);

  db() { }
  virtual ~db();

  // create a column_family and return the handle of column family
  // through the argument handle.
  virtual status createcolumnfamily(const columnfamilyoptions& options,
                                    const std::string& column_family_name,
                                    columnfamilyhandle** handle);

  // drop a column family specified by column_family handle. this call
  // only records a drop record in the manifest and prevents the column
  // family from flushing and compacting.
  virtual status dropcolumnfamily(columnfamilyhandle* column_family);

  // set the database entry for "key" to "value".
  // if "key" already exists, it will be overwritten.
  // returns ok on success, and a non-ok status on error.
  // note: consider setting options.sync = true.
  virtual status put(const writeoptions& options,
                     columnfamilyhandle* column_family, const slice& key,
                     const slice& value) = 0;
  virtual status put(const writeoptions& options, const slice& key,
                     const slice& value) {
    return put(options, defaultcolumnfamily(), key, value);
  }

  // remove the database entry (if any) for "key".  returns ok on
  // success, and a non-ok status on error.  it is not an error if "key"
  // did not exist in the database.
  // note: consider setting options.sync = true.
  virtual status delete(const writeoptions& options,
                        columnfamilyhandle* column_family,
                        const slice& key) = 0;
  virtual status delete(const writeoptions& options, const slice& key) {
    return delete(options, defaultcolumnfamily(), key);
  }

  // merge the database entry for "key" with "value".  returns ok on success,
  // and a non-ok status on error. the semantics of this operation is
  // determined by the user provided merge_operator when opening db.
  // note: consider setting options.sync = true.
  virtual status merge(const writeoptions& options,
                       columnfamilyhandle* column_family, const slice& key,
                       const slice& value) = 0;
  virtual status merge(const writeoptions& options, const slice& key,
                       const slice& value) {
    return merge(options, defaultcolumnfamily(), key, value);
  }

  // apply the specified updates to the database.
  // returns ok on success, non-ok on failure.
  // note: consider setting options.sync = true.
  virtual status write(const writeoptions& options, writebatch* updates) = 0;

  // if the database contains an entry for "key" store the
  // corresponding value in *value and return ok.
  //
  // if there is no entry for "key" leave *value unchanged and return
  // a status for which status::isnotfound() returns true.
  //
  // may return some other status on an error.
  virtual status get(const readoptions& options,
                     columnfamilyhandle* column_family, const slice& key,
                     std::string* value) = 0;
  virtual status get(const readoptions& options, const slice& key, std::string* value) {
    return get(options, defaultcolumnfamily(), key, value);
  }

  // if keys[i] does not exist in the database, then the i'th returned
  // status will be one for which status::isnotfound() is true, and
  // (*values)[i] will be set to some arbitrary value (often ""). otherwise,
  // the i'th returned status will have status::ok() true, and (*values)[i]
  // will store the value associated with keys[i].
  //
  // (*values) will always be resized to be the same size as (keys).
  // similarly, the number of returned statuses will be the number of keys.
  // note: keys will not be "de-duplicated". duplicate keys will return
  // duplicate values in order.
  virtual std::vector<status> multiget(
      const readoptions& options,
      const std::vector<columnfamilyhandle*>& column_family,
      const std::vector<slice>& keys, std::vector<std::string>* values) = 0;
  virtual std::vector<status> multiget(const readoptions& options,
                                       const std::vector<slice>& keys,
                                       std::vector<std::string>* values) {
    return multiget(options, std::vector<columnfamilyhandle*>(
                                 keys.size(), defaultcolumnfamily()),
                    keys, values);
  }

  // if the key definitely does not exist in the database, then this method
  // returns false, else true. if the caller wants to obtain value when the key
  // is found in memory, a bool for 'value_found' must be passed. 'value_found'
  // will be true on return if value has been set properly.
  // this check is potentially lighter-weight than invoking db::get(). one way
  // to make this lighter weight is to avoid doing any ios.
  // default implementation here returns true and sets 'value_found' to false
  virtual bool keymayexist(const readoptions& options,
                           columnfamilyhandle* column_family, const slice& key,
                           std::string* value, bool* value_found = nullptr) {
    if (value_found != nullptr) {
      *value_found = false;
    }
    return true;
  }
  virtual bool keymayexist(const readoptions& options, const slice& key,
                           std::string* value, bool* value_found = nullptr) {
    return keymayexist(options, defaultcolumnfamily(), key, value, value_found);
  }

  // return a heap-allocated iterator over the contents of the database.
  // the result of newiterator() is initially invalid (caller must
  // call one of the seek methods on the iterator before using it).
  //
  // caller should delete the iterator when it is no longer needed.
  // the returned iterator should be deleted before this db is deleted.
  virtual iterator* newiterator(const readoptions& options,
                                columnfamilyhandle* column_family) = 0;
  virtual iterator* newiterator(const readoptions& options) {
    return newiterator(options, defaultcolumnfamily());
  }
  // returns iterators from a consistent database state across multiple
  // column families. iterators are heap allocated and need to be deleted
  // before the db is deleted
  virtual status newiterators(
      const readoptions& options,
      const std::vector<columnfamilyhandle*>& column_families,
      std::vector<iterator*>* iterators) = 0;

  // return a handle to the current db state.  iterators created with
  // this handle will all observe a stable snapshot of the current db
  // state.  the caller must call releasesnapshot(result) when the
  // snapshot is no longer needed.
  //
  // nullptr will be returned if the db fails to take a snapshot or does
  // not support snapshot.
  virtual const snapshot* getsnapshot() = 0;

  // release a previously acquired snapshot.  the caller must not
  // use "snapshot" after this call.
  virtual void releasesnapshot(const snapshot* snapshot) = 0;

  // db implementations can export properties about their state
  // via this method.  if "property" is a valid property understood by this
  // db implementation, fills "*value" with its current value and returns
  // true.  otherwise returns false.
  //
  //
  // valid property names include:
  //
  //  "rocksdb.num-files-at-level<n>" - return the number of files at level <n>,
  //     where <n> is an ascii representation of a level number (e.g. "0").
  //  "rocksdb.stats" - returns a multi-line string that describes statistics
  //     about the internal operation of the db.
  //  "rocksdb.sstables" - returns a multi-line string that describes all
  //     of the sstables that make up the db contents.
  virtual bool getproperty(columnfamilyhandle* column_family,
                           const slice& property, std::string* value) = 0;
  virtual bool getproperty(const slice& property, std::string* value) {
    return getproperty(defaultcolumnfamily(), property, value);
  }

  // similar to getproperty(), but only works for a subset of properties whose
  // return value is an integer. return the value by integer.
  virtual bool getintproperty(columnfamilyhandle* column_family,
                              const slice& property, uint64_t* value) = 0;
  virtual bool getintproperty(const slice& property, uint64_t* value) {
    return getintproperty(defaultcolumnfamily(), property, value);
  }

  // for each i in [0,n-1], store in "sizes[i]", the approximate
  // file system space used by keys in "[range[i].start .. range[i].limit)".
  //
  // note that the returned sizes measure file system space usage, so
  // if the user data compresses by a factor of ten, the returned
  // sizes will be one-tenth the size of the corresponding user data size.
  //
  // the results may not include the sizes of recently written data.
  virtual void getapproximatesizes(columnfamilyhandle* column_family,
                                   const range* range, int n,
                                   uint64_t* sizes) = 0;
  virtual void getapproximatesizes(const range* range, int n, uint64_t* sizes) {
    getapproximatesizes(defaultcolumnfamily(), range, n, sizes);
  }

  // compact the underlying storage for the key range [*begin,*end].
  // the actual compaction interval might be superset of [*begin, *end].
  // in particular, deleted and overwritten versions are discarded,
  // and the data is rearranged to reduce the cost of operations
  // needed to access the data.  this operation should typically only
  // be invoked by users who understand the underlying implementation.
  //
  // begin==nullptr is treated as a key before all keys in the database.
  // end==nullptr is treated as a key after all keys in the database.
  // therefore the following call will compact the entire database:
  //    db->compactrange(nullptr, nullptr);
  // note that after the entire database is compacted, all data are pushed
  // down to the last level containing any data. if the total data size
  // after compaction is reduced, that level might not be appropriate for
  // hosting all the files. in this case, client could set reduce_level
  // to true, to move the files back to the minimum level capable of holding
  // the data set or a given level (specified by non-negative target_level).
  // compaction outputs should be placed in options.db_paths[target_path_id].
  // behavior is undefined if target_path_id is out of range.
  virtual status compactrange(columnfamilyhandle* column_family,
                              const slice* begin, const slice* end,
                              bool reduce_level = false, int target_level = -1,
                              uint32_t target_path_id = 0) = 0;
  virtual status compactrange(const slice* begin, const slice* end,
                              bool reduce_level = false, int target_level = -1,
                              uint32_t target_path_id = 0) {
    return compactrange(defaultcolumnfamily(), begin, end, reduce_level,
                        target_level, target_path_id);
  }

  // number of levels used for this db.
  virtual int numberlevels(columnfamilyhandle* column_family) = 0;
  virtual int numberlevels() { return numberlevels(defaultcolumnfamily()); }

  // maximum level to which a new compacted memtable is pushed if it
  // does not create overlap.
  virtual int maxmemcompactionlevel(columnfamilyhandle* column_family) = 0;
  virtual int maxmemcompactionlevel() {
    return maxmemcompactionlevel(defaultcolumnfamily());
  }

  // number of files in level-0 that would stop writes.
  virtual int level0stopwritetrigger(columnfamilyhandle* column_family) = 0;
  virtual int level0stopwritetrigger() {
    return level0stopwritetrigger(defaultcolumnfamily());
  }

  // get db name -- the exact same name that was provided as an argument to
  // db::open()
  virtual const std::string& getname() const = 0;

  // get env object from the db
  virtual env* getenv() const = 0;

  // get db options that we use
  virtual const options& getoptions(columnfamilyhandle* column_family)
      const = 0;
  virtual const options& getoptions() const {
    return getoptions(defaultcolumnfamily());
  }

  // flush all mem-table data.
  virtual status flush(const flushoptions& options,
                       columnfamilyhandle* column_family) = 0;
  virtual status flush(const flushoptions& options) {
    return flush(options, defaultcolumnfamily());
  }

  // the sequence number of the most recent transaction.
  virtual sequencenumber getlatestsequencenumber() const = 0;

#ifndef rocksdb_lite

  // prevent file deletions. compactions will continue to occur,
  // but no obsolete files will be deleted. calling this multiple
  // times have the same effect as calling it once.
  virtual status disablefiledeletions() = 0;

  // allow compactions to delete obsolete files.
  // if force == true, the call to enablefiledeletions() will guarantee that
  // file deletions are enabled after the call, even if disablefiledeletions()
  // was called multiple times before.
  // if force == false, enablefiledeletions will only enable file deletion
  // after it's been called at least as many times as disablefiledeletions(),
  // enabling the two methods to be called by two threads concurrently without
  // synchronization -- i.e., file deletions will be enabled only after both
  // threads call enablefiledeletions()
  virtual status enablefiledeletions(bool force = true) = 0;

  // getlivefiles followed by getsortedwalfiles can generate a lossless backup

  // this method is deprecated. use the getlivefilesmetadata to get more
  // detailed information on the live files.
  // retrieve the list of all files in the database. the files are
  // relative to the dbname and are not absolute paths. the valid size of the
  // manifest file is returned in manifest_file_size. the manifest file is an
  // ever growing file, but only the portion specified by manifest_file_size is
  // valid for this snapshot.
  // setting flush_memtable to true does flush before recording the live files.
  // setting flush_memtable to false is useful when we don't want to wait for
  // flush which may have to wait for compaction to complete taking an
  // indeterminate time.
  //
  // in case you have multiple column families, even if flush_memtable is true,
  // you still need to call getsortedwalfiles after getlivefiles to compensate
  // for new data that arrived to already-flushed column families while other
  // column families were flushing
  virtual status getlivefiles(std::vector<std::string>&,
                              uint64_t* manifest_file_size,
                              bool flush_memtable = true) = 0;

  // retrieve the sorted list of all wal files with earliest file first
  virtual status getsortedwalfiles(vectorlogptr& files) = 0;

  // sets iter to an iterator that is positioned at a write-batch containing
  // seq_number. if the sequence number is non existent, it returns an iterator
  // at the first available seq_no after the requested seq_no
  // returns status::ok if iterator is valid
  // must set wal_ttl_seconds or wal_size_limit_mb to large values to
  // use this api, else the wal files will get
  // cleared aggressively and the iterator might keep getting invalid before
  // an update is read.
  virtual status getupdatessince(
      sequencenumber seq_number, unique_ptr<transactionlogiterator>* iter,
      const transactionlogiterator::readoptions&
          read_options = transactionlogiterator::readoptions()) = 0;

  // delete the file name from the db directory and update the internal state to
  // reflect that. supports deletion of sst and log files only. 'name' must be
  // path relative to the db directory. eg. 000001.sst, /archive/000003.log
  virtual status deletefile(std::string name) = 0;

  // returns a list of all table files with their level, start key
  // and end key
  virtual void getlivefilesmetadata(std::vector<livefilemetadata>* metadata) {}

#endif  // rocksdb_lite

  // sets the globally unique id created at database creation time by invoking
  // env::generateuniqueid(), in identity. returns status::ok if identity could
  // be set properly
  virtual status getdbidentity(std::string& identity) = 0;

  // returns default column family handle
  virtual columnfamilyhandle* defaultcolumnfamily() const = 0;

#ifndef rocksdb_lite
  virtual status getpropertiesofalltables(columnfamilyhandle* column_family,
                                          tablepropertiescollection* props) = 0;
  virtual status getpropertiesofalltables(tablepropertiescollection* props) {
    return getpropertiesofalltables(defaultcolumnfamily(), props);
  }
#endif  // rocksdb_lite

 private:
  // no copying allowed
  db(const db&);
  void operator=(const db&);
};

// destroy the contents of the specified database.
// be very careful using this method.
status destroydb(const std::string& name, const options& options);

#ifndef rocksdb_lite
// if a db cannot be opened, you may attempt to call this method to
// resurrect as much of the contents of the database as possible.
// some data may be lost, so be careful when calling this function
// on a database that contains important information.
status repairdb(const std::string& dbname, const options& options);
#endif

}  // namespace rocksdb

#endif  // storage_rocksdb_include_db_h_
