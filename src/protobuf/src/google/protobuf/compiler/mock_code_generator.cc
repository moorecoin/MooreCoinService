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

#include <google/protobuf/compiler/mock_code_generator.h>

#include <google/protobuf/testing/file.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>
#include <gtest/gtest.h>
#include <google/protobuf/stubs/stl_util.h>

namespace google {
namespace protobuf {
namespace compiler {

// returns the list of the names of files in all_files in the form of a
// comma-separated string.
string commaseparatedlist(const vector<const filedescriptor*> all_files) {
  vector<string> names;
  for (int i = 0; i < all_files.size(); i++) {
    names.push_back(all_files[i]->name());
  }
  return joinstrings(names, ",");
}

static const char* kfirstinsertionpointname = "first_mock_insertion_point";
static const char* ksecondinsertionpointname = "second_mock_insertion_point";
static const char* kfirstinsertionpoint =
    "# @@protoc_insertion_point(first_mock_insertion_point) is here\n";
static const char* ksecondinsertionpoint =
    "  # @@protoc_insertion_point(second_mock_insertion_point) is here\n";

mockcodegenerator::mockcodegenerator(const string& name)
    : name_(name) {}

mockcodegenerator::~mockcodegenerator() {}

void mockcodegenerator::expectgenerated(
    const string& name,
    const string& parameter,
    const string& insertions,
    const string& file,
    const string& first_message_name,
    const string& first_parsed_file_name,
    const string& output_directory) {
  string content;
  assert_true(file::readfiletostring(
      output_directory + "/" + getoutputfilename(name, file), &content));

  vector<string> lines;
  splitstringusing(content, "\n", &lines);

  while (!lines.empty() && lines.back().empty()) {
    lines.pop_back();
  }
  for (int i = 0; i < lines.size(); i++) {
    lines[i] += "\n";
  }

  vector<string> insertion_list;
  if (!insertions.empty()) {
    splitstringusing(insertions, ",", &insertion_list);
  }

  assert_eq(lines.size(), 3 + insertion_list.size() * 2);
  expect_eq(getoutputfilecontent(name, parameter, file,
                                 first_parsed_file_name, first_message_name),
            lines[0]);

  expect_eq(kfirstinsertionpoint, lines[1 + insertion_list.size()]);
  expect_eq(ksecondinsertionpoint, lines[2 + insertion_list.size() * 2]);

  for (int i = 0; i < insertion_list.size(); i++) {
    expect_eq(getoutputfilecontent(insertion_list[i], "first_insert",
                                   file, file, first_message_name),
              lines[1 + i]);
    // second insertion point is indented, so the inserted text should
    // automatically be indented too.
    expect_eq("  " + getoutputfilecontent(insertion_list[i], "second_insert",
                                          file, file, first_message_name),
              lines[2 + insertion_list.size() + i]);
  }
}

bool mockcodegenerator::generate(
    const filedescriptor* file,
    const string& parameter,
    generatorcontext* context,
    string* error) const {
  for (int i = 0; i < file->message_type_count(); i++) {
    if (hasprefixstring(file->message_type(i)->name(), "mockcodegenerator_")) {
      string command = stripprefixstring(file->message_type(i)->name(),
                                         "mockcodegenerator_");
      if (command == "error") {
        *error = "saw message type mockcodegenerator_error.";
        return false;
      } else if (command == "exit") {
        cerr << "saw message type mockcodegenerator_exit." << endl;
        exit(123);
      } else if (command == "abort") {
        cerr << "saw message type mockcodegenerator_abort." << endl;
        abort();
      } else if (command == "hassourcecodeinfo") {
        filedescriptorproto file_descriptor_proto;
        file->copysourcecodeinfoto(&file_descriptor_proto);
        bool has_source_code_info =
            file_descriptor_proto.has_source_code_info() &&
            file_descriptor_proto.source_code_info().location_size() > 0;
        cerr << "saw message type mockcodegenerator_hassourcecodeinfo: "
             << has_source_code_info << "." << endl;
        abort();
      } else {
        google_log(fatal) << "unknown mockcodegenerator command: " << command;
      }
    }
  }

  if (hasprefixstring(parameter, "insert=")) {
    vector<string> insert_into;
    splitstringusing(stripprefixstring(parameter, "insert="),
                     ",", &insert_into);

    for (int i = 0; i < insert_into.size(); i++) {
      {
        scoped_ptr<io::zerocopyoutputstream> output(
            context->openforinsert(
              getoutputfilename(insert_into[i], file),
              kfirstinsertionpointname));
        io::printer printer(output.get(), '$');
        printer.printraw(getoutputfilecontent(name_, "first_insert",
                                              file, context));
        if (printer.failed()) {
          *error = "mockcodegenerator detected write error.";
          return false;
        }
      }

      {
        scoped_ptr<io::zerocopyoutputstream> output(
            context->openforinsert(
              getoutputfilename(insert_into[i], file),
              ksecondinsertionpointname));
        io::printer printer(output.get(), '$');
        printer.printraw(getoutputfilecontent(name_, "second_insert",
                                              file, context));
        if (printer.failed()) {
          *error = "mockcodegenerator detected write error.";
          return false;
        }
      }
    }
  } else {
    scoped_ptr<io::zerocopyoutputstream> output(
        context->open(getoutputfilename(name_, file)));

    io::printer printer(output.get(), '$');
    printer.printraw(getoutputfilecontent(name_, parameter,
                                          file, context));
    printer.printraw(kfirstinsertionpoint);
    printer.printraw(ksecondinsertionpoint);

    if (printer.failed()) {
      *error = "mockcodegenerator detected write error.";
      return false;
    }
  }

  return true;
}

string mockcodegenerator::getoutputfilename(const string& generator_name,
                                            const filedescriptor* file) {
  return getoutputfilename(generator_name, file->name());
}

string mockcodegenerator::getoutputfilename(const string& generator_name,
                                            const string& file) {
  return file + ".mockcodegenerator." + generator_name;
}

string mockcodegenerator::getoutputfilecontent(
    const string& generator_name,
    const string& parameter,
    const filedescriptor* file,
    generatorcontext *context) {
  vector<const filedescriptor*> all_files;
  context->listparsedfiles(&all_files);
  return getoutputfilecontent(
      generator_name, parameter, file->name(),
      commaseparatedlist(all_files),
      file->message_type_count() > 0 ?
          file->message_type(0)->name() : "(none)");
}

string mockcodegenerator::getoutputfilecontent(
    const string& generator_name,
    const string& parameter,
    const string& file,
    const string& parsed_file_list,
    const string& first_message_name) {
  return strings::substitute("$0: $1, $2, $3, $4\n",
      generator_name, parameter, file,
      first_message_name, parsed_file_list);
}

}  // namespace compiler
}  // namespace protobuf
}  // namespace google
