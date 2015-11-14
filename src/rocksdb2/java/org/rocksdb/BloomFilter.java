// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

/**
 * this class creates a new filter policy that uses a bloom filter
 * with approximately the specified number of bits per key.
 * a good value for bitsperkey is 10, which yields a filter
 * with ~ 1% false positive rate.
 *
 * default value of bits per key is 10.
 */
public class bloomfilter extends filter {
  private static final int default_bits_per_key = 10;
  private final int bitsperkey_;

  public bloomfilter() {
    this(default_bits_per_key);
  }

  public bloomfilter(int bitsperkey) {
    super();
    bitsperkey_ = bitsperkey;

    createnewfilter();
  }

  @override
  protected void createnewfilter() {
    createnewfilter0(bitsperkey_);
  }

  private native void createnewfilter0(int bitskeykey);
}
