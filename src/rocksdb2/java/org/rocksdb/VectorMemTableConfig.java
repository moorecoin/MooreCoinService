package org.rocksdb;

/**
 * the config for vector memtable representation.
 */
public class vectormemtableconfig extends memtableconfig {
  public static final int default_reserved_size = 0;
  public vectormemtableconfig() {
    reservedsize_ = default_reserved_size;
  }

  /**
   * set the initial size of the vector that will be used
   * by the memtable created based on this config.
   *
   * @param size the initial size of the vector.
   * @return the reference to the current config.
   */
  public vectormemtableconfig setreservedsize(int size) {
    reservedsize_ = size;
    return this;
  }

  /**
   * returns the initial size of the vector used by the memtable
   * created based on this config.
   *
   * @return the initial size of the vector.
   */
  public int reservedsize() {
    return reservedsize_;
  }

  @override protected long newmemtablefactoryhandle() {
    return newmemtablefactoryhandle(reservedsize_);
  }

  private native long newmemtablefactoryhandle(long reservedsize);
  private int reservedsize_;
}
