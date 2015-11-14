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

#include <google/protobuf/compiler/plugin.h>

#include <iostream>
#include <set>

#ifdef _win32
#include <io.h>
#include <fcntl.h>
#ifndef stdin_fileno
#define stdin_fileno 0
#endif
#ifndef stdout_fileno
#define stdout_fileno 1
#endif
#else
#include <unistd.h>
#endif

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/compiler/plugin.pb.h>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>


namespace google {
namespace protobuf {
namespace compiler {

class generatorresponsecontext : public generatorcontext {
 public:
  generatorresponsecontext(codegeneratorresponse* response,
                           const vector<const filedescriptor*>& parsed_files)
      : response_(response),
        parsed_files_(parsed_files) {}
  virtual ~generatorresponsecontext() {}

  // implements generatorcontext --------------------------------------

  virtual io::zerocopyoutputstream* open(const string& filename) {
    codegeneratorresponse::file* file = response_->add_file();
    file->set_name(filename);
    return new io::stringoutputstream(file->mutable_content());
  }

  virtual io::zerocopyoutputstream* openforinsert(
      const string& filename, const string& insertion_point) {
    codegeneratorresponse::file* file = response_->add_file();
    file->set_name(filename);
    file->set_insertion_point(insertion_point);
    return new io::stringoutputstream(file->mutable_content());
  }

  void listparsedfiles(vector<const filedescriptor*>* output) {
    *output = parsed_files_;
  }

 private:
  codegeneratorresponse* response_;
  const vector<const filedescriptor*>& parsed_files_;
};

int pluginmain(int argc, char* argv[], const codegenerator* generator) {

  if (argc > 1) {
    cerr << argv[0] << ": unknown option: " << argv[1] << endl;
    return 1;
  }

#ifdef _win32
  _setmode(stdin_fileno, _o_binary);
  _setmode(stdout_fileno, _o_binary);
#endif

  codegeneratorrequest request;
  if (!request.parsefromfiledescriptor(stdin_fileno)) {
    cerr << argv[0] << ": protoc sent unparseable request to plugin." << endl;
    return 1;
  }

  descriptorpool pool;
  for (int i = 0; i < request.proto_file_size(); i++) {
    const filedescriptor* file = pool.buildfile(request.proto_file(i));
    if (file == null) {
      // buildfile() already wrote an error message.
      return 1;
    }
  }

  vector<const filedescriptor*> parsed_files;
  for (int i = 0; i < request.file_to_generate_size(); i++) {
    parsed_files.push_back(pool.findfilebyname(request.file_to_generate(i)));
    if (parsed_files.back() == null) {
      cerr << argv[0] << ": protoc asked plugin to generate a file but "
              "did not provide a descriptor for the file: "
           << request.file_to_generate(i) << endl;
      return 1;
    }
  }

  codegeneratorresponse response;
  generatorresponsecontext context(&response, parsed_files);

  for (int i = 0; i < parsed_files.size(); i++) {
    const filedescriptor* file = parsed_files[i];

    string error;
    bool succeeded = generator->generate(
        file, request.parameter(), &context, &error);

    if (!succeeded && error.empty()) {
      error = "code generator returned false but provided no error "
              "description.";
    }
    if (!error.empty()) {
      response.set_error(file->name() + ": " + error);
      break;
    }
  }

  if (!response.serializetofiledescriptor(stdout_fileno)) {
    cerr << argv[0] << ": error writing to stdout." << endl;
    return 1;
  }

  return 0;
}

}  // namespace compiler
}  // namespace protobuf
}  // namespace google
