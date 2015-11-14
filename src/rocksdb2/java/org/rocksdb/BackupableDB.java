// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

/**
 * a subclass of rocksdb which supports backup-related operations.
 *
 * @see backupabledboptions
 */
public class backupabledb extends rocksdb {
  /**
   * open a backupabledb under the specified path.
   * note that the backup path should be set properly in the
   * input backupabledboptions.
   *
   * @param opt options for db.
   * @param bopt backup related options.
   * @param the db path for storing data.  the path for storing
   *     backup should be specified in the backupabledboptions.
   * @return reference to the opened backupabledb.
   */
  public static backupabledb open(
      options opt, backupabledboptions bopt, string db_path)
      throws rocksdbexception {

    rocksdb db = rocksdb.open(opt, db_path);
    backupabledb bdb = new backupabledb();
    bdb.open(db.nativehandle_, bopt.nativehandle_);

    // prevent the rocksdb object from attempting to delete
    // the underly c++ db object.
    db.disownnativehandle();

    return bdb;
  }

  /**
   * captures the state of the database in the latest backup.
   * note that this function is not thread-safe.
   *
   * @param flushbeforebackup if true, then all data will be flushed
   *     before creating backup.
   */
  public void createnewbackup(boolean flushbeforebackup) {
    createnewbackup(nativehandle_, flushbeforebackup);
  }
  
  /**
   * deletes old backups, keeping latest numbackupstokeep alive.
   * 
   * @param numbackupstokeep number of latest backups to keep.
   */
  public void purgeoldbackups(int numbackupstokeep) {
    purgeoldbackups(nativehandle_, numbackupstokeep);
  }


  /**
   * close the backupabledb instance and release resource.
   *
   * internally, backupabledb owns the rocksdb::db pointer to its
   * associated rocksdb.  the release of that rocksdb pointer is
   * handled in the destructor of the c++ rocksdb::backupabledb and
   * should be transparent to java developers.
   */
  @override public synchronized void close() {
    if (isinitialized()) {
      super.close();
    }
  }

  /**
   * a protected construction that will be used in the static factory
   * method backupabledb.open().
   */
  protected backupabledb() {
    super();
  }

  @override protected void finalize() {
    close();
  }

  protected native void open(long rocksdbhandle, long backupdboptionshandle);
  protected native void createnewbackup(long handle, boolean flag);
  protected native void purgeoldbackups(long handle, int numbackupstokeep);
}
