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

package com.google.protobuf;

/**
 * blocking equivalent to {@link service}.
 *
 * @author kenton@google.com kenton varda
 * @author cpovirk@google.com chris povirk
 */
public interface blockingservice {
  /**
   * equivalent to {@link service#getdescriptorfortype}.
   */
  descriptors.servicedescriptor getdescriptorfortype();

  /**
   * equivalent to {@link service#callmethod}, except that
   * {@code callblockingmethod()} returns the result of the rpc or throws a
   * {@link serviceexception} if there is a failure, rather than passing the
   * information to a callback.
   */
  message callblockingmethod(descriptors.methoddescriptor method,
                             rpccontroller controller,
                             message request) throws serviceexception;

  /**
   * equivalent to {@link service#getrequestprototype}.
   */
  message getrequestprototype(descriptors.methoddescriptor method);

  /**
   * equivalent to {@link service#getresponseprototype}.
   */
  message getresponseprototype(descriptors.methoddescriptor method);
}
