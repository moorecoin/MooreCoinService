// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

public enum compressiontype {
  no_compression((byte) 0),
  snappy_compression((byte) 1),
  zlib_compression((byte) 2),
  bzlib2_compression((byte) 3),
  lz4_compression((byte) 4),
  lz4hc_compression((byte) 5);
  
  private final byte value_;

  private compressiontype(byte value) {
    value_ = value;
  }

  public byte getvalue() {
    return value_;
  }
}
