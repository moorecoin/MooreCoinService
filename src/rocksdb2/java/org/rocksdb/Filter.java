// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

/**
 * filters are stored in rocksdb and are consulted automatically
 * by rocksdb to decide whether or not to read some
 * information from disk. in many cases, a filter can cut down the
 * number of disk seeks form a handful to a single disk seek per
 * db::get() call.
 */
public abstract class filter extends rocksobject {
  protected abstract void createnewfilter();

  /**
   * deletes underlying c++ filter pointer.
   *
   * note that this function should be called only after all
   * rocksdb instances referencing the filter are closed.
   * otherwise an undefined behavior will occur.
   */
  @override protected void disposeinternal() {
    assert(isinitialized());
    disposeinternal(nativehandle_);
  }

  private native void disposeinternal(long handle);
}
