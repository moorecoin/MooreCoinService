package org.rocksdb;

/**
 * the config for skip-list memtable representation.
 */
public class skiplistmemtableconfig extends memtableconfig {
  public skiplistmemtableconfig() {
  }

  @override protected long newmemtablefactoryhandle() {
    return newmemtablefactoryhandle0();
  }

  private native long newmemtablefactoryhandle0();
}
