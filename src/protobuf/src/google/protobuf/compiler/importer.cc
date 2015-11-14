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
//  based on original protocol buffers design by
//  sanjay ghemawat, jeff dean, and others.

#ifdef _msc_ver
#include <io.h>
#else
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <algorithm>

#include <google/protobuf/compiler/importer.h>

#include <google/protobuf/compiler/parser.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {

#ifdef _win32
#ifndef f_ok
#define f_ok 00  // not defined by msvc for whatever reason
#endif
#include <ctype.h>
#endif

// returns true if the text looks like a windows-style absolute path, starting
// with a drive letter.  example:  "c:\foo".  todo(kenton):  share this with
// copy in command_line_interface.cc?
static bool iswindowsabsolutepath(const string& text) {
#if defined(_win32) || defined(__cygwin__)
  return text.size() >= 3 && text[1] == ':' &&
         isalpha(text[0]) &&
         (text[2] == '/' || text[2] == '\\') &&
         text.find_last_of(':') == 1;
#else
  return false;
#endif
}

multifileerrorcollector::~multifileerrorcollector() {}

// this class serves two purposes:
// - it implements the errorcollector interface (used by tokenizer and parser)
//   in terms of multifileerrorcollector, using a particular filename.
// - it lets us check if any errors have occurred.
class sourcetreedescriptordatabase::singlefileerrorcollector
    : public io::errorcollector {
 public:
  singlefileerrorcollector(const string& filename,
                           multifileerrorcollector* multi_file_error_collector)
    : filename_(filename),
      multi_file_error_collector_(multi_file_error_collector),
      had_errors_(false) {}
  ~singlefileerrorcollector() {}

  bool had_errors() { return had_errors_; }

  // implements errorcollector ---------------------------------------
  void adderror(int line, int column, const string& message) {
    if (multi_file_error_collector_ != null) {
      multi_file_error_collector_->adderror(filename_, line, column, message);
    }
    had_errors_ = true;
  }

 private:
  string filename_;
  multifileerrorcollector* multi_file_error_collector_;
  bool had_errors_;
};

// ===================================================================

sourcetreedescriptordatabase::sourcetreedescriptordatabase(
    sourcetree* source_tree)
  : source_tree_(source_tree),
    error_collector_(null),
    using_validation_error_collector_(false),
    validation_error_collector_(this) {}

sourcetreedescriptordatabase::~sourcetreedescriptordatabase() {}

bool sourcetreedescriptordatabase::findfilebyname(
    const string& filename, filedescriptorproto* output) {
  scoped_ptr<io::zerocopyinputstream> input(source_tree_->open(filename));
  if (input == null) {
    if (error_collector_ != null) {
      error_collector_->adderror(filename, -1, 0, "file not found.");
    }
    return false;
  }

  // set up the tokenizer and parser.
  singlefileerrorcollector file_error_collector(filename, error_collector_);
  io::tokenizer tokenizer(input.get(), &file_error_collector);

  parser parser;
  if (error_collector_ != null) {
    parser.recorderrorsto(&file_error_collector);
  }
  if (using_validation_error_collector_) {
    parser.recordsourcelocationsto(&source_locations_);
  }

  // parse it.
  output->set_name(filename);
  return parser.parse(&tokenizer, output) &&
         !file_error_collector.had_errors();
}

bool sourcetreedescriptordatabase::findfilecontainingsymbol(
    const string& symbol_name, filedescriptorproto* output) {
  return false;
}

bool sourcetreedescriptordatabase::findfilecontainingextension(
    const string& containing_type, int field_number,
    filedescriptorproto* output) {
  return false;
}

// -------------------------------------------------------------------

sourcetreedescriptordatabase::validationerrorcollector::
validationerrorcollector(sourcetreedescriptordatabase* owner)
  : owner_(owner) {}

sourcetreedescriptordatabase::validationerrorcollector::
~validationerrorcollector() {}

void sourcetreedescriptordatabase::validationerrorcollector::adderror(
    const string& filename,
    const string& element_name,
    const message* descriptor,
    errorlocation location,
    const string& message) {
  if (owner_->error_collector_ == null) return;

  int line, column;
  owner_->source_locations_.find(descriptor, location, &line, &column);
  owner_->error_collector_->adderror(filename, line, column, message);
}

// ===================================================================

importer::importer(sourcetree* source_tree,
                   multifileerrorcollector* error_collector)
  : database_(source_tree),
    pool_(&database_, database_.getvalidationerrorcollector()) {
  database_.recorderrorsto(error_collector);
}

importer::~importer() {}

const filedescriptor* importer::import(const string& filename) {
  return pool_.findfilebyname(filename);
}

// ===================================================================

sourcetree::~sourcetree() {}

disksourcetree::disksourcetree() {}

disksourcetree::~disksourcetree() {}

static inline char lastchar(const string& str) {
  return str[str.size() - 1];
}

// given a path, returns an equivalent path with these changes:
// - on windows, any backslashes are replaced with forward slashes.
// - any instances of the directory "." are removed.
// - any consecutive '/'s are collapsed into a single slash.
// note that the resulting string may be empty.
//
// todo(kenton):  it would be nice to handle "..", e.g. so that we can figure
//   out that "foo/bar.proto" is inside "baz/../foo".  however, if baz is a
//   symlink or doesn't exist, then things get complicated, and we can't
//   actually determine this without investigating the filesystem, probably
//   in non-portable ways.  so, we punt.
//
// todo(kenton):  it would be nice to use realpath() here except that it
//   resolves symbolic links.  this could cause problems if people place
//   symbolic links in their source tree.  for example, if you executed:
//     protoc --proto_path=foo foo/bar/baz.proto
//   then if foo/bar is a symbolic link, foo/bar/baz.proto will canonicalize
//   to a path which does not appear to be under foo, and thus the compiler
//   will complain that baz.proto is not inside the --proto_path.
static string canonicalizepath(string path) {
#ifdef _win32
  // the win32 api accepts forward slashes as a path delimiter even though
  // backslashes are standard.  let's avoid confusion and use only forward
  // slashes.
  if (hasprefixstring(path, "\\\\")) {
    // avoid converting two leading backslashes.
    path = "\\\\" + stringreplace(path.substr(2), "\\", "/", true);
  } else {
    path = stringreplace(path, "\\", "/", true);
  }
#endif

  vector<string> parts;
  vector<string> canonical_parts;
  splitstringusing(path, "/", &parts);  // note:  removes empty parts.
  for (int i = 0; i < parts.size(); i++) {
    if (parts[i] == ".") {
      // ignore.
    } else {
      canonical_parts.push_back(parts[i]);
    }
  }
  string result = joinstrings(canonical_parts, "/");
  if (!path.empty() && path[0] == '/') {
    // restore leading slash.
    result = '/' + result;
  }
  if (!path.empty() && lastchar(path) == '/' &&
      !result.empty() && lastchar(result) != '/') {
    // restore trailing slash.
    result += '/';
  }
  return result;
}

static inline bool containsparentreference(const string& path) {
  return path == ".." ||
         hasprefixstring(path, "../") ||
         hassuffixstring(path, "/..") ||
         path.find("/../") != string::npos;
}

// maps a file from an old location to a new one.  typically, old_prefix is
// a virtual path and new_prefix is its corresponding disk path.  returns
// false if the filename did not start with old_prefix, otherwise replaces
// old_prefix with new_prefix and stores the result in *result.  examples:
//   string result;
//   assert(applymapping("foo/bar", "", "baz", &result));
//   assert(result == "baz/foo/bar");
//
//   assert(applymapping("foo/bar", "foo", "baz", &result));
//   assert(result == "baz/bar");
//
//   assert(applymapping("foo", "foo", "bar", &result));
//   assert(result == "bar");
//
//   assert(!applymapping("foo/bar", "baz", "qux", &result));
//   assert(!applymapping("foo/bar", "baz", "qux", &result));
//   assert(!applymapping("foobar", "foo", "baz", &result));
static bool applymapping(const string& filename,
                         const string& old_prefix,
                         const string& new_prefix,
                         string* result) {
  if (old_prefix.empty()) {
    // old_prefix matches any relative path.
    if (containsparentreference(filename)) {
      // we do not allow the file name to use "..".
      return false;
    }
    if (hasprefixstring(filename, "/") ||
        iswindowsabsolutepath(filename)) {
      // this is an absolute path, so it isn't matched by the empty string.
      return false;
    }
    result->assign(new_prefix);
    if (!result->empty()) result->push_back('/');
    result->append(filename);
    return true;
  } else if (hasprefixstring(filename, old_prefix)) {
    // old_prefix is a prefix of the filename.  is it the whole filename?
    if (filename.size() == old_prefix.size()) {
      // yep, it's an exact match.
      *result = new_prefix;
      return true;
    } else {
      // not an exact match.  is the next character a '/'?  otherwise,
      // this isn't actually a match at all.  e.g. the prefix "foo/bar"
      // does not match the filename "foo/barbaz".
      int after_prefix_start = -1;
      if (filename[old_prefix.size()] == '/') {
        after_prefix_start = old_prefix.size() + 1;
      } else if (filename[old_prefix.size() - 1] == '/') {
        // old_prefix is never empty, and canonicalized paths never have
        // consecutive '/' characters.
        after_prefix_start = old_prefix.size();
      }
      if (after_prefix_start != -1) {
        // yep.  so the prefixes are directories and the filename is a file
        // inside them.
        string after_prefix = filename.substr(after_prefix_start);
        if (containsparentreference(after_prefix)) {
          // we do not allow the file name to use "..".
          return false;
        }
        result->assign(new_prefix);
        if (!result->empty()) result->push_back('/');
        result->append(after_prefix);
        return true;
      }
    }
  }

  return false;
}

void disksourcetree::mappath(const string& virtual_path,
                             const string& disk_path) {
  mappings_.push_back(mapping(virtual_path, canonicalizepath(disk_path)));
}

disksourcetree::diskfiletovirtualfileresult
disksourcetree::diskfiletovirtualfile(
    const string& disk_file,
    string* virtual_file,
    string* shadowing_disk_file) {
  int mapping_index = -1;
  string canonical_disk_file = canonicalizepath(disk_file);

  for (int i = 0; i < mappings_.size(); i++) {
    // apply the mapping in reverse.
    if (applymapping(canonical_disk_file, mappings_[i].disk_path,
                     mappings_[i].virtual_path, virtual_file)) {
      // success.
      mapping_index = i;
      break;
    }
  }

  if (mapping_index == -1) {
    return no_mapping;
  }

  // iterate through all mappings with higher precedence and verify that none
  // of them map this file to some other existing file.
  for (int i = 0; i < mapping_index; i++) {
    if (applymapping(*virtual_file, mappings_[i].virtual_path,
                     mappings_[i].disk_path, shadowing_disk_file)) {
      if (access(shadowing_disk_file->c_str(), f_ok) >= 0) {
        // file exists.
        return shadowed;
      }
    }
  }
  shadowing_disk_file->clear();

  // verify that we can open the file.  note that this also has the side-effect
  // of verifying that we are not canonicalizing away any non-existent
  // directories.
  scoped_ptr<io::zerocopyinputstream> stream(opendiskfile(disk_file));
  if (stream == null) {
    return cannot_open;
  }

  return success;
}

bool disksourcetree::virtualfiletodiskfile(const string& virtual_file,
                                           string* disk_file) {
  scoped_ptr<io::zerocopyinputstream> stream(openvirtualfile(virtual_file,
                                                             disk_file));
  return stream != null;
}

io::zerocopyinputstream* disksourcetree::open(const string& filename) {
  return openvirtualfile(filename, null);
}

io::zerocopyinputstream* disksourcetree::openvirtualfile(
    const string& virtual_file,
    string* disk_file) {
  if (virtual_file != canonicalizepath(virtual_file) ||
      containsparentreference(virtual_file)) {
    // we do not allow importing of paths containing things like ".." or
    // consecutive slashes since the compiler expects files to be uniquely
    // identified by file name.
    return null;
  }

  for (int i = 0; i < mappings_.size(); i++) {
    string temp_disk_file;
    if (applymapping(virtual_file, mappings_[i].virtual_path,
                     mappings_[i].disk_path, &temp_disk_file)) {
      io::zerocopyinputstream* stream = opendiskfile(temp_disk_file);
      if (stream != null) {
        if (disk_file != null) {
          *disk_file = temp_disk_file;
        }
        return stream;
      }

      if (errno == eacces) {
        // the file exists but is not readable.
        // todo(kenton):  find a way to report this more nicely.
        google_log(warning) << "read access is denied for file: " << temp_disk_file;
        return null;
      }
    }
  }

  return null;
}

io::zerocopyinputstream* disksourcetree::opendiskfile(
    const string& filename) {
  int file_descriptor;
  do {
    file_descriptor = open(filename.c_str(), o_rdonly);
  } while (file_descriptor < 0 && errno == eintr);
  if (file_descriptor >= 0) {
    io::fileinputstream* result = new io::fileinputstream(file_descriptor);
    result->setcloseondelete(true);
    return result;
  } else {
    return null;
  }
}

}  // namespace compiler
}  // namespace protobuf
}  // namespace google
