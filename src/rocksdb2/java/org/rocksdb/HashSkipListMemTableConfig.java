package org.rocksdb;

/**
 * the config for hash skip-list mem-table representation.
 * such mem-table representation contains a fix-sized array of
 * buckets, where each bucket points to a skiplist (or null if the
 * bucket is empty).
 *
 * note that since this mem-table representation relies on the
 * key prefix, it is required to invoke one of the useprefixextractor
 * functions to specify how to extract key prefix given a key.
 * if proper prefix-extractor is not set, then rocksdb will
 * use the default memtable representation (skiplist) instead
 * and post a warning in the log.
 */
public class hashskiplistmemtableconfig extends memtableconfig {
  public static final int default_bucket_count = 1000000;
  public static final int default_branching_factor = 4;
  public static final int default_height = 4;

  public hashskiplistmemtableconfig() {
    bucketcount_ = default_bucket_count;
    branchingfactor_ = default_branching_factor;
    height_ = default_height;
  }

  /**
   * set the number of hash buckets used in the hash skiplist memtable.
   * default = 1000000.
   *
   * @param count the number of hash buckets used in the hash
   *    skiplist memtable.
   * @return the reference to the current hashskiplistmemtableconfig.
   */
  public hashskiplistmemtableconfig setbucketcount(long count) {
    bucketcount_ = count;
    return this;
  }

  /**
   * @return the number of hash buckets
   */
  public long bucketcount() {
    return bucketcount_;
  }

  /**
   * set the height of the skip list.  default = 4.
   *
   * @return the reference to the current hashskiplistmemtableconfig.
   */
  public hashskiplistmemtableconfig setheight(int height) {
    height_ = height;
    return this;
  }

  /**
   * @return the height of the skip list.
   */
  public int height() {
    return height_;
  }

  /**
   * set the branching factor used in the hash skip-list memtable.
   * this factor controls the probabilistic size ratio between adjacent
   * links in the skip list.
   *
   * @param bf the probabilistic size ratio between adjacent link
   *     lists in the skip list.
   * @return the reference to the current hashskiplistmemtableconfig.
   */
  public hashskiplistmemtableconfig setbranchingfactor(int bf) {
    branchingfactor_ = bf;
    return this;
  }

  /**
   * @return branching factor, the probabilistic size ratio between
   *     adjacent links in the skip list.
   */
  public int branchingfactor() {
    return branchingfactor_;
  }

  @override protected long newmemtablefactoryhandle() {
    return newmemtablefactoryhandle(
        bucketcount_, height_, branchingfactor_);
  }

  private native long newmemtablefactoryhandle(
      long bucketcount, int height, int branchingfactor);

  private long bucketcount_;
  private int branchingfactor_;
  private int height_;
}
