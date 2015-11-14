// protocol buffers - google's data interchange format
// copyright 2008 google inc.  all rights reserved.
// http://code.google.com/p/protobuf/
//
// redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * neither the name of google inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// this software is provided by the copyright holders and contributors
// "as is" and any express or implied warranties, including, but not
// limited to, the implied warranties of merchantability and fitness for
// a particular purpose are disclaimed. in no event shall the copyright
// owner or contributors be liable for any direct, indirect, incidental,
// special, exemplary, or consequential damages (including, but not
// limited to, procurement of substitute goods or services; loss of use,
// data, or profits; or business interruption) however caused and on any
// theory of liability, whether in contract, strict liability, or tort
// (including negligence or otherwise) arising in any way out of the use
// of this software, even if advised of the possibility of such damage.

// author: kenton@google.com (kenton varda)
// emulates google3/file/base/file.cc

#include <google/protobuf/testing/file.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _msc_ver
#define win32_lean_and_mean  // yeah, right
#include <windows.h>         // find*file().  :(
#include <io.h>
#include <direct.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif
#include <errno.h>

namespace google {
namespace protobuf {

#ifdef _win32
#define mkdir(name, mode) mkdir(name)
// windows doesn't have symbolic links.
#define lstat stat
#ifndef f_ok
#define f_ok 00  // not defined by msvc for whatever reason
#endif
#endif

bool file::exists(const string& name) {
  return access(name.c_str(), f_ok) == 0;
}

bool file::readfiletostring(const string& name, string* output) {
  char buffer[1024];
  file* file = fopen(name.c_str(), "rb");
  if (file == null) return false;

  while (true) {
    size_t n = fread(buffer, 1, sizeof(buffer), file);
    if (n <= 0) break;
    output->append(buffer, n);
  }

  int error = ferror(file);
  if (fclose(file) != 0) return false;
  return error == 0;
}

void file::readfiletostringordie(const string& name, string* output) {
  google_check(readfiletostring(name, output)) << "could not read: " << name;
}

void file::writestringtofileordie(const string& contents, const string& name) {
  file* file = fopen(name.c_str(), "wb");
  google_check(file != null)
      << "fopen(" << name << ", \"wb\"): " << strerror(errno);
  google_check_eq(fwrite(contents.data(), 1, contents.size(), file),
                  contents.size())
      << "fwrite(" << name << "): " << strerror(errno);
  google_check(fclose(file) == 0)
      << "fclose(" << name << "): " << strerror(errno);
}

bool file::createdir(const string& name, int mode) {
  return mkdir(name.c_str(), mode) == 0;
}

bool file::recursivelycreatedir(const string& path, int mode) {
  if (createdir(path, mode)) return true;

  if (exists(path)) return false;

  // try creating the parent.
  string::size_type slashpos = path.find_last_of('/');
  if (slashpos == string::npos) {
    // no parent given.
    return false;
  }

  return recursivelycreatedir(path.substr(0, slashpos), mode) &&
         createdir(path, mode);
}

void file::deleterecursively(const string& name,
                             void* dummy1, void* dummy2) {
  // we don't care too much about error checking here since this is only used
  // in tests to delete temporary directories that are under /tmp anyway.

#ifdef _msc_ver
  // this interface is so weird.
  win32_find_data find_data;
  handle find_handle = findfirstfile((name + "/*").c_str(), &find_data);
  if (find_handle == invalid_handle_value) {
    // just delete it, whatever it is.
    deletefile(name.c_str());
    removedirectory(name.c_str());
    return;
  }

  do {
    string entry_name = find_data.cfilename;
    if (entry_name != "." && entry_name != "..") {
      string path = name + "/" + entry_name;
      if (find_data.dwfileattributes & file_attribute_directory) {
        deleterecursively(path, null, null);
        removedirectory(path.c_str());
      } else {
        deletefile(path.c_str());
      }
    }
  } while(findnextfile(find_handle, &find_data));
  findclose(find_handle);

  removedirectory(name.c_str());
#else
  // use opendir()!  yay!
  // lstat = don't follow symbolic links.
  struct stat stats;
  if (lstat(name.c_str(), &stats) != 0) return;

  if (s_isdir(stats.st_mode)) {
    dir* dir = opendir(name.c_str());
    if (dir != null) {
      while (true) {
        struct dirent* entry = readdir(dir);
        if (entry == null) break;
        string entry_name = entry->d_name;
        if (entry_name != "." && entry_name != "..") {
          deleterecursively(name + "/" + entry_name, null, null);
        }
      }
    }

    closedir(dir);
    rmdir(name.c_str());

  } else if (s_isreg(stats.st_mode)) {
    remove(name.c_str());
  }
#endif
}

}  // namespace protobuf
}  // namespace google
