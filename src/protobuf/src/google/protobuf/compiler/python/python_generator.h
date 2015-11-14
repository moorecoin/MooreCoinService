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

// author: robinson@google.com (will robinson)
//
// generates python code for a given .proto file.

#ifndef google_protobuf_compiler_python_generator_h__
#define google_protobuf_compiler_python_generator_h__

#include <string>

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {

class descriptor;
class enumdescriptor;
class enumvaluedescriptor;
class fielddescriptor;
class servicedescriptor;

namespace io { class printer; }

namespace compiler {
namespace python {

// codegenerator implementation for generated python protocol buffer classes.
// if you create your own protocol compiler binary and you want it to support
// python output, you can do so by registering an instance of this
// codegenerator with the commandlineinterface in your main() function.
class libprotoc_export generator : public codegenerator {
 public:
  generator();
  virtual ~generator();

  // codegenerator methods.
  virtual bool generate(const filedescriptor* file,
                        const string& parameter,
                        generatorcontext* generator_context,
                        string* error) const;

 private:
  void printimports() const;
  void printfiledescriptor() const;
  void printtoplevelenums() const;
  void printallnestedenumsinfile() const;
  void printnestedenums(const descriptor& descriptor) const;
  void printenum(const enumdescriptor& enum_descriptor) const;

  void printtoplevelextensions() const;

  void printfielddescriptor(
      const fielddescriptor& field, bool is_extension) const;
  void printfielddescriptorsindescriptor(
      const descriptor& message_descriptor,
      bool is_extension,
      const string& list_variable_name,
      int (descriptor::*countfn)() const,
      const fielddescriptor* (descriptor::*getterfn)(int) const) const;
  void printfieldsindescriptor(const descriptor& message_descriptor) const;
  void printextensionsindescriptor(const descriptor& message_descriptor) const;
  void printmessagedescriptors() const;
  void printdescriptor(const descriptor& message_descriptor) const;
  void printnesteddescriptors(const descriptor& containing_descriptor) const;

  void printmessages() const;
  void printmessage(const descriptor& message_descriptor) const;
  void printnestedmessages(const descriptor& containing_descriptor) const;

  void fixforeignfieldsindescriptors() const;
  void fixforeignfieldsindescriptor(
      const descriptor& descriptor,
      const descriptor* containing_descriptor) const;
  void fixforeignfieldsinfield(const descriptor* containing_type,
                               const fielddescriptor& field,
                               const string& python_dict_name) const;
  void addmessagetofiledescriptor(const descriptor& descriptor) const;
  string fieldreferencingexpression(const descriptor* containing_type,
                                    const fielddescriptor& field,
                                    const string& python_dict_name) const;
  template <typename descriptort>
  void fixcontainingtypeindescriptor(
      const descriptort& descriptor,
      const descriptor* containing_descriptor) const;

  void fixforeignfieldsinextensions() const;
  void fixforeignfieldsinextension(
      const fielddescriptor& extension_field) const;
  void fixforeignfieldsinnestedextensions(const descriptor& descriptor) const;

  void printservices() const;
  void printservicedescriptor(const servicedescriptor& descriptor) const;
  void printserviceclass(const servicedescriptor& descriptor) const;
  void printservicestub(const servicedescriptor& descriptor) const;

  void printenumvaluedescriptor(const enumvaluedescriptor& descriptor) const;
  string optionsvalue(const string& class_name,
                      const string& serialized_options) const;
  bool generatingdescriptorproto() const;

  template <typename descriptort>
  string moduleleveldescriptorname(const descriptort& descriptor) const;
  string modulelevelmessagename(const descriptor& descriptor) const;
  string modulelevelservicedescriptorname(
      const servicedescriptor& descriptor) const;

  template <typename descriptort, typename descriptorprotot>
  void printserializedpbinterval(
      const descriptort& descriptor, descriptorprotot& proto) const;

  void fixalldescriptoroptions() const;
  void fixoptionsforfield(const fielddescriptor& field) const;
  void fixoptionsforenum(const enumdescriptor& descriptor) const;
  void fixoptionsformessage(const descriptor& descriptor) const;

  // very coarse-grained lock to ensure that generate() is reentrant.
  // guards file_, printer_ and file_descriptor_serialized_.
  mutable mutex mutex_;
  mutable const filedescriptor* file_;  // set in generate().  under mutex_.
  mutable string file_descriptor_serialized_;
  mutable io::printer* printer_;  // set in generate().  under mutex_.

  google_disallow_evil_constructors(generator);
};

}  // namespace python
}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_compiler_python_generator_h__
