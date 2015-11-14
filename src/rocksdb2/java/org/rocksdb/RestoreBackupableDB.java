// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

/**
 * this class is used to access information about backups and restore from them.
 *
 * note that dispose() must be called before this instance become out-of-scope
 * to release the allocated memory in c++.
 *
 * @param options instance of backupabledboptions.
 */
public class restorebackupabledb extends rocksobject {
  public restorebackupabledb(backupabledboptions options) {
    super();
    nativehandle_ = newrestorebackupabledb(options.nativehandle_);
  }

  /**
   * restore from backup with backup_id
   * important -- if options_.share_table_files == true and you restore db
   * from some backup that is not the latest, and you start creating new
   * backups from the new db, they will probably fail.
   *
   * example: let's say you have backups 1, 2, 3, 4, 5 and you restore 3.
   * if you add new data to the db and try creating a new backup now, the
   * database will diverge from backups 4 and 5 and the new backup will fail.
   * if you want to create new backup, you will first have to delete backups 4
   * and 5.
   */
  public void restoredbfrombackup(long backupid, string dbdir, string waldir,
      restoreoptions restoreoptions) throws rocksdbexception {
    restoredbfrombackup0(nativehandle_, backupid, dbdir, waldir,
        restoreoptions.nativehandle_);
  }

  /**
   * restore from the latest backup.
   */
  public void restoredbfromlatestbackup(string dbdir, string waldir,
      restoreoptions restoreoptions) throws rocksdbexception {
    restoredbfromlatestbackup0(nativehandle_, dbdir, waldir,
        restoreoptions.nativehandle_);
  }

  /**
   * deletes old backups, keeping latest numbackupstokeep alive.
   *
   * @param number of latest backups to keep
   */
  public void purgeoldbackups(int numbackupstokeep) throws rocksdbexception {
    purgeoldbackups0(nativehandle_, numbackupstokeep);
  }

  /**
   * deletes a specific backup.
   *
   * @param id of backup to delete.
   */
  public void deletebackup(long backupid) throws rocksdbexception {
    deletebackup0(nativehandle_, backupid);
  }

  /**
   * release the memory allocated for the current instance
   * in the c++ side.
   */
  @override public synchronized void disposeinternal() {
    assert(isinitialized());
    dispose(nativehandle_);
  }

  private native long newrestorebackupabledb(long options);
  private native void restoredbfrombackup0(long nativehandle, long backupid,
      string dbdir, string waldir, long restoreoptions) throws rocksdbexception;
  private native void restoredbfromlatestbackup0(long nativehandle,
      string dbdir, string waldir, long restoreoptions) throws rocksdbexception;
  private native void purgeoldbackups0(long nativehandle, int numbackupstokeep);
  private native void deletebackup0(long nativehandle, long backupid);
  private native void dispose(long nativehandle);
}
