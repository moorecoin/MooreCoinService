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
// this file exists solely to document the google::protobuf namespace.
// it is not compiled into anything, but it may be read by an automated
// documentation generator.

namespace google {

// core components of the protocol buffers runtime library.
//
// the files in this package represent the core of the protocol buffer
// system.  all of them are part of the libprotobuf library.
//
// a note on thread-safety:
//
// thread-safety in the protocol buffer library follows a simple rule:
// unless explicitly noted otherwise, it is always safe to use an object
// from multiple threads simultaneously as long as the object is declared
// const in all threads (or, it is only used in ways that would be allowed
// if it were declared const).  however, if an object is accessed in one
// thread in a way that would not be allowed if it were const, then it is
// not safe to access that object in any other thread simultaneously.
//
// put simply, read-only access to an object can happen in multiple threads
// simultaneously, but write access can only happen in a single thread at
// a time.
//
// the implementation does contain some "const" methods which actually modify
// the object behind the scenes -- e.g., to cache results -- but in these cases
// mutex locking is used to make the access thread-safe.
namespace protobuf {}
}  // namespace google
