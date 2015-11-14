// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

import java.util.list;
import java.util.map;
import java.util.hashmap;
import java.io.closeable;
import java.io.ioexception;
import org.rocksdb.util.environment;

/**
 * a rocksdb is a persistent ordered map from keys to values.  it is safe for
 * concurrent access from multiple threads without any external synchronization.
 * all methods of this class could potentially throw rocksdbexception, which
 * indicates sth wrong at the rocksdb library side and the call failed.
 */
public class rocksdb extends rocksobject {
  public static final int not_found = -1;
  private static final string[] compressionlibs_ = {
      "snappy", "z", "bzip2", "lz4", "lz4hc"};

  /**
   * loads the necessary library files.
   * calling this method twice will have no effect.
   */
  public static synchronized void loadlibrary() {
    // loading possibly necessary libraries.
    for (string lib : compressionlibs_) {
      try {
      system.loadlibrary(lib);
      } catch (unsatisfiedlinkerror e) {
        // since it may be optional, we ignore its loading failure here.
      }
    }
    // however, if any of them is required.  we will see error here.
    system.loadlibrary("rocksdbjni");
  }

  /**
   * tries to load the necessary library files from the given list of
   * directories.
   *
   * @param paths a list of strings where each describes a directory
   *     of a library.
   */
  public static synchronized void loadlibrary(list<string> paths) {
    for (string lib : compressionlibs_) {
      for (string path : paths) {
        try {
          system.load(path + "/" + environment.getsharedlibraryname(lib));
          break;
        } catch (unsatisfiedlinkerror e) {
          // since they are optional, we ignore loading fails.
        }
      }
    }
    boolean success = false;
    unsatisfiedlinkerror err = null;
    for (string path : paths) {
      try {
        system.load(path + "/" + environment.getjnilibraryname("rocksdbjni"));
        success = true;
        break;
      } catch (unsatisfiedlinkerror e) {
        err = e;
      }
    }
    if (success == false) {
      throw err;
    }
  }

  /**
   * the factory constructor of rocksdb that opens a rocksdb instance given
   * the path to the database using the default options w/ createifmissing
   * set to true.
   *
   * @param path the path to the rocksdb.
   * @param status an out value indicating the status of the open().
   * @return a rocksdb instance on success, null if the specified rocksdb can
   *     not be opened.
   *
   * @see options.setcreateifmissing()
   * @see options.createifmissing()
   */
  public static rocksdb open(string path) throws rocksdbexception {
    rocksdb db = new rocksdb();

    // this allows to use the rocksjni default options instead of
    // the c++ one.
    options options = new options();
    return open(options, path);
  }

  /**
   * the factory constructor of rocksdb that opens a rocksdb instance given
   * the path to the database using the specified options and db path.
   *
   * options instance *should* not be disposed before all dbs using this options
   * instance have been closed. if user doesn't call options dispose explicitly,
   * then this options instance will be gc'd automatically.
   *
   * options instance can be re-used to open multiple dbs if db statistics is
   * not used. if db statistics are required, then its recommended to open db
   * with new options instance as underlying native statistics instance does not
   * use any locks to prevent concurrent updates.
   */
  public static rocksdb open(options options, string path)
      throws rocksdbexception {
    // when non-default options is used, keeping an options reference
    // in rocksdb can prevent java to gc during the life-time of
    // the currently-created rocksdb.
    rocksdb db = new rocksdb();
    db.open(options.nativehandle_, path);

    db.storeoptionsinstance(options);
    return db;
  }

  private void storeoptionsinstance(options options) {
    options_ = options;
  }

  @override protected void disposeinternal() {
    assert(isinitialized());
    disposeinternal(nativehandle_);
  }

  /**
   * close the rocksdb instance.
   * this function is equivalent to dispose().
   */
  public void close() {
    dispose();
  }

  /**
   * set the database entry for "key" to "value".
   *
   * @param key the specified key to be inserted.
   * @param value the value associated with the specified key.
   */
  public void put(byte[] key, byte[] value) throws rocksdbexception {
    put(nativehandle_, key, key.length, value, value.length);
  }

  /**
   * set the database entry for "key" to "value".
   *
   * @param key the specified key to be inserted.
   * @param value the value associated with the specified key.
   */
  public void put(writeoptions writeopts, byte[] key, byte[] value)
      throws rocksdbexception {
    put(nativehandle_, writeopts.nativehandle_,
        key, key.length, value, value.length);
  }

  /**
   * apply the specified updates to the database.
   */
  public void write(writeoptions writeopts, writebatch updates)
      throws rocksdbexception {
    write(writeopts.nativehandle_, updates.nativehandle_);
  }

  /**
   * get the value associated with the specified key.
   *
   * @param key the key to retrieve the value.
   * @param value the out-value to receive the retrieved value.
   * @return the size of the actual value that matches the specified
   *     {@code key} in byte.  if the return value is greater than the
   *     length of {@code value}, then it indicates that the size of the
   *     input buffer {@code value} is insufficient and partial result will
   *     be returned.  rocksdb.not_found will be returned if the value not
   *     found.
   */
  public int get(byte[] key, byte[] value) throws rocksdbexception {
    return get(nativehandle_, key, key.length, value, value.length);
  }

  /**
   * get the value associated with the specified key.
   *
   * @param key the key to retrieve the value.
   * @param value the out-value to receive the retrieved value.
   * @return the size of the actual value that matches the specified
   *     {@code key} in byte.  if the return value is greater than the
   *     length of {@code value}, then it indicates that the size of the
   *     input buffer {@code value} is insufficient and partial result will
   *     be returned.  rocksdb.not_found will be returned if the value not
   *     found.
   */
  public int get(readoptions opt, byte[] key, byte[] value)
      throws rocksdbexception {
    return get(nativehandle_, opt.nativehandle_,
               key, key.length, value, value.length);
  }

  /**
   * the simplified version of get which returns a new byte array storing
   * the value associated with the specified input key if any.  null will be
   * returned if the specified key is not found.
   *
   * @param key the key retrieve the value.
   * @return a byte array storing the value associated with the input key if
   *     any.  null if it does not find the specified key.
   *
   * @see rocksdbexception
   */
  public byte[] get(byte[] key) throws rocksdbexception {
    return get(nativehandle_, key, key.length);
  }

  /**
   * the simplified version of get which returns a new byte array storing
   * the value associated with the specified input key if any.  null will be
   * returned if the specified key is not found.
   *
   * @param key the key retrieve the value.
   * @param opt read options.
   * @return a byte array storing the value associated with the input key if
   *     any.  null if it does not find the specified key.
   *
   * @see rocksdbexception
   */
  public byte[] get(readoptions opt, byte[] key) throws rocksdbexception {
    return get(nativehandle_, opt.nativehandle_, key, key.length);
  }

  /**
   * returns a map of keys for which values were found in db.
   *
   * @param keys list of keys for which values need to be retrieved.
   * @return map where key of map is the key passed by user and value for map
   * entry is the corresponding value in db.
   *
   * @see rocksdbexception
   */
  public map<byte[], byte[]> multiget(list<byte[]> keys)
      throws rocksdbexception {
    assert(keys.size() != 0);

    list<byte[]> values = multiget(
        nativehandle_, keys, keys.size());

    map<byte[], byte[]> keyvaluemap = new hashmap<byte[], byte[]>();
    for(int i = 0; i < values.size(); i++) {
      if(values.get(i) == null) {
        continue;
      }

      keyvaluemap.put(keys.get(i), values.get(i));
    }

    return keyvaluemap;
  }


  /**
   * returns a map of keys for which values were found in db.
   *
   * @param list of keys for which values need to be retrieved.
   * @param opt read options.
   * @return map where key of map is the key passed by user and value for map
   * entry is the corresponding value in db.
   *
   * @see rocksdbexception
   */
  public map<byte[], byte[]> multiget(readoptions opt, list<byte[]> keys)
      throws rocksdbexception {
    assert(keys.size() != 0);

    list<byte[]> values = multiget(
        nativehandle_, opt.nativehandle_, keys, keys.size());

    map<byte[], byte[]> keyvaluemap = new hashmap<byte[], byte[]>();
    for(int i = 0; i < values.size(); i++) {
      if(values.get(i) == null) {
        continue;
      }

      keyvaluemap.put(keys.get(i), values.get(i));
    }

    return keyvaluemap;
  }

  /**
   * remove the database entry (if any) for "key".  returns ok on
   * success, and a non-ok status on error.  it is not an error if "key"
   * did not exist in the database.
   */
  public void remove(byte[] key) throws rocksdbexception {
    remove(nativehandle_, key, key.length);
  }

  /**
   * remove the database entry (if any) for "key".  returns ok on
   * success, and a non-ok status on error.  it is not an error if "key"
   * did not exist in the database.
   */
  public void remove(writeoptions writeopt, byte[] key)
      throws rocksdbexception {
    remove(nativehandle_, writeopt.nativehandle_, key, key.length);
  }

  /**
   * return a heap-allocated iterator over the contents of the database.
   * the result of newiterator() is initially invalid (caller must
   * call one of the seek methods on the iterator before using it).
   *
   * caller should close the iterator when it is no longer needed.
   * the returned iterator should be closed before this db is closed.
   *
   * @return instance of iterator object.
   */
  public rocksiterator newiterator() {
    return new rocksiterator(iterator0(nativehandle_));
  }

  /**
   * private constructor.
   */
  protected rocksdb() {
    super();
  }

  // native methods
  protected native void open(
      long optionshandle, string path) throws rocksdbexception;
  protected native void put(
      long handle, byte[] key, int keylen,
      byte[] value, int valuelen) throws rocksdbexception;
  protected native void put(
      long handle, long writeopthandle,
      byte[] key, int keylen,
      byte[] value, int valuelen) throws rocksdbexception;
  protected native void write(
      long writeopthandle, long batchhandle) throws rocksdbexception;
  protected native int get(
      long handle, byte[] key, int keylen,
      byte[] value, int valuelen) throws rocksdbexception;
  protected native int get(
      long handle, long readopthandle, byte[] key, int keylen,
      byte[] value, int valuelen) throws rocksdbexception;
  protected native list<byte[]> multiget(
      long dbhandle, list<byte[]> keys, int keyscount);
  protected native list<byte[]> multiget(
      long dbhandle, long ropthandle, list<byte[]> keys, int keyscount);
  protected native byte[] get(
      long handle, byte[] key, int keylen) throws rocksdbexception;
  protected native byte[] get(
      long handle, long readopthandle,
      byte[] key, int keylen) throws rocksdbexception;
  protected native void remove(
      long handle, byte[] key, int keylen) throws rocksdbexception;
  protected native void remove(
      long handle, long writeopthandle,
      byte[] key, int keylen) throws rocksdbexception;
  protected native long iterator0(long opthandle);
  private native void disposeinternal(long handle);

  protected options options_;
}
