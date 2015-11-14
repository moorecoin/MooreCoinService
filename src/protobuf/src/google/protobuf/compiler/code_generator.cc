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

#include <google/protobuf/compiler/code_generator.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {

codegenerator::~codegenerator() {}
generatorcontext::~generatorcontext() {}

io::zerocopyoutputstream* generatorcontext::openforinsert(
    const string& filename, const string& insertion_point) {
  google_log(fatal) << "this generatorcontext does not support insertion.";
  return null;  // make compiler happy
}

void generatorcontext::listparsedfiles(
    vector<const filedescriptor*>* output) {
  google_log(fatal) << "this generatorcontext does not support listparsedfiles";
}

// parses a set of comma-delimited name/value pairs.
void parsegeneratorparameter(const string& text,
                             vector<pair<string, string> >* output) {
  vector<string> parts;
  splitstringusing(text, ",", &parts);

  for (int i = 0; i < parts.size(); i++) {
    string::size_type equals_pos = parts[i].find_first_of('=');
    pair<string, string> value;
    if (equals_pos == string::npos) {
      value.first = parts[i];
      value.second = "";
    } else {
      value.first = parts[i].substr(0, equals_pos);
      value.second = parts[i].substr(equals_pos + 1);
    }
    output->push_back(value);
  }
}

}  // namespace compiler
}  // namespace protobuf
}  // namespace google
