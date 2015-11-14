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
// this file exists solely to document the google::protobuf::io namespace.
// it is not compiled into anything, but it may be read by an automated
// documentation generator.

namespace google {

namespace protobuf {

// auxiliary classes used for i/o.
//
// the protocol buffer library uses the classes in this package to deal with
// i/o and encoding/decoding raw bytes.  most users will not need to
// deal with this package.  however, users who want to adapt the system to
// work with their own i/o abstractions -- e.g., to allow protocol buffers
// to be read from a different kind of input stream without the need for a
// temporary buffer -- should take a closer look.
namespace io {}

}  // namespace protobuf
}  // namespace google
