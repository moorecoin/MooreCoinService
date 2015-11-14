# protocol buffers - google's data interchange format
# copyright 2008 google inc.  all rights reserved.
# http://code.google.com/p/protobuf/
#
# redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * neither the name of google inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# this software is provided by the copyright holders and contributors
# "as is" and any express or implied warranties, including, but not
# limited to, the implied warranties of merchantability and fitness for
# a particular purpose are disclaimed. in no event shall the copyright
# owner or contributors be liable for any direct, indirect, incidental,
# special, exemplary, or consequential damages (including, but not
# limited to, procurement of substitute goods or services; loss of use,
# data, or profits; or business interruption) however caused and on any
# theory of liability, whether in contract, strict liability, or tort
# (including negligence or otherwise) arising in any way out of the use
# of this software, even if advised of the possibility of such damage.

"""deprecated:  declares the rpc service interfaces.

this module declares the abstract interfaces underlying proto2 rpc
services.  these are intended to be independent of any particular rpc
implementation, so that proto2 services can be used on top of a variety
of implementations.  starting with version 2.3.0, rpc implementations should
not try to build on these, but should instead provide code generator plugins
which generate code specific to the particular rpc implementation.  this way
the generated code can be more appropriate for the implementation in use
and can avoid unnecessary layers of indirection.
"""

__author__ = 'petar@google.com (petar petrov)'


class rpcexception(exception):
  """exception raised on failed blocking rpc method call."""
  pass


class service(object):

  """abstract base interface for protocol-buffer-based rpc services.

  services themselves are abstract classes (implemented either by servers or as
  stubs), but they subclass this base interface. the methods of this
  interface can be used to call the methods of the service without knowing
  its exact type at compile time (analogous to the message interface).
  """

  def getdescriptor():
    """retrieves this service's descriptor."""
    raise notimplementederror

  def callmethod(self, method_descriptor, rpc_controller,
                 request, done):
    """calls a method of the service specified by method_descriptor.

    if "done" is none then the call is blocking and the response
    message will be returned directly.  otherwise the call is asynchronous
    and "done" will later be called with the response value.

    in the blocking case, rpcexception will be raised on error.

    preconditions:
    * method_descriptor.service == getdescriptor
    * request is of the exact same classes as returned by
      getrequestclass(method).
    * after the call has started, the request must not be modified.
    * "rpc_controller" is of the correct type for the rpc implementation being
      used by this service.  for stubs, the "correct type" depends on the
      rpcchannel which the stub is using.

    postconditions:
    * "done" will be called when the method is complete.  this may be
      before callmethod() returns or it may be at some point in the future.
    * if the rpc failed, the response value passed to "done" will be none.
      further details about the failure can be found by querying the
      rpccontroller.
    """
    raise notimplementederror

  def getrequestclass(self, method_descriptor):
    """returns the class of the request message for the specified method.

    callmethod() requires that the request is of a particular subclass of
    message. getrequestclass() gets the default instance of this required
    type.

    example:
      method = service.getdescriptor().findmethodbyname("foo")
      request = stub.getrequestclass(method)()
      request.parsefromstring(input)
      service.callmethod(method, request, callback)
    """
    raise notimplementederror

  def getresponseclass(self, method_descriptor):
    """returns the class of the response message for the specified method.

    this method isn't really needed, as the rpcchannel's callmethod constructs
    the response protocol message. it's provided anyway in case it is useful
    for the caller to know the response type in advance.
    """
    raise notimplementederror


class rpccontroller(object):

  """an rpccontroller mediates a single method call.

  the primary purpose of the controller is to provide a way to manipulate
  settings specific to the rpc implementation and to find out about rpc-level
  errors. the methods provided by the rpccontroller interface are intended
  to be a "least common denominator" set of features which we expect all
  implementations to support.  specific implementations may provide more
  advanced features (e.g. deadline propagation).
  """

  # client-side methods below

  def reset(self):
    """resets the rpccontroller to its initial state.

    after the rpccontroller has been reset, it may be reused in
    a new call. must not be called while an rpc is in progress.
    """
    raise notimplementederror

  def failed(self):
    """returns true if the call failed.

    after a call has finished, returns true if the call failed.  the possible
    reasons for failure depend on the rpc implementation.  failed() must not
    be called before a call has finished.  if failed() returns true, the
    contents of the response message are undefined.
    """
    raise notimplementederror

  def errortext(self):
    """if failed is true, returns a human-readable description of the error."""
    raise notimplementederror

  def startcancel(self):
    """initiate cancellation.

    advises the rpc system that the caller desires that the rpc call be
    canceled.  the rpc system may cancel it immediately, may wait awhile and
    then cancel it, or may not even cancel the call at all.  if the call is
    canceled, the "done" callback will still be called and the rpccontroller
    will indicate that the call failed at that time.
    """
    raise notimplementederror

  # server-side methods below

  def setfailed(self, reason):
    """sets a failure reason.

    causes failed() to return true on the client side.  "reason" will be
    incorporated into the message returned by errortext().  if you find
    you need to return machine-readable information about failures, you
    should incorporate it into your response protocol buffer and should
    not call setfailed().
    """
    raise notimplementederror

  def iscanceled(self):
    """checks if the client cancelled the rpc.

    if true, indicates that the client canceled the rpc, so the server may
    as well give up on replying to it.  the server should still call the
    final "done" callback.
    """
    raise notimplementederror

  def notifyoncancel(self, callback):
    """sets a callback to invoke on cancel.

    asks that the given callback be called when the rpc is canceled.  the
    callback will always be called exactly once.  if the rpc completes without
    being canceled, the callback will be called after completion.  if the rpc
    has already been canceled when notifyoncancel() is called, the callback
    will be called immediately.

    notifyoncancel() must be called no more than once per request.
    """
    raise notimplementederror


class rpcchannel(object):

  """abstract interface for an rpc channel.

  an rpcchannel represents a communication line to a service which can be used
  to call that service's methods.  the service may be running on another
  machine. normally, you should not use an rpcchannel directly, but instead
  construct a stub {@link service} wrapping it.  example:

  example:
    rpcchannel channel = rpcimpl.channel("remotehost.example.com:1234")
    rpccontroller controller = rpcimpl.controller()
    myservice service = myservice_stub(channel)
    service.mymethod(controller, request, callback)
  """

  def callmethod(self, method_descriptor, rpc_controller,
                 request, response_class, done):
    """calls the method identified by the descriptor.

    call the given method of the remote service.  the signature of this
    procedure looks the same as service.callmethod(), but the requirements
    are less strict in one important way:  the request object doesn't have to
    be of any specific class as long as its descriptor is method.input_type.
    """
    raise notimplementederror
