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

#include <google/protobuf/compiler/java/java_generator.h>
#include <google/protobuf/compiler/java/java_file.h>
#include <google/protobuf/compiler/java/java_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace java {


javagenerator::javagenerator() {}
javagenerator::~javagenerator() {}

bool javagenerator::generate(const filedescriptor* file,
                             const string& parameter,
                             generatorcontext* context,
                             string* error) const {
  // -----------------------------------------------------------------
  // parse generator options

  // name a file where we will write a list of generated file names, one
  // per line.
  string output_list_file;


  vector<pair<string, string> > options;
  parsegeneratorparameter(parameter, &options);

  for (int i = 0; i < options.size(); i++) {
    if (options[i].first == "output_list_file") {
      output_list_file = options[i].second;
    } else {
      *error = "unknown generator option: " + options[i].first;
      return false;
    }
  }

  // -----------------------------------------------------------------


  if (file->options().optimize_for() == fileoptions::lite_runtime &&
      file->options().java_generate_equals_and_hash()) {
    *error = "the \"java_generate_equals_and_hash\" option is incompatible "
             "with \"optimize_for = lite_runtime\".  you must optimize for "
             "speed or code_size if you want to use this option.";
    return false;
  }

  filegenerator file_generator(file);
  if (!file_generator.validate(error)) {
    return false;
  }

  string package_dir = javapackagetodir(file_generator.java_package());

  vector<string> all_files;

  string java_filename = package_dir;
  java_filename += file_generator.classname();
  java_filename += ".java";
  all_files.push_back(java_filename);

  // generate main java file.
  scoped_ptr<io::zerocopyoutputstream> output(
    context->open(java_filename));
  io::printer printer(output.get(), '$');
  file_generator.generate(&printer);

  // generate sibling files.
  file_generator.generatesiblings(package_dir, context, &all_files);

  // generate output list if requested.
  if (!output_list_file.empty()) {
    // generate output list.  this is just a simple text file placed in a
    // deterministic location which lists the .java files being generated.
    scoped_ptr<io::zerocopyoutputstream> srclist_raw_output(
      context->open(output_list_file));
    io::printer srclist_printer(srclist_raw_output.get(), '$');
    for (int i = 0; i < all_files.size(); i++) {
      srclist_printer.print("$filename$\n", "filename", all_files[i]);
    }
  }

  return true;
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
