// copyright (c) 2013, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

import java.util.*;

/**
 * a rocksdbexception encapsulates the error of an operation.  this exception
 * type is used to describe an internal error from the c++ rocksdb library.
 */
public class rocksdbexception extends exception {
  /**
   * the private construct used by a set of public static factory method.
   *
   * @param msg the specified error message.
   */
  public rocksdbexception(string msg) {
    super(msg);
  }
}
