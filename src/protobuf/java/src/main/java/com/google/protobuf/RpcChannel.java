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
 * <p>abstract interface for an rpc channel.  an {@code rpcchannel} represents a
 * communication line to a {@link service} which can be used to call that
 * {@link service}'s methods.  the {@link service} may be running on another
 * machine.  normally, you should not call an {@code rpcchannel} directly, but
 * instead construct a stub {@link service} wrapping it.  example:
 *
 * <pre>
 *   rpcchannel channel = rpcimpl.newchannel("remotehost.example.com:1234");
 *   rpccontroller controller = rpcimpl.newcontroller();
 *   myservice service = myservice.newstub(channel);
 *   service.mymethod(controller, request, callback);
 * </pre>
 *
 * <p>starting with version 2.3.0, rpc implementations should not try to build
 * on this, but should instead provide code generator plugins which generate
 * code specific to the particular rpc implementation.  this way the generated
 * code can be more appropriate for the implementation in use and can avoid
 * unnecessary layers of indirection.
 *
 * @author kenton@google.com kenton varda
 */
public interface rpcchannel {
  /**
   * call the given method of the remote service.  this method is similar to
   * {@code service.callmethod()} with one important difference:  the caller
   * decides the types of the {@code message} objects, not the callee.  the
   * request may be of any type as long as
   * {@code request.getdescriptor() == method.getinputtype()}.
   * the response passed to the callback will be of the same type as
   * {@code responseprototype} (which must have
   * {@code getdescriptor() == method.getoutputtype()}).
   */
  void callmethod(descriptors.methoddescriptor method,
                  rpccontroller controller,
                  message request,
                  message responseprototype,
                  rpccallback<message> done);
}
