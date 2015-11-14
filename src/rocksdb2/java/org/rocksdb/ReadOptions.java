// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

/**
 * the class that controls the get behavior.
 *
 * note that dispose() must be called before an options instance
 * become out-of-scope to release the allocated memory in c++.
 */
public class readoptions extends rocksobject {
  public readoptions() {
    super();
    newreadoptions();
  }
  private native void newreadoptions();

  /**
   * if true, all data read from underlying storage will be
   * verified against corresponding checksums.
   * default: true
   *
   * @return true if checksum verification is on.
   */
  public boolean verifychecksums() {
    assert(isinitialized());
    return verifychecksums(nativehandle_);
  }
  private native boolean verifychecksums(long handle);

  /**
   * if true, all data read from underlying storage will be
   * verified against corresponding checksums.
   * default: true
   *
   * @param verifychecksums if true, then checksum verification
   *     will be performed on every read.
   * @return the reference to the current readoptions.
   */
  public readoptions setverifychecksums(boolean verifychecksums) {
    assert(isinitialized());
    setverifychecksums(nativehandle_, verifychecksums);
    return this;
  }
  private native void setverifychecksums(
      long handle, boolean verifychecksums);

  // todo(yhchiang): this option seems to be block-based table only.
  //                 move this to a better place?
  /**
   * fill the cache when loading the block-based sst formated db.
   * callers may wish to set this field to false for bulk scans.
   * default: true
   *
   * @return true if the fill-cache behavior is on.
   */
  public boolean fillcache() {
    assert(isinitialized());
    return fillcache(nativehandle_);
  }
  private native boolean fillcache(long handle);

  /**
   * fill the cache when loading the block-based sst formated db.
   * callers may wish to set this field to false for bulk scans.
   * default: true
   *
   * @param fillcache if true, then fill-cache behavior will be
   *     performed.
   * @return the reference to the current readoptions.
   */
  public readoptions setfillcache(boolean fillcache) {
    assert(isinitialized());
    setfillcache(nativehandle_, fillcache);
    return this;
  }
  private native void setfillcache(
      long handle, boolean fillcache);

  /**
   * specify to create a tailing iterator -- a special iterator that has a
   * view of the complete database (i.e. it can also be used to read newly
   * added data) and is optimized for sequential reads. it will return records
   * that were inserted into the database after the creation of the iterator.
   * default: false
   * not supported in rocksdb_lite mode!
   *
   * @return true if tailing iterator is enabled.
   */
  public boolean tailing() {
    assert(isinitialized());
    return tailing(nativehandle_);
  }
  private native boolean tailing(long handle);

  /**
   * specify to create a tailing iterator -- a special iterator that has a
   * view of the complete database (i.e. it can also be used to read newly
   * added data) and is optimized for sequential reads. it will return records
   * that were inserted into the database after the creation of the iterator.
   * default: false
   * not supported in rocksdb_lite mode!
   *
   * @param tailing if true, then tailing iterator will be enabled.
   * @return the reference to the current readoptions.
   */
  public readoptions settailing(boolean tailing) {
    assert(isinitialized());
    settailing(nativehandle_, tailing);
    return this;
  }
  private native void settailing(
      long handle, boolean tailing);


  @override protected void disposeinternal() {
    assert(isinitialized());
    disposeinternal(nativehandle_);
  }
  private native void disposeinternal(long handle);

}
