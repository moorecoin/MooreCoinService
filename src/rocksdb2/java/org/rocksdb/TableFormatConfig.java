// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
package org.rocksdb;

/**
 * tableformatconfig is used to config the internal table format of a rocksdb.
 * to make a rocksdb to use a specific table format, its associated
 * tableformatconfig should be properly set and passed into options via
 * options.settableformatconfig() and open the db using that options.
 */
public abstract class tableformatconfig {
  /**
   * this function should only be called by options.settableformatconfig(),
   * which will create a c++ shared-pointer to the c++ tablefactory
   * that associated with the java tableformatconfig.
   */
  abstract protected long newtablefactoryhandle();
}
