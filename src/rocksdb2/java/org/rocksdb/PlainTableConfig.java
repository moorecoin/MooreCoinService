// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
package org.rocksdb;

/**
 * the config for plain table sst format.
 *
 * plaintable is a rocksdb's sst file format optimized for low query latency
 * on pure-memory or really low-latency media.  it also support prefix
 * hash feature.
 */
public class plaintableconfig extends tableformatconfig {
  public static final int variable_length = 0;
  public static final int default_bloom_bits_per_key = 10;
  public static final double default_hash_table_ratio = 0.75;
  public static final int default_index_sparseness = 16;

  public plaintableconfig() {
    keysize_ = variable_length;
    bloombitsperkey_ = default_bloom_bits_per_key;
    hashtableratio_ = default_hash_table_ratio;
    indexsparseness_ = default_index_sparseness;
  }

  /**
   * set the length of the user key. if it is set to be variable_length,
   * then it indicates the user keys are variable-lengthed.  otherwise,
   * all the keys need to have the same length in byte.
   * default: variable_length
   *
   * @param keysize the length of the user key.
   * @return the reference to the current config.
   */
  public plaintableconfig setkeysize(int keysize) {
    keysize_ = keysize;
    return this;
  }

  /**
   * @return the specified size of the user key.  if variable_length,
   *     then it indicates variable-length key.
   */
  public int keysize() {
    return keysize_;
  }

  /**
   * set the number of bits per key used by the internal bloom filter
   * in the plain table sst format.
   *
   * @param bitsperkey the number of bits per key for bloom filer.
   * @return the reference to the current config.
   */
  public plaintableconfig setbloombitsperkey(int bitsperkey) {
    bloombitsperkey_ = bitsperkey;
    return this;
  }

  /**
   * @return the number of bits per key used for the bloom filter.
   */
  public int bloombitsperkey() {
    return bloombitsperkey_;
  }

  /**
   * hashtableratio is the desired utilization of the hash table used
   * for prefix hashing.  the ideal ratio would be the number of
   * prefixes / the number of hash buckets.  if this value is set to
   * zero, then hash table will not be used.
   *
   * @param ratio the hash table ratio.
   * @return the reference to the current config.
   */
  public plaintableconfig sethashtableratio(double ratio) {
    hashtableratio_ = ratio;
    return this;
  }

  /**
   * @return the hash table ratio.
   */
  public double hashtableratio() {
    return hashtableratio_;
  }

  /**
   * index sparseness determines the index interval for keys inside the
   * same prefix.  this number is equal to the maximum number of linear
   * search required after hash and binary search.  if it's set to 0,
   * then each key will be indexed.
   *
   * @param sparseness the index sparseness.
   * @return the reference to the current config.
   */
  public plaintableconfig setindexsparseness(int sparseness) {
    indexsparseness_ = sparseness;
    return this;
  }

  /**
   * @return the index sparseness.
   */
  public int indexsparseness() {
    return indexsparseness_;
  }

  @override protected long newtablefactoryhandle() {
    return newtablefactoryhandle(keysize_, bloombitsperkey_,
        hashtableratio_, indexsparseness_);
  }

  private native long newtablefactoryhandle(
      int keysize, int bloombitsperkey,
      double hashtableratio, int indexsparseness);

  private int keysize_;
  private int bloombitsperkey_;
  private double hashtableratio_;
  private int indexsparseness_;
}
