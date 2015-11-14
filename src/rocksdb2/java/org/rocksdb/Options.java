// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

/**
 * options to control the behavior of a database.  it will be used
 * during the creation of a rocksdb (i.e., rocksdb.open()).
 *
 * if dispose() function is not called, then it will be gc'd automatically and
 * native resources will be released as part of the process.
 */
public class options extends rocksobject {
  static final long default_cache_size = 8 << 20;
  static final int default_num_shard_bits = -1;
  /**
   * construct options for opening a rocksdb.
   *
   * this constructor will create (by allocating a block of memory)
   * an rocksdb::options in the c++ side.
   */
  public options() {
    super();
    cachesize_ = default_cache_size;
    numshardbits_ = default_num_shard_bits;
    newoptions();
    env_ = rocksenv.getdefault();
  }

  /**
   * if this value is set to true, then the database will be created
   * if it is missing during rocksdb.open().
   * default: false
   *
   * @param flag a flag indicating whether to create a database the
   *     specified database in rocksdb.open() operation is missing.
   * @return the instance of the current options.
   * @see rocksdb.open()
   */
  public options setcreateifmissing(boolean flag) {
    assert(isinitialized());
    setcreateifmissing(nativehandle_, flag);
    return this;
  }

  /**
   * use the specified object to interact with the environment,
   * e.g. to read/write files, schedule background work, etc.
   * default: rocksenv.getdefault()
   */
  public options setenv(rocksenv env) {
    assert(isinitialized());
    setenv(nativehandle_, env.nativehandle_);
    env_ = env;
    return this;
  }
  private native void setenv(long opthandle, long envhandle);

  public rocksenv getenv() {
    return env_;
  }
  private native long getenvhandle(long handle);

  /**
   * return true if the create_if_missing flag is set to true.
   * if true, the database will be created if it is missing.
   *
   * @return true if the createifmissing option is set to true.
   * @see setcreateifmissing()
   */
  public boolean createifmissing() {
    assert(isinitialized());
    return createifmissing(nativehandle_);
  }

  /**
   * amount of data to build up in memory (backed by an unsorted log
   * on disk) before converting to a sorted on-disk file.
   *
   * larger values increase performance, especially during bulk loads.
   * up to max_write_buffer_number write buffers may be held in memory
   * at the same time, so you may wish to adjust this parameter
   * to control memory usage.
   *
   * also, a larger write buffer will result in a longer recovery time
   * the next time the database is opened.
   *
   * default: 4mb
   * @param writebuffersize the size of write buffer.
   * @return the instance of the current options.
   * @see rocksdb.open()
   */
  public options setwritebuffersize(long writebuffersize) {
    assert(isinitialized());
    setwritebuffersize(nativehandle_, writebuffersize);
    return this;
  }

  /**
   * return size of write buffer size.
   *
   * @return size of write buffer.
   * @see setwritebuffersize()
   */
  public long writebuffersize()  {
    assert(isinitialized());
    return writebuffersize(nativehandle_);
  }

  /**
   * the maximum number of write buffers that are built up in memory.
   * the default is 2, so that when 1 write buffer is being flushed to
   * storage, new writes can continue to the other write buffer.
   * default: 2
   *
   * @param maxwritebuffernumber maximum number of write buffers.
   * @return the instance of the current options.
   * @see rocksdb.open()
   */
  public options setmaxwritebuffernumber(int maxwritebuffernumber) {
    assert(isinitialized());
    setmaxwritebuffernumber(nativehandle_, maxwritebuffernumber);
    return this;
  }

  /**
   * returns maximum number of write buffers.
   *
   * @return maximum number of write buffers.
   * @see setmaxwritebuffernumber()
   */
  public int maxwritebuffernumber() {
    assert(isinitialized());
    return maxwritebuffernumber(nativehandle_);
  }

  /**
   * if true, an error will be thrown during rocksdb.open() if the
   * database already exists.
   *
   * @return if true, an error is raised when the specified database
   *    already exists before open.
   */
  public boolean errorifexists() {
    assert(isinitialized());
    return errorifexists(nativehandle_);
  }
  private native boolean errorifexists(long handle);

  /**
   * if true, an error will be thrown during rocksdb.open() if the
   * database already exists.
   * default: false
   *
   * @param errorifexists if true, an exception will be thrown
   *     during rocksdb.open() if the database already exists.
   * @return the reference to the current option.
   * @see rocksdb.open()
   */
  public options seterrorifexists(boolean errorifexists) {
    assert(isinitialized());
    seterrorifexists(nativehandle_, errorifexists);
    return this;
  }
  private native void seterrorifexists(long handle, boolean errorifexists);

  /**
   * if true, the implementation will do aggressive checking of the
   * data it is processing and will stop early if it detects any
   * errors.  this may have unforeseen ramifications: for example, a
   * corruption of one db entry may cause a large number of entries to
   * become unreadable or for the entire db to become unopenable.
   * if any of the  writes to the database fails (put, delete, merge, write),
   * the database will switch to read-only mode and fail all other
   * write operations.
   *
   * @return a boolean indicating whether paranoid-check is on.
   */
  public boolean paranoidchecks() {
    assert(isinitialized());
    return paranoidchecks(nativehandle_);
  }
  private native boolean paranoidchecks(long handle);

  /**
   * if true, the implementation will do aggressive checking of the
   * data it is processing and will stop early if it detects any
   * errors.  this may have unforeseen ramifications: for example, a
   * corruption of one db entry may cause a large number of entries to
   * become unreadable or for the entire db to become unopenable.
   * if any of the  writes to the database fails (put, delete, merge, write),
   * the database will switch to read-only mode and fail all other
   * write operations.
   * default: true
   *
   * @param paranoidchecks a flag to indicate whether paranoid-check
   *     is on.
   * @return the reference to the current option.
   */
  public options setparanoidchecks(boolean paranoidchecks) {
    assert(isinitialized());
    setparanoidchecks(nativehandle_, paranoidchecks);
    return this;
  }
  private native void setparanoidchecks(
      long handle, boolean paranoidchecks);

  /**
   * number of open files that can be used by the db.  you may need to
   * increase this if your database has a large working set. value -1 means
   * files opened are always kept open. you can estimate number of files based
   * on target_file_size_base and target_file_size_multiplier for level-based
   * compaction. for universal-style compaction, you can usually set it to -1.
   *
   * @return the maximum number of open files.
   */
  public int maxopenfiles() {
    assert(isinitialized());
    return maxopenfiles(nativehandle_);
  }
  private native int maxopenfiles(long handle);

  /**
   * number of open files that can be used by the db.  you may need to
   * increase this if your database has a large working set. value -1 means
   * files opened are always kept open. you can estimate number of files based
   * on target_file_size_base and target_file_size_multiplier for level-based
   * compaction. for universal-style compaction, you can usually set it to -1.
   * default: 5000
   *
   * @param maxopenfiles the maximum number of open files.
   * @return the reference to the current option.
   */
  public options setmaxopenfiles(int maxopenfiles) {
    assert(isinitialized());
    setmaxopenfiles(nativehandle_, maxopenfiles);
    return this;
  }
  private native void setmaxopenfiles(long handle, int maxopenfiles);

  /**
   * if true, then the contents of data files are not synced
   * to stable storage. their contents remain in the os buffers till the
   * os decides to flush them. this option is good for bulk-loading
   * of data. once the bulk-loading is complete, please issue a
   * sync to the os to flush all dirty buffesrs to stable storage.
   *
   * @return if true, then data-sync is disabled.
   */
  public boolean disabledatasync() {
    assert(isinitialized());
    return disabledatasync(nativehandle_);
  }
  private native boolean disabledatasync(long handle);

  /**
   * if true, then the contents of data files are not synced
   * to stable storage. their contents remain in the os buffers till the
   * os decides to flush them. this option is good for bulk-loading
   * of data. once the bulk-loading is complete, please issue a
   * sync to the os to flush all dirty buffesrs to stable storage.
   * default: false
   *
   * @param disabledatasync a boolean flag to specify whether to
   *     disable data sync.
   * @return the reference to the current option.
   */
  public options setdisabledatasync(boolean disabledatasync) {
    assert(isinitialized());
    setdisabledatasync(nativehandle_, disabledatasync);
    return this;
  }
  private native void setdisabledatasync(long handle, boolean disabledatasync);

  /**
   * if true, then every store to stable storage will issue a fsync.
   * if false, then every store to stable storage will issue a fdatasync.
   * this parameter should be set to true while storing data to
   * filesystem like ext3 that can lose files after a reboot.
   *
   * @return true if fsync is used.
   */
  public boolean usefsync() {
    assert(isinitialized());
    return usefsync(nativehandle_);
  }
  private native boolean usefsync(long handle);

  /**
   * if true, then every store to stable storage will issue a fsync.
   * if false, then every store to stable storage will issue a fdatasync.
   * this parameter should be set to true while storing data to
   * filesystem like ext3 that can lose files after a reboot.
   * default: false
   *
   * @param usefsync a boolean flag to specify whether to use fsync
   * @return the reference to the current option.
   */
  public options setusefsync(boolean usefsync) {
    assert(isinitialized());
    setusefsync(nativehandle_, usefsync);
    return this;
  }
  private native void setusefsync(long handle, boolean usefsync);

  /**
   * the time interval in seconds between each two consecutive stats logs.
   * this number controls how often a new scribe log about
   * db deploy stats is written out.
   * -1 indicates no logging at all.
   *
   * @return the time interval in seconds between each two consecutive
   *     stats logs.
   */
  public int dbstatsloginterval() {
    assert(isinitialized());
    return dbstatsloginterval(nativehandle_);
  }
  private native int dbstatsloginterval(long handle);

  /**
   * the time interval in seconds between each two consecutive stats logs.
   * this number controls how often a new scribe log about
   * db deploy stats is written out.
   * -1 indicates no logging at all.
   * default value is 1800 (half an hour).
   *
   * @param dbstatsloginterval the time interval in seconds between each
   *     two consecutive stats logs.
   * @return the reference to the current option.
   */
  public options setdbstatsloginterval(int dbstatsloginterval) {
    assert(isinitialized());
    setdbstatsloginterval(nativehandle_, dbstatsloginterval);
    return this;
  }
  private native void setdbstatsloginterval(
      long handle, int dbstatsloginterval);

  /**
   * returns the directory of info log.
   *
   * if it is empty, the log files will be in the same dir as data.
   * if it is non empty, the log files will be in the specified dir,
   * and the db data dir's absolute path will be used as the log file
   * name's prefix.
   *
   * @return the path to the info log directory
   */
  public string dblogdir() {
    assert(isinitialized());
    return dblogdir(nativehandle_);
  }
  private native string dblogdir(long handle);

  /**
   * this specifies the info log dir.
   * if it is empty, the log files will be in the same dir as data.
   * if it is non empty, the log files will be in the specified dir,
   * and the db data dir's absolute path will be used as the log file
   * name's prefix.
   *
   * @param dblogdir the path to the info log directory
   * @return the reference to the current option.
   */
  public options setdblogdir(string dblogdir) {
    assert(isinitialized());
    setdblogdir(nativehandle_, dblogdir);
    return this;
  }
  private native void setdblogdir(long handle, string dblogdir);

  /**
   * returns the path to the write-ahead-logs (wal) directory.
   *
   * if it is empty, the log files will be in the same dir as data,
   *   dbname is used as the data dir by default
   * if it is non empty, the log files will be in kept the specified dir.
   * when destroying the db,
   *   all log files in wal_dir and the dir itself is deleted
   *
   * @return the path to the write-ahead-logs (wal) directory.
   */
  public string waldir() {
    assert(isinitialized());
    return waldir(nativehandle_);
  }
  private native string waldir(long handle);

  /**
   * this specifies the absolute dir path for write-ahead logs (wal).
   * if it is empty, the log files will be in the same dir as data,
   *   dbname is used as the data dir by default
   * if it is non empty, the log files will be in kept the specified dir.
   * when destroying the db,
   *   all log files in wal_dir and the dir itself is deleted
   *
   * @param waldir the path to the write-ahead-log directory.
   * @return the reference to the current option.
   */
  public options setwaldir(string waldir) {
    assert(isinitialized());
    setwaldir(nativehandle_, waldir);
    return this;
  }
  private native void setwaldir(long handle, string waldir);

  /**
   * the periodicity when obsolete files get deleted. the default
   * value is 6 hours. the files that get out of scope by compaction
   * process will still get automatically delete on every compaction,
   * regardless of this setting
   *
   * @return the time interval in micros when obsolete files will be deleted.
   */
  public long deleteobsoletefilesperiodmicros() {
    assert(isinitialized());
    return deleteobsoletefilesperiodmicros(nativehandle_);
  }
  private native long deleteobsoletefilesperiodmicros(long handle);

  /**
   * the periodicity when obsolete files get deleted. the default
   * value is 6 hours. the files that get out of scope by compaction
   * process will still get automatically delete on every compaction,
   * regardless of this setting
   *
   * @param micros the time interval in micros
   * @return the reference to the current option.
   */
  public options setdeleteobsoletefilesperiodmicros(long micros) {
    assert(isinitialized());
    setdeleteobsoletefilesperiodmicros(nativehandle_, micros);
    return this;
  }
  private native void setdeleteobsoletefilesperiodmicros(
      long handle, long micros);

  /**
   * returns the maximum number of concurrent background compaction jobs,
   * submitted to the default low priority thread pool.
   * when increasing this number, we may also want to consider increasing
   * number of threads in low priority thread pool.
   * default: 1
   *
   * @return the maximum number of concurrent background compaction jobs.
   * @see env.setbackgroundthreads()
   */
  public int maxbackgroundcompactions() {
    assert(isinitialized());
    return maxbackgroundcompactions(nativehandle_);
  }

  /**
   * creates statistics object which collects metrics about database operations.
     statistics objects should not be shared between db instances as
     it does not use any locks to prevent concurrent updates.
   *
   * @return the instance of the current options.
   * @see rocksdb.open()
   */
  public options createstatistics() {
    assert(isinitialized());
    createstatistics(nativehandle_);
    return this;
  }

  /**
   * returns statistics object. calls createstatistics() if
   * c++ returns null pointer for statistics.
   *
   * @return the instance of the statistics object.
   * @see createstatistics()
   */
  public statistics statisticsptr() {
    assert(isinitialized());

    long statsptr = statisticsptr(nativehandle_);
    if(statsptr == 0) {
      createstatistics();
      statsptr = statisticsptr(nativehandle_);
    }

    return new statistics(statsptr);
  }

  /**
   * specifies the maximum number of concurrent background compaction jobs,
   * submitted to the default low priority thread pool.
   * if you're increasing this, also consider increasing number of threads in
   * low priority thread pool. for more information, see
   * default: 1
   *
   * @param maxbackgroundcompactions the maximum number of background
   *     compaction jobs.
   * @return the reference to the current option.
   *
   * @see env.setbackgroundthreads()
   * @see maxbackgroundflushes()
   */
  public options setmaxbackgroundcompactions(int maxbackgroundcompactions) {
    assert(isinitialized());
    setmaxbackgroundcompactions(nativehandle_, maxbackgroundcompactions);
    return this;
  }

  /**
   * returns the maximum number of concurrent background flush jobs.
   * if you're increasing this, also consider increasing number of threads in
   * high priority thread pool. for more information, see
   * default: 1
   *
   * @return the maximum number of concurrent background flush jobs.
   * @see env.setbackgroundthreads()
   */
  public int maxbackgroundflushes() {
    assert(isinitialized());
    return maxbackgroundflushes(nativehandle_);
  }
  private native int maxbackgroundflushes(long handle);

  /**
   * specifies the maximum number of concurrent background flush jobs.
   * if you're increasing this, also consider increasing number of threads in
   * high priority thread pool. for more information, see
   * default: 1
   *
   * @param maxbackgroundflushes
   * @return the reference to the current option.
   *
   * @see env.setbackgroundthreads()
   * @see maxbackgroundcompactions()
   */
  public options setmaxbackgroundflushes(int maxbackgroundflushes) {
    assert(isinitialized());
    setmaxbackgroundflushes(nativehandle_, maxbackgroundflushes);
    return this;
  }
  private native void setmaxbackgroundflushes(
      long handle, int maxbackgroundflushes);

  /**
   * returns the maximum size of a info log file. if the current log file
   * is larger than this size, a new info log file will be created.
   * if 0, all logs will be written to one log file.
   *
   * @return the maximum size of the info log file.
   */
  public long maxlogfilesize() {
    assert(isinitialized());
    return maxlogfilesize(nativehandle_);
  }
  private native long maxlogfilesize(long handle);

  /**
   * specifies the maximum size of a info log file. if the current log file
   * is larger than `max_log_file_size`, a new info log file will
   * be created.
   * if 0, all logs will be written to one log file.
   *
   * @param maxlogfilesize the maximum size of a info log file.
   * @return the reference to the current option.
   */
  public options setmaxlogfilesize(long maxlogfilesize) {
    assert(isinitialized());
    setmaxlogfilesize(nativehandle_, maxlogfilesize);
    return this;
  }
  private native void setmaxlogfilesize(long handle, long maxlogfilesize);

  /**
   * returns the time interval for the info log file to roll (in seconds).
   * if specified with non-zero value, log file will be rolled
   * if it has been active longer than `log_file_time_to_roll`.
   * default: 0 (disabled)
   *
   * @return the time interval in seconds.
   */
  public long logfiletimetoroll() {
    assert(isinitialized());
    return logfiletimetoroll(nativehandle_);
  }
  private native long logfiletimetoroll(long handle);

  /**
   * specifies the time interval for the info log file to roll (in seconds).
   * if specified with non-zero value, log file will be rolled
   * if it has been active longer than `log_file_time_to_roll`.
   * default: 0 (disabled)
   *
   * @param logfiletimetoroll the time interval in seconds.
   * @return the reference to the current option.
   */
  public options setlogfiletimetoroll(long logfiletimetoroll) {
    assert(isinitialized());
    setlogfiletimetoroll(nativehandle_, logfiletimetoroll);
    return this;
  }
  private native void setlogfiletimetoroll(
      long handle, long logfiletimetoroll);

  /**
   * returns the maximum number of info log files to be kept.
   * default: 1000
   *
   * @return the maximum number of info log files to be kept.
   */
  public long keeplogfilenum() {
    assert(isinitialized());
    return keeplogfilenum(nativehandle_);
  }
  private native long keeplogfilenum(long handle);

  /**
   * specifies the maximum number of info log files to be kept.
   * default: 1000
   *
   * @param keeplogfilenum the maximum number of info log files to be kept.
   * @return the reference to the current option.
   */
  public options setkeeplogfilenum(long keeplogfilenum) {
    assert(isinitialized());
    setkeeplogfilenum(nativehandle_, keeplogfilenum);
    return this;
  }
  private native void setkeeplogfilenum(long handle, long keeplogfilenum);

  /**
   * manifest file is rolled over on reaching this limit.
   * the older manifest file be deleted.
   * the default value is max_int so that roll-over does not take place.
   *
   * @return the size limit of a manifest file.
   */
  public long maxmanifestfilesize() {
    assert(isinitialized());
    return maxmanifestfilesize(nativehandle_);
  }
  private native long maxmanifestfilesize(long handle);

  /**
   * manifest file is rolled over on reaching this limit.
   * the older manifest file be deleted.
   * the default value is max_int so that roll-over does not take place.
   *
   * @param maxmanifestfilesize the size limit of a manifest file.
   * @return the reference to the current option.
   */
  public options setmaxmanifestfilesize(long maxmanifestfilesize) {
    assert(isinitialized());
    setmaxmanifestfilesize(nativehandle_, maxmanifestfilesize);
    return this;
  }
  private native void setmaxmanifestfilesize(
      long handle, long maxmanifestfilesize);

  /**
   * number of shards used for table cache.
   *
   * @return the number of shards used for table cache.
   */
  public int tablecachenumshardbits() {
    assert(isinitialized());
    return tablecachenumshardbits(nativehandle_);
  }
  private native int tablecachenumshardbits(long handle);

  /**
   * number of shards used for table cache.
   *
   * @param tablecachenumshardbits the number of chards
   * @return the reference to the current option.
   */
  public options settablecachenumshardbits(int tablecachenumshardbits) {
    assert(isinitialized());
    settablecachenumshardbits(nativehandle_, tablecachenumshardbits);
    return this;
  }
  private native void settablecachenumshardbits(
      long handle, int tablecachenumshardbits);

  /**
   * during data eviction of table's lru cache, it would be inefficient
   * to strictly follow lru because this piece of memory will not really
   * be released unless its refcount falls to zero. instead, make two
   * passes: the first pass will release items with refcount = 1,
   * and if not enough space releases after scanning the number of
   * elements specified by this parameter, we will remove items in lru
   * order.
   *
   * @return scan count limit
   */
  public int tablecacheremovescancountlimit() {
    assert(isinitialized());
    return tablecacheremovescancountlimit(nativehandle_);
  }
  private native int tablecacheremovescancountlimit(long handle);

  /**
   * during data eviction of table's lru cache, it would be inefficient
   * to strictly follow lru because this piece of memory will not really
   * be released unless its refcount falls to zero. instead, make two
   * passes: the first pass will release items with refcount = 1,
   * and if not enough space releases after scanning the number of
   * elements specified by this parameter, we will remove items in lru
   * order.
   *
   * @param limit scan count limit
   * @return the reference to the current option.
   */
  public options settablecacheremovescancountlimit(int limit) {
    assert(isinitialized());
    settablecacheremovescancountlimit(nativehandle_, limit);
    return this;
  }
  private native void settablecacheremovescancountlimit(
      long handle, int limit);

  /**
   * walttlseconds() and walsizelimitmb() affect how archived logs
   * will be deleted.
   * 1. if both set to 0, logs will be deleted asap and will not get into
   *    the archive.
   * 2. if wal_ttl_seconds is 0 and wal_size_limit_mb is not 0,
   *    wal files will be checked every 10 min and if total size is greater
   *    then wal_size_limit_mb, they will be deleted starting with the
   *    earliest until size_limit is met. all empty files will be deleted.
   * 3. if wal_ttl_seconds is not 0 and wal_size_limit_mb is 0, then
   *    wal files will be checked every wal_ttl_secondsi / 2 and those that
   *    are older than wal_ttl_seconds will be deleted.
   * 4. if both are not 0, wal files will be checked every 10 min and both
   *    checks will be performed with ttl being first.
   *
   * @return the wal-ttl seconds
   * @see walsizelimitmb()
   */
  public long walttlseconds() {
    assert(isinitialized());
    return walttlseconds(nativehandle_);
  }
  private native long walttlseconds(long handle);

  /**
   * walttlseconds() and walsizelimitmb() affect how archived logs
   * will be deleted.
   * 1. if both set to 0, logs will be deleted asap and will not get into
   *    the archive.
   * 2. if wal_ttl_seconds is 0 and wal_size_limit_mb is not 0,
   *    wal files will be checked every 10 min and if total size is greater
   *    then wal_size_limit_mb, they will be deleted starting with the
   *    earliest until size_limit is met. all empty files will be deleted.
   * 3. if wal_ttl_seconds is not 0 and wal_size_limit_mb is 0, then
   *    wal files will be checked every wal_ttl_secondsi / 2 and those that
   *    are older than wal_ttl_seconds will be deleted.
   * 4. if both are not 0, wal files will be checked every 10 min and both
   *    checks will be performed with ttl being first.
   *
   * @param walttlseconds the ttl seconds
   * @return the reference to the current option.
   * @see setwalsizelimitmb()
   */
  public options setwalttlseconds(long walttlseconds) {
    assert(isinitialized());
    setwalttlseconds(nativehandle_, walttlseconds);
    return this;
  }
  private native void setwalttlseconds(long handle, long walttlseconds);

  /**
   * walttlseconds() and walsizelimitmb() affect how archived logs
   * will be deleted.
   * 1. if both set to 0, logs will be deleted asap and will not get into
   *    the archive.
   * 2. if wal_ttl_seconds is 0 and wal_size_limit_mb is not 0,
   *    wal files will be checked every 10 min and if total size is greater
   *    then wal_size_limit_mb, they will be deleted starting with the
   *    earliest until size_limit is met. all empty files will be deleted.
   * 3. if wal_ttl_seconds is not 0 and wal_size_limit_mb is 0, then
   *    wal files will be checked every wal_ttl_secondsi / 2 and those that
   *    are older than wal_ttl_seconds will be deleted.
   * 4. if both are not 0, wal files will be checked every 10 min and both
   *    checks will be performed with ttl being first.
   *
   * @return size limit in mega-bytes.
   * @see walsizelimitmb()
   */
  public long walsizelimitmb() {
    assert(isinitialized());
    return walsizelimitmb(nativehandle_);
  }
  private native long walsizelimitmb(long handle);

  /**
   * walttlseconds() and walsizelimitmb() affect how archived logs
   * will be deleted.
   * 1. if both set to 0, logs will be deleted asap and will not get into
   *    the archive.
   * 2. if wal_ttl_seconds is 0 and wal_size_limit_mb is not 0,
   *    wal files will be checked every 10 min and if total size is greater
   *    then wal_size_limit_mb, they will be deleted starting with the
   *    earliest until size_limit is met. all empty files will be deleted.
   * 3. if wal_ttl_seconds is not 0 and wal_size_limit_mb is 0, then
   *    wal files will be checked every wal_ttl_secondsi / 2 and those that
   *    are older than wal_ttl_seconds will be deleted.
   * 4. if both are not 0, wal files will be checked every 10 min and both
   *    checks will be performed with ttl being first.
   *
   * @param sizelimitmb size limit in mega-bytes.
   * @return the reference to the current option.
   * @see setwalsizelimitmb()
   */
  public options setwalsizelimitmb(long sizelimitmb) {
    assert(isinitialized());
    setwalsizelimitmb(nativehandle_, sizelimitmb);
    return this;
  }
  private native void setwalsizelimitmb(long handle, long sizelimitmb);

  /**
   * number of bytes to preallocate (via fallocate) the manifest
   * files.  default is 4mb, which is reasonable to reduce random io
   * as well as prevent overallocation for mounts that preallocate
   * large amounts of data (such as xfs's allocsize option).
   *
   * @return size in bytes.
   */
  public long manifestpreallocationsize() {
    assert(isinitialized());
    return manifestpreallocationsize(nativehandle_);
  }
  private native long manifestpreallocationsize(long handle);

  /**
   * number of bytes to preallocate (via fallocate) the manifest
   * files.  default is 4mb, which is reasonable to reduce random io
   * as well as prevent overallocation for mounts that preallocate
   * large amounts of data (such as xfs's allocsize option).
   *
   * @param size the size in byte
   * @return the reference to the current option.
   */
  public options setmanifestpreallocationsize(long size) {
    assert(isinitialized());
    setmanifestpreallocationsize(nativehandle_, size);
    return this;
  }
  private native void setmanifestpreallocationsize(
      long handle, long size);

  /**
   * data being read from file storage may be buffered in the os
   * default: true
   *
   * @return if true, then os buffering is allowed.
   */
  public boolean allowosbuffer() {
    assert(isinitialized());
    return allowosbuffer(nativehandle_);
  }
  private native boolean allowosbuffer(long handle);

  /**
   * data being read from file storage may be buffered in the os
   * default: true
   *
   * @param allowosbufferif true, then os buffering is allowed.
   * @return the reference to the current option.
   */
  public options setallowosbuffer(boolean allowosbuffer) {
    assert(isinitialized());
    setallowosbuffer(nativehandle_, allowosbuffer);
    return this;
  }
  private native void setallowosbuffer(
      long handle, boolean allowosbuffer);

  /**
   * allow the os to mmap file for reading sst tables.
   * default: false
   *
   * @return true if mmap reads are allowed.
   */
  public boolean allowmmapreads() {
    assert(isinitialized());
    return allowmmapreads(nativehandle_);
  }
  private native boolean allowmmapreads(long handle);

  /**
   * allow the os to mmap file for reading sst tables.
   * default: false
   *
   * @param allowmmapreads true if mmap reads are allowed.
   * @return the reference to the current option.
   */
  public options setallowmmapreads(boolean allowmmapreads) {
    assert(isinitialized());
    setallowmmapreads(nativehandle_, allowmmapreads);
    return this;
  }
  private native void setallowmmapreads(
      long handle, boolean allowmmapreads);

  /**
   * allow the os to mmap file for writing. default: false
   *
   * @return true if mmap writes are allowed.
   */
  public boolean allowmmapwrites() {
    assert(isinitialized());
    return allowmmapwrites(nativehandle_);
  }
  private native boolean allowmmapwrites(long handle);

  /**
   * allow the os to mmap file for writing. default: false
   *
   * @param allowmmapwrites true if mmap writes are allowd.
   * @return the reference to the current option.
   */
  public options setallowmmapwrites(boolean allowmmapwrites) {
    assert(isinitialized());
    setallowmmapwrites(nativehandle_, allowmmapwrites);
    return this;
  }
  private native void setallowmmapwrites(
      long handle, boolean allowmmapwrites);

  /**
   * disable child process inherit open files. default: true
   *
   * @return true if child process inheriting open files is disabled.
   */
  public boolean isfdcloseonexec() {
    assert(isinitialized());
    return isfdcloseonexec(nativehandle_);
  }
  private native boolean isfdcloseonexec(long handle);

  /**
   * disable child process inherit open files. default: true
   *
   * @param isfdcloseonexec true if child process inheriting open
   *     files is disabled.
   * @return the reference to the current option.
   */
  public options setisfdcloseonexec(boolean isfdcloseonexec) {
    assert(isinitialized());
    setisfdcloseonexec(nativehandle_, isfdcloseonexec);
    return this;
  }
  private native void setisfdcloseonexec(
      long handle, boolean isfdcloseonexec);

  /**
   * skip log corruption error on recovery (if client is ok with
   * losing most recent changes)
   * default: false
   *
   * @return true if log corruption errors are skipped during recovery.
   */
  public boolean skiplogerroronrecovery() {
    assert(isinitialized());
    return skiplogerroronrecovery(nativehandle_);
  }
  private native boolean skiplogerroronrecovery(long handle);

  /**
   * skip log corruption error on recovery (if client is ok with
   * losing most recent changes)
   * default: false
   *
   * @param skip true if log corruption errors are skipped during recovery.
   * @return the reference to the current option.
   */
  public options setskiplogerroronrecovery(boolean skip) {
    assert(isinitialized());
    setskiplogerroronrecovery(nativehandle_, skip);
    return this;
  }
  private native void setskiplogerroronrecovery(
      long handle, boolean skip);

  /**
   * if not zero, dump rocksdb.stats to log every stats_dump_period_sec
   * default: 3600 (1 hour)
   *
   * @return time interval in seconds.
   */
  public int statsdumpperiodsec() {
    assert(isinitialized());
    return statsdumpperiodsec(nativehandle_);
  }
  private native int statsdumpperiodsec(long handle);

  /**
   * if not zero, dump rocksdb.stats to log every stats_dump_period_sec
   * default: 3600 (1 hour)
   *
   * @param statsdumpperiodsec time interval in seconds.
   * @return the reference to the current option.
   */
  public options setstatsdumpperiodsec(int statsdumpperiodsec) {
    assert(isinitialized());
    setstatsdumpperiodsec(nativehandle_, statsdumpperiodsec);
    return this;
  }
  private native void setstatsdumpperiodsec(
      long handle, int statsdumpperiodsec);

  /**
   * if set true, will hint the underlying file system that the file
   * access pattern is random, when a sst file is opened.
   * default: true
   *
   * @return true if hinting random access is on.
   */
  public boolean adviserandomonopen() {
    return adviserandomonopen(nativehandle_);
  }
  private native boolean adviserandomonopen(long handle);

  /**
   * if set true, will hint the underlying file system that the file
   * access pattern is random, when a sst file is opened.
   * default: true
   *
   * @param adviserandomonopen true if hinting random access is on.
   * @return the reference to the current option.
   */
  public options setadviserandomonopen(boolean adviserandomonopen) {
    assert(isinitialized());
    setadviserandomonopen(nativehandle_, adviserandomonopen);
    return this;
  }
  private native void setadviserandomonopen(
      long handle, boolean adviserandomonopen);

  /**
   * use adaptive mutex, which spins in the user space before resorting
   * to kernel. this could reduce context switch when the mutex is not
   * heavily contended. however, if the mutex is hot, we could end up
   * wasting spin time.
   * default: false
   *
   * @return true if adaptive mutex is used.
   */
  public boolean useadaptivemutex() {
    assert(isinitialized());
    return useadaptivemutex(nativehandle_);
  }
  private native boolean useadaptivemutex(long handle);

  /**
   * use adaptive mutex, which spins in the user space before resorting
   * to kernel. this could reduce context switch when the mutex is not
   * heavily contended. however, if the mutex is hot, we could end up
   * wasting spin time.
   * default: false
   *
   * @param useadaptivemutex true if adaptive mutex is used.
   * @return the reference to the current option.
   */
  public options setuseadaptivemutex(boolean useadaptivemutex) {
    assert(isinitialized());
    setuseadaptivemutex(nativehandle_, useadaptivemutex);
    return this;
  }
  private native void setuseadaptivemutex(
      long handle, boolean useadaptivemutex);

  /**
   * allows os to incrementally sync files to disk while they are being
   * written, asynchronously, in the background.
   * issue one request for every bytes_per_sync written. 0 turns it off.
   * default: 0
   *
   * @return size in bytes
   */
  public long bytespersync() {
    return bytespersync(nativehandle_);
  }
  private native long bytespersync(long handle);

  /**
   * allows os to incrementally sync files to disk while they are being
   * written, asynchronously, in the background.
   * issue one request for every bytes_per_sync written. 0 turns it off.
   * default: 0
   *
   * @param bytespersync size in bytes
   * @return the reference to the current option.
   */
  public options setbytespersync(long bytespersync) {
    assert(isinitialized());
    setbytespersync(nativehandle_, bytespersync);
    return this;
  }
  private native void setbytespersync(
      long handle, long bytespersync);

  /**
   * allow rocksdb to use thread local storage to optimize performance.
   * default: true
   *
   * @return true if thread-local storage is allowed
   */
  public boolean allowthreadlocal() {
    assert(isinitialized());
    return allowthreadlocal(nativehandle_);
  }
  private native boolean allowthreadlocal(long handle);

  /**
   * allow rocksdb to use thread local storage to optimize performance.
   * default: true
   *
   * @param allowthreadlocal true if thread-local storage is allowed.
   * @return the reference to the current option.
   */
  public options setallowthreadlocal(boolean allowthreadlocal) {
    assert(isinitialized());
    setallowthreadlocal(nativehandle_, allowthreadlocal);
    return this;
  }
  private native void setallowthreadlocal(
      long handle, boolean allowthreadlocal);

  /**
   * set the config for mem-table.
   *
   * @param config the mem-table config.
   * @return the instance of the current options.
   */
  public options setmemtableconfig(memtableconfig config) {
    setmemtablefactory(nativehandle_, config.newmemtablefactoryhandle());
    return this;
  }

  /**
   * returns the name of the current mem table representation.
   * memtable format can be set using settableformatconfig.
   *
   * @return the name of the currently-used memtable factory.
   * @see settableformatconfig()
   */
  public string memtablefactoryname() {
    assert(isinitialized());
    return memtablefactoryname(nativehandle_);
  }

  /**
   * set the config for table format.
   *
   * @param config the table format config.
   * @return the reference of the current options.
   */
  public options settableformatconfig(tableformatconfig config) {
    settablefactory(nativehandle_, config.newtablefactoryhandle());
    return this;
  }

  /**
   * @return the name of the currently used table factory.
   */
  public string tablefactoryname() {
    assert(isinitialized());
    return tablefactoryname(nativehandle_);
  }

  /**
   * this prefix-extractor uses the first n bytes of a key as its prefix.
   *
   * in some hash-based memtable representation such as hashlinkedlist
   * and hashskiplist, prefixes are used to partition the keys into
   * several buckets.  prefix extractor is used to specify how to
   * extract the prefix given a key.
   *
   * @param n use the first n bytes of a key as its prefix.
   */
  public options usefixedlengthprefixextractor(int n) {
    assert(isinitialized());
    usefixedlengthprefixextractor(nativehandle_, n);
    return this;
  }

///////////////////////////////////////////////////////////////////////
  /**
   * number of keys between restart points for delta encoding of keys.
   * this parameter can be changed dynamically.  most clients should
   * leave this parameter alone.
   * default: 16
   *
   * @return the number of keys between restart points.
   */
  public int blockrestartinterval() {
    return blockrestartinterval(nativehandle_);
  }
  private native int blockrestartinterval(long handle);

  /**
   * number of keys between restart points for delta encoding of keys.
   * this parameter can be changed dynamically.  most clients should
   * leave this parameter alone.
   * default: 16
   *
   * @param blockrestartinterval the number of keys between restart points.
   * @return the reference to the current option.
   */
  public options setblockrestartinterval(int blockrestartinterval) {
    setblockrestartinterval(nativehandle_, blockrestartinterval);
    return this;
  }
  private native void setblockrestartinterval(
      long handle, int blockrestartinterval);

  /**
   * compress blocks using the specified compression algorithm.  this
     parameter can be changed dynamically.
   *
   * default: snappy_compression, which gives lightweight but fast compression.
   *
   * @return compression type.
   */
  public compressiontype compressiontype() {
    return compressiontype.values()[compressiontype(nativehandle_)];
  }
  private native byte compressiontype(long handle);

  /**
   * compress blocks using the specified compression algorithm.  this
     parameter can be changed dynamically.
   *
   * default: snappy_compression, which gives lightweight but fast compression.
   *
   * @param compressiontype compression type.
   * @return the reference to the current option.
   */
  public options setcompressiontype(compressiontype compressiontype) {
    setcompressiontype(nativehandle_, compressiontype.getvalue());
    return this;
  }
  private native void setcompressiontype(long handle, byte compressiontype);

   /**
   * compaction style for db.
   *
   * @return compaction style.
   */
  public compactionstyle compactionstyle() {
    return compactionstyle.values()[compactionstyle(nativehandle_)];
  }
  private native byte compactionstyle(long handle);

  /**
   * set compaction style for db.
   *
   * default: level.
   *
   * @param compactionstyle compaction style.
   * @return the reference to the current option.
   */
  public options setcompactionstyle(compactionstyle compactionstyle) {
    setcompactionstyle(nativehandle_, compactionstyle.getvalue());
    return this;
  }
  private native void setcompactionstyle(long handle, byte compactionstyle);

  /**
   * if level-styled compaction is used, then this number determines
   * the total number of levels.
   *
   * @return the number of levels.
   */
  public int numlevels() {
    return numlevels(nativehandle_);
  }
  private native int numlevels(long handle);

  /**
   * set the number of levels for this database
   * if level-styled compaction is used, then this number determines
   * the total number of levels.
   *
   * @param numlevels the number of levels.
   * @return the reference to the current option.
   */
  public options setnumlevels(int numlevels) {
    setnumlevels(nativehandle_, numlevels);
    return this;
  }
  private native void setnumlevels(
      long handle, int numlevels);

  /**
   * the number of files in leve 0 to trigger compaction from level-0 to
   * level-1.  a value < 0 means that level-0 compaction will not be
   * triggered by number of files at all.
   * default: 4
   *
   * @return the number of files in level 0 to trigger compaction.
   */
  public int levelzerofilenumcompactiontrigger() {
    return levelzerofilenumcompactiontrigger(nativehandle_);
  }
  private native int levelzerofilenumcompactiontrigger(long handle);

  /**
   * number of files to trigger level-0 compaction. a value <0 means that
   * level-0 compaction will not be triggered by number of files at all.
   * default: 4
   *
   * @param numfiles the number of files in level-0 to trigger compaction.
   * @return the reference to the current option.
   */
  public options setlevelzerofilenumcompactiontrigger(
      int numfiles) {
    setlevelzerofilenumcompactiontrigger(
        nativehandle_, numfiles);
    return this;
  }
  private native void setlevelzerofilenumcompactiontrigger(
      long handle, int numfiles);

  /**
   * soft limit on the number of level-0 files. we start slowing down writes
   * at this point. a value < 0 means that no writing slow down will be
   * triggered by number of files in level-0.
   *
   * @return the soft limit on the number of level-0 files.
   */
  public int levelzeroslowdownwritestrigger() {
    return levelzeroslowdownwritestrigger(nativehandle_);
  }
  private native int levelzeroslowdownwritestrigger(long handle);

  /**
   * soft limit on number of level-0 files. we start slowing down writes at this
   * point. a value <0 means that no writing slow down will be triggered by
   * number of files in level-0.
   *
   * @param numfiles soft limit on number of level-0 files.
   * @return the reference to the current option.
   */
  public options setlevelzeroslowdownwritestrigger(
      int numfiles) {
    setlevelzeroslowdownwritestrigger(nativehandle_, numfiles);
    return this;
  }
  private native void setlevelzeroslowdownwritestrigger(
      long handle, int numfiles);

  /**
   * maximum number of level-0 files.  we stop writes at this point.
   *
   * @return the hard limit of the number of level-0 file.
   */
  public int levelzerostopwritestrigger() {
    return levelzerostopwritestrigger(nativehandle_);
  }
  private native int levelzerostopwritestrigger(long handle);

  /**
   * maximum number of level-0 files.  we stop writes at this point.
   *
   * @param numfiles the hard limit of the number of level-0 files.
   * @return the reference to the current option.
   */
  public options setlevelzerostopwritestrigger(int numfiles) {
    setlevelzerostopwritestrigger(nativehandle_, numfiles);
    return this;
  }
  private native void setlevelzerostopwritestrigger(
      long handle, int numfiles);

  /**
   * the highest level to which a new compacted memtable is pushed if it
   * does not create overlap.  we try to push to level 2 to avoid the
   * relatively expensive level 0=>1 compactions and to avoid some
   * expensive manifest file operations.  we do not push all the way to
   * the largest level since that can generate a lot of wasted disk
   * space if the same key space is being repeatedly overwritten.
   *
   * @return the highest level where a new compacted memtable will be pushed.
   */
  public int maxmemcompactionlevel() {
    return maxmemcompactionlevel(nativehandle_);
  }
  private native int maxmemcompactionlevel(long handle);

  /**
   * the highest level to which a new compacted memtable is pushed if it
   * does not create overlap.  we try to push to level 2 to avoid the
   * relatively expensive level 0=>1 compactions and to avoid some
   * expensive manifest file operations.  we do not push all the way to
   * the largest level since that can generate a lot of wasted disk
   * space if the same key space is being repeatedly overwritten.
   *
   * @param maxmemcompactionlevel the highest level to which a new compacted
   *     mem-table will be pushed.
   * @return the reference to the current option.
   */
  public options setmaxmemcompactionlevel(int maxmemcompactionlevel) {
    setmaxmemcompactionlevel(nativehandle_, maxmemcompactionlevel);
    return this;
  }
  private native void setmaxmemcompactionlevel(
      long handle, int maxmemcompactionlevel);

  /**
   * the target file size for compaction.
   * this targetfilesizebase determines a level-1 file size.
   * target file size for level l can be calculated by
   * targetfilesizebase * (targetfilesizemultiplier ^ (l-1))
   * for example, if targetfilesizebase is 2mb and
   * target_file_size_multiplier is 10, then each file on level-1 will
   * be 2mb, and each file on level 2 will be 20mb,
   * and each file on level-3 will be 200mb.
   * by default targetfilesizebase is 2mb.
   *
   * @return the target size of a level-0 file.
   *
   * @see targetfilesizemultiplier()
   */
  public int targetfilesizebase() {
    return targetfilesizebase(nativehandle_);
  }
  private native int targetfilesizebase(long handle);

  /**
   * the target file size for compaction.
   * this targetfilesizebase determines a level-1 file size.
   * target file size for level l can be calculated by
   * targetfilesizebase * (targetfilesizemultiplier ^ (l-1))
   * for example, if targetfilesizebase is 2mb and
   * target_file_size_multiplier is 10, then each file on level-1 will
   * be 2mb, and each file on level 2 will be 20mb,
   * and each file on level-3 will be 200mb.
   * by default targetfilesizebase is 2mb.
   *
   * @param targetfilesizebase the target size of a level-0 file.
   * @return the reference to the current option.
   *
   * @see settargetfilesizemultiplier()
   */
  public options settargetfilesizebase(int targetfilesizebase) {
    settargetfilesizebase(nativehandle_, targetfilesizebase);
    return this;
  }
  private native void settargetfilesizebase(
      long handle, int targetfilesizebase);

  /**
   * targetfilesizemultiplier defines the size ratio between a
   * level-(l+1) file and level-l file.
   * by default targetfilesizemultiplier is 1, meaning
   * files in different levels have the same target.
   *
   * @return the size ratio between a level-(l+1) file and level-l file.
   */
  public int targetfilesizemultiplier() {
    return targetfilesizemultiplier(nativehandle_);
  }
  private native int targetfilesizemultiplier(long handle);

  /**
   * targetfilesizemultiplier defines the size ratio between a
   * level-l file and level-(l+1) file.
   * by default target_file_size_multiplier is 1, meaning
   * files in different levels have the same target.
   *
   * @param multiplier the size ratio between a level-(l+1) file
   *     and level-l file.
   * @return the reference to the current option.
   */
  public options settargetfilesizemultiplier(int multiplier) {
    settargetfilesizemultiplier(nativehandle_, multiplier);
    return this;
  }
  private native void settargetfilesizemultiplier(
      long handle, int multiplier);

  /**
   * the upper-bound of the total size of level-1 files in bytes.
   * maximum number of bytes for level l can be calculated as
   * (maxbytesforlevelbase) * (maxbytesforlevelmultiplier ^ (l-1))
   * for example, if maxbytesforlevelbase is 20mb, and if
   * max_bytes_for_level_multiplier is 10, total data size for level-1
   * will be 20mb, total file size for level-2 will be 200mb,
   * and total file size for level-3 will be 2gb.
   * by default 'maxbytesforlevelbase' is 10mb.
   *
   * @return the upper-bound of the total size of leve-1 files in bytes.
   * @see maxbytesforlevelmultiplier()
   */
  public long maxbytesforlevelbase() {
    return maxbytesforlevelbase(nativehandle_);
  }
  private native long maxbytesforlevelbase(long handle);

  /**
   * the upper-bound of the total size of level-1 files in bytes.
   * maximum number of bytes for level l can be calculated as
   * (maxbytesforlevelbase) * (maxbytesforlevelmultiplier ^ (l-1))
   * for example, if maxbytesforlevelbase is 20mb, and if
   * max_bytes_for_level_multiplier is 10, total data size for level-1
   * will be 20mb, total file size for level-2 will be 200mb,
   * and total file size for level-3 will be 2gb.
   * by default 'maxbytesforlevelbase' is 10mb.
   *
   * @return maxbytesforlevelbase the upper-bound of the total size of
   *     leve-1 files in bytes.
   * @return the reference to the current option.
   * @see setmaxbytesforlevelmultiplier()
   */
  public options setmaxbytesforlevelbase(long maxbytesforlevelbase) {
    setmaxbytesforlevelbase(nativehandle_, maxbytesforlevelbase);
    return this;
  }
  private native void setmaxbytesforlevelbase(
      long handle, long maxbytesforlevelbase);

  /**
   * the ratio between the total size of level-(l+1) files and the total
   * size of level-l files for all l.
   * default: 10
   *
   * @return the ratio between the total size of level-(l+1) files and
   *     the total size of level-l files for all l.
   * @see maxbytesforlevelbase()
   */
  public int maxbytesforlevelmultiplier() {
    return maxbytesforlevelmultiplier(nativehandle_);
  }
  private native int maxbytesforlevelmultiplier(long handle);

  /**
   * the ratio between the total size of level-(l+1) files and the total
   * size of level-l files for all l.
   * default: 10
   *
   * @param multiplier the ratio between the total size of level-(l+1)
   *     files and the total size of level-l files for all l.
   * @return the reference to the current option.
   * @see setmaxbytesforlevelbase()
   */
  public options setmaxbytesforlevelmultiplier(int multiplier) {
    setmaxbytesforlevelmultiplier(nativehandle_, multiplier);
    return this;
  }
  private native void setmaxbytesforlevelmultiplier(
      long handle, int multiplier);

  /**
   * maximum number of bytes in all compacted files.  we avoid expanding
   * the lower level file set of a compaction if it would make the
   * total compaction cover more than
   * (expanded_compaction_factor * targetfilesizelevel()) many bytes.
   *
   * @return the maximum number of bytes in all compacted files.
   * @see sourcecompactionfactor()
   */
  public int expandedcompactionfactor() {
    return expandedcompactionfactor(nativehandle_);
  }
  private native int expandedcompactionfactor(long handle);

  /**
   * maximum number of bytes in all compacted files.  we avoid expanding
   * the lower level file set of a compaction if it would make the
   * total compaction cover more than
   * (expanded_compaction_factor * targetfilesizelevel()) many bytes.
   *
   * @param expandedcompactionfactor the maximum number of bytes in all
   *     compacted files.
   * @return the reference to the current option.
   * @see setsourcecompactionfactor()
   */
  public options setexpandedcompactionfactor(int expandedcompactionfactor) {
    setexpandedcompactionfactor(nativehandle_, expandedcompactionfactor);
    return this;
  }
  private native void setexpandedcompactionfactor(
      long handle, int expandedcompactionfactor);

  /**
   * maximum number of bytes in all source files to be compacted in a
   * single compaction run. we avoid picking too many files in the
   * source level so that we do not exceed the total source bytes
   * for compaction to exceed
   * (source_compaction_factor * targetfilesizelevel()) many bytes.
   * default:1, i.e. pick maxfilesize amount of data as the source of
   * a compaction.
   *
   * @return the maximum number of bytes in all source files to be compactedo.
   * @see expendedcompactionfactor()
   */
  public int sourcecompactionfactor() {
    return sourcecompactionfactor(nativehandle_);
  }
  private native int sourcecompactionfactor(long handle);

  /**
   * maximum number of bytes in all source files to be compacted in a
   * single compaction run. we avoid picking too many files in the
   * source level so that we do not exceed the total source bytes
   * for compaction to exceed
   * (source_compaction_factor * targetfilesizelevel()) many bytes.
   * default:1, i.e. pick maxfilesize amount of data as the source of
   * a compaction.
   *
   * @param sourcecompactionfactor the maximum number of bytes in all
   *     source files to be compacted in a single compaction run.
   * @return the reference to the current option.
   * @see setexpendedcompactionfactor()
   */
  public options setsourcecompactionfactor(int sourcecompactionfactor) {
    setsourcecompactionfactor(nativehandle_, sourcecompactionfactor);
    return this;
  }
  private native void setsourcecompactionfactor(
      long handle, int sourcecompactionfactor);

  /**
   * control maximum bytes of overlaps in grandparent (i.e., level+2) before we
   * stop building a single file in a level->level+1 compaction.
   *
   * @return maximum bytes of overlaps in "grandparent" level.
   */
  public int maxgrandparentoverlapfactor() {
    return maxgrandparentoverlapfactor(nativehandle_);
  }
  private native int maxgrandparentoverlapfactor(long handle);

  /**
   * control maximum bytes of overlaps in grandparent (i.e., level+2) before we
   * stop building a single file in a level->level+1 compaction.
   *
   * @param maxgrandparentoverlapfactor maximum bytes of overlaps in
   *     "grandparent" level.
   * @return the reference to the current option.
   */
  public options setmaxgrandparentoverlapfactor(
      int maxgrandparentoverlapfactor) {
    setmaxgrandparentoverlapfactor(nativehandle_, maxgrandparentoverlapfactor);
    return this;
  }
  private native void setmaxgrandparentoverlapfactor(
      long handle, int maxgrandparentoverlapfactor);

  /**
   * puts are delayed 0-1 ms when any level has a compaction score that exceeds
   * soft_rate_limit. this is ignored when == 0.0.
   * constraint: soft_rate_limit <= hard_rate_limit. if this constraint does not
   * hold, rocksdb will set soft_rate_limit = hard_rate_limit
   * default: 0 (disabled)
   *
   * @return soft-rate-limit for put delay.
   */
  public double softratelimit() {
    return softratelimit(nativehandle_);
  }
  private native double softratelimit(long handle);

  /**
   * puts are delayed 0-1 ms when any level has a compaction score that exceeds
   * soft_rate_limit. this is ignored when == 0.0.
   * constraint: soft_rate_limit <= hard_rate_limit. if this constraint does not
   * hold, rocksdb will set soft_rate_limit = hard_rate_limit
   * default: 0 (disabled)
   *
   * @param softratelimit the soft-rate-limit of a compaction score
   *     for put delay.
   * @return the reference to the current option.
   */
  public options setsoftratelimit(double softratelimit) {
    setsoftratelimit(nativehandle_, softratelimit);
    return this;
  }
  private native void setsoftratelimit(
      long handle, double softratelimit);

  /**
   * puts are delayed 1ms at a time when any level has a compaction score that
   * exceeds hard_rate_limit. this is ignored when <= 1.0.
   * default: 0 (disabled)
   *
   * @return the hard-rate-limit of a compaction score for put delay.
   */
  public double hardratelimit() {
    return hardratelimit(nativehandle_);
  }
  private native double hardratelimit(long handle);

  /**
   * puts are delayed 1ms at a time when any level has a compaction score that
   * exceeds hard_rate_limit. this is ignored when <= 1.0.
   * default: 0 (disabled)
   *
   * @param hardratelimit the hard-rate-limit of a compaction score for put
   *     delay.
   * @return the reference to the current option.
   */
  public options sethardratelimit(double hardratelimit) {
    sethardratelimit(nativehandle_, hardratelimit);
    return this;
  }
  private native void sethardratelimit(
      long handle, double hardratelimit);

  /**
   * the maximum time interval a put will be stalled when hard_rate_limit
   * is enforced.  if 0, then there is no limit.
   * default: 1000
   *
   * @return the maximum time interval a put will be stalled when
   *     hard_rate_limit is enforced.
   */
  public int ratelimitdelaymaxmilliseconds() {
    return ratelimitdelaymaxmilliseconds(nativehandle_);
  }
  private native int ratelimitdelaymaxmilliseconds(long handle);

  /**
   * the maximum time interval a put will be stalled when hard_rate_limit
   * is enforced. if 0, then there is no limit.
   * default: 1000
   *
   * @param ratelimitdelaymaxmilliseconds the maximum time interval a put
   *     will be stalled.
   * @return the reference to the current option.
   */
  public options setratelimitdelaymaxmilliseconds(
      int ratelimitdelaymaxmilliseconds) {
    setratelimitdelaymaxmilliseconds(
        nativehandle_, ratelimitdelaymaxmilliseconds);
    return this;
  }
  private native void setratelimitdelaymaxmilliseconds(
      long handle, int ratelimitdelaymaxmilliseconds);

  /**
   * the size of one block in arena memory allocation.
   * if <= 0, a proper value is automatically calculated (usually 1/10 of
   * writer_buffer_size).
   *
   * there are two additonal restriction of the the specified size:
   * (1) size should be in the range of [4096, 2 << 30] and
   * (2) be the multiple of the cpu word (which helps with the memory
   * alignment).
   *
   * we'll automatically check and adjust the size number to make sure it
   * conforms to the restrictions.
   * default: 0
   *
   * @return the size of an arena block
   */
  public long arenablocksize() {
    return arenablocksize(nativehandle_);
  }
  private native long arenablocksize(long handle);

  /**
   * the size of one block in arena memory allocation.
   * if <= 0, a proper value is automatically calculated (usually 1/10 of
   * writer_buffer_size).
   *
   * there are two additonal restriction of the the specified size:
   * (1) size should be in the range of [4096, 2 << 30] and
   * (2) be the multiple of the cpu word (which helps with the memory
   * alignment).
   *
   * we'll automatically check and adjust the size number to make sure it
   * conforms to the restrictions.
   * default: 0
   *
   * @param arenablocksize the size of an arena block
   * @return the reference to the current option.
   */
  public options setarenablocksize(long arenablocksize) {
    setarenablocksize(nativehandle_, arenablocksize);
    return this;
  }
  private native void setarenablocksize(
      long handle, long arenablocksize);

  /**
   * disable automatic compactions. manual compactions can still
   * be issued on this column family
   *
   * @return true if auto-compactions are disabled.
   */
  public boolean disableautocompactions() {
    return disableautocompactions(nativehandle_);
  }
  private native boolean disableautocompactions(long handle);

  /**
   * disable automatic compactions. manual compactions can still
   * be issued on this column family
   *
   * @param disableautocompactions true if auto-compactions are disabled.
   * @return the reference to the current option.
   */
  public options setdisableautocompactions(boolean disableautocompactions) {
    setdisableautocompactions(nativehandle_, disableautocompactions);
    return this;
  }
  private native void setdisableautocompactions(
      long handle, boolean disableautocompactions);

  /**
   * purge duplicate/deleted keys when a memtable is flushed to storage.
   * default: true
   *
   * @return true if purging keys is disabled.
   */
  public boolean purgeredundantkvswhileflush() {
    return purgeredundantkvswhileflush(nativehandle_);
  }
  private native boolean purgeredundantkvswhileflush(long handle);

  /**
   * purge duplicate/deleted keys when a memtable is flushed to storage.
   * default: true
   *
   * @param purgeredundantkvswhileflush true if purging keys is disabled.
   * @return the reference to the current option.
   */
  public options setpurgeredundantkvswhileflush(
      boolean purgeredundantkvswhileflush) {
    setpurgeredundantkvswhileflush(
        nativehandle_, purgeredundantkvswhileflush);
    return this;
  }
  private native void setpurgeredundantkvswhileflush(
      long handle, boolean purgeredundantkvswhileflush);

  /**
   * if true, compaction will verify checksum on every read that happens
   * as part of compaction
   * default: true
   *
   * @return true if compaction verifies checksum on every read.
   */
  public boolean verifychecksumsincompaction() {
    return verifychecksumsincompaction(nativehandle_);
  }
  private native boolean verifychecksumsincompaction(long handle);

  /**
   * if true, compaction will verify checksum on every read that happens
   * as part of compaction
   * default: true
   *
   * @param verifychecksumsincompaction true if compaction verifies
   *     checksum on every read.
   * @return the reference to the current option.
   */
  public options setverifychecksumsincompaction(
      boolean verifychecksumsincompaction) {
    setverifychecksumsincompaction(
        nativehandle_, verifychecksumsincompaction);
    return this;
  }
  private native void setverifychecksumsincompaction(
      long handle, boolean verifychecksumsincompaction);

  /**
   * use keymayexist api to filter deletes when this is true.
   * if keymayexist returns false, i.e. the key definitely does not exist, then
   * the delete is a noop. keymayexist only incurs in-memory look up.
   * this optimization avoids writing the delete to storage when appropriate.
   * default: false
   *
   * @return true if filter-deletes behavior is on.
   */
  public boolean filterdeletes() {
    return filterdeletes(nativehandle_);
  }
  private native boolean filterdeletes(long handle);

  /**
   * use keymayexist api to filter deletes when this is true.
   * if keymayexist returns false, i.e. the key definitely does not exist, then
   * the delete is a noop. keymayexist only incurs in-memory look up.
   * this optimization avoids writing the delete to storage when appropriate.
   * default: false
   *
   * @param filterdeletes true if filter-deletes behavior is on.
   * @return the reference to the current option.
   */
  public options setfilterdeletes(boolean filterdeletes) {
    setfilterdeletes(nativehandle_, filterdeletes);
    return this;
  }
  private native void setfilterdeletes(
      long handle, boolean filterdeletes);

  /**
   * an iteration->next() sequentially skips over keys with the same
   * user-key unless this option is set. this number specifies the number
   * of keys (with the same userkey) that will be sequentially
   * skipped before a reseek is issued.
   * default: 8
   *
   * @return the number of keys could be skipped in a iteration.
   */
  public long maxsequentialskipiniterations() {
    return maxsequentialskipiniterations(nativehandle_);
  }
  private native long maxsequentialskipiniterations(long handle);

  /**
   * an iteration->next() sequentially skips over keys with the same
   * user-key unless this option is set. this number specifies the number
   * of keys (with the same userkey) that will be sequentially
   * skipped before a reseek is issued.
   * default: 8
   *
   * @param maxsequentialskipiniterations the number of keys could
   *     be skipped in a iteration.
   * @return the reference to the current option.
   */
  public options setmaxsequentialskipiniterations(long maxsequentialskipiniterations) {
    setmaxsequentialskipiniterations(nativehandle_, maxsequentialskipiniterations);
    return this;
  }
  private native void setmaxsequentialskipiniterations(
      long handle, long maxsequentialskipiniterations);

  /**
   * allows thread-safe inplace updates.
   * if inplace_callback function is not set,
   *   put(key, new_value) will update inplace the existing_value iff
   *   * key exists in current memtable
   *   * new sizeof(new_value) <= sizeof(existing_value)
   *   * existing_value for that key is a put i.e. ktypevalue
   * if inplace_callback function is set, check doc for inplace_callback.
   * default: false.
   *
   * @return true if thread-safe inplace updates are allowed.
   */
  public boolean inplaceupdatesupport() {
    return inplaceupdatesupport(nativehandle_);
  }
  private native boolean inplaceupdatesupport(long handle);

  /**
   * allows thread-safe inplace updates.
   * if inplace_callback function is not set,
   *   put(key, new_value) will update inplace the existing_value iff
   *   * key exists in current memtable
   *   * new sizeof(new_value) <= sizeof(existing_value)
   *   * existing_value for that key is a put i.e. ktypevalue
   * if inplace_callback function is set, check doc for inplace_callback.
   * default: false.
   *
   * @param inplaceupdatesupport true if thread-safe inplace updates
   *     are allowed.
   * @return the reference to the current option.
   */
  public options setinplaceupdatesupport(boolean inplaceupdatesupport) {
    setinplaceupdatesupport(nativehandle_, inplaceupdatesupport);
    return this;
  }
  private native void setinplaceupdatesupport(
      long handle, boolean inplaceupdatesupport);

  /**
   * number of locks used for inplace update
   * default: 10000, if inplace_update_support = true, else 0.
   *
   * @return the number of locks used for inplace update.
   */
  public long inplaceupdatenumlocks() {
    return inplaceupdatenumlocks(nativehandle_);
  }
  private native long inplaceupdatenumlocks(long handle);

  /**
   * number of locks used for inplace update
   * default: 10000, if inplace_update_support = true, else 0.
   *
   * @param inplaceupdatenumlocks the number of locks used for
   *     inplace updates.
   * @return the reference to the current option.
   */
  public options setinplaceupdatenumlocks(long inplaceupdatenumlocks) {
    setinplaceupdatenumlocks(nativehandle_, inplaceupdatenumlocks);
    return this;
  }
  private native void setinplaceupdatenumlocks(
      long handle, long inplaceupdatenumlocks);

  /**
   * returns the number of bits used in the prefix bloom filter.
   *
   * this value will be used only when a prefix-extractor is specified.
   *
   * @return the number of bloom-bits.
   * @see usefixedlengthprefixextractor()
   */
  public int memtableprefixbloombits() {
    return memtableprefixbloombits(nativehandle_);
  }
  private native int memtableprefixbloombits(long handle);

  /**
   * sets the number of bits used in the prefix bloom filter.
   *
   * this value will be used only when a prefix-extractor is specified.
   *
   * @param memtableprefixbloombits the number of bits used in the
   *     prefix bloom filter.
   * @return the reference to the current option.
   */
  public options setmemtableprefixbloombits(int memtableprefixbloombits) {
    setmemtableprefixbloombits(nativehandle_, memtableprefixbloombits);
    return this;
  }
  private native void setmemtableprefixbloombits(
      long handle, int memtableprefixbloombits);

  /**
   * the number of hash probes per key used in the mem-table.
   *
   * @return the number of hash probes per key.
   */
  public int memtableprefixbloomprobes() {
    return memtableprefixbloomprobes(nativehandle_);
  }
  private native int memtableprefixbloomprobes(long handle);

  /**
   * the number of hash probes per key used in the mem-table.
   *
   * @param memtableprefixbloomprobes the number of hash probes per key.
   * @return the reference to the current option.
   */
  public options setmemtableprefixbloomprobes(int memtableprefixbloomprobes) {
    setmemtableprefixbloomprobes(nativehandle_, memtableprefixbloomprobes);
    return this;
  }
  private native void setmemtableprefixbloomprobes(
      long handle, int memtableprefixbloomprobes);

  /**
   * control locality of bloom filter probes to improve cache miss rate.
   * this option only applies to memtable prefix bloom and plaintable
   * prefix bloom. it essentially limits the max number of cache lines each
   * bloom filter check can touch.
   * this optimization is turned off when set to 0. the number should never
   * be greater than number of probes. this option can boost performance
   * for in-memory workload but should use with care since it can cause
   * higher false positive rate.
   * default: 0
   *
   * @return the level of locality of bloom-filter probes.
   * @see setmemtableprefixbloomprobes
   */
  public int bloomlocality() {
    return bloomlocality(nativehandle_);
  }
  private native int bloomlocality(long handle);

  /**
   * control locality of bloom filter probes to improve cache miss rate.
   * this option only applies to memtable prefix bloom and plaintable
   * prefix bloom. it essentially limits the max number of cache lines each
   * bloom filter check can touch.
   * this optimization is turned off when set to 0. the number should never
   * be greater than number of probes. this option can boost performance
   * for in-memory workload but should use with care since it can cause
   * higher false positive rate.
   * default: 0
   *
   * @param bloomlocality the level of locality of bloom-filter probes.
   * @return the reference to the current option.
   */
  public options setbloomlocality(int bloomlocality) {
    setbloomlocality(nativehandle_, bloomlocality);
    return this;
  }
  private native void setbloomlocality(
      long handle, int bloomlocality);

  /**
   * maximum number of successive merge operations on a key in the memtable.
   *
   * when a merge operation is added to the memtable and the maximum number of
   * successive merges is reached, the value of the key will be calculated and
   * inserted into the memtable instead of the merge operation. this will
   * ensure that there are never more than max_successive_merges merge
   * operations in the memtable.
   *
   * default: 0 (disabled)
   *
   * @return the maximum number of successive merges.
   */
  public long maxsuccessivemerges() {
    return maxsuccessivemerges(nativehandle_);
  }
  private native long maxsuccessivemerges(long handle);

  /**
   * maximum number of successive merge operations on a key in the memtable.
   *
   * when a merge operation is added to the memtable and the maximum number of
   * successive merges is reached, the value of the key will be calculated and
   * inserted into the memtable instead of the merge operation. this will
   * ensure that there are never more than max_successive_merges merge
   * operations in the memtable.
   *
   * default: 0 (disabled)
   *
   * @param maxsuccessivemerges the maximum number of successive merges.
   * @return the reference to the current option.
   */
  public options setmaxsuccessivemerges(long maxsuccessivemerges) {
    setmaxsuccessivemerges(nativehandle_, maxsuccessivemerges);
    return this;
  }
  private native void setmaxsuccessivemerges(
      long handle, long maxsuccessivemerges);

  /**
   * the minimum number of write buffers that will be merged together
   * before writing to storage.  if set to 1, then
   * all write buffers are fushed to l0 as individual files and this increases
   * read amplification because a get request has to check in all of these
   * files. also, an in-memory merge may result in writing lesser
   * data to storage if there are duplicate records in each of these
   * individual write buffers.  default: 1
   *
   * @return the minimum number of write buffers that will be merged together.
   */
  public int minwritebuffernumbertomerge() {
    return minwritebuffernumbertomerge(nativehandle_);
  }
  private native int minwritebuffernumbertomerge(long handle);

  /**
   * the minimum number of write buffers that will be merged together
   * before writing to storage.  if set to 1, then
   * all write buffers are fushed to l0 as individual files and this increases
   * read amplification because a get request has to check in all of these
   * files. also, an in-memory merge may result in writing lesser
   * data to storage if there are duplicate records in each of these
   * individual write buffers.  default: 1
   *
   * @param minwritebuffernumbertomerge the minimum number of write buffers
   *     that will be merged together.
   * @return the reference to the current option.
   */
  public options setminwritebuffernumbertomerge(int minwritebuffernumbertomerge) {
    setminwritebuffernumbertomerge(nativehandle_, minwritebuffernumbertomerge);
    return this;
  }
  private native void setminwritebuffernumbertomerge(
      long handle, int minwritebuffernumbertomerge);

  /**
   * the number of partial merge operands to accumulate before partial
   * merge will be performed. partial merge will not be called
   * if the list of values to merge is less than min_partial_merge_operands.
   *
   * if min_partial_merge_operands < 2, then it will be treated as 2.
   *
   * default: 2
   *
   * @return
   */
  public int minpartialmergeoperands() {
    return minpartialmergeoperands(nativehandle_);
  }
  private native int minpartialmergeoperands(long handle);

  /**
   * the number of partial merge operands to accumulate before partial
   * merge will be performed. partial merge will not be called
   * if the list of values to merge is less than min_partial_merge_operands.
   *
   * if min_partial_merge_operands < 2, then it will be treated as 2.
   *
   * default: 2
   *
   * @param minpartialmergeoperands
   * @return the reference to the current option.
   */
  public options setminpartialmergeoperands(int minpartialmergeoperands) {
    setminpartialmergeoperands(nativehandle_, minpartialmergeoperands);
    return this;
  }
  private native void setminpartialmergeoperands(
      long handle, int minpartialmergeoperands);

  /**
   * release the memory allocated for the current instance
   * in the c++ side.
   */
  @override protected void disposeinternal() {
    assert(isinitialized());
    disposeinternal(nativehandle_);
  }

  static final int default_plain_table_bloom_bits_per_key = 10;
  static final double default_plain_table_hash_table_ratio = 0.75;
  static final int default_plain_table_index_sparseness = 16;

  private native void newoptions();
  private native void disposeinternal(long handle);
  private native void setcreateifmissing(long handle, boolean flag);
  private native boolean createifmissing(long handle);
  private native void setwritebuffersize(long handle, long writebuffersize);
  private native long writebuffersize(long handle);
  private native void setmaxwritebuffernumber(
      long handle, int maxwritebuffernumber);
  private native int maxwritebuffernumber(long handle);
  private native void setmaxbackgroundcompactions(
      long handle, int maxbackgroundcompactions);
  private native int maxbackgroundcompactions(long handle);
  private native void createstatistics(long opthandle);
  private native long statisticsptr(long opthandle);

  private native void setmemtablefactory(long handle, long factoryhandle);
  private native string memtablefactoryname(long handle);

  private native void settablefactory(long handle, long factoryhandle);
  private native string tablefactoryname(long handle);

  private native void usefixedlengthprefixextractor(
      long handle, int prefixlength);

  long cachesize_;
  int numshardbits_;
  rocksenv env_;
}
