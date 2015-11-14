// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb.test;

import org.rocksdb.*;

public class backupabledbtest {
  static final string db_path = "/tmp/backupablejni_db";
  static final string backup_path = "/tmp/backupablejni_db_backup";
  static {
    rocksdb.loadlibrary();
  }
  public static void main(string[] args) {

    options opt = new options();
    opt.setcreateifmissing(true);

    backupabledboptions bopt = new backupabledboptions(backup_path, false,
        true, false, true, 0, 0);
    backupabledb bdb = null;

    try {
      bdb = backupabledb.open(opt, bopt, db_path);

      bdb.put("abc".getbytes(), "def".getbytes());
      bdb.put("ghi".getbytes(), "jkl".getbytes());
      bdb.createnewbackup(true);

      // delete record after backup
      bdb.remove("abc".getbytes());
      byte[] value = bdb.get("abc".getbytes());
      assert(value == null);
      bdb.close();

      // restore from backup
      restoreoptions ropt = new restoreoptions(false);
      restorebackupabledb rdb = new restorebackupabledb(bopt);
      rdb.restoredbfromlatestbackup(db_path, db_path,
          ropt);
      rdb.dispose();
      ropt.dispose();

      // verify that backed up data contains deleted record
      bdb = backupabledb.open(opt, bopt, db_path);
      value = bdb.get("abc".getbytes());
      assert(new string(value).equals("def"));

      system.out.println("backup and restore test passed");
    } catch (rocksdbexception e) {
      system.err.format("[error]: %s%n", e);
      e.printstacktrace();
    } finally {
      opt.dispose();
      bopt.dispose();
      if (bdb != null) {
        bdb.close();
      }
    }
  }
}
