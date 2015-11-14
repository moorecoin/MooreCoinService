// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb.util;

public class sizeunit {
  public static final long kb = 1024l;
  public static final long mb = kb * kb;
  public static final long gb = kb * mb;
  public static final long tb = kb * gb;
  public static final long pb = kb * tb;

  private sizeunit() {}
}
