// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

/**
 * a rocksenv is an interface used by the rocksdb implementation to access
 * operating system functionality like the filesystem etc.
 *
 * all env implementations are safe for concurrent access from
 * multiple threads without any external synchronization.
 */
public class rocksenv extends rocksobject {
  public static final int flush_pool = 0;
  public static final int compaction_pool = 1;

  static {
    default_env_ = new rocksenv(getdefaultenvinternal());
  }
  private static native long getdefaultenvinternal();

  /**
   * returns the default environment suitable for the current operating
   * system.
   *
   * the result of getdefault() is a singleton whose ownership belongs
   * to rocksdb c++.  as a result, the returned rocksenv will not
   * have the ownership of its c++ resource, and calling its dispose()
   * will be no-op.
   */
  public static rocksenv getdefault() {
    return default_env_;
  }

  /**
   * sets the number of background worker threads of the flush pool
   * for this environment.
   * default number: 1
   */
  public rocksenv setbackgroundthreads(int num) {
    return setbackgroundthreads(num, flush_pool);
  }

  /**
   * sets the number of background worker threads of the specified thread
   * pool for this environment.
   *
   * @param num the number of threads
   * @param poolid the id to specified a thread pool.  should be either
   *     flush_pool or compaction_pool.
   * default number: 1
   */
  public rocksenv setbackgroundthreads(int num, int poolid) {
    setbackgroundthreads(nativehandle_, num, poolid);
    return this;
  }
  private native void setbackgroundthreads(
      long handle, int num, int priority);

  /**
   * returns the length of the queue associated with the specified
   * thread pool.
   *
   * @param poolid the id to specified a thread pool.  should be either
   *     flush_pool or compaction_pool.
   */
  public int getthreadpoolqueuelen(int poolid) {
    return getthreadpoolqueuelen(nativehandle_, poolid);
  }
  private native int getthreadpoolqueuelen(long handle, int poolid);

  /**
   * package-private constructor that uses the specified native handle
   * to construct a rocksenv.  note that the ownership of the input handle
   * belongs to the caller, and the newly created rocksenv will not take
   * the ownership of the input handle.  as a result, calling dispose()
   * of the created rocksenv will be no-op.
   */
  rocksenv(long handle) {
    super();
    nativehandle_ = handle;
    disownnativehandle();
  }

  /**
   * the helper function of dispose() which all subclasses of rocksobject
   * must implement to release their associated c++ resource.
   */
  protected void disposeinternal() {
    disposeinternal(nativehandle_);
  }
  private native void disposeinternal(long handle);

  /**
   * the static default rocksenv.  the ownership of its native handle
   * belongs to rocksdb c++ and is not able to be released on the java
   * side.
   */
  static rocksenv default_env_;
}
