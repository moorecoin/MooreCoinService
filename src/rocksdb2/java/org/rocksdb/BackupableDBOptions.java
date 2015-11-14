// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

/**
 * backupabledboptions to control the behavior of a backupable database.
 * it will be used during the creation of a backupabledb.
 *
 * note that dispose() must be called before an options instance
 * become out-of-scope to release the allocated memory in c++.
 *
 * @param path where to keep the backup files. has to be different than dbname.
       best to set this to dbname_ + "/backups"
 * @param sharetablefiles if share_table_files == true, backup will assume that
 *     table files with same name have the same contents. this enables
 *     incremental backups and avoids unnecessary data copies. if
 *     share_table_files == false, each backup will be on its own and will not
 *     share any data with other backups. default: true
 * @param sync if sync == true, we can guarantee you'll get consistent backup
 *     even on a machine crash/reboot. backup process is slower with sync
 *     enabled. if sync == false, we don't guarantee anything on machine reboot.
 *     however, chances are some of the backups are consistent. default: true
 * @param destroyolddata if true, it will delete whatever backups there are
 *     already. default: false
 * @param backuplogfiles if false, we won't backup log files. this option can be
 *     useful for backing up in-memory databases where log file are persisted,
 *     but table files are in memory. default: true
 * @param backupratelimit max bytes that can be transferred in a second during
 *     backup. if 0 or negative, then go as fast as you can. default: 0
 * @param restoreratelimit max bytes that can be transferred in a second during
 *     restore. if 0 or negative, then go as fast as you can. default: 0
 */
public class backupabledboptions extends rocksobject {
  public backupabledboptions(string path, boolean sharetablefiles, boolean sync,
      boolean destroyolddata, boolean backuplogfiles, long backupratelimit,
      long restoreratelimit) {
    super();

    backupratelimit = (backupratelimit <= 0) ? 0 : backupratelimit;
    restoreratelimit = (restoreratelimit <= 0) ? 0 : restoreratelimit;

    newbackupabledboptions(path, sharetablefiles, sync, destroyolddata,
        backuplogfiles, backupratelimit, restoreratelimit);
  }

  /**
   * returns the path to the backupabledb directory.
   *
   * @return the path to the backupabledb directory.
   */
  public string backupdir() {
    assert(isinitialized());
    return backupdir(nativehandle_);
  }

  /**
   * release the memory allocated for the current instance
   * in the c++ side.
   */
  @override protected void disposeinternal() {
    assert(isinitialized());
    disposeinternal(nativehandle_);
  }

  private native void newbackupabledboptions(string path,
      boolean sharetablefiles, boolean sync, boolean destroyolddata,
      boolean backuplogfiles, long backupratelimit, long restoreratelimit);
  private native string backupdir(long handle);
  private native void disposeinternal(long handle);
}
