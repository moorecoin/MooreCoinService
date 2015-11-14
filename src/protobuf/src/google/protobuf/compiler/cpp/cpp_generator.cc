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

#include <google/protobuf/compiler/cpp/cpp_generator.h>

#include <vector>
#include <utility>

#include <google/protobuf/compiler/cpp/cpp_file.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/descriptor.pb.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

cppgenerator::cppgenerator() {}
cppgenerator::~cppgenerator() {}

bool cppgenerator::generate(const filedescriptor* file,
                            const string& parameter,
                            generatorcontext* generator_context,
                            string* error) const {
  vector<pair<string, string> > options;
  parsegeneratorparameter(parameter, &options);

  // -----------------------------------------------------------------
  // parse generator options

  // todo(kenton):  if we ever have more options, we may want to create a
  //   class that encapsulates them which we can pass down to all the
  //   generator classes.  currently we pass dllexport_decl down to all of
  //   them via the constructors, but we don't want to have to add another
  //   constructor parameter for every option.

  // if the dllexport_decl option is passed to the compiler, we need to write
  // it in front of every symbol that should be exported if this .proto is
  // compiled into a windows dll.  e.g., if the user invokes the protocol
  // compiler as:
  //   protoc --cpp_out=dllexport_decl=foo_export:outdir foo.proto
  // then we'll define classes like this:
  //   class foo_export foo {
  //     ...
  //   }
  // foo_export is a macro which should expand to __declspec(dllexport) or
  // __declspec(dllimport) depending on what is being compiled.
  options file_options;

  for (int i = 0; i < options.size(); i++) {
    if (options[i].first == "dllexport_decl") {
      file_options.dllexport_decl = options[i].second;
    } else if (options[i].first == "safe_boundary_check") {
      file_options.safe_boundary_check = true;
    } else {
      *error = "unknown generator option: " + options[i].first;
      return false;
    }
  }

  // -----------------------------------------------------------------


  string basename = stripproto(file->name());
  basename.append(".pb");

  filegenerator file_generator(file, file_options);

  // generate header.
  {
    scoped_ptr<io::zerocopyoutputstream> output(
      generator_context->open(basename + ".h"));
    io::printer printer(output.get(), '$');
    file_generator.generateheader(&printer);
  }

  // generate cc file.
  {
    scoped_ptr<io::zerocopyoutputstream> output(
      generator_context->open(basename + ".cc"));
    io::printer printer(output.get(), '$');
    file_generator.generatesource(&printer);
  }

  return true;
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
