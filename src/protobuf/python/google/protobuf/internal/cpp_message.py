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

"""contains helper functions used to create protocol message classes from
descriptor objects at runtime backed by the protocol buffer c++ api.
"""

__author__ = 'petar@google.com (petar petrov)'

import copy_reg
import operator
from google.protobuf.internal import _net_proto2___python
from google.protobuf.internal import enum_type_wrapper
from google.protobuf import message


_label_repeated = _net_proto2___python.label_repeated
_label_optional = _net_proto2___python.label_optional
_cpptype_message = _net_proto2___python.cpptype_message
_type_message = _net_proto2___python.type_message


def getdescriptorpool():
  """creates a new descriptorpool c++ object."""
  return _net_proto2___python.newcdescriptorpool()


_pool = getdescriptorpool()


def getfielddescriptor(full_field_name):
  """searches for a field descriptor given a full field name."""
  return _pool.findfieldbyname(full_field_name)


def buildfile(content):
  """registers a new proto file in the underlying c++ descriptor pool."""
  _net_proto2___python.buildfile(content)


def getextensiondescriptor(full_extension_name):
  """searches for extension descriptor given a full field name."""
  return _pool.findextensionbyname(full_extension_name)


def newcmessage(full_message_name):
  """creates a new c++ protocol message by its name."""
  return _net_proto2___python.newcmessage(full_message_name)


def scalarproperty(cdescriptor):
  """returns a scalar property for the given descriptor."""

  def getter(self):
    return self._cmsg.getscalar(cdescriptor)

  def setter(self, value):
    self._cmsg.setscalar(cdescriptor, value)

  return property(getter, setter)


def compositeproperty(cdescriptor, message_type):
  """returns a python property the given composite field."""

  def getter(self):
    sub_message = self._composite_fields.get(cdescriptor.name, none)
    if sub_message is none:
      cmessage = self._cmsg.newsubmessage(cdescriptor)
      sub_message = message_type._concrete_class(__cmessage=cmessage)
      self._composite_fields[cdescriptor.name] = sub_message
    return sub_message

  return property(getter)


class repeatedscalarcontainer(object):
  """container for repeated scalar fields."""

  __slots__ = ['_message', '_cfield_descriptor', '_cmsg']

  def __init__(self, msg, cfield_descriptor):
    self._message = msg
    self._cmsg = msg._cmsg
    self._cfield_descriptor = cfield_descriptor

  def append(self, value):
    self._cmsg.addrepeatedscalar(
        self._cfield_descriptor, value)

  def extend(self, sequence):
    for element in sequence:
      self.append(element)

  def insert(self, key, value):
    values = self[slice(none, none, none)]
    values.insert(key, value)
    self._cmsg.assignrepeatedscalar(self._cfield_descriptor, values)

  def remove(self, value):
    values = self[slice(none, none, none)]
    values.remove(value)
    self._cmsg.assignrepeatedscalar(self._cfield_descriptor, values)

  def __setitem__(self, key, value):
    values = self[slice(none, none, none)]
    values[key] = value
    self._cmsg.assignrepeatedscalar(self._cfield_descriptor, values)

  def __getitem__(self, key):
    return self._cmsg.getrepeatedscalar(self._cfield_descriptor, key)

  def __delitem__(self, key):
    self._cmsg.deleterepeatedfield(self._cfield_descriptor, key)

  def __len__(self):
    return len(self[slice(none, none, none)])

  def __eq__(self, other):
    if self is other:
      return true
    if not operator.issequencetype(other):
      raise typeerror(
          'can only compare repeated scalar fields against sequences.')
    # we are presumably comparing against some other sequence type.
    return other == self[slice(none, none, none)]

  def __ne__(self, other):
    return not self == other

  def __hash__(self):
    raise typeerror('unhashable object')

  def sort(self, *args, **kwargs):
    # maintain compatibility with the previous interface.
    if 'sort_function' in kwargs:
      kwargs['cmp'] = kwargs.pop('sort_function')
    self._cmsg.assignrepeatedscalar(self._cfield_descriptor,
                                    sorted(self, *args, **kwargs))


def repeatedscalarproperty(cdescriptor):
  """returns a python property the given repeated scalar field."""

  def getter(self):
    container = self._composite_fields.get(cdescriptor.name, none)
    if container is none:
      container = repeatedscalarcontainer(self, cdescriptor)
      self._composite_fields[cdescriptor.name] = container
    return container

  def setter(self, new_value):
    raise attributeerror('assignment not allowed to repeated field '
                         '"%s" in protocol message object.' % cdescriptor.name)

  doc = 'magic attribute generated for "%s" proto field.' % cdescriptor.name
  return property(getter, setter, doc=doc)


class repeatedcompositecontainer(object):
  """container for repeated composite fields."""

  __slots__ = ['_message', '_subclass', '_cfield_descriptor', '_cmsg']

  def __init__(self, msg, cfield_descriptor, subclass):
    self._message = msg
    self._cmsg = msg._cmsg
    self._subclass = subclass
    self._cfield_descriptor = cfield_descriptor

  def add(self, **kwargs):
    cmessage = self._cmsg.addmessage(self._cfield_descriptor)
    return self._subclass(__cmessage=cmessage, __owner=self._message, **kwargs)

  def extend(self, elem_seq):
    """extends by appending the given sequence of elements of the same type
    as this one, copying each individual message.
    """
    for message in elem_seq:
      self.add().mergefrom(message)

  def remove(self, value):
    # todo(protocol-devel): this is inefficient as it needs to generate a
    # message pointer for each message only to do index().  move this to a c++
    # extension function.
    self.__delitem__(self[slice(none, none, none)].index(value))

  def mergefrom(self, other):
    for message in other[:]:
      self.add().mergefrom(message)

  def __getitem__(self, key):
    cmessages = self._cmsg.getrepeatedmessage(
        self._cfield_descriptor, key)
    subclass = self._subclass
    if not isinstance(cmessages, list):
      return subclass(__cmessage=cmessages, __owner=self._message)

    return [subclass(__cmessage=m, __owner=self._message) for m in cmessages]

  def __delitem__(self, key):
    self._cmsg.deleterepeatedfield(
        self._cfield_descriptor, key)

  def __len__(self):
    return self._cmsg.fieldlength(self._cfield_descriptor)

  def __eq__(self, other):
    """compares the current instance with another one."""
    if self is other:
      return true
    if not isinstance(other, self.__class__):
      raise typeerror('can only compare repeated composite fields against '
                      'other repeated composite fields.')
    messages = self[slice(none, none, none)]
    other_messages = other[slice(none, none, none)]
    return messages == other_messages

  def __hash__(self):
    raise typeerror('unhashable object')

  def sort(self, cmp=none, key=none, reverse=false, **kwargs):
    # maintain compatibility with the old interface.
    if cmp is none and 'sort_function' in kwargs:
      cmp = kwargs.pop('sort_function')

    # the cmp function, if provided, is passed the results of the key function,
    # so we only need to wrap one of them.
    if key is none:
      index_key = self.__getitem__
    else:
      index_key = lambda i: key(self[i])

    # sort the list of current indexes by the underlying object.
    indexes = range(len(self))
    indexes.sort(cmp=cmp, key=index_key, reverse=reverse)

    # apply the transposition.
    for dest, src in enumerate(indexes):
      if dest == src:
        continue
      self._cmsg.swaprepeatedfieldelements(self._cfield_descriptor, dest, src)
      # don't swap the same value twice.
      indexes[src] = src


def repeatedcompositeproperty(cdescriptor, message_type):
  """returns a python property for the given repeated composite field."""

  def getter(self):
    container = self._composite_fields.get(cdescriptor.name, none)
    if container is none:
      container = repeatedcompositecontainer(
          self, cdescriptor, message_type._concrete_class)
      self._composite_fields[cdescriptor.name] = container
    return container

  def setter(self, new_value):
    raise attributeerror('assignment not allowed to repeated field '
                         '"%s" in protocol message object.' % cdescriptor.name)

  doc = 'magic attribute generated for "%s" proto field.' % cdescriptor.name
  return property(getter, setter, doc=doc)


class extensiondict(object):
  """extension dictionary added to each protocol message."""

  def __init__(self, msg):
    self._message = msg
    self._cmsg = msg._cmsg
    self._values = {}

  def __setitem__(self, extension, value):
    from google.protobuf import descriptor
    if not isinstance(extension, descriptor.fielddescriptor):
      raise keyerror('bad extension %r.' % (extension,))
    cdescriptor = extension._cdescriptor
    if (cdescriptor.label != _label_optional or
        cdescriptor.cpp_type == _cpptype_message):
      raise typeerror('extension %r is repeated and/or a composite type.' % (
          extension.full_name,))
    self._cmsg.setscalar(cdescriptor, value)
    self._values[extension] = value

  def __getitem__(self, extension):
    from google.protobuf import descriptor
    if not isinstance(extension, descriptor.fielddescriptor):
      raise keyerror('bad extension %r.' % (extension,))

    cdescriptor = extension._cdescriptor
    if (cdescriptor.label != _label_repeated and
        cdescriptor.cpp_type != _cpptype_message):
      return self._cmsg.getscalar(cdescriptor)

    ext = self._values.get(extension, none)
    if ext is not none:
      return ext

    ext = self._createnewhandle(extension)
    self._values[extension] = ext
    return ext

  def clearextension(self, extension):
    from google.protobuf import descriptor
    if not isinstance(extension, descriptor.fielddescriptor):
      raise keyerror('bad extension %r.' % (extension,))
    self._cmsg.clearfieldbydescriptor(extension._cdescriptor)
    if extension in self._values:
      del self._values[extension]

  def hasextension(self, extension):
    from google.protobuf import descriptor
    if not isinstance(extension, descriptor.fielddescriptor):
      raise keyerror('bad extension %r.' % (extension,))
    return self._cmsg.hasfieldbydescriptor(extension._cdescriptor)

  def _findextensionbyname(self, name):
    """tries to find a known extension with the specified name.

    args:
      name: extension full name.

    returns:
      extension field descriptor.
    """
    return self._message._extensions_by_name.get(name, none)

  def _createnewhandle(self, extension):
    cdescriptor = extension._cdescriptor
    if (cdescriptor.label != _label_repeated and
        cdescriptor.cpp_type == _cpptype_message):
      cmessage = self._cmsg.newsubmessage(cdescriptor)
      return extension.message_type._concrete_class(__cmessage=cmessage)

    if cdescriptor.label == _label_repeated:
      if cdescriptor.cpp_type == _cpptype_message:
        return repeatedcompositecontainer(
            self._message, cdescriptor, extension.message_type._concrete_class)
      else:
        return repeatedscalarcontainer(self._message, cdescriptor)
    # this shouldn't happen!
    assert false
    return none


def newmessage(bases, message_descriptor, dictionary):
  """creates a new protocol message *class*."""
  _addclassattributesfornestedextensions(message_descriptor, dictionary)
  _addenumvalues(message_descriptor, dictionary)
  _adddescriptors(message_descriptor, dictionary)
  return bases


def initmessage(message_descriptor, cls):
  """constructs a new message instance (called before instance's __init__)."""
  cls._extensions_by_name = {}
  _addinitmethod(message_descriptor, cls)
  _addmessagemethods(message_descriptor, cls)
  _addpropertiesforextensions(message_descriptor, cls)
  copy_reg.pickle(cls, lambda obj: (cls, (), obj.__getstate__()))


def _adddescriptors(message_descriptor, dictionary):
  """sets up a new protocol message class dictionary.

  args:
    message_descriptor: a descriptor instance describing this message type.
    dictionary: class dictionary to which we'll add a '__slots__' entry.
  """
  dictionary['__descriptors'] = {}
  for field in message_descriptor.fields:
    dictionary['__descriptors'][field.name] = getfielddescriptor(
        field.full_name)

  dictionary['__slots__'] = list(dictionary['__descriptors'].iterkeys()) + [
      '_cmsg', '_owner', '_composite_fields', 'extensions', '_hack_refcounts']


def _addenumvalues(message_descriptor, dictionary):
  """sets class-level attributes for all enum fields defined in this message.

  args:
    message_descriptor: descriptor object for this message type.
    dictionary: class dictionary that should be populated.
  """
  for enum_type in message_descriptor.enum_types:
    dictionary[enum_type.name] = enum_type_wrapper.enumtypewrapper(enum_type)
    for enum_value in enum_type.values:
      dictionary[enum_value.name] = enum_value.number


def _addclassattributesfornestedextensions(message_descriptor, dictionary):
  """adds class attributes for the nested extensions."""
  extension_dict = message_descriptor.extensions_by_name
  for extension_name, extension_field in extension_dict.iteritems():
    assert extension_name not in dictionary
    dictionary[extension_name] = extension_field


def _addinitmethod(message_descriptor, cls):
  """adds an __init__ method to cls."""

  # create and attach message field properties to the message class.
  # this can be done just once per message class, since property setters and
  # getters are passed the message instance.
  # this makes message instantiation extremely fast, and at the same time it
  # doesn't require the creation of property objects for each message instance,
  # which saves a lot of memory.
  for field in message_descriptor.fields:
    field_cdescriptor = cls.__descriptors[field.name]
    if field.label == _label_repeated:
      if field.cpp_type == _cpptype_message:
        value = repeatedcompositeproperty(field_cdescriptor, field.message_type)
      else:
        value = repeatedscalarproperty(field_cdescriptor)
    elif field.cpp_type == _cpptype_message:
      value = compositeproperty(field_cdescriptor, field.message_type)
    else:
      value = scalarproperty(field_cdescriptor)
    setattr(cls, field.name, value)

    # attach a constant with the field number.
    constant_name = field.name.upper() + '_field_number'
    setattr(cls, constant_name, field.number)

  def init(self, **kwargs):
    """message constructor."""
    cmessage = kwargs.pop('__cmessage', none)
    if cmessage:
      self._cmsg = cmessage
    else:
      self._cmsg = newcmessage(message_descriptor.full_name)

    # keep a reference to the owner, as the owner keeps a reference to the
    # underlying protocol buffer message.
    owner = kwargs.pop('__owner', none)
    if owner:
      self._owner = owner

    if message_descriptor.is_extendable:
      self.extensions = extensiondict(self)
    else:
      # reference counting in the c++ code is broken and depends on
      # the extensions reference to keep this object alive during unit
      # tests (see b/4856052).  remove this once b/4945904 is fixed.
      self._hack_refcounts = self
    self._composite_fields = {}

    for field_name, field_value in kwargs.iteritems():
      field_cdescriptor = self.__descriptors.get(field_name, none)
      if not field_cdescriptor:
        raise valueerror('protocol message has no "%s" field.' % field_name)
      if field_cdescriptor.label == _label_repeated:
        if field_cdescriptor.cpp_type == _cpptype_message:
          field_name = getattr(self, field_name)
          for val in field_value:
            field_name.add().mergefrom(val)
        else:
          getattr(self, field_name).extend(field_value)
      elif field_cdescriptor.cpp_type == _cpptype_message:
        getattr(self, field_name).mergefrom(field_value)
      else:
        setattr(self, field_name, field_value)

  init.__module__ = none
  init.__doc__ = none
  cls.__init__ = init


def _ismessagesetextension(field):
  """checks if a field is a message set extension."""
  return (field.is_extension and
          field.containing_type.has_options and
          field.containing_type.getoptions().message_set_wire_format and
          field.type == _type_message and
          field.message_type == field.extension_scope and
          field.label == _label_optional)


def _addmessagemethods(message_descriptor, cls):
  """adds the methods to a protocol message class."""
  if message_descriptor.is_extendable:

    def clearextension(self, extension):
      self.extensions.clearextension(extension)

    def hasextension(self, extension):
      return self.extensions.hasextension(extension)

  def hasfield(self, field_name):
    return self._cmsg.hasfield(field_name)

  def clearfield(self, field_name):
    child_cmessage = none
    if field_name in self._composite_fields:
      child_field = self._composite_fields[field_name]
      del self._composite_fields[field_name]

      child_cdescriptor = self.__descriptors[field_name]
      # todo(anuraag): support clearing repeated message fields as well.
      if (child_cdescriptor.label != _label_repeated and
          child_cdescriptor.cpp_type == _cpptype_message):
        child_field._owner = none
        child_cmessage = child_field._cmsg

    if child_cmessage is not none:
      self._cmsg.clearfield(field_name, child_cmessage)
    else:
      self._cmsg.clearfield(field_name)

  def clear(self):
    cmessages_to_release = []
    for field_name, child_field in self._composite_fields.iteritems():
      child_cdescriptor = self.__descriptors[field_name]
      # todo(anuraag): support clearing repeated message fields as well.
      if (child_cdescriptor.label != _label_repeated and
          child_cdescriptor.cpp_type == _cpptype_message):
        child_field._owner = none
        cmessages_to_release.append((child_cdescriptor, child_field._cmsg))
    self._composite_fields.clear()
    self._cmsg.clear(cmessages_to_release)

  def isinitialized(self, errors=none):
    if self._cmsg.isinitialized():
      return true
    if errors is not none:
      errors.extend(self.findinitializationerrors());
    return false

  def serializetostring(self):
    if not self.isinitialized():
      raise message.encodeerror(
          'message %s is missing required fields: %s' % (
          self._cmsg.full_name, ','.join(self.findinitializationerrors())))
    return self._cmsg.serializetostring()

  def serializepartialtostring(self):
    return self._cmsg.serializepartialtostring()

  def parsefromstring(self, serialized):
    self.clear()
    self.mergefromstring(serialized)

  def mergefromstring(self, serialized):
    byte_size = self._cmsg.mergefromstring(serialized)
    if byte_size < 0:
      raise message.decodeerror('unable to merge from string.')
    return byte_size

  def mergefrom(self, msg):
    if not isinstance(msg, cls):
      raise typeerror(
          "parameter to mergefrom() must be instance of same class: "
          "expected %s got %s." % (cls.__name__, type(msg).__name__))
    self._cmsg.mergefrom(msg._cmsg)

  def copyfrom(self, msg):
    self._cmsg.copyfrom(msg._cmsg)

  def bytesize(self):
    return self._cmsg.bytesize()

  def setinparent(self):
    return self._cmsg.setinparent()

  def listfields(self):
    all_fields = []
    field_list = self._cmsg.listfields()
    fields_by_name = cls.descriptor.fields_by_name
    for is_extension, field_name in field_list:
      if is_extension:
        extension = cls._extensions_by_name[field_name]
        all_fields.append((extension, self.extensions[extension]))
      else:
        field_descriptor = fields_by_name[field_name]
        all_fields.append(
            (field_descriptor, getattr(self, field_name)))
    all_fields.sort(key=lambda item: item[0].number)
    return all_fields

  def findinitializationerrors(self):
    return self._cmsg.findinitializationerrors()

  def __str__(self):
    return self._cmsg.debugstring()

  def __eq__(self, other):
    if self is other:
      return true
    if not isinstance(other, self.__class__):
      return false
    return self.listfields() == other.listfields()

  def __ne__(self, other):
    return not self == other

  def __hash__(self):
    raise typeerror('unhashable object')

  def __unicode__(self):
    # lazy import to prevent circular import when text_format imports this file.
    from google.protobuf import text_format
    return text_format.messagetostring(self, as_utf8=true).decode('utf-8')

  # attach the local methods to the message class.
  for key, value in locals().copy().iteritems():
    if key not in ('key', 'value', '__builtins__', '__name__', '__doc__'):
      setattr(cls, key, value)

  # static methods:

  def registerextension(extension_handle):
    extension_handle.containing_type = cls.descriptor
    cls._extensions_by_name[extension_handle.full_name] = extension_handle

    if _ismessagesetextension(extension_handle):
      # messageset extension.  also register under type name.
      cls._extensions_by_name[
          extension_handle.message_type.full_name] = extension_handle
  cls.registerextension = staticmethod(registerextension)

  def fromstring(string):
    msg = cls()
    msg.mergefromstring(string)
    return msg
  cls.fromstring = staticmethod(fromstring)



def _addpropertiesforextensions(message_descriptor, cls):
  """adds properties for all fields in this protocol message type."""
  extension_dict = message_descriptor.extensions_by_name
  for extension_name, extension_field in extension_dict.iteritems():
    constant_name = extension_name.upper() + '_field_number'
    setattr(cls, constant_name, extension_field.number)
