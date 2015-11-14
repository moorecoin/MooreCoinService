//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef rocksdb_lite

#include "rocksdb/utilities/backupable_db.h"
#include "db/filename.h"
#include "util/coding.h"
#include "util/crc32c.h"
#include "rocksdb/transaction_log.h"

#define __stdc_format_macros

#include <inttypes.h>
#include <algorithm>
#include <vector>
#include <map>
#include <string>
#include <limits>
#include <atomic>
#include <unordered_map>

namespace rocksdb {

namespace {
class backupratelimiter {
 public:
  backupratelimiter(env* env, uint64_t max_bytes_per_second,
                   uint64_t bytes_per_check)
      : env_(env),
        max_bytes_per_second_(max_bytes_per_second),
        bytes_per_check_(bytes_per_check),
        micros_start_time_(env->nowmicros()),
        bytes_since_start_(0) {}

  void reportandwait(uint64_t bytes_since_last_call) {
    bytes_since_start_ += bytes_since_last_call;
    if (bytes_since_start_ < bytes_per_check_) {
      // not enough bytes to be rate-limited
      return;
    }

    uint64_t now = env_->nowmicros();
    uint64_t interval = now - micros_start_time_;
    uint64_t should_take_micros =
        (bytes_since_start_ * kmicrosinsecond) / max_bytes_per_second_;

    if (should_take_micros > interval) {
      env_->sleepformicroseconds(should_take_micros - interval);
      now = env_->nowmicros();
    }
    // reset interval
    micros_start_time_ = now;
    bytes_since_start_ = 0;
  }

 private:
  env* env_;
  uint64_t max_bytes_per_second_;
  uint64_t bytes_per_check_;
  uint64_t micros_start_time_;
  uint64_t bytes_since_start_;
  static const uint64_t kmicrosinsecond = 1000 * 1000ll;
};
}  // namespace

void backupabledboptions::dump(logger* logger) const {
  log(logger, "        options.backup_dir: %s", backup_dir.c_str());
  log(logger, "        options.backup_env: %p", backup_env);
  log(logger, " options.share_table_files: %d",
      static_cast<int>(share_table_files));
  log(logger, "          options.info_log: %p", info_log);
  log(logger, "              options.sync: %d", static_cast<int>(sync));
  log(logger, "  options.destroy_old_data: %d",
      static_cast<int>(destroy_old_data));
  log(logger, "  options.backup_log_files: %d",
      static_cast<int>(backup_log_files));
  log(logger, " options.backup_rate_limit: %" priu64, backup_rate_limit);
  log(logger, "options.restore_rate_limit: %" priu64, restore_rate_limit);
}

// -------- backupengineimpl class ---------
class backupengineimpl : public backupengine {
 public:
  backupengineimpl(env* db_env, const backupabledboptions& options,
                   bool read_only = false);
  ~backupengineimpl();
  status createnewbackup(db* db, bool flush_before_backup = false);
  status purgeoldbackups(uint32_t num_backups_to_keep);
  status deletebackup(backupid backup_id);
  void stopbackup() {
    stop_backup_.store(true, std::memory_order_release);
  }

  void getbackupinfo(std::vector<backupinfo>* backup_info);
  status restoredbfrombackup(backupid backup_id, const std::string& db_dir,
                             const std::string& wal_dir,
                             const restoreoptions& restore_options =
                                 restoreoptions());
  status restoredbfromlatestbackup(const std::string& db_dir,
                                   const std::string& wal_dir,
                                   const restoreoptions& restore_options =
                                       restoreoptions()) {
    return restoredbfrombackup(latest_backup_id_, db_dir, wal_dir,
                               restore_options);
  }

 private:
  void deletechildren(const std::string& dir, uint32_t file_type_filter = 0);

  struct fileinfo {
    fileinfo(const std::string& fname, uint64_t sz, uint32_t checksum)
      : refs(0), filename(fname), size(sz), checksum_value(checksum) {}

    int refs;
    const std::string filename;
    const uint64_t size;
    uint32_t checksum_value;
  };

  class backupmeta {
   public:
    backupmeta(const std::string& meta_filename,
        std::unordered_map<std::string, fileinfo>* file_infos, env* env)
      : timestamp_(0), size_(0), meta_filename_(meta_filename),
        file_infos_(file_infos), env_(env) {}

    ~backupmeta() {}

    void recordtimestamp() {
      env_->getcurrenttime(&timestamp_);
    }
    int64_t gettimestamp() const {
      return timestamp_;
    }
    uint64_t getsize() const {
      return size_;
    }
    void setsequencenumber(uint64_t sequence_number) {
      sequence_number_ = sequence_number;
    }
    uint64_t getsequencenumber() {
      return sequence_number_;
    }

    status addfile(const fileinfo& file_info);

    void delete(bool delete_meta = true);

    bool empty() {
      return files_.empty();
    }

    const std::vector<std::string>& getfiles() {
      return files_;
    }

    status loadfromfile(const std::string& backup_dir);
    status storetofile(bool sync);

   private:
    int64_t timestamp_;
    // sequence number is only approximate, should not be used
    // by clients
    uint64_t sequence_number_;
    uint64_t size_;
    std::string const meta_filename_;
    // files with relative paths (without "/" prefix!!)
    std::vector<std::string> files_;
    std::unordered_map<std::string, fileinfo>* file_infos_;
    env* env_;

    static const size_t max_backup_meta_file_size_ = 10 * 1024 * 1024;  // 10mb
  };  // backupmeta

  inline std::string getabsolutepath(
      const std::string &relative_path = "") const {
    assert(relative_path.size() == 0 || relative_path[0] != '/');
    return options_.backup_dir + "/" + relative_path;
  }
  inline std::string getprivatedirrel() const {
    return "private";
  }
  inline std::string getsharedchecksumdirrel() const {
    return "shared_checksum";
  }
  inline std::string getprivatefilerel(backupid backup_id,
                                       bool tmp = false,
                                       const std::string& file = "") const {
    assert(file.size() == 0 || file[0] != '/');
    return getprivatedirrel() + "/" + std::to_string(backup_id) +
           (tmp ? ".tmp" : "") + "/" + file;
  }
  inline std::string getsharedfilerel(const std::string& file = "",
                                      bool tmp = false) const {
    assert(file.size() == 0 || file[0] != '/');
    return "shared/" + file + (tmp ? ".tmp" : "");
  }
  inline std::string getsharedfilewithchecksumrel(const std::string& file = "",
                                                  bool tmp = false) const {
    assert(file.size() == 0 || file[0] != '/');
    return getsharedchecksumdirrel() + "/" + file + (tmp ? ".tmp" : "");
  }
  inline std::string getsharedfilewithchecksum(const std::string& file,
                                               const uint32_t checksum_value,
                                               const uint64_t file_size) const {
    assert(file.size() == 0 || file[0] != '/');
    std::string file_copy = file;
    return file_copy.insert(file_copy.find_last_of('.'),
                            "_" + std::to_string(checksum_value)
                              + "_" + std::to_string(file_size));
  }
  inline std::string getfilefromchecksumfile(const std::string& file) const {
    assert(file.size() == 0 || file[0] != '/');
    std::string file_copy = file;
    size_t first_underscore = file_copy.find_first_of('_');
    return file_copy.erase(first_underscore,
                           file_copy.find_last_of('.') - first_underscore);
  }
  inline std::string getlatestbackupfile(bool tmp = false) const {
    return getabsolutepath(std::string("latest_backup") + (tmp ? ".tmp" : ""));
  }
  inline std::string getbackupmetadir() const {
    return getabsolutepath("meta");
  }
  inline std::string getbackupmetafile(backupid backup_id) const {
    return getbackupmetadir() + "/" + std::to_string(backup_id);
  }

  status getlatestbackupfilecontents(uint32_t* latest_backup);
  status putlatestbackupfilecontents(uint32_t latest_backup);
  // if size_limit == 0, there is no size limit, copy everything
  status copyfile(const std::string& src,
                  const std::string& dst,
                  env* src_env,
                  env* dst_env,
                  bool sync,
                  backupratelimiter* rate_limiter,
                  uint64_t* size = nullptr,
                  uint32_t* checksum_value = nullptr,
                  uint64_t size_limit = 0);
  // if size_limit == 0, there is no size limit, copy everything
  status backupfile(backupid backup_id,
                    backupmeta* backup,
                    bool shared,
                    const std::string& src_dir,
                    const std::string& src_fname,  // starts with "/"
                    backupratelimiter* rate_limiter,
                    uint64_t size_limit = 0,
                    bool shared_checksum = false);

  status calculatechecksum(const std::string& src,
                           env* src_env,
                           uint64_t size_limit,
                           uint32_t* checksum_value);

  // will delete all the files we don't need anymore
  // if full_scan == true, it will do the full scan of files/ directory
  // and delete all the files that are not referenced from backuped_file_infos__
  void garbagecollection(bool full_scan);

  // backup state data
  backupid latest_backup_id_;
  std::map<backupid, backupmeta> backups_;
  std::unordered_map<std::string, fileinfo> backuped_file_infos_;
  std::vector<backupid> obsolete_backups_;
  std::atomic<bool> stop_backup_;

  // options data
  backupabledboptions options_;
  env* db_env_;
  env* backup_env_;

  // directories
  unique_ptr<directory> backup_directory_;
  unique_ptr<directory> shared_directory_;
  unique_ptr<directory> meta_directory_;
  unique_ptr<directory> private_directory_;

  static const size_t kdefaultcopyfilebuffersize = 5 * 1024 * 1024ll;  // 5mb
  size_t copy_file_buffer_size_;
  bool read_only_;
};

backupengine* backupengine::newbackupengine(
    env* db_env, const backupabledboptions& options) {
  return new backupengineimpl(db_env, options);
}

backupengineimpl::backupengineimpl(env* db_env,
                                   const backupabledboptions& options,
                                   bool read_only)
    : stop_backup_(false),
      options_(options),
      db_env_(db_env),
      backup_env_(options.backup_env != nullptr ? options.backup_env : db_env_),
      copy_file_buffer_size_(kdefaultcopyfilebuffersize),
      read_only_(read_only) {
  if (read_only_) {
    log(options_.info_log, "starting read_only backup engine");
  }
  options_.dump(options_.info_log);

  if (!read_only_) {
    // create all the dirs we need
    backup_env_->createdirifmissing(getabsolutepath());
    backup_env_->newdirectory(getabsolutepath(), &backup_directory_);
    if (options_.share_table_files) {
      if (options_.share_files_with_checksum) {
        backup_env_->createdirifmissing(getabsolutepath(
            getsharedfilewithchecksumrel()));
        backup_env_->newdirectory(getabsolutepath(
            getsharedfilewithchecksumrel()), &shared_directory_);
      } else {
        backup_env_->createdirifmissing(getabsolutepath(getsharedfilerel()));
        backup_env_->newdirectory(getabsolutepath(getsharedfilerel()),
                                  &shared_directory_);
      }
    }
    backup_env_->createdirifmissing(getabsolutepath(getprivatedirrel()));
    backup_env_->newdirectory(getabsolutepath(getprivatedirrel()),
                              &private_directory_);
    backup_env_->createdirifmissing(getbackupmetadir());
    backup_env_->newdirectory(getbackupmetadir(), &meta_directory_);
  }

  std::vector<std::string> backup_meta_files;
  backup_env_->getchildren(getbackupmetadir(), &backup_meta_files);
  // create backups_ structure
  for (auto& file : backup_meta_files) {
    backupid backup_id = 0;
    sscanf(file.c_str(), "%u", &backup_id);
    if (backup_id == 0 || file != std::to_string(backup_id)) {
      if (!read_only_) {
        // invalid file name, delete that
        backup_env_->deletefile(getbackupmetadir() + "/" + file);
      }
      continue;
    }
    assert(backups_.find(backup_id) == backups_.end());
    backups_.insert(std::make_pair(
        backup_id, backupmeta(getbackupmetafile(backup_id),
                              &backuped_file_infos_, backup_env_)));
  }

  if (options_.destroy_old_data) {  // destory old data
    assert(!read_only_);
    for (auto& backup : backups_) {
      backup.second.delete();
      obsolete_backups_.push_back(backup.first);
    }
    backups_.clear();
    // start from beginning
    latest_backup_id_ = 0;
    // garbagecollection() will do the actual deletion
  } else {  // load data from storage
    // load the backups if any
    for (auto& backup : backups_) {
      status s = backup.second.loadfromfile(options_.backup_dir);
      if (!s.ok()) {
        log(options_.info_log, "backup %u corrupted -- %s", backup.first,
            s.tostring().c_str());
        if (!read_only_) {
          log(options_.info_log, "-> deleting backup %u", backup.first);
        }
        backup.second.delete(!read_only_);
        obsolete_backups_.push_back(backup.first);
      }
    }
    // delete obsolete backups from the structure
    for (auto ob : obsolete_backups_) {
      backups_.erase(ob);
    }

    status s = getlatestbackupfilecontents(&latest_backup_id_);

    // if latest backup file is corrupted or non-existent
    // set latest backup as the biggest backup we have
    // or 0 if we have no backups
    if (!s.ok() ||
        backups_.find(latest_backup_id_) == backups_.end()) {
      auto itr = backups_.end();
      latest_backup_id_ = (itr == backups_.begin()) ? 0 : (--itr)->first;
    }
  }

  // delete any backups that claim to be later than latest
  for (auto itr = backups_.upper_bound(latest_backup_id_);
       itr != backups_.end();) {
    itr->second.delete();
    obsolete_backups_.push_back(itr->first);
    itr = backups_.erase(itr);
  }

  if (!read_only_) {
    putlatestbackupfilecontents(latest_backup_id_);  // ignore errors
    garbagecollection(true);
  }
  log(options_.info_log, "initialized backupengine, the latest backup is %u.",
      latest_backup_id_);
}

backupengineimpl::~backupengineimpl() { logflush(options_.info_log); }

status backupengineimpl::createnewbackup(db* db, bool flush_before_backup) {
  assert(!read_only_);
  status s;
  std::vector<std::string> live_files;
  vectorlogptr live_wal_files;
  uint64_t manifest_file_size = 0;
  uint64_t sequence_number = db->getlatestsequencenumber();

  s = db->disablefiledeletions();
  if (s.ok()) {
    // this will return live_files prefixed with "/"
    s = db->getlivefiles(live_files, &manifest_file_size, flush_before_backup);
  }
  // if we didn't flush before backup, we need to also get wal files
  if (s.ok() && !flush_before_backup && options_.backup_log_files) {
    // returns file names prefixed with "/"
    s = db->getsortedwalfiles(live_wal_files);
  }
  if (!s.ok()) {
    db->enablefiledeletions(false);
    return s;
  }

  backupid new_backup_id = latest_backup_id_ + 1;
  assert(backups_.find(new_backup_id) == backups_.end());
  auto ret = backups_.insert(std::make_pair(
      new_backup_id, backupmeta(getbackupmetafile(new_backup_id),
                                &backuped_file_infos_, backup_env_)));
  assert(ret.second == true);
  auto& new_backup = ret.first->second;
  new_backup.recordtimestamp();
  new_backup.setsequencenumber(sequence_number);

  log(options_.info_log, "started the backup process -- creating backup %u",
      new_backup_id);

  // create temporary private dir
  s = backup_env_->createdir(
      getabsolutepath(getprivatefilerel(new_backup_id, true)));

  unique_ptr<backupratelimiter> rate_limiter;
  if (options_.backup_rate_limit > 0) {
    copy_file_buffer_size_ = options_.backup_rate_limit / 10;
    rate_limiter.reset(new backupratelimiter(db_env_,
          options_.backup_rate_limit, copy_file_buffer_size_));
  }

  // copy live_files
  for (size_t i = 0; s.ok() && i < live_files.size(); ++i) {
    uint64_t number;
    filetype type;
    bool ok = parsefilename(live_files[i], &number, &type);
    if (!ok) {
      assert(false);
      return status::corruption("can't parse file name. this is very bad");
    }
    // we should only get sst, manifest and current files here
    assert(type == ktablefile || type == kdescriptorfile ||
           type == kcurrentfile);

    // rules:
    // * if it's ktablefile, then it's shared
    // * if it's kdescriptorfile, limit the size to manifest_file_size
    s = backupfile(new_backup_id,
                   &new_backup,
                   options_.share_table_files && type == ktablefile,
                   db->getname(),            /* src_dir */
                   live_files[i],            /* src_fname */
                   rate_limiter.get(),
                   (type == kdescriptorfile) ? manifest_file_size : 0,
                   options_.share_files_with_checksum && type == ktablefile);
  }

  // copy wal files
  for (size_t i = 0; s.ok() && i < live_wal_files.size(); ++i) {
    if (live_wal_files[i]->type() == kalivelogfile) {
      // we only care about live log files
      // copy the file into backup_dir/files/<new backup>/
      s = backupfile(new_backup_id,
                     &new_backup,
                     false, /* not shared */
                     db->getoptions().wal_dir,
                     live_wal_files[i]->pathname(),
                     rate_limiter.get());
    }
  }

  // we copied all the files, enable file deletions
  db->enablefiledeletions(false);

  if (s.ok()) {
    // move tmp private backup to real backup folder
    s = backup_env_->renamefile(
        getabsolutepath(getprivatefilerel(new_backup_id, true)),  // tmp
        getabsolutepath(getprivatefilerel(new_backup_id, false)));
  }

  if (s.ok()) {
    // persist the backup metadata on the disk
    s = new_backup.storetofile(options_.sync);
  }
  if (s.ok()) {
    // install the newly created backup meta! (atomic)
    s = putlatestbackupfilecontents(new_backup_id);
  }
  if (s.ok() && options_.sync) {
    unique_ptr<directory> backup_private_directory;
    backup_env_->newdirectory(
        getabsolutepath(getprivatefilerel(new_backup_id, false)),
        &backup_private_directory);
    if (backup_private_directory != nullptr) {
      backup_private_directory->fsync();
    }
    if (private_directory_ != nullptr) {
      private_directory_->fsync();
    }
    if (meta_directory_ != nullptr) {
      meta_directory_->fsync();
    }
    if (shared_directory_ != nullptr) {
      shared_directory_->fsync();
    }
    if (backup_directory_ != nullptr) {
      backup_directory_->fsync();
    }
  }

  if (!s.ok()) {
    // clean all the files we might have created
    log(options_.info_log, "backup failed -- %s", s.tostring().c_str());
    backups_.erase(new_backup_id);
    garbagecollection(true);
    return s;
  }

  // here we know that we succeeded and installed the new backup
  // in the latest_backup file
  latest_backup_id_ = new_backup_id;
  log(options_.info_log, "backup done. all is good");
  return s;
}

status backupengineimpl::purgeoldbackups(uint32_t num_backups_to_keep) {
  assert(!read_only_);
  log(options_.info_log, "purging old backups, keeping %u",
      num_backups_to_keep);
  while (num_backups_to_keep < backups_.size()) {
    log(options_.info_log, "deleting backup %u", backups_.begin()->first);
    backups_.begin()->second.delete();
    obsolete_backups_.push_back(backups_.begin()->first);
    backups_.erase(backups_.begin());
  }
  garbagecollection(false);
  return status::ok();
}

status backupengineimpl::deletebackup(backupid backup_id) {
  assert(!read_only_);
  log(options_.info_log, "deleting backup %u", backup_id);
  auto backup = backups_.find(backup_id);
  if (backup == backups_.end()) {
    return status::notfound("backup not found");
  }
  backup->second.delete();
  obsolete_backups_.push_back(backup_id);
  backups_.erase(backup);
  garbagecollection(false);
  return status::ok();
}

void backupengineimpl::getbackupinfo(std::vector<backupinfo>* backup_info) {
  backup_info->reserve(backups_.size());
  for (auto& backup : backups_) {
    if (!backup.second.empty()) {
      backup_info->push_back(backupinfo(
          backup.first, backup.second.gettimestamp(), backup.second.getsize()));
    }
  }
}

status backupengineimpl::restoredbfrombackup(
    backupid backup_id, const std::string& db_dir, const std::string& wal_dir,
    const restoreoptions& restore_options) {
  auto backup_itr = backups_.find(backup_id);
  if (backup_itr == backups_.end()) {
    return status::notfound("backup not found");
  }
  auto& backup = backup_itr->second;
  if (backup.empty()) {
    return status::notfound("backup not found");
  }

  log(options_.info_log, "restoring backup id %u\n", backup_id);
  log(options_.info_log, "keep_log_files: %d\n",
      static_cast<int>(restore_options.keep_log_files));

  // just in case. ignore errors
  db_env_->createdirifmissing(db_dir);
  db_env_->createdirifmissing(wal_dir);

  if (restore_options.keep_log_files) {
    // delete files in db_dir, but keep all the log files
    deletechildren(db_dir, 1 << klogfile);
    // move all the files from archive dir to wal_dir
    std::string archive_dir = archivaldirectory(wal_dir);
    std::vector<std::string> archive_files;
    db_env_->getchildren(archive_dir, &archive_files);  // ignore errors
    for (const auto& f : archive_files) {
      uint64_t number;
      filetype type;
      bool ok = parsefilename(f, &number, &type);
      if (ok && type == klogfile) {
        log(options_.info_log, "moving log file from archive/ to wal_dir: %s",
            f.c_str());
        status s =
            db_env_->renamefile(archive_dir + "/" + f, wal_dir + "/" + f);
        if (!s.ok()) {
          // if we can't move log file from archive_dir to wal_dir,
          // we should fail, since it might mean data loss
          return s;
        }
      }
    }
  } else {
    deletechildren(wal_dir);
    deletechildren(archivaldirectory(wal_dir));
    deletechildren(db_dir);
  }

  unique_ptr<backupratelimiter> rate_limiter;
  if (options_.restore_rate_limit > 0) {
    copy_file_buffer_size_ = options_.restore_rate_limit / 10;
    rate_limiter.reset(new backupratelimiter(db_env_,
          options_.restore_rate_limit, copy_file_buffer_size_));
  }
  status s;
  for (auto& file : backup.getfiles()) {
    std::string dst;
    // 1. extract the filename
    size_t slash = file.find_last_of('/');
    // file will either be shared/<file>, shared_checksum/<file_crc32_size>
    // or private/<number>/<file>
    assert(slash != std::string::npos);
    dst = file.substr(slash + 1);

    // if the file was in shared_checksum, extract the real file name
    // in this case the file is <number>_<checksum>_<size>.<type>
    if (file.substr(0, slash) == getsharedchecksumdirrel()) {
      dst = getfilefromchecksumfile(dst);
    }

    // 2. find the filetype
    uint64_t number;
    filetype type;
    bool ok = parsefilename(dst, &number, &type);
    if (!ok) {
      return status::corruption("backup corrupted");
    }
    // 3. construct the final path
    // klogfile lives in wal_dir and all the rest live in db_dir
    dst = ((type == klogfile) ? wal_dir : db_dir) +
      "/" + dst;

    log(options_.info_log, "restoring %s to %s\n", file.c_str(), dst.c_str());
    uint32_t checksum_value;
    s = copyfile(getabsolutepath(file), dst, backup_env_, db_env_, false,
                 rate_limiter.get(), nullptr /* size */, &checksum_value);
    if (!s.ok()) {
      break;
    }

    const auto iter = backuped_file_infos_.find(file);
    assert(iter != backuped_file_infos_.end());
    if (iter->second.checksum_value != checksum_value) {
      s = status::corruption("checksum check failed");
      break;
    }
  }

  log(options_.info_log, "restoring done -- %s\n", s.tostring().c_str());
  return s;
}

// latest backup id is an ascii representation of latest backup id
status backupengineimpl::getlatestbackupfilecontents(uint32_t* latest_backup) {
  status s;
  unique_ptr<sequentialfile> file;
  s = backup_env_->newsequentialfile(getlatestbackupfile(),
                                     &file,
                                     envoptions());
  if (!s.ok()) {
    return s;
  }

  char buf[11];
  slice data;
  s = file->read(10, &data, buf);
  if (!s.ok() || data.size() == 0) {
    return s.ok() ? status::corruption("latest backup file corrupted") : s;
  }
  buf[data.size()] = 0;

  *latest_backup = 0;
  sscanf(data.data(), "%u", latest_backup);
  if (backup_env_->fileexists(getbackupmetafile(*latest_backup)) == false) {
    s = status::corruption("latest backup file corrupted");
  }
  return status::ok();
}

// this operation has to be atomic
// writing 4 bytes to the file is atomic alright, but we should *never*
// do something like 1. delete file, 2. write new file
// we write to a tmp file and then atomically rename
status backupengineimpl::putlatestbackupfilecontents(uint32_t latest_backup) {
  assert(!read_only_);
  status s;
  unique_ptr<writablefile> file;
  envoptions env_options;
  env_options.use_mmap_writes = false;
  s = backup_env_->newwritablefile(getlatestbackupfile(true),
                                   &file,
                                   env_options);
  if (!s.ok()) {
    backup_env_->deletefile(getlatestbackupfile(true));
    return s;
  }

  char file_contents[10];
  int len = sprintf(file_contents, "%u\n", latest_backup);
  s = file->append(slice(file_contents, len));
  if (s.ok() && options_.sync) {
    file->sync();
  }
  if (s.ok()) {
    s = file->close();
  }
  if (s.ok()) {
    // atomically replace real file with new tmp
    s = backup_env_->renamefile(getlatestbackupfile(true),
                                getlatestbackupfile(false));
  }
  return s;
}

status backupengineimpl::copyfile(
    const std::string& src,
    const std::string& dst, env* src_env,
    env* dst_env, bool sync,
    backupratelimiter* rate_limiter, uint64_t* size,
    uint32_t* checksum_value,
    uint64_t size_limit) {
  status s;
  unique_ptr<writablefile> dst_file;
  unique_ptr<sequentialfile> src_file;
  envoptions env_options;
  env_options.use_mmap_writes = false;
  env_options.use_os_buffer = false;
  if (size != nullptr) {
    *size = 0;
  }
  if (checksum_value != nullptr) {
    *checksum_value = 0;
  }

  // check if size limit is set. if not, set it to very big number
  if (size_limit == 0) {
    size_limit = std::numeric_limits<uint64_t>::max();
  }

  s = src_env->newsequentialfile(src, &src_file, env_options);
  if (s.ok()) {
    s = dst_env->newwritablefile(dst, &dst_file, env_options);
  }
  if (!s.ok()) {
    return s;
  }

  unique_ptr<char[]> buf(new char[copy_file_buffer_size_]);
  slice data;

  do {
    if (stop_backup_.load(std::memory_order_acquire)) {
      return status::incomplete("backup stopped");
    }
    size_t buffer_to_read = (copy_file_buffer_size_ < size_limit) ?
      copy_file_buffer_size_ : size_limit;
    s = src_file->read(buffer_to_read, &data, buf.get());
    size_limit -= data.size();

    if (!s.ok()) {
      return s;
    }

    if (size != nullptr) {
      *size += data.size();
    }
    if (checksum_value != nullptr) {
      *checksum_value = crc32c::extend(*checksum_value, data.data(),
                                       data.size());
    }
    s = dst_file->append(data);
    if (rate_limiter != nullptr) {
      rate_limiter->reportandwait(data.size());
    }
  } while (s.ok() && data.size() > 0 && size_limit > 0);

  if (s.ok() && sync) {
    s = dst_file->sync();
  }

  return s;
}

// src_fname will always start with "/"
status backupengineimpl::backupfile(backupid backup_id, backupmeta* backup,
                                    bool shared, const std::string& src_dir,
                                    const std::string& src_fname,
                                    backupratelimiter* rate_limiter,
                                    uint64_t size_limit,
                                    bool shared_checksum) {

  assert(src_fname.size() > 0 && src_fname[0] == '/');
  std::string dst_relative = src_fname.substr(1);
  std::string dst_relative_tmp;
  status s;
  uint64_t size;
  uint32_t checksum_value = 0;

  if (shared && shared_checksum) {
    // add checksum and file length to the file name
    s = calculatechecksum(src_dir + src_fname,
                          db_env_,
                          size_limit,
                          &checksum_value);
    if (s.ok()) {
        s = db_env_->getfilesize(src_dir + src_fname, &size);
    }
    if (!s.ok()) {
         return s;
    }
    dst_relative = getsharedfilewithchecksum(dst_relative, checksum_value,
                                             size);
    dst_relative_tmp = getsharedfilewithchecksumrel(dst_relative, true);
    dst_relative = getsharedfilewithchecksumrel(dst_relative, false);
  } else if (shared) {
    dst_relative_tmp = getsharedfilerel(dst_relative, true);
    dst_relative = getsharedfilerel(dst_relative, false);
  } else {
    dst_relative_tmp = getprivatefilerel(backup_id, true, dst_relative);
    dst_relative = getprivatefilerel(backup_id, false, dst_relative);
  }
  std::string dst_path = getabsolutepath(dst_relative);
  std::string dst_path_tmp = getabsolutepath(dst_relative_tmp);

  // if it's shared, we also need to check if it exists -- if it does,
  // no need to copy it again
  if (shared && backup_env_->fileexists(dst_path)) {
    if (shared_checksum) {
      log(options_.info_log,
          "%s already present, with checksum %u and size %" priu64,
          src_fname.c_str(), checksum_value, size);
    } else {
      backup_env_->getfilesize(dst_path, &size);  // ignore error
      log(options_.info_log, "%s already present, calculate checksum",
          src_fname.c_str());
      s = calculatechecksum(src_dir + src_fname,
                            db_env_,
                            size_limit,
                            &checksum_value);
    }
  } else {
    log(options_.info_log, "copying %s", src_fname.c_str());
    s = copyfile(src_dir + src_fname,
                 dst_path_tmp,
                 db_env_,
                 backup_env_,
                 options_.sync,
                 rate_limiter,
                 &size,
                 &checksum_value,
                 size_limit);
    if (s.ok() && shared) {
      s = backup_env_->renamefile(dst_path_tmp, dst_path);
    }
  }
  if (s.ok()) {
    s = backup->addfile(fileinfo(dst_relative, size, checksum_value));
  }
  return s;
}

status backupengineimpl::calculatechecksum(const std::string& src, env* src_env,
                                           uint64_t size_limit,
                                           uint32_t* checksum_value) {
  *checksum_value = 0;
  if (size_limit == 0) {
    size_limit = std::numeric_limits<uint64_t>::max();
  }

  envoptions env_options;
  env_options.use_mmap_writes = false;
  env_options.use_os_buffer = false;

  std::unique_ptr<sequentialfile> src_file;
  status s = src_env->newsequentialfile(src, &src_file, env_options);
  if (!s.ok()) {
    return s;
  }

  std::unique_ptr<char[]> buf(new char[copy_file_buffer_size_]);
  slice data;

  do {
    if (stop_backup_.load(std::memory_order_acquire)) {
      return status::incomplete("backup stopped");
    }
    size_t buffer_to_read = (copy_file_buffer_size_ < size_limit) ?
      copy_file_buffer_size_ : size_limit;
    s = src_file->read(buffer_to_read, &data, buf.get());

    if (!s.ok()) {
      return s;
    }

    size_limit -= data.size();
    *checksum_value = crc32c::extend(*checksum_value, data.data(), data.size());
  } while (data.size() > 0 && size_limit > 0);

  return s;
}

void backupengineimpl::deletechildren(const std::string& dir,
                                      uint32_t file_type_filter) {
  std::vector<std::string> children;
  db_env_->getchildren(dir, &children);  // ignore errors

  for (const auto& f : children) {
    uint64_t number;
    filetype type;
    bool ok = parsefilename(f, &number, &type);
    if (ok && (file_type_filter & (1 << type))) {
      // don't delete this file
      continue;
    }
    db_env_->deletefile(dir + "/" + f);  // ignore errors
  }
}

void backupengineimpl::garbagecollection(bool full_scan) {
  assert(!read_only_);
  log(options_.info_log, "starting garbage collection");
  std::vector<std::string> to_delete;
  for (auto& itr : backuped_file_infos_) {
    if (itr.second.refs == 0) {
      status s = backup_env_->deletefile(getabsolutepath(itr.first));
      log(options_.info_log, "deleting %s -- %s", itr.first.c_str(),
          s.tostring().c_str());
      to_delete.push_back(itr.first);
    }
  }
  for (auto& td : to_delete) {
    backuped_file_infos_.erase(td);
  }
  if (!full_scan) {
    // take care of private dirs -- if full_scan == true, then full_scan will
    // take care of them
    for (auto backup_id : obsolete_backups_) {
      std::string private_dir = getprivatefilerel(backup_id);
      status s = backup_env_->deletedir(getabsolutepath(private_dir));
      log(options_.info_log, "deleting private dir %s -- %s",
          private_dir.c_str(), s.tostring().c_str());
    }
  }
  obsolete_backups_.clear();

  if (full_scan) {
    log(options_.info_log, "starting full scan garbage collection");
    // delete obsolete shared files
    std::vector<std::string> shared_children;
    backup_env_->getchildren(getabsolutepath(getsharedfilerel()),
                             &shared_children);
    for (auto& child : shared_children) {
      std::string rel_fname = getsharedfilerel(child);
      // if it's not refcounted, delete it
      if (backuped_file_infos_.find(rel_fname) == backuped_file_infos_.end()) {
        // this might be a directory, but deletefile will just fail in that
        // case, so we're good
        status s = backup_env_->deletefile(getabsolutepath(rel_fname));
        if (s.ok()) {
          log(options_.info_log, "deleted %s", rel_fname.c_str());
        }
      }
    }

    // delete obsolete private files
    std::vector<std::string> private_children;
    backup_env_->getchildren(getabsolutepath(getprivatedirrel()),
                             &private_children);
    for (auto& child : private_children) {
      backupid backup_id = 0;
      bool tmp_dir = child.find(".tmp") != std::string::npos;
      sscanf(child.c_str(), "%u", &backup_id);
      if (!tmp_dir &&  // if it's tmp_dir, delete it
          (backup_id == 0 || backups_.find(backup_id) != backups_.end())) {
        // it's either not a number or it's still alive. continue
        continue;
      }
      // here we have to delete the dir and all its children
      std::string full_private_path =
          getabsolutepath(getprivatefilerel(backup_id, tmp_dir));
      std::vector<std::string> subchildren;
      backup_env_->getchildren(full_private_path, &subchildren);
      for (auto& subchild : subchildren) {
        status s = backup_env_->deletefile(full_private_path + subchild);
        if (s.ok()) {
          log(options_.info_log, "deleted %s",
              (full_private_path + subchild).c_str());
        }
      }
      // finally delete the private dir
      status s = backup_env_->deletedir(full_private_path);
      log(options_.info_log, "deleted dir %s -- %s", full_private_path.c_str(),
          s.tostring().c_str());
    }
  }
}

// ------- backupmeta class --------

status backupengineimpl::backupmeta::addfile(const fileinfo& file_info) {
  size_ += file_info.size;
  files_.push_back(file_info.filename);

  auto itr = file_infos_->find(file_info.filename);
  if (itr == file_infos_->end()) {
    auto ret = file_infos_->insert({file_info.filename, file_info});
    if (ret.second) {
      ret.first->second.refs = 1;
    } else {
      // if this happens, something is seriously wrong
      return status::corruption("in memory metadata insertion error");
    }
  } else {
    if (itr->second.checksum_value != file_info.checksum_value) {
      return status::corruption("checksum mismatch for existing backup file");
    }
    ++itr->second.refs;  // increase refcount if already present
  }

  return status::ok();
}

void backupengineimpl::backupmeta::delete(bool delete_meta) {
  for (const auto& file : files_) {
    auto itr = file_infos_->find(file);
    assert(itr != file_infos_->end());
    --(itr->second.refs);  // decrease refcount
  }
  files_.clear();
  // delete meta file
  if (delete_meta) {
    env_->deletefile(meta_filename_);
  }
  timestamp_ = 0;
}

// each backup meta file is of the format:
// <timestamp>
// <seq number>
// <number of files>
// <file1> <crc32(literal string)> <crc32_value>
// <file2> <crc32(literal string)> <crc32_value>
// ...
status backupengineimpl::backupmeta::loadfromfile(
    const std::string& backup_dir) {
  assert(empty());
  status s;
  unique_ptr<sequentialfile> backup_meta_file;
  s = env_->newsequentialfile(meta_filename_, &backup_meta_file, envoptions());
  if (!s.ok()) {
    return s;
  }

  unique_ptr<char[]> buf(new char[max_backup_meta_file_size_ + 1]);
  slice data;
  s = backup_meta_file->read(max_backup_meta_file_size_, &data, buf.get());

  if (!s.ok() || data.size() == max_backup_meta_file_size_) {
    return s.ok() ? status::corruption("file size too big") : s;
  }
  buf[data.size()] = 0;

  uint32_t num_files = 0;
  int bytes_read = 0;
  sscanf(data.data(), "%" prid64 "%n", &timestamp_, &bytes_read);
  data.remove_prefix(bytes_read + 1);  // +1 for '\n'
  sscanf(data.data(), "%" priu64 "%n", &sequence_number_, &bytes_read);
  data.remove_prefix(bytes_read + 1);  // +1 for '\n'
  sscanf(data.data(), "%u%n", &num_files, &bytes_read);
  data.remove_prefix(bytes_read + 1);  // +1 for '\n'

  std::vector<fileinfo> files;

  for (uint32_t i = 0; s.ok() && i < num_files; ++i) {
    auto line = getsliceuntil(&data, '\n');
    std::string filename = getsliceuntil(&line, ' ').tostring();

    uint64_t size;
    s = env_->getfilesize(backup_dir + "/" + filename, &size);
    if (!s.ok()) {
      return s;
    }

    if (line.empty()) {
      return status::corruption("file checksum is missing");
    }

    uint32_t checksum_value = 0;
    if (line.starts_with("crc32 ")) {
      line.remove_prefix(6);
      sscanf(line.data(), "%u", &checksum_value);
      if (memcmp(line.data(), std::to_string(checksum_value).c_str(),
                 line.size() - 1) != 0) {
        return status::corruption("invalid checksum value");
      }
    } else {
      return status::corruption("unknown checksum type");
    }

    files.emplace_back(filename, size, checksum_value);
  }

  if (s.ok() && data.size() > 0) {
    // file has to be read completely. if not, we count it as corruption
    s = status::corruption("tailing data in backup meta file");
  }

  if (s.ok()) {
    for (const auto& file_info : files) {
      s = addfile(file_info);
      if (!s.ok()) {
        break;
      }
    }
  }

  return s;
}

status backupengineimpl::backupmeta::storetofile(bool sync) {
  status s;
  unique_ptr<writablefile> backup_meta_file;
  envoptions env_options;
  env_options.use_mmap_writes = false;
  s = env_->newwritablefile(meta_filename_ + ".tmp", &backup_meta_file,
                            env_options);
  if (!s.ok()) {
    return s;
  }

  unique_ptr<char[]> buf(new char[max_backup_meta_file_size_]);
  int len = 0, buf_size = max_backup_meta_file_size_;
  len += snprintf(buf.get(), buf_size, "%" prid64 "\n", timestamp_);
  len += snprintf(buf.get() + len, buf_size - len, "%" priu64 "\n",
                  sequence_number_);
  len += snprintf(buf.get() + len, buf_size - len, "%zu\n", files_.size());
  for (const auto& file : files_) {
    const auto& iter = file_infos_->find(file);

    assert(iter != file_infos_->end());
    // use crc32 for now, switch to something else if needed
    len += snprintf(buf.get() + len, buf_size - len, "%s crc32 %u\n",
                    file.c_str(), iter->second.checksum_value);
  }

  s = backup_meta_file->append(slice(buf.get(), (size_t)len));
  if (s.ok() && sync) {
    s = backup_meta_file->sync();
  }
  if (s.ok()) {
    s = backup_meta_file->close();
  }
  if (s.ok()) {
    s = env_->renamefile(meta_filename_ + ".tmp", meta_filename_);
  }
  return s;
}

// -------- backupenginereadonlyimpl ---------
class backupenginereadonlyimpl : public backupenginereadonly {
 public:
  backupenginereadonlyimpl(env* db_env, const backupabledboptions& options)
      : backup_engine_(new backupengineimpl(db_env, options, true)) {}

  virtual ~backupenginereadonlyimpl() {}

  virtual void getbackupinfo(std::vector<backupinfo>* backup_info) {
    backup_engine_->getbackupinfo(backup_info);
  }

  virtual status restoredbfrombackup(
      backupid backup_id, const std::string& db_dir, const std::string& wal_dir,
      const restoreoptions& restore_options = restoreoptions()) {
    return backup_engine_->restoredbfrombackup(backup_id, db_dir, wal_dir,
                                               restore_options);
  }

  virtual status restoredbfromlatestbackup(
      const std::string& db_dir, const std::string& wal_dir,
      const restoreoptions& restore_options = restoreoptions()) {
    return backup_engine_->restoredbfromlatestbackup(db_dir, wal_dir,
                                                     restore_options);
  }

 private:
  std::unique_ptr<backupengineimpl> backup_engine_;
};

backupenginereadonly* backupenginereadonly::newreadonlybackupengine(
    env* db_env, const backupabledboptions& options) {
  if (options.destroy_old_data) {
    assert(false);
    return nullptr;
  }
  return new backupenginereadonlyimpl(db_env, options);
}

// --- backupabledb methods --------

backupabledb::backupabledb(db* db, const backupabledboptions& options)
    : stackabledb(db),
      backup_engine_(new backupengineimpl(db->getenv(), options)) {}

backupabledb::~backupabledb() {
  delete backup_engine_;
}

status backupabledb::createnewbackup(bool flush_before_backup) {
  return backup_engine_->createnewbackup(this, flush_before_backup);
}

void backupabledb::getbackupinfo(std::vector<backupinfo>* backup_info) {
  backup_engine_->getbackupinfo(backup_info);
}

status backupabledb::purgeoldbackups(uint32_t num_backups_to_keep) {
  return backup_engine_->purgeoldbackups(num_backups_to_keep);
}

status backupabledb::deletebackup(backupid backup_id) {
  return backup_engine_->deletebackup(backup_id);
}

void backupabledb::stopbackup() {
  backup_engine_->stopbackup();
}

// --- restorebackupabledb methods ------

restorebackupabledb::restorebackupabledb(env* db_env,
                                         const backupabledboptions& options)
    : backup_engine_(new backupengineimpl(db_env, options)) {}

restorebackupabledb::~restorebackupabledb() {
  delete backup_engine_;
}

void
restorebackupabledb::getbackupinfo(std::vector<backupinfo>* backup_info) {
  backup_engine_->getbackupinfo(backup_info);
}

status restorebackupabledb::restoredbfrombackup(
    backupid backup_id, const std::string& db_dir, const std::string& wal_dir,
    const restoreoptions& restore_options) {
  return backup_engine_->restoredbfrombackup(backup_id, db_dir, wal_dir,
                                             restore_options);
}

status restorebackupabledb::restoredbfromlatestbackup(
    const std::string& db_dir, const std::string& wal_dir,
    const restoreoptions& restore_options) {
  return backup_engine_->restoredbfromlatestbackup(db_dir, wal_dir,
                                                   restore_options);
}

status restorebackupabledb::purgeoldbackups(uint32_t num_backups_to_keep) {
  return backup_engine_->purgeoldbackups(num_backups_to_keep);
}

status restorebackupabledb::deletebackup(backupid backup_id) {
  return backup_engine_->deletebackup(backup_id);
}

}  // namespace rocksdb

#endif  // rocksdb_lite
