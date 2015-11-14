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
// this file exists solely to document the google::protobuf::compiler namespace.
// it is not compiled into anything, but it may be read by an automated
// documentation generator.

namespace google {

namespace protobuf {

// implementation of the protocol buffer compiler.
//
// this package contains code for parsing .proto files and generating code
// based on them.  there are two reasons you might be interested in this
// package:
// - you want to parse .proto files at runtime.  in this case, you should
//   look at importer.h.  since this functionality is widely useful, it is
//   included in the libprotobuf base library; you do not have to link against
//   libprotoc.
// - you want to write a custom protocol compiler which generates different
//   kinds of code, e.g. code in a different language which is not supported
//   by the official compiler.  for this purpose, command_line_interface.h
//   provides you with a complete compiler front-end, so all you need to do
//   is write a custom implementation of codegenerator and a trivial main()
//   function.  you can even make your compiler support the official languages
//   in addition to your own.  since this functionality is only useful to those
//   writing custom compilers, it is in a separate library called "libprotoc"
//   which you will have to link against.
namespace compiler {}

}  // namespace protobuf
}  // namespace google
