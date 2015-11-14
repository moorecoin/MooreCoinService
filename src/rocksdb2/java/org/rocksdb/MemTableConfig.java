// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
package org.rocksdb;

/**
 * memtableconfig is used to config the internal mem-table of a rocksdb.
 * it is required for each memtable to have one such sub-class to allow
 * java developers to use it.
 *
 * to make a rocksdb to use a specific memtable format, its associated
 * memtableconfig should be properly set and passed into options
 * via options.setmemtablefactory() and open the db using that options.
 *
 * @see options
 */
public abstract class memtableconfig {
  /**
   * this function should only be called by options.setmemtableconfig(),
   * which will create a c++ shared-pointer to the c++ memtablerepfactory
   * that associated with the java memtableconfig.
   *
   * @see options.setmemtablefactory()
   */
  abstract protected long newmemtablefactoryhandle();
}
