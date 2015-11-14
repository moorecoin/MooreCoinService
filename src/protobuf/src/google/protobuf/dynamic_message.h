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
// defines an implementation of message which can emulate types which are not
// known at compile-time.

#ifndef google_protobuf_dynamic_message_h__
#define google_protobuf_dynamic_message_h__

#include <google/protobuf/message.h>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {

// defined in other files.
class descriptor;        // descriptor.h
class descriptorpool;    // descriptor.h

// constructs implementations of message which can emulate types which are not
// known at compile-time.
//
// sometimes you want to be able to manipulate protocol types that you don't
// know about at compile time.  it would be nice to be able to construct
// a message object which implements the message type given by any arbitrary
// descriptor.  dynamicmessage provides this.
//
// as it turns out, a dynamicmessage needs to construct extra
// information about its type in order to operate.  most of this information
// can be shared between all dynamicmessages of the same type.  but, caching
// this information in some sort of global map would be a bad idea, since
// the cached information for a particular descriptor could outlive the
// descriptor itself.  to avoid this problem, dynamicmessagefactory
// encapsulates this "cache".  all dynamicmessages of the same type created
// from the same factory will share the same support data.  any descriptors
// used with a particular factory must outlive the factory.
class libprotobuf_export dynamicmessagefactory : public messagefactory {
 public:
  // construct a dynamicmessagefactory that will search for extensions in
  // the descriptorpool in which the extendee is defined.
  dynamicmessagefactory();

  // construct a dynamicmessagefactory that will search for extensions in
  // the given descriptorpool.
  //
  // deprecated:  use codedinputstream::setextensionregistry() to tell the
  //   parser to look for extensions in an alternate pool.  however, note that
  //   this is almost never what you want to do.  almost all users should use
  //   the zero-arg constructor.
  dynamicmessagefactory(const descriptorpool* pool);

  ~dynamicmessagefactory();

  // call this to tell the dynamicmessagefactory that if it is given a
  // descriptor d for which:
  //   d->file()->pool() == descriptorpool::generated_pool(),
  // then it should delegate to messagefactory::generated_factory() instead
  // of constructing a dynamic implementation of the message.  in theory there
  // is no down side to doing this, so it may become the default in the future.
  void setdelegatetogeneratedfactory(bool enable) {
    delegate_to_generated_factory_ = enable;
  }

  // implements messagefactory ---------------------------------------

  // given a descriptor, constructs the default (prototype) message of that
  // type.  you can then call that message's new() method to construct a
  // mutable message of that type.
  //
  // calling this method twice with the same descriptor returns the same
  // object.  the returned object remains property of the factory and will
  // be destroyed when the factory is destroyed.  also, any objects created
  // by calling the prototype's new() method share some data with the
  // prototype, so these must be destroyed before the dynamicmessagefactory
  // is destroyed.
  //
  // the given descriptor must outlive the returned message, and hence must
  // outlive the dynamicmessagefactory.
  //
  // the method is thread-safe.
  const message* getprototype(const descriptor* type);

 private:
  const descriptorpool* pool_;
  bool delegate_to_generated_factory_;

  // this struct just contains a hash_map.  we can't #include <google/protobuf/stubs/hash.h> from
  // this header due to hacks needed for hash_map portability in the open source
  // release.  namely, stubs/hash.h, which defines hash_map portably, is not a
  // public header (for good reason), but dynamic_message.h is, and public
  // headers may only #include other public headers.
  struct prototypemap;
  scoped_ptr<prototypemap> prototypes_;
  mutable mutex prototypes_mutex_;

  friend class dynamicmessage;
  const message* getprototypenolock(const descriptor* type);

  google_disallow_evil_constructors(dynamicmessagefactory);
};

}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_dynamic_message_h__
