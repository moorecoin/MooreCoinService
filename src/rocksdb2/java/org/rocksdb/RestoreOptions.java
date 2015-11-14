// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

/**
 * restoreoptions to control the behavior of restore.
 *
 * note that dispose() must be called before this instance become out-of-scope
 * to release the allocated memory in c++.
 *
 * @param if true, restore won't overwrite the existing log files in wal_dir. it
 *     will also move all log files from archive directory to wal_dir. use this
 *     option in combination with backupabledboptions::backup_log_files = false
 *     for persisting in-memory databases.
 *     default: false
 */
public class restoreoptions extends rocksobject {
  public restoreoptions(boolean keeplogfiles) {
    super();
    nativehandle_ = newrestoreoptions(keeplogfiles);
  }

  /**
   * release the memory allocated for the current instance
   * in the c++ side.
   */
  @override public synchronized void disposeinternal() {
    assert(isinitialized());
    dispose(nativehandle_);
  }

  private native long newrestoreoptions(boolean keeplogfiles);
  private native void dispose(long handle);
}
