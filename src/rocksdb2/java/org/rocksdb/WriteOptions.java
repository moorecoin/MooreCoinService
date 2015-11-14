// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

/**
 * options that control write operations.
 *
 * note that developers should call writeoptions.dispose() to release the
 * c++ side memory before a writeoptions instance runs out of scope.
 */
public class writeoptions extends rocksobject {
  public writeoptions() {
    super();
    newwriteoptions();
  }

  @override protected void disposeinternal() {
    assert(isinitialized());
    disposeinternal(nativehandle_);
  }

  /**
   * if true, the write will be flushed from the operating system
   * buffer cache (by calling writablefile::sync()) before the write
   * is considered complete.  if this flag is true, writes will be
   * slower.
   *
   * if this flag is false, and the machine crashes, some recent
   * writes may be lost.  note that if it is just the process that
   * crashes (i.e., the machine does not reboot), no writes will be
   * lost even if sync==false.
   *
   * in other words, a db write with sync==false has similar
   * crash semantics as the "write()" system call.  a db write
   * with sync==true has similar crash semantics to a "write()"
   * system call followed by "fdatasync()".
   *
   * default: false
   *
   * @param flag a boolean flag to indicate whether a write
   *     should be synchronized.
   * @return the instance of the current writeoptions.
   */
  public writeoptions setsync(boolean flag) {
    setsync(nativehandle_, flag);
    return this;
  }

  /**
   * if true, the write will be flushed from the operating system
   * buffer cache (by calling writablefile::sync()) before the write
   * is considered complete.  if this flag is true, writes will be
   * slower.
   *
   * if this flag is false, and the machine crashes, some recent
   * writes may be lost.  note that if it is just the process that
   * crashes (i.e., the machine does not reboot), no writes will be
   * lost even if sync==false.
   *
   * in other words, a db write with sync==false has similar
   * crash semantics as the "write()" system call.  a db write
   * with sync==true has similar crash semantics to a "write()"
   * system call followed by "fdatasync()".
   */
  public boolean sync() {
    return sync(nativehandle_);
  }

  /**
   * if true, writes will not first go to the write ahead log,
   * and the write may got lost after a crash.
   *
   * @param flag a boolean flag to specify whether to disable
   *     write-ahead-log on writes.
   * @return the instance of the current writeoptions.
   */
  public writeoptions setdisablewal(boolean flag) {
    setdisablewal(nativehandle_, flag);
    return this;
  }

  /**
   * if true, writes will not first go to the write ahead log,
   * and the write may got lost after a crash.
   */
  public boolean disablewal() {
    return disablewal(nativehandle_);
  }

  private native void newwriteoptions();
  private native void setsync(long handle, boolean flag);
  private native boolean sync(long handle);
  private native void setdisablewal(long handle, boolean flag);
  private native boolean disablewal(long handle);
  private native void disposeinternal(long handle);
}
