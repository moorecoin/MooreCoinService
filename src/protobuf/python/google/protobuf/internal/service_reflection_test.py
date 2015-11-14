#! /usr/bin/python
#
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

"""tests for google.protobuf.internal.service_reflection."""

__author__ = 'petar@google.com (petar petrov)'

import unittest
from google.protobuf import unittest_pb2
from google.protobuf import service_reflection
from google.protobuf import service


class foounittest(unittest.testcase):

  def testservice(self):
    class mockrpcchannel(service.rpcchannel):
      def callmethod(self, method, controller, request, response, callback):
        self.method = method
        self.controller = controller
        self.request = request
        callback(response)

    class mockrpccontroller(service.rpccontroller):
      def setfailed(self, msg):
        self.failure_message = msg

    self.callback_response = none

    class myservice(unittest_pb2.testservice):
      pass

    self.callback_response = none

    def mycallback(response):
      self.callback_response = response

    rpc_controller = mockrpccontroller()
    channel = mockrpcchannel()
    srvc = myservice()
    srvc.foo(rpc_controller, unittest_pb2.foorequest(), mycallback)
    self.assertequal('method foo not implemented.',
                     rpc_controller.failure_message)
    self.assertequal(none, self.callback_response)

    rpc_controller.failure_message = none

    service_descriptor = unittest_pb2.testservice.getdescriptor()
    srvc.callmethod(service_descriptor.methods[1], rpc_controller,
                    unittest_pb2.barrequest(), mycallback)
    self.assertequal('method bar not implemented.',
                     rpc_controller.failure_message)
    self.assertequal(none, self.callback_response)
    
    class myserviceimpl(unittest_pb2.testservice):
      def foo(self, rpc_controller, request, done):
        self.foo_called = true
      def bar(self, rpc_controller, request, done):
        self.bar_called = true

    srvc = myserviceimpl()
    rpc_controller.failure_message = none
    srvc.foo(rpc_controller, unittest_pb2.foorequest(), mycallback)
    self.assertequal(none, rpc_controller.failure_message)
    self.assertequal(true, srvc.foo_called)

    rpc_controller.failure_message = none
    srvc.callmethod(service_descriptor.methods[1], rpc_controller,
                    unittest_pb2.barrequest(), mycallback)
    self.assertequal(none, rpc_controller.failure_message)
    self.assertequal(true, srvc.bar_called)

  def testservicestub(self):
    class mockrpcchannel(service.rpcchannel):
      def callmethod(self, method, controller, request,
                     response_class, callback):
        self.method = method
        self.controller = controller
        self.request = request
        callback(response_class())

    self.callback_response = none

    def mycallback(response):
      self.callback_response = response

    channel = mockrpcchannel()
    stub = unittest_pb2.testservice_stub(channel)
    rpc_controller = 'controller'
    request = 'request'

    # getdescriptor now static, still works as instance method for compatability
    self.assertequal(unittest_pb2.testservice_stub.getdescriptor(),
                     stub.getdescriptor())

    # invoke method.
    stub.foo(rpc_controller, request, mycallback)

    self.asserttrue(isinstance(self.callback_response,
                               unittest_pb2.fooresponse))
    self.assertequal(request, channel.request)
    self.assertequal(rpc_controller, channel.controller)
    self.assertequal(stub.getdescriptor().methods[0], channel.method)


if __name__ == '__main__':
  unittest.main()
