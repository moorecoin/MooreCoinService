// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

public enum compactionstyle {
  level((byte) 0),
  universal((byte) 1),
  fifo((byte) 2);
  
  private final byte value_;

  private compactionstyle(byte value) {
    value_ = value;
  }

  public byte getvalue() {
    return value_;
  }
}
