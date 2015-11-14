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
//
// this file is the public interface to the .proto file parser.

#ifndef google_protobuf_compiler_importer_h__
#define google_protobuf_compiler_importer_h__

#include <string>
#include <vector>
#include <set>
#include <utility>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor_database.h>
#include <google/protobuf/compiler/parser.h>

namespace google {
namespace protobuf {

namespace io { class zerocopyinputstream; }

namespace compiler {

// defined in this file.
class importer;
class multifileerrorcollector;
class sourcetree;
class disksourcetree;

// todo(kenton):  move all sourcetree stuff to a separate file?

// an implementation of descriptordatabase which loads files from a sourcetree
// and parses them.
//
// note:  this class is not thread-safe since it maintains a table of source
//   code locations for error reporting.  however, when a descriptorpool wraps
//   a descriptordatabase, it uses mutex locking to make sure only one method
//   of the database is called at a time, even if the descriptorpool is used
//   from multiple threads.  therefore, there is only a problem if you create
//   multiple descriptorpools wrapping the same sourcetreedescriptordatabase
//   and use them from multiple threads.
//
// note:  this class does not implement findfilecontainingsymbol() or
//   findfilecontainingextension(); these will always return false.
class libprotobuf_export sourcetreedescriptordatabase : public descriptordatabase {
 public:
  sourcetreedescriptordatabase(sourcetree* source_tree);
  ~sourcetreedescriptordatabase();

  // instructs the sourcetreedescriptordatabase to report any parse errors
  // to the given multifileerrorcollector.  this should be called before
  // parsing.  error_collector must remain valid until either this method
  // is called again or the sourcetreedescriptordatabase is destroyed.
  void recorderrorsto(multifileerrorcollector* error_collector) {
    error_collector_ = error_collector;
  }

  // gets a descriptorpool::errorcollector which records errors to the
  // multifileerrorcollector specified with recorderrorsto().  this collector
  // has the ability to determine exact line and column numbers of errors
  // from the information given to it by the descriptorpool.
  descriptorpool::errorcollector* getvalidationerrorcollector() {
    using_validation_error_collector_ = true;
    return &validation_error_collector_;
  }

  // implements descriptordatabase -----------------------------------
  bool findfilebyname(const string& filename, filedescriptorproto* output);
  bool findfilecontainingsymbol(const string& symbol_name,
                                filedescriptorproto* output);
  bool findfilecontainingextension(const string& containing_type,
                                   int field_number,
                                   filedescriptorproto* output);

 private:
  class singlefileerrorcollector;

  sourcetree* source_tree_;
  multifileerrorcollector* error_collector_;

  class libprotobuf_export validationerrorcollector : public descriptorpool::errorcollector {
   public:
    validationerrorcollector(sourcetreedescriptordatabase* owner);
    ~validationerrorcollector();

    // implements errorcollector ---------------------------------------
    void adderror(const string& filename,
                  const string& element_name,
                  const message* descriptor,
                  errorlocation location,
                  const string& message);

   private:
    sourcetreedescriptordatabase* owner_;
  };
  friend class validationerrorcollector;

  bool using_validation_error_collector_;
  sourcelocationtable source_locations_;
  validationerrorcollector validation_error_collector_;
};

// simple interface for parsing .proto files.  this wraps the process
// of opening the file, parsing it with a parser, recursively parsing all its
// imports, and then cross-linking the results to produce a filedescriptor.
//
// this is really just a thin wrapper around sourcetreedescriptordatabase.
// you may find that sourcetreedescriptordatabase is more flexible.
//
// todo(kenton):  i feel like this class is not well-named.
class libprotobuf_export importer {
 public:
  importer(sourcetree* source_tree,
           multifileerrorcollector* error_collector);
  ~importer();

  // import the given file and build a filedescriptor representing it.  if
  // the file is already in the descriptorpool, the existing filedescriptor
  // will be returned.  the filedescriptor is property of the descriptorpool,
  // and will remain valid until it is destroyed.  if any errors occur, they
  // will be reported using the error collector and import() will return null.
  //
  // a particular importer object will only report errors for a particular
  // file once.  all future attempts to import the same file will return null
  // without reporting any errors.  the idea is that you might want to import
  // a lot of files without seeing the same errors over and over again.  if
  // you want to see errors for the same files repeatedly, you can use a
  // separate importer object to import each one (but use the same
  // descriptorpool so that they can be cross-linked).
  const filedescriptor* import(const string& filename);

  // the descriptorpool in which all imported filedescriptors and their
  // contents are stored.
  inline const descriptorpool* pool() const {
    return &pool_;
  }

 private:
  sourcetreedescriptordatabase database_;
  descriptorpool pool_;

  google_disallow_evil_constructors(importer);
};

// if the importer encounters problems while trying to import the proto files,
// it reports them to a multifileerrorcollector.
class libprotobuf_export multifileerrorcollector {
 public:
  inline multifileerrorcollector() {}
  virtual ~multifileerrorcollector();

  // line and column numbers are zero-based.  a line number of -1 indicates
  // an error with the entire file (e.g. "not found").
  virtual void adderror(const string& filename, int line, int column,
                        const string& message) = 0;

 private:
  google_disallow_evil_constructors(multifileerrorcollector);
};

// abstract interface which represents a directory tree containing proto files.
// used by the default implementation of importer to resolve import statements
// most users will probably want to use the disksourcetree implementation,
// below.
class libprotobuf_export sourcetree {
 public:
  inline sourcetree() {}
  virtual ~sourcetree();

  // open the given file and return a stream that reads it, or null if not
  // found.  the caller takes ownership of the returned object.  the filename
  // must be a path relative to the root of the source tree and must not
  // contain "." or ".." components.
  virtual io::zerocopyinputstream* open(const string& filename) = 0;

 private:
  google_disallow_evil_constructors(sourcetree);
};

// an implementation of sourcetree which loads files from locations on disk.
// multiple mappings can be set up to map locations in the disksourcetree to
// locations in the physical filesystem.
class libprotobuf_export disksourcetree : public sourcetree {
 public:
  disksourcetree();
  ~disksourcetree();

  // map a path on disk to a location in the sourcetree.  the path may be
  // either a file or a directory.  if it is a directory, the entire tree
  // under it will be mapped to the given virtual location.  to map a directory
  // to the root of the source tree, pass an empty string for virtual_path.
  //
  // if multiple mapped paths apply when opening a file, they will be searched
  // in order.  for example, if you do:
  //   mappath("bar", "foo/bar");
  //   mappath("", "baz");
  // and then you do:
  //   open("bar/qux");
  // the disksourcetree will first try to open foo/bar/qux, then baz/bar/qux,
  // returning the first one that opens successfuly.
  //
  // disk_path may be an absolute path or relative to the current directory,
  // just like a path you'd pass to open().
  void mappath(const string& virtual_path, const string& disk_path);

  // return type for diskfiletovirtualfile().
  enum diskfiletovirtualfileresult {
    success,
    shadowed,
    cannot_open,
    no_mapping
  };

  // given a path to a file on disk, find a virtual path mapping to that
  // file.  the first mapping created with mappath() whose disk_path contains
  // the filename is used.  however, that virtual path may not actually be
  // usable to open the given file.  possible return values are:
  // * success: the mapping was found.  *virtual_file is filled in so that
  //   calling open(*virtual_file) will open the file named by disk_file.
  // * shadowed: a mapping was found, but using open() to open this virtual
  //   path will end up returning some different file.  this is because some
  //   other mapping with a higher precedence also matches this virtual path
  //   and maps it to a different file that exists on disk.  *virtual_file
  //   is filled in as it would be in the success case.  *shadowing_disk_file
  //   is filled in with the disk path of the file which would be opened if
  //   you were to call open(*virtual_file).
  // * cannot_open: the mapping was found and was not shadowed, but the
  //   file specified cannot be opened.  when this value is returned,
  //   errno will indicate the reason the file cannot be opened.  *virtual_file
  //   will be set to the virtual path as in the success case, even though
  //   it is not useful.
  // * no_mapping: indicates that no mapping was found which contains this
  //   file.
  diskfiletovirtualfileresult
    diskfiletovirtualfile(const string& disk_file,
                          string* virtual_file,
                          string* shadowing_disk_file);

  // given a virtual path, find the path to the file on disk.
  // return true and update disk_file with the on-disk path if the file exists.
  // return false and leave disk_file untouched if the file doesn't exist.
  bool virtualfiletodiskfile(const string& virtual_file, string* disk_file);

  // implements sourcetree -------------------------------------------
  io::zerocopyinputstream* open(const string& filename);

 private:
  struct mapping {
    string virtual_path;
    string disk_path;

    inline mapping(const string& virtual_path_param,
                   const string& disk_path_param)
      : virtual_path(virtual_path_param), disk_path(disk_path_param) {}
  };
  vector<mapping> mappings_;

  // like open(), but returns the on-disk path in disk_file if disk_file is
  // non-null and the file could be successfully opened.
  io::zerocopyinputstream* openvirtualfile(const string& virtual_file,
                                           string* disk_file);

  // like open() but given the actual on-disk path.
  io::zerocopyinputstream* opendiskfile(const string& filename);

  google_disallow_evil_constructors(disksourcetree);
};

}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_compiler_importer_h__
