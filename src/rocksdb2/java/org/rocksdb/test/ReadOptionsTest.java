// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb.test;

import java.util.random;
import org.rocksdb.rocksdb;
import org.rocksdb.readoptions;

public class readoptionstest {
  static {
    rocksdb.loadlibrary();
  }
  public static void main(string[] args) {
    readoptions opt = new readoptions();
    random rand = new random();
    { // verifychecksums test
      boolean boolvalue = rand.nextboolean();
      opt.setverifychecksums(boolvalue);
      assert(opt.verifychecksums() == boolvalue);
    }

    { // fillcache test
      boolean boolvalue = rand.nextboolean();
      opt.setfillcache(boolvalue);
      assert(opt.fillcache() == boolvalue);
    }

    { // tailing test
      boolean boolvalue = rand.nextboolean();
      opt.settailing(boolvalue);
      assert(opt.tailing() == boolvalue);
    }

    opt.dispose();
    system.out.println("passed readoptionstest");
  }
}
