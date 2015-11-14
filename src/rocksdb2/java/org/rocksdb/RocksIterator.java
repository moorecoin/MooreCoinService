// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

/**
 * an iterator yields a sequence of key/value pairs from a source.
 * the following class defines the interface.  multiple implementations
 * are provided by this library.  in particular, iterators are provided
 * to access the contents of a table or a db.
 *
 * multiple threads can invoke const methods on an rocksiterator without
 * external synchronization, but if any of the threads may call a
 * non-const method, all threads accessing the same rocksiterator must use
 * external synchronization.
 */
public class rocksiterator extends rocksobject {
  public rocksiterator(long nativehandle) {
    super();
    nativehandle_ = nativehandle;
  }

  /**
   * an iterator is either positioned at a key/value pair, or
   * not valid.  this method returns true iff the iterator is valid.
   * @return true if iterator is valid.
   */
  public boolean isvalid() {
    assert(isinitialized());
    return isvalid0(nativehandle_);
  }

  /**
   * position at the first key in the source.  the iterator is valid()
   * after this call iff the source is not empty.
   */
  public void seektofirst() {
    assert(isinitialized());
    seektofirst0(nativehandle_);
  }

  /**
   * position at the last key in the source.  the iterator is
   * valid() after this call iff the source is not empty.
   */
  public void seektolast() {
    assert(isinitialized());
    seektolast0(nativehandle_);
  }

  /**
   * moves to the next entry in the source.  after this call, valid() is
   * true iff the iterator was not positioned at the last entry in the source.
   * requires: valid()
   */
  public void next() {
    assert(isinitialized());
    next0(nativehandle_);
  }

  /**
   * moves to the previous entry in the source.  after this call, valid() is
   * true iff the iterator was not positioned at the first entry in source.
   * requires: valid()
   */
  public void prev() {
    assert(isinitialized());
    prev0(nativehandle_);
  }

  /**
   * return the key for the current entry.  the underlying storage for
   * the returned slice is valid only until the next modification of
   * the iterator.
   * requires: valid()
   * @return key for the current entry.
   */
  public byte[] key() {
    assert(isinitialized());
    return key0(nativehandle_);
  }

  /**
   * return the value for the current entry.  the underlying storage for
   * the returned slice is valid only until the next modification of
   * the iterator.
   * requires: !atend() && !atstart()
   * @return value for the current entry.
   */
  public byte[] value() {
    assert(isinitialized());
    return value0(nativehandle_);
  }

  /**
   * position at the first key in the source that at or past target
   * the iterator is valid() after this call iff the source contains
   * an entry that comes at or past target.
   */
  public void seek(byte[] target) {
    assert(isinitialized());
    seek0(nativehandle_, target, target.length);
  }

  /**
   * if an error has occurred, return it.  else return an ok status.
   * if non-blocking io is requested and this operation cannot be
   * satisfied without doing some io, then this returns status::incomplete().
   *
   */
  public void status() throws rocksdbexception {
    assert(isinitialized());
    status0(nativehandle_);
  }

  /**
   * deletes underlying c++ iterator pointer.
   */
  @override protected void disposeinternal() {
    assert(isinitialized());
    disposeinternal(nativehandle_);
  }

  private native boolean isvalid0(long handle);
  private native void disposeinternal(long handle);
  private native void seektofirst0(long handle);
  private native void seektolast0(long handle);
  private native void next0(long handle);
  private native void prev0(long handle);
  private native byte[] key0(long handle);
  private native byte[] value0(long handle);
  private native void seek0(long handle, byte[] target, int targetlen);
  private native void status0(long handle);
}
