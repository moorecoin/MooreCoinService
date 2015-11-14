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

# this code is meant to work on python 2.4 and above only.

"""contains a metaclass and helper functions used to create
protocol message classes from descriptor objects at runtime.

recall that a metaclass is the "type" of a class.
(a class is to a metaclass what an instance is to a class.)

in this case, we use the generatedprotocolmessagetype metaclass
to inject all the useful functionality into the classes
output by the protocol compiler at compile-time.

the upshot of all this is that the real implementation
details for all pure-python protocol buffers are *here in
this file*.
"""

__author__ = 'robinson@google.com (will robinson)'


from google.protobuf.internal import api_implementation
from google.protobuf import descriptor as descriptor_mod
from google.protobuf import message

_fielddescriptor = descriptor_mod.fielddescriptor


if api_implementation.type() == 'cpp':
  if api_implementation.version() == 2:
    from google.protobuf.internal.cpp import cpp_message
    _newmessage = cpp_message.newmessage
    _initmessage = cpp_message.initmessage
  else:
    from google.protobuf.internal import cpp_message
    _newmessage = cpp_message.newmessage
    _initmessage = cpp_message.initmessage
else:
  from google.protobuf.internal import python_message
  _newmessage = python_message.newmessage
  _initmessage = python_message.initmessage


class generatedprotocolmessagetype(type):

  """metaclass for protocol message classes created at runtime from descriptors.

  we add implementations for all methods described in the message class.  we
  also create properties to allow getting/setting all fields in the protocol
  message.  finally, we create slots to prevent users from accidentally
  "setting" nonexistent fields in the protocol message, which then wouldn't get
  serialized / deserialized properly.

  the protocol compiler currently uses this metaclass to create protocol
  message classes at runtime.  clients can also manually create their own
  classes at runtime, as in this example:

  mydescriptor = descriptor(.....)
  class myprotoclass(message):
    __metaclass__ = generatedprotocolmessagetype
    descriptor = mydescriptor
  myproto_instance = myprotoclass()
  myproto.foo_field = 23
  ...
  """

  # must be consistent with the protocol-compiler code in
  # proto2/compiler/internal/generator.*.
  _descriptor_key = 'descriptor'

  def __new__(cls, name, bases, dictionary):
    """custom allocation for runtime-generated class types.

    we override __new__ because this is apparently the only place
    where we can meaningfully set __slots__ on the class we're creating(?).
    (the interplay between metaclasses and slots is not very well-documented).

    args:
      name: name of the class (ignored, but required by the
        metaclass protocol).
      bases: base classes of the class we're constructing.
        (should be message.message).  we ignore this field, but
        it's required by the metaclass protocol
      dictionary: the class dictionary of the class we're
        constructing.  dictionary[_descriptor_key] must contain
        a descriptor object describing this protocol message
        type.

    returns:
      newly-allocated class.
    """
    descriptor = dictionary[generatedprotocolmessagetype._descriptor_key]
    bases = _newmessage(bases, descriptor, dictionary)
    superclass = super(generatedprotocolmessagetype, cls)

    new_class = superclass.__new__(cls, name, bases, dictionary)
    setattr(descriptor, '_concrete_class', new_class)
    return new_class

  def __init__(cls, name, bases, dictionary):
    """here we perform the majority of our work on the class.
    we add enum getters, an __init__ method, implementations
    of all message methods, and properties for all fields
    in the protocol type.

    args:
      name: name of the class (ignored, but required by the
        metaclass protocol).
      bases: base classes of the class we're constructing.
        (should be message.message).  we ignore this field, but
        it's required by the metaclass protocol
      dictionary: the class dictionary of the class we're
        constructing.  dictionary[_descriptor_key] must contain
        a descriptor object describing this protocol message
        type.
    """
    descriptor = dictionary[generatedprotocolmessagetype._descriptor_key]
    _initmessage(descriptor, cls)
    superclass = super(generatedprotocolmessagetype, cls)
    superclass.__init__(name, bases, dictionary)


def parsemessage(descriptor, byte_str):
  """generate a new message instance from this descriptor and a byte string.

  args:
    descriptor: protobuf descriptor object
    byte_str: serialized protocol buffer byte string

  returns:
    newly created protobuf message object.
  """

  class _resultclass(message.message):
    __metaclass__ = generatedprotocolmessagetype
    descriptor = descriptor

  new_msg = _resultclass()
  new_msg.parsefromstring(byte_str)
  return new_msg
