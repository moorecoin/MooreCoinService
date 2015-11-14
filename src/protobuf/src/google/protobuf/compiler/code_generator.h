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
// defines the abstract interface implemented by each of the language-specific
// code generators.

#ifndef google_protobuf_compiler_code_generator_h__
#define google_protobuf_compiler_code_generator_h__

#include <google/protobuf/stubs/common.h>
#include <string>
#include <vector>
#include <utility>

namespace google {
namespace protobuf {

namespace io { class zerocopyoutputstream; }
class filedescriptor;

namespace compiler {

// defined in this file.
class codegenerator;
class generatorcontext;

// the abstract interface to a class which generates code implementing a
// particular proto file in a particular language.  a number of these may
// be registered with commandlineinterface to support various languages.
class libprotoc_export codegenerator {
 public:
  inline codegenerator() {}
  virtual ~codegenerator();

  // generates code for the given proto file, generating one or more files in
  // the given output directory.
  //
  // a parameter to be passed to the generator can be specified on the
  // command line.  this is intended to be used by java and similar languages
  // to specify which specific class from the proto file is to be generated,
  // though it could have other uses as well.  it is empty if no parameter was
  // given.
  //
  // returns true if successful.  otherwise, sets *error to a description of
  // the problem (e.g. "invalid parameter") and returns false.
  virtual bool generate(const filedescriptor* file,
                        const string& parameter,
                        generatorcontext* generator_context,
                        string* error) const = 0;

 private:
  google_disallow_evil_constructors(codegenerator);
};

// codegenerators generate one or more files in a given directory.  this
// abstract interface represents the directory to which the codegenerator is
// to write and other information about the context in which the generator
// runs.
class libprotoc_export generatorcontext {
 public:
  inline generatorcontext() {}
  virtual ~generatorcontext();

  // opens the given file, truncating it if it exists, and returns a
  // zerocopyoutputstream that writes to the file.  the caller takes ownership
  // of the returned object.  this method never fails (a dummy stream will be
  // returned instead).
  //
  // the filename given should be relative to the root of the source tree.
  // e.g. the c++ generator, when generating code for "foo/bar.proto", will
  // generate the files "foo/bar.pb.h" and "foo/bar.pb.cc"; note that
  // "foo/" is included in these filenames.  the filename is not allowed to
  // contain "." or ".." components.
  virtual io::zerocopyoutputstream* open(const string& filename) = 0;

  // creates a zerocopyoutputstream which will insert code into the given file
  // at the given insertion point.  see plugin.proto (plugin.pb.h) for more
  // information on insertion points.  the default implementation
  // assert-fails -- it exists only for backwards-compatibility.
  //
  // warning:  this feature is currently experimental and is subject to change.
  virtual io::zerocopyoutputstream* openforinsert(
      const string& filename, const string& insertion_point);

  // returns a vector of filedescriptors for all the files being compiled
  // in this run.  useful for languages, such as go, that treat files
  // differently when compiled as a set rather than individually.
  virtual void listparsedfiles(vector<const filedescriptor*>* output);

 private:
  google_disallow_evil_constructors(generatorcontext);
};

// the type generatorcontext was once called outputdirectory. this typedef
// provides backward compatibility.
typedef generatorcontext outputdirectory;

// several code generators treat the parameter argument as holding a
// list of options separated by commas.  this helper function parses
// a set of comma-delimited name/value pairs: e.g.,
//   "foo=bar,baz,qux=corge"
// parses to the pairs:
//   ("foo", "bar"), ("baz", ""), ("qux", "corge")
extern void parsegeneratorparameter(const string&,
            vector<pair<string, string> >*);

}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_compiler_code_generator_h__
