// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

/**
 * rocksobject is the base-class of all rocksdb classes that has a pointer to
 * some c++ rocksdb object.
 *
 * rocksobject has dispose() function, which releases its associated c++ resource.
 * this function can be either called manually, or being called automatically
 * during the regular java gc process.  however, since java may wrongly assume a
 * rocksobject only contains a long member variable and think it is small in size,
 * java may give rocksobject low priority in the gc process.  for this, it is
 * suggested to call dispose() manually.  however, it is safe to let rocksobject go
 * out-of-scope without manually calling dispose() as dispose() will be called
 * in the finalizer during the regular gc process.
 */
public abstract class rocksobject {
  protected rocksobject() {
    nativehandle_ = 0;
    owninghandle_ = true;
  }

  /**
   * release the c++ object manually pointed by the native handle.
   *
   * note that dispose() will also be called during the gc process
   * if it was not called before its rocksobject went out-of-scope.
   * however, since java may wrongly wrongly assume those objects are
   * small in that they seems to only hold a long variable. as a result,
   * they might have low priority in the gc process.  to prevent this,
   * it is suggested to call dispose() manually.
   *
   * note that once an instance of rocksobject has been disposed,
   * calling its function will lead undefined behavior.
   */
  public final synchronized void dispose() {
    if (isowningnativehandle() && isinitialized()) {
      disposeinternal();
    }
    nativehandle_ = 0;
    disownnativehandle();
  }

  /**
   * the helper function of dispose() which all subclasses of rocksobject
   * must implement to release their associated c++ resource.
   */
  protected abstract void disposeinternal();

  /**
   * revoke ownership of the native object.
   *
   * this will prevent the object from attempting to delete the underlying
   * native object in its finalizer. this must be used when another object
   * takes over ownership of the native object or both will attempt to delete
   * the underlying object when garbage collected.
   *
   * when disownnativehandle() is called, dispose() will simply set nativehandle_
   * to 0 without releasing its associated c++ resource.  as a result,
   * incorrectly use this function may cause memory leak, and this function call
   * will not affect the return value of isinitialized().
   *
   * @see dispose()
   * @see isinitialized()
   */
  protected void disownnativehandle() {
    owninghandle_ = false;
  }

  /**
   * returns true if the current rocksobject is responsable to release its
   * native handle.
   *
   * @return true if the current rocksobject is responsible to release its
   *   native handle.
   *
   * @see disownnativehandle()
   * @see dispose()
   */
  protected boolean isowningnativehandle() {
    return owninghandle_;
  }

  /**
   * returns true if the associated native handle has been initialized.
   *
   * @return true if the associated native handle has been initialized.
   *
   * @see dispose()
   */
  protected boolean isinitialized() {
    return (nativehandle_ != 0);
  }

  /**
   * simply calls dispose() and release its c++ resource if it has not
   * yet released.
   */
  @override protected void finalize() {
    dispose();
  }

  /**
   * a long variable holding c++ pointer pointing to some rocksdb c++ object.
   */
  protected long nativehandle_;

  /**
   * a flag indicating whether the current rocksobject is responsible to
   * release the c++ object stored in its nativehandle_.
   */
  private boolean owninghandle_;
}
