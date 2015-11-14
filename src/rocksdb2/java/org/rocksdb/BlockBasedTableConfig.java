// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
package org.rocksdb;

/**
 * the config for plain table sst format.
 *
 * blockbasedtable is a rocksdb's default sst file format.
 */
public class blockbasedtableconfig extends tableformatconfig {

  public blockbasedtableconfig() {
    noblockcache_ = false;
    blockcachesize_ = 8 * 1024 * 1024;
    blocksize_ =  4 * 1024;
    blocksizedeviation_ =10;
    blockrestartinterval_ =16;
    wholekeyfiltering_ = true;
    bitsperkey_ = 0;
  }

  /**
   * disable block cache. if this is set to true,
   * then no block cache should be used, and the block_cache should
   * point to a nullptr object.
   * default: false
   *
   * @param noblockcache if use block cache
   * @return the reference to the current config.
   */
  public blockbasedtableconfig setnoblockcache(boolean noblockcache) {
    noblockcache_ = noblockcache;
    return this;
  }

  /**
   * @return if block cache is disabled
   */
  public boolean noblockcache() {
    return noblockcache_;
  }

  /**
   * set the amount of cache in bytes that will be used by rocksdb.
   * if cachesize is non-positive, then cache will not be used.
   * default: 8m
   *
   * @param blockcachesize block cache size in bytes
   * @return the reference to the current config.
   */
  public blockbasedtableconfig setblockcachesize(long blockcachesize) {
    blockcachesize_ = blockcachesize;
    return this;
  }

  /**
   * @return block cache size in bytes
   */
  public long blockcachesize() {
    return blockcachesize_;
  }

  /**
   * controls the number of shards for the block cache.
   * this is applied only if cachesize is set to non-negative.
   *
   * @param numshardbits the number of shard bits.  the resulting
   *     number of shards would be 2 ^ numshardbits.  any negative
   *     number means use default settings."
   * @return the reference to the current option.
   */
  public blockbasedtableconfig setcachenumshardbits(int numshardbits) {
    numshardbits_ = numshardbits;
    return this;
  }

  /**
   * returns the number of shard bits used in the block cache.
   * the resulting number of shards would be 2 ^ (returned value).
   * any negative number means use default settings.
   *
   * @return the number of shard bits used in the block cache.
   */
  public int cachenumshardbits() {
    return numshardbits_;
  }

  /**
   * approximate size of user data packed per block.  note that the
   * block size specified here corresponds to uncompressed data.  the
   * actual size of the unit read from disk may be smaller if
   * compression is enabled.  this parameter can be changed dynamically.
   * default: 4k
   *
   * @param blocksize block size in bytes
   * @return the reference to the current config.
   */
  public blockbasedtableconfig setblocksize(long blocksize) {
    blocksize_ = blocksize;
    return this;
  }

  /**
   * @return block size in bytes
   */
  public long blocksize() {
    return blocksize_;
  }

  /**
   * this is used to close a block before it reaches the configured
   * 'block_size'. if the percentage of free space in the current block is less
   * than this specified number and adding a new record to the block will
   * exceed the configured block size, then this block will be closed and the
   * new record will be written to the next block.
   * default is 10.
   *
   * @param blocksizedeviation the deviation to block size allowed
   * @return the reference to the current config.
   */
  public blockbasedtableconfig setblocksizedeviation(int blocksizedeviation) {
    blocksizedeviation_ = blocksizedeviation;
    return this;
  }

  /**
   * @return the hash table ratio.
   */
  public int blocksizedeviation() {
    return blocksizedeviation_;
  }

  /**
   * set block restart interval
   *
   * @param restartinterval block restart interval.
   * @return the reference to the current config.
   */
  public blockbasedtableconfig setblockrestartinterval(int restartinterval) {
    blockrestartinterval_ = restartinterval;
    return this;
  }

  /**
   * @return block restart interval
   */
  public int blockrestartinterval() {
    return blockrestartinterval_;
  }

  /**
   * if true, place whole keys in the filter (not just prefixes).
   * this must generally be true for gets to be efficient.
   * default: true
   *
   * @param wholekeyfiltering if enable whole key filtering
   * @return the reference to the current config.
   */
  public blockbasedtableconfig setwholekeyfiltering(boolean wholekeyfiltering) {
    wholekeyfiltering_ = wholekeyfiltering;
    return this;
  }

  /**
   * @return if whole key filtering is enabled
   */
  public boolean wholekeyfiltering() {
    return wholekeyfiltering_;
  }

  /**
   * use the specified filter policy to reduce disk reads.
   *
   * filter should not be disposed before options instances using this filter is
   * disposed. if dispose() function is not called, then filter object will be
   * gc'd automatically.
   *
   * filter instance can be re-used in multiple options instances.
   *
   * @param filter policy java instance.
   * @return the reference to the current config.
   */
  public blockbasedtableconfig setfilterbitsperkey(int bitsperkey) {
    bitsperkey_ = bitsperkey;
    return this;
  }

  @override protected long newtablefactoryhandle() {
    return newtablefactoryhandle(noblockcache_, blockcachesize_, numshardbits_,
        blocksize_, blocksizedeviation_, blockrestartinterval_,
        wholekeyfiltering_, bitsperkey_);
  }

  private native long newtablefactoryhandle(
      boolean noblockcache, long blockcachesize, int numshardbits,
      long blocksize, int blocksizedeviation, int blockrestartinterval,
      boolean wholekeyfiltering, int bitsperkey);

  private boolean noblockcache_;
  private long blockcachesize_;
  private int numshardbits_;
  private long shard;
  private long blocksize_;
  private int blocksizedeviation_;
  private int blockrestartinterval_;
  private boolean wholekeyfiltering_;
  private int bitsperkey_;
}
