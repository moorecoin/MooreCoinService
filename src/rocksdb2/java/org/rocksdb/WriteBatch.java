// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

import java.util.*;

/**
 * writebatch holds a collection of updates to apply atomically to a db.
 *
 * the updates are applied in the order in which they are added
 * to the writebatch.  for example, the value of "key" will be "v3"
 * after the following batch is written:
 *
 *    batch.put("key", "v1");
 *    batch.remove("key");
 *    batch.put("key", "v2");
 *    batch.put("key", "v3");
 *
 * multiple threads can invoke const methods on a writebatch without
 * external synchronization, but if any of the threads may call a
 * non-const method, all threads accessing the same writebatch must use
 * external synchronization.
 */
public class writebatch extends rocksobject {
  public writebatch() {
    super();
    newwritebatch(0);
  }

  public writebatch(int reserved_bytes) {
    nativehandle_ = 0;
    newwritebatch(reserved_bytes);
  }

  /**
   * returns the number of updates in the batch.
   */
  public native int count();

  /**
   * store the mapping "key->value" in the database.
   */
  public void put(byte[] key, byte[] value) {
    put(key, key.length, value, value.length);
  }

  /**
   * merge "value" with the existing value of "key" in the database.
   * "key->merge(existing, value)"
   */
  public void merge(byte[] key, byte[] value) {
    merge(key, key.length, value, value.length);
  }

  /**
   * if the database contains a mapping for "key", erase it.  else do nothing.
   */
  public void remove(byte[] key) {
    remove(key, key.length);
  }

  /**
   * append a blob of arbitrary size to the records in this batch. the blob will
   * be stored in the transaction log but not in any other file. in particular,
   * it will not be persisted to the sst files. when iterating over this
   * writebatch, writebatch::handler::logdata will be called with the contents
   * of the blob as it is encountered. blobs, puts, deletes, and merges will be
   * encountered in the same order in thich they were inserted. the blob will
   * not consume sequence number(s) and will not increase the count of the batch
   *
   * example application: add timestamps to the transaction log for use in
   * replication.
   */
  public void putlogdata(byte[] blob) {
    putlogdata(blob, blob.length);
  }

  /**
   * clear all updates buffered in this batch
   */
  public native void clear();

  /**
   * delete the c++ side pointer.
   */
  @override protected void disposeinternal() {
    assert(isinitialized());
    disposeinternal(nativehandle_);
  }

  private native void newwritebatch(int reserved_bytes);
  private native void put(byte[] key, int keylen,
                          byte[] value, int valuelen);
  private native void merge(byte[] key, int keylen,
                            byte[] value, int valuelen);
  private native void remove(byte[] key, int keylen);
  private native void putlogdata(byte[] blob, int bloblen);
  private native void disposeinternal(long handle);
}

/**
 * package-private class which provides java api to access
 * c++ writebatchinternal.
 */
class writebatchinternal {
  static native void setsequence(writebatch batch, long sn);
  static native long sequence(writebatch batch);
  static native void append(writebatch b1, writebatch b2);
}
