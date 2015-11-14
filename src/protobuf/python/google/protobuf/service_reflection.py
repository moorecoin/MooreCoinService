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

"""contains metaclasses used to create protocol service and service stub
classes from servicedescriptor objects at runtime.

the generatedservicetype and generatedservicestubtype metaclasses are used to
inject all useful functionality into the classes output by the protocol
compiler at compile-time.
"""

__author__ = 'petar@google.com (petar petrov)'


class generatedservicetype(type):

  """metaclass for service classes created at runtime from servicedescriptors.

  implementations for all methods described in the service class are added here
  by this class. we also create properties to allow getting/setting all fields
  in the protocol message.

  the protocol compiler currently uses this metaclass to create protocol service
  classes at runtime. clients can also manually create their own classes at
  runtime, as in this example:

  mydescriptor = servicedescriptor(.....)
  class myprotoservice(service.service):
    __metaclass__ = generatedservicetype
    descriptor = mydescriptor
  myservice_instance = myprotoservice()
  ...
  """

  _descriptor_key = 'descriptor'

  def __init__(cls, name, bases, dictionary):
    """creates a message service class.

    args:
      name: name of the class (ignored, but required by the metaclass
        protocol).
      bases: base classes of the class being constructed.
      dictionary: the class dictionary of the class being constructed.
        dictionary[_descriptor_key] must contain a servicedescriptor object
        describing this protocol service type.
    """
    # don't do anything if this class doesn't have a descriptor. this happens
    # when a service class is subclassed.
    if generatedservicetype._descriptor_key not in dictionary:
      return
    descriptor = dictionary[generatedservicetype._descriptor_key]
    service_builder = _servicebuilder(descriptor)
    service_builder.buildservice(cls)


class generatedservicestubtype(generatedservicetype):

  """metaclass for service stubs created at runtime from servicedescriptors.

  this class has similar responsibilities as generatedservicetype, except that
  it creates the service stub classes.
  """

  _descriptor_key = 'descriptor'

  def __init__(cls, name, bases, dictionary):
    """creates a message service stub class.

    args:
      name: name of the class (ignored, here).
      bases: base classes of the class being constructed.
      dictionary: the class dictionary of the class being constructed.
        dictionary[_descriptor_key] must contain a servicedescriptor object
        describing this protocol service type.
    """
    super(generatedservicestubtype, cls).__init__(name, bases, dictionary)
    # don't do anything if this class doesn't have a descriptor. this happens
    # when a service stub is subclassed.
    if generatedservicestubtype._descriptor_key not in dictionary:
      return
    descriptor = dictionary[generatedservicestubtype._descriptor_key]
    service_stub_builder = _servicestubbuilder(descriptor)
    service_stub_builder.buildservicestub(cls)


class _servicebuilder(object):

  """this class constructs a protocol service class using a service descriptor.

  given a service descriptor, this class constructs a class that represents
  the specified service descriptor. one service builder instance constructs
  exactly one service class. that means all instances of that class share the
  same builder.
  """

  def __init__(self, service_descriptor):
    """initializes an instance of the service class builder.

    args:
      service_descriptor: servicedescriptor to use when constructing the
        service class.
    """
    self.descriptor = service_descriptor

  def buildservice(self, cls):
    """constructs the service class.

    args:
      cls: the class that will be constructed.
    """

    # callmethod needs to operate with an instance of the service class. this
    # internal wrapper function exists only to be able to pass the service
    # instance to the method that does the real callmethod work.
    def _wrapcallmethod(srvc, method_descriptor,
                        rpc_controller, request, callback):
      return self._callmethod(srvc, method_descriptor,
                       rpc_controller, request, callback)
    self.cls = cls
    cls.callmethod = _wrapcallmethod
    cls.getdescriptor = staticmethod(lambda: self.descriptor)
    cls.getdescriptor.__doc__ = "returns the service descriptor."
    cls.getrequestclass = self._getrequestclass
    cls.getresponseclass = self._getresponseclass
    for method in self.descriptor.methods:
      setattr(cls, method.name, self._generatenonimplementedmethod(method))

  def _callmethod(self, srvc, method_descriptor,
                  rpc_controller, request, callback):
    """calls the method described by a given method descriptor.

    args:
      srvc: instance of the service for which this method is called.
      method_descriptor: descriptor that represent the method to call.
      rpc_controller: rpc controller to use for this method's execution.
      request: request protocol message.
      callback: a callback to invoke after the method has completed.
    """
    if method_descriptor.containing_service != self.descriptor:
      raise runtimeerror(
          'callmethod() given method descriptor for wrong service type.')
    method = getattr(srvc, method_descriptor.name)
    return method(rpc_controller, request, callback)

  def _getrequestclass(self, method_descriptor):
    """returns the class of the request protocol message.

    args:
      method_descriptor: descriptor of the method for which to return the
        request protocol message class.

    returns:
      a class that represents the input protocol message of the specified
      method.
    """
    if method_descriptor.containing_service != self.descriptor:
      raise runtimeerror(
          'getrequestclass() given method descriptor for wrong service type.')
    return method_descriptor.input_type._concrete_class

  def _getresponseclass(self, method_descriptor):
    """returns the class of the response protocol message.

    args:
      method_descriptor: descriptor of the method for which to return the
        response protocol message class.

    returns:
      a class that represents the output protocol message of the specified
      method.
    """
    if method_descriptor.containing_service != self.descriptor:
      raise runtimeerror(
          'getresponseclass() given method descriptor for wrong service type.')
    return method_descriptor.output_type._concrete_class

  def _generatenonimplementedmethod(self, method):
    """generates and returns a method that can be set for a service methods.

    args:
      method: descriptor of the service method for which a method is to be
        generated.

    returns:
      a method that can be added to the service class.
    """
    return lambda inst, rpc_controller, request, callback: (
        self._nonimplementedmethod(method.name, rpc_controller, callback))

  def _nonimplementedmethod(self, method_name, rpc_controller, callback):
    """the body of all methods in the generated service class.

    args:
      method_name: name of the method being executed.
      rpc_controller: rpc controller used to execute this method.
      callback: a callback which will be invoked when the method finishes.
    """
    rpc_controller.setfailed('method %s not implemented.' % method_name)
    callback(none)


class _servicestubbuilder(object):

  """constructs a protocol service stub class using a service descriptor.

  given a service descriptor, this class constructs a suitable stub class.
  a stub is just a type-safe wrapper around an rpcchannel which emulates a
  local implementation of the service.

  one service stub builder instance constructs exactly one class. it means all
  instances of that class share the same service stub builder.
  """

  def __init__(self, service_descriptor):
    """initializes an instance of the service stub class builder.

    args:
      service_descriptor: servicedescriptor to use when constructing the
        stub class.
    """
    self.descriptor = service_descriptor

  def buildservicestub(self, cls):
    """constructs the stub class.

    args:
      cls: the class that will be constructed.
    """

    def _servicestubinit(stub, rpc_channel):
      stub.rpc_channel = rpc_channel
    self.cls = cls
    cls.__init__ = _servicestubinit
    for method in self.descriptor.methods:
      setattr(cls, method.name, self._generatestubmethod(method))

  def _generatestubmethod(self, method):
    return (lambda inst, rpc_controller, request, callback=none:
        self._stubmethod(inst, method, rpc_controller, request, callback))

  def _stubmethod(self, stub, method_descriptor,
                  rpc_controller, request, callback):
    """the body of all service methods in the generated stub class.

    args:
      stub: stub instance.
      method_descriptor: descriptor of the invoked method.
      rpc_controller: rpc controller to execute the method.
      request: request protocol message.
      callback: a callback to execute when the method finishes.
    returns:
      response message (in case of blocking call).
    """
    return stub.rpc_channel.callmethod(
        method_descriptor, rpc_controller, request,
        method_descriptor.output_type._concrete_class, callback)
