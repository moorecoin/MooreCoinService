package org.rocksdb;

/**
 * the config for hash linked list memtable representation
 * such memtable contains a fix-sized array of buckets, where
 * each bucket points to a sorted singly-linked
 * list (or null if the bucket is empty).
 *
 * note that since this mem-table representation relies on the
 * key prefix, it is required to invoke one of the useprefixextractor
 * functions to specify how to extract key prefix given a key.
 * if proper prefix-extractor is not set, then rocksdb will
 * use the default memtable representation (skiplist) instead
 * and post a warning in the log.
 */
public class hashlinkedlistmemtableconfig extends memtableconfig {
  public static final long default_bucket_count = 50000;

  public hashlinkedlistmemtableconfig() {
    bucketcount_ = default_bucket_count;
  }

  /**
   * set the number of buckets in the fixed-size array used
   * in the hash linked-list mem-table.
   *
   * @param count the number of hash buckets.
   * @return the reference to the current hashlinkedlistmemtableconfig.
   */
  public hashlinkedlistmemtableconfig setbucketcount(long count) {
    bucketcount_ = count;
    return this;
  }

  /**
   * returns the number of buckets that will be used in the memtable
   * created based on this config.
   *
   * @return the number of buckets
   */
  public long bucketcount() {
    return bucketcount_;
  }

  @override protected long newmemtablefactoryhandle() {
    return newmemtablefactoryhandle(bucketcount_);
  }

  private native long newmemtablefactoryhandle(long bucketcount);

  private long bucketcount_;
}
