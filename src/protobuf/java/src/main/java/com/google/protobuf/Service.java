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
 * abstract base interface for protocol-buffer-based rpc services.  services
 * themselves are abstract classes (implemented either by servers or as
 * stubs), but they subclass this base interface.  the methods of this
 * interface can be used to call the methods of the service without knowing
 * its exact type at compile time (analogous to the message interface).
 *
 * <p>starting with version 2.3.0, rpc implementations should not try to build
 * on this, but should instead provide code generator plugins which generate
 * code specific to the particular rpc implementation.  this way the generated
 * code can be more appropriate for the implementation in use and can avoid
 * unnecessary layers of indirection.
 *
 * @author kenton@google.com kenton varda
 */
public interface service {
  /**
   * get the {@code servicedescriptor} describing this service and its methods.
   */
  descriptors.servicedescriptor getdescriptorfortype();

  /**
   * <p>call a method of the service specified by methoddescriptor.  this is
   * normally implemented as a simple {@code switch()} that calls the standard
   * definitions of the service's methods.
   *
   * <p>preconditions:
   * <ul>
   *   <li>{@code method.getservice() == getdescriptorfortype()}
   *   <li>{@code request} is of the exact same class as the object returned by
   *       {@code getrequestprototype(method)}.
   *   <li>{@code controller} is of the correct type for the rpc implementation
   *       being used by this service.  for stubs, the "correct type" depends
   *       on the rpcchannel which the stub is using.  server-side service
   *       implementations are expected to accept whatever type of
   *       {@code rpccontroller} the server-side rpc implementation uses.
   * </ul>
   *
   * <p>postconditions:
   * <ul>
   *   <li>{@code done} will be called when the method is complete.  this may be
   *       before {@code callmethod()} returns or it may be at some point in
   *       the future.
   *   <li>the parameter to {@code done} is the response.  it must be of the
   *       exact same type as would be returned by
   *       {@code getresponseprototype(method)}.
   *   <li>if the rpc failed, the parameter to {@code done} will be
   *       {@code null}.  further details about the failure can be found by
   *       querying {@code controller}.
   * </ul>
   */
  void callmethod(descriptors.methoddescriptor method,
                  rpccontroller controller,
                  message request,
                  rpccallback<message> done);

  /**
   * <p>{@code callmethod()} requires that the request passed in is of a
   * particular subclass of {@code message}.  {@code getrequestprototype()}
   * gets the default instances of this type for a given method.  you can then
   * call {@code message.newbuilderfortype()} on this instance to
   * construct a builder to build an object which you can then pass to
   * {@code callmethod()}.
   *
   * <p>example:
   * <pre>
   *   methoddescriptor method =
   *     service.getdescriptorfortype().findmethodbyname("foo");
   *   message request =
   *     stub.getrequestprototype(method).newbuilderfortype()
   *         .mergefrom(input).build();
   *   service.callmethod(method, request, callback);
   * </pre>
   */
  message getrequestprototype(descriptors.methoddescriptor method);

  /**
   * like {@code getrequestprototype()}, but gets a prototype of the response
   * message.  {@code getresponseprototype()} is generally not needed because
   * the {@code service} implementation constructs the response message itself,
   * but it may be useful in some cases to know ahead of time what type of
   * object will be returned.
   */
  message getresponseprototype(descriptors.methoddescriptor method);
}
