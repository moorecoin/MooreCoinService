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

#ifndef google_protobuf_compiler_java_service_h__
#define google_protobuf_compiler_java_service_h__

#include <map>
#include <google/protobuf/descriptor.h>

namespace google {
namespace protobuf {
  namespace io {
    class printer;             // printer.h
  }
}

namespace protobuf {
namespace compiler {
namespace java {

class servicegenerator {
 public:
  explicit servicegenerator(const servicedescriptor* descriptor);
  ~servicegenerator();

  void generate(io::printer* printer);

 private:

  // generate the getdescriptorfortype() method.
  void generategetdescriptorfortype(io::printer* printer);

  // generate a java interface for the service.
  void generateinterface(io::printer* printer);

  // generate newreflectiveservice() method.
  void generatenewreflectiveservicemethod(io::printer* printer);

  // generate newreflectiveblockingservice() method.
  void generatenewreflectiveblockingservicemethod(io::printer* printer);

  // generate abstract method declarations for all methods.
  void generateabstractmethods(io::printer* printer);

  // generate the implementation of service.callmethod().
  void generatecallmethod(io::printer* printer);

  // generate the implementation of blockingservice.callblockingmethod().
  void generatecallblockingmethod(io::printer* printer);

  // generate the implementations of service.get{request,response}prototype().
  enum requestorresponse { request, response };
  void generategetprototype(requestorresponse which, io::printer* printer);

  // generate a stub implementation of the service.
  void generatestub(io::printer* printer);

  // generate a method signature, possibly abstract, without body or trailing
  // semicolon.
  enum isabstract { is_abstract, is_concrete };
  void generatemethodsignature(io::printer* printer,
                               const methoddescriptor* method,
                               isabstract is_abstract);

  // generate a blocking stub interface and implementation of the service.
  void generateblockingstub(io::printer* printer);

  // generate the method signature for one method of a blocking stub.
  void generateblockingmethodsignature(io::printer* printer,
                                       const methoddescriptor* method);

  const servicedescriptor* descriptor_;

  google_disallow_evil_constructors(servicegenerator);
};

}  // namespace java
}  // namespace compiler
}  // namespace protobuf

#endif  // net_proto2_compiler_java_service_h__
}  // namespace google
