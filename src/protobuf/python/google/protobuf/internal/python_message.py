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
#
# todo(robinson): helpers for verbose, common checks like seeing if a
# descriptor's cpp_type is cpptype_message.

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

try:
  from cstringio import stringio
except importerror:
  from stringio import stringio
import copy_reg
import struct
import weakref

# we use "as" to avoid name collisions with variables.
from google.protobuf.internal import containers
from google.protobuf.internal import decoder
from google.protobuf.internal import encoder
from google.protobuf.internal import enum_type_wrapper
from google.protobuf.internal import message_listener as message_listener_mod
from google.protobuf.internal import type_checkers
from google.protobuf.internal import wire_format
from google.protobuf import descriptor as descriptor_mod
from google.protobuf import message as message_mod
from google.protobuf import text_format

_fielddescriptor = descriptor_mod.fielddescriptor


def newmessage(bases, descriptor, dictionary):
  _addclassattributesfornestedextensions(descriptor, dictionary)
  _addslots(descriptor, dictionary)
  return bases


def initmessage(descriptor, cls):
  cls._decoders_by_tag = {}
  cls._extensions_by_name = {}
  cls._extensions_by_number = {}
  if (descriptor.has_options and
      descriptor.getoptions().message_set_wire_format):
    cls._decoders_by_tag[decoder.message_set_item_tag] = (
        decoder.messagesetitemdecoder(cls._extensions_by_number))

  # attach stuff to each fielddescriptor for quick lookup later on.
  for field in descriptor.fields:
    _attachfieldhelpers(cls, field)

  _addenumvalues(descriptor, cls)
  _addinitmethod(descriptor, cls)
  _addpropertiesforfields(descriptor, cls)
  _addpropertiesforextensions(descriptor, cls)
  _addstaticmethods(cls)
  _addmessagemethods(descriptor, cls)
  _addprivatehelpermethods(cls)
  copy_reg.pickle(cls, lambda obj: (cls, (), obj.__getstate__()))


# stateless helpers for generatedprotocolmessagetype below.
# outside clients should not access these directly.
#
# i opted not to make any of these methods on the metaclass, to make it more
# clear that i'm not really using any state there and to keep clients from
# thinking that they have direct access to these construction helpers.


def _propertyname(proto_field_name):
  """returns the name of the public property attribute which
  clients can use to get and (in some cases) set the value
  of a protocol message field.

  args:
    proto_field_name: the protocol message field name, exactly
      as it appears (or would appear) in a .proto file.
  """
  # todo(robinson): escape python keywords (e.g., yield), and test this support.
  # nnorwitz makes my day by writing:
  # """
  # fyi.  see the keyword module in the stdlib. this could be as simple as:
  #
  # if keyword.iskeyword(proto_field_name):
  #   return proto_field_name + "_"
  # return proto_field_name
  # """
  # kenton says:  the above is a bad idea.  people rely on being able to use
  #   getattr() and setattr() to reflectively manipulate field values.  if we
  #   rename the properties, then every such user has to also make sure to apply
  #   the same transformation.  note that currently if you name a field "yield",
  #   you can still access it just fine using getattr/setattr -- it's not even
  #   that cumbersome to do so.
  # todo(kenton):  remove this method entirely if/when everyone agrees with my
  #   position.
  return proto_field_name


def _verifyextensionhandle(message, extension_handle):
  """verify that the given extension handle is valid."""

  if not isinstance(extension_handle, _fielddescriptor):
    raise keyerror('hasextension() expects an extension handle, got: %s' %
                   extension_handle)

  if not extension_handle.is_extension:
    raise keyerror('"%s" is not an extension.' % extension_handle.full_name)

  if not extension_handle.containing_type:
    raise keyerror('"%s" is missing a containing_type.'
                   % extension_handle.full_name)

  if extension_handle.containing_type is not message.descriptor:
    raise keyerror('extension "%s" extends message type "%s", but this '
                   'message is of type "%s".' %
                   (extension_handle.full_name,
                    extension_handle.containing_type.full_name,
                    message.descriptor.full_name))


def _addslots(message_descriptor, dictionary):
  """adds a __slots__ entry to dictionary, containing the names of all valid
  attributes for this message type.

  args:
    message_descriptor: a descriptor instance describing this message type.
    dictionary: class dictionary to which we'll add a '__slots__' entry.
  """
  dictionary['__slots__'] = ['_cached_byte_size',
                             '_cached_byte_size_dirty',
                             '_fields',
                             '_unknown_fields',
                             '_is_present_in_parent',
                             '_listener',
                             '_listener_for_children',
                             '__weakref__']


def _ismessagesetextension(field):
  return (field.is_extension and
          field.containing_type.has_options and
          field.containing_type.getoptions().message_set_wire_format and
          field.type == _fielddescriptor.type_message and
          field.message_type == field.extension_scope and
          field.label == _fielddescriptor.label_optional)


def _attachfieldhelpers(cls, field_descriptor):
  is_repeated = (field_descriptor.label == _fielddescriptor.label_repeated)
  is_packed = (field_descriptor.has_options and
               field_descriptor.getoptions().packed)

  if _ismessagesetextension(field_descriptor):
    field_encoder = encoder.messagesetitemencoder(field_descriptor.number)
    sizer = encoder.messagesetitemsizer(field_descriptor.number)
  else:
    field_encoder = type_checkers.type_to_encoder[field_descriptor.type](
        field_descriptor.number, is_repeated, is_packed)
    sizer = type_checkers.type_to_sizer[field_descriptor.type](
        field_descriptor.number, is_repeated, is_packed)

  field_descriptor._encoder = field_encoder
  field_descriptor._sizer = sizer
  field_descriptor._default_constructor = _defaultvalueconstructorforfield(
      field_descriptor)

  def adddecoder(wiretype, is_packed):
    tag_bytes = encoder.tagbytes(field_descriptor.number, wiretype)
    cls._decoders_by_tag[tag_bytes] = (
        type_checkers.type_to_decoder[field_descriptor.type](
            field_descriptor.number, is_repeated, is_packed,
            field_descriptor, field_descriptor._default_constructor))

  adddecoder(type_checkers.field_type_to_wire_type[field_descriptor.type],
             false)

  if is_repeated and wire_format.istypepackable(field_descriptor.type):
    # to support wire compatibility of adding packed = true, add a decoder for
    # packed values regardless of the field's options.
    adddecoder(wire_format.wiretype_length_delimited, true)


def _addclassattributesfornestedextensions(descriptor, dictionary):
  extension_dict = descriptor.extensions_by_name
  for extension_name, extension_field in extension_dict.iteritems():
    assert extension_name not in dictionary
    dictionary[extension_name] = extension_field


def _addenumvalues(descriptor, cls):
  """sets class-level attributes for all enum fields defined in this message.

  also exporting a class-level object that can name enum values.

  args:
    descriptor: descriptor object for this message type.
    cls: class we're constructing for this message type.
  """
  for enum_type in descriptor.enum_types:
    setattr(cls, enum_type.name, enum_type_wrapper.enumtypewrapper(enum_type))
    for enum_value in enum_type.values:
      setattr(cls, enum_value.name, enum_value.number)


def _defaultvalueconstructorforfield(field):
  """returns a function which returns a default value for a field.

  args:
    field: fielddescriptor object for this field.

  the returned function has one argument:
    message: message instance containing this field, or a weakref proxy
      of same.

  that function in turn returns a default value for this field.  the default
    value may refer back to |message| via a weak reference.
  """

  if field.label == _fielddescriptor.label_repeated:
    if field.has_default_value and field.default_value != []:
      raise valueerror('repeated field default value not empty list: %s' % (
          field.default_value))
    if field.cpp_type == _fielddescriptor.cpptype_message:
      # we can't look at _concrete_class yet since it might not have
      # been set.  (depends on order in which we initialize the classes).
      message_type = field.message_type
      def makerepeatedmessagedefault(message):
        return containers.repeatedcompositefieldcontainer(
            message._listener_for_children, field.message_type)
      return makerepeatedmessagedefault
    else:
      type_checker = type_checkers.gettypechecker(field.cpp_type, field.type)
      def makerepeatedscalardefault(message):
        return containers.repeatedscalarfieldcontainer(
            message._listener_for_children, type_checker)
      return makerepeatedscalardefault

  if field.cpp_type == _fielddescriptor.cpptype_message:
    # _concrete_class may not yet be initialized.
    message_type = field.message_type
    def makesubmessagedefault(message):
      result = message_type._concrete_class()
      result._setlistener(message._listener_for_children)
      return result
    return makesubmessagedefault

  def makescalardefault(message):
    # todo(protobuf-team): this may be broken since there may not be
    # default_value.  combine with has_default_value somehow.
    return field.default_value
  return makescalardefault


def _addinitmethod(message_descriptor, cls):
  """adds an __init__ method to cls."""
  fields = message_descriptor.fields
  def init(self, **kwargs):
    self._cached_byte_size = 0
    self._cached_byte_size_dirty = len(kwargs) > 0
    self._fields = {}
    # _unknown_fields is () when empty for efficiency, and will be turned into
    # a list if fields are added.
    self._unknown_fields = ()
    self._is_present_in_parent = false
    self._listener = message_listener_mod.nullmessagelistener()
    self._listener_for_children = _listener(self)
    for field_name, field_value in kwargs.iteritems():
      field = _getfieldbyname(message_descriptor, field_name)
      if field is none:
        raise typeerror("%s() got an unexpected keyword argument '%s'" %
                        (message_descriptor.name, field_name))
      if field.label == _fielddescriptor.label_repeated:
        copy = field._default_constructor(self)
        if field.cpp_type == _fielddescriptor.cpptype_message:  # composite
          for val in field_value:
            copy.add().mergefrom(val)
        else:  # scalar
          copy.extend(field_value)
        self._fields[field] = copy
      elif field.cpp_type == _fielddescriptor.cpptype_message:
        copy = field._default_constructor(self)
        copy.mergefrom(field_value)
        self._fields[field] = copy
      else:
        setattr(self, field_name, field_value)

  init.__module__ = none
  init.__doc__ = none
  cls.__init__ = init


def _getfieldbyname(message_descriptor, field_name):
  """returns a field descriptor by field name.

  args:
    message_descriptor: a descriptor describing all fields in message.
    field_name: the name of the field to retrieve.
  returns:
    the field descriptor associated with the field name.
  """
  try:
    return message_descriptor.fields_by_name[field_name]
  except keyerror:
    raise valueerror('protocol message has no "%s" field.' % field_name)


def _addpropertiesforfields(descriptor, cls):
  """adds properties for all fields in this protocol message type."""
  for field in descriptor.fields:
    _addpropertiesforfield(field, cls)

  if descriptor.is_extendable:
    # _extensiondict is just an adaptor with no state so we allocate a new one
    # every time it is accessed.
    cls.extensions = property(lambda self: _extensiondict(self))


def _addpropertiesforfield(field, cls):
  """adds a public property for a protocol message field.
  clients can use this property to get and (in the case
  of non-repeated scalar fields) directly set the value
  of a protocol message field.

  args:
    field: a fielddescriptor for this field.
    cls: the class we're constructing.
  """
  # catch it if we add other types that we should
  # handle specially here.
  assert _fielddescriptor.max_cpptype == 10

  constant_name = field.name.upper() + "_field_number"
  setattr(cls, constant_name, field.number)

  if field.label == _fielddescriptor.label_repeated:
    _addpropertiesforrepeatedfield(field, cls)
  elif field.cpp_type == _fielddescriptor.cpptype_message:
    _addpropertiesfornonrepeatedcompositefield(field, cls)
  else:
    _addpropertiesfornonrepeatedscalarfield(field, cls)


def _addpropertiesforrepeatedfield(field, cls):
  """adds a public property for a "repeated" protocol message field.  clients
  can use this property to get the value of the field, which will be either a
  _repeatedscalarfieldcontainer or _repeatedcompositefieldcontainer (see
  below).

  note that when clients add values to these containers, we perform
  type-checking in the case of repeated scalar fields, and we also set any
  necessary "has" bits as a side-effect.

  args:
    field: a fielddescriptor for this field.
    cls: the class we're constructing.
  """
  proto_field_name = field.name
  property_name = _propertyname(proto_field_name)

  def getter(self):
    field_value = self._fields.get(field)
    if field_value is none:
      # construct a new object to represent this field.
      field_value = field._default_constructor(self)

      # atomically check if another thread has preempted us and, if not, swap
      # in the new object we just created.  if someone has preempted us, we
      # take that object and discard ours.
      # warning:  we are relying on setdefault() being atomic.  this is true
      #   in cpython but we haven't investigated others.  this warning appears
      #   in several other locations in this file.
      field_value = self._fields.setdefault(field, field_value)
    return field_value
  getter.__module__ = none
  getter.__doc__ = 'getter for %s.' % proto_field_name

  # we define a setter just so we can throw an exception with a more
  # helpful error message.
  def setter(self, new_value):
    raise attributeerror('assignment not allowed to repeated field '
                         '"%s" in protocol message object.' % proto_field_name)

  doc = 'magic attribute generated for "%s" proto field.' % proto_field_name
  setattr(cls, property_name, property(getter, setter, doc=doc))


def _addpropertiesfornonrepeatedscalarfield(field, cls):
  """adds a public property for a nonrepeated, scalar protocol message field.
  clients can use this property to get and directly set the value of the field.
  note that when the client sets the value of a field by using this property,
  all necessary "has" bits are set as a side-effect, and we also perform
  type-checking.

  args:
    field: a fielddescriptor for this field.
    cls: the class we're constructing.
  """
  proto_field_name = field.name
  property_name = _propertyname(proto_field_name)
  type_checker = type_checkers.gettypechecker(field.cpp_type, field.type)
  default_value = field.default_value
  valid_values = set()

  def getter(self):
    # todo(protobuf-team): this may be broken since there may not be
    # default_value.  combine with has_default_value somehow.
    return self._fields.get(field, default_value)
  getter.__module__ = none
  getter.__doc__ = 'getter for %s.' % proto_field_name
  def setter(self, new_value):
    type_checker.checkvalue(new_value)
    self._fields[field] = new_value
    # check _cached_byte_size_dirty inline to improve performance, since scalar
    # setters are called frequently.
    if not self._cached_byte_size_dirty:
      self._modified()

  setter.__module__ = none
  setter.__doc__ = 'setter for %s.' % proto_field_name

  # add a property to encapsulate the getter/setter.
  doc = 'magic attribute generated for "%s" proto field.' % proto_field_name
  setattr(cls, property_name, property(getter, setter, doc=doc))


def _addpropertiesfornonrepeatedcompositefield(field, cls):
  """adds a public property for a nonrepeated, composite protocol message field.
  a composite field is a "group" or "message" field.

  clients can use this property to get the value of the field, but cannot
  assign to the property directly.

  args:
    field: a fielddescriptor for this field.
    cls: the class we're constructing.
  """
  # todo(robinson): remove duplication with similar method
  # for non-repeated scalars.
  proto_field_name = field.name
  property_name = _propertyname(proto_field_name)

  # todo(komarek): can anyone explain to me why we cache the message_type this
  # way, instead of referring to field.message_type inside of getter(self)?
  # what if someone sets message_type later on (which makes for simpler
  # dyanmic proto descriptor and class creation code).
  message_type = field.message_type

  def getter(self):
    field_value = self._fields.get(field)
    if field_value is none:
      # construct a new object to represent this field.
      field_value = message_type._concrete_class()  # use field.message_type?
      field_value._setlistener(self._listener_for_children)

      # atomically check if another thread has preempted us and, if not, swap
      # in the new object we just created.  if someone has preempted us, we
      # take that object and discard ours.
      # warning:  we are relying on setdefault() being atomic.  this is true
      #   in cpython but we haven't investigated others.  this warning appears
      #   in several other locations in this file.
      field_value = self._fields.setdefault(field, field_value)
    return field_value
  getter.__module__ = none
  getter.__doc__ = 'getter for %s.' % proto_field_name

  # we define a setter just so we can throw an exception with a more
  # helpful error message.
  def setter(self, new_value):
    raise attributeerror('assignment not allowed to composite field '
                         '"%s" in protocol message object.' % proto_field_name)

  # add a property to encapsulate the getter.
  doc = 'magic attribute generated for "%s" proto field.' % proto_field_name
  setattr(cls, property_name, property(getter, setter, doc=doc))


def _addpropertiesforextensions(descriptor, cls):
  """adds properties for all fields in this protocol message type."""
  extension_dict = descriptor.extensions_by_name
  for extension_name, extension_field in extension_dict.iteritems():
    constant_name = extension_name.upper() + "_field_number"
    setattr(cls, constant_name, extension_field.number)


def _addstaticmethods(cls):
  # todo(robinson): this probably needs to be thread-safe(?)
  def registerextension(extension_handle):
    extension_handle.containing_type = cls.descriptor
    _attachfieldhelpers(cls, extension_handle)

    # try to insert our extension, failing if an extension with the same number
    # already exists.
    actual_handle = cls._extensions_by_number.setdefault(
        extension_handle.number, extension_handle)
    if actual_handle is not extension_handle:
      raise assertionerror(
          'extensions "%s" and "%s" both try to extend message type "%s" with '
          'field number %d.' %
          (extension_handle.full_name, actual_handle.full_name,
           cls.descriptor.full_name, extension_handle.number))

    cls._extensions_by_name[extension_handle.full_name] = extension_handle

    handle = extension_handle  # avoid line wrapping
    if _ismessagesetextension(handle):
      # messageset extension.  also register under type name.
      cls._extensions_by_name[
          extension_handle.message_type.full_name] = extension_handle

  cls.registerextension = staticmethod(registerextension)

  def fromstring(s):
    message = cls()
    message.mergefromstring(s)
    return message
  cls.fromstring = staticmethod(fromstring)


def _ispresent(item):
  """given a (fielddescriptor, value) tuple from _fields, return true if the
  value should be included in the list returned by listfields()."""

  if item[0].label == _fielddescriptor.label_repeated:
    return bool(item[1])
  elif item[0].cpp_type == _fielddescriptor.cpptype_message:
    return item[1]._is_present_in_parent
  else:
    return true


def _addlistfieldsmethod(message_descriptor, cls):
  """helper for _addmessagemethods()."""

  def listfields(self):
    all_fields = [item for item in self._fields.iteritems() if _ispresent(item)]
    all_fields.sort(key = lambda item: item[0].number)
    return all_fields

  cls.listfields = listfields


def _addhasfieldmethod(message_descriptor, cls):
  """helper for _addmessagemethods()."""

  singular_fields = {}
  for field in message_descriptor.fields:
    if field.label != _fielddescriptor.label_repeated:
      singular_fields[field.name] = field

  def hasfield(self, field_name):
    try:
      field = singular_fields[field_name]
    except keyerror:
      raise valueerror(
          'protocol message has no singular "%s" field.' % field_name)

    if field.cpp_type == _fielddescriptor.cpptype_message:
      value = self._fields.get(field)
      return value is not none and value._is_present_in_parent
    else:
      return field in self._fields
  cls.hasfield = hasfield


def _addclearfieldmethod(message_descriptor, cls):
  """helper for _addmessagemethods()."""
  def clearfield(self, field_name):
    try:
      field = message_descriptor.fields_by_name[field_name]
    except keyerror:
      raise valueerror('protocol message has no "%s" field.' % field_name)

    if field in self._fields:
      # note:  if the field is a sub-message, its listener will still point
      #   at us.  that's fine, because the worst than can happen is that it
      #   will call _modified() and invalidate our byte size.  big deal.
      del self._fields[field]

    # always call _modified() -- even if nothing was changed, this is
    # a mutating method, and thus calling it should cause the field to become
    # present in the parent message.
    self._modified()

  cls.clearfield = clearfield


def _addclearextensionmethod(cls):
  """helper for _addmessagemethods()."""
  def clearextension(self, extension_handle):
    _verifyextensionhandle(self, extension_handle)

    # similar to clearfield(), above.
    if extension_handle in self._fields:
      del self._fields[extension_handle]
    self._modified()
  cls.clearextension = clearextension


def _addclearmethod(message_descriptor, cls):
  """helper for _addmessagemethods()."""
  def clear(self):
    # clear fields.
    self._fields = {}
    self._unknown_fields = ()
    self._modified()
  cls.clear = clear


def _addhasextensionmethod(cls):
  """helper for _addmessagemethods()."""
  def hasextension(self, extension_handle):
    _verifyextensionhandle(self, extension_handle)
    if extension_handle.label == _fielddescriptor.label_repeated:
      raise keyerror('"%s" is repeated.' % extension_handle.full_name)

    if extension_handle.cpp_type == _fielddescriptor.cpptype_message:
      value = self._fields.get(extension_handle)
      return value is not none and value._is_present_in_parent
    else:
      return extension_handle in self._fields
  cls.hasextension = hasextension


def _addequalsmethod(message_descriptor, cls):
  """helper for _addmessagemethods()."""
  def __eq__(self, other):
    if (not isinstance(other, message_mod.message) or
        other.descriptor != self.descriptor):
      return false

    if self is other:
      return true

    if not self.listfields() == other.listfields():
      return false

    # sort unknown fields because their order shouldn't affect equality test.
    unknown_fields = list(self._unknown_fields)
    unknown_fields.sort()
    other_unknown_fields = list(other._unknown_fields)
    other_unknown_fields.sort()

    return unknown_fields == other_unknown_fields

  cls.__eq__ = __eq__


def _addstrmethod(message_descriptor, cls):
  """helper for _addmessagemethods()."""
  def __str__(self):
    return text_format.messagetostring(self)
  cls.__str__ = __str__


def _addunicodemethod(unused_message_descriptor, cls):
  """helper for _addmessagemethods()."""

  def __unicode__(self):
    return text_format.messagetostring(self, as_utf8=true).decode('utf-8')
  cls.__unicode__ = __unicode__


def _addsetlistenermethod(cls):
  """helper for _addmessagemethods()."""
  def setlistener(self, listener):
    if listener is none:
      self._listener = message_listener_mod.nullmessagelistener()
    else:
      self._listener = listener
  cls._setlistener = setlistener


def _bytesfornonrepeatedelement(value, field_number, field_type):
  """returns the number of bytes needed to serialize a non-repeated element.
  the returned byte count includes space for tag information and any
  other additional space associated with serializing value.

  args:
    value: value we're serializing.
    field_number: field number of this value.  (since the field number
      is stored as part of a varint-encoded tag, this has an impact
      on the total bytes required to serialize the value).
    field_type: the type of the field.  one of the type_* constants
      within fielddescriptor.
  """
  try:
    fn = type_checkers.type_to_byte_size_fn[field_type]
    return fn(field_number, value)
  except keyerror:
    raise message_mod.encodeerror('unrecognized field type: %d' % field_type)


def _addbytesizemethod(message_descriptor, cls):
  """helper for _addmessagemethods()."""

  def bytesize(self):
    if not self._cached_byte_size_dirty:
      return self._cached_byte_size

    size = 0
    for field_descriptor, field_value in self.listfields():
      size += field_descriptor._sizer(field_value)

    for tag_bytes, value_bytes in self._unknown_fields:
      size += len(tag_bytes) + len(value_bytes)

    self._cached_byte_size = size
    self._cached_byte_size_dirty = false
    self._listener_for_children.dirty = false
    return size

  cls.bytesize = bytesize


def _addserializetostringmethod(message_descriptor, cls):
  """helper for _addmessagemethods()."""

  def serializetostring(self):
    # check if the message has all of its required fields set.
    errors = []
    if not self.isinitialized():
      raise message_mod.encodeerror(
          'message %s is missing required fields: %s' % (
          self.descriptor.full_name, ','.join(self.findinitializationerrors())))
    return self.serializepartialtostring()
  cls.serializetostring = serializetostring


def _addserializepartialtostringmethod(message_descriptor, cls):
  """helper for _addmessagemethods()."""

  def serializepartialtostring(self):
    out = stringio()
    self._internalserialize(out.write)
    return out.getvalue()
  cls.serializepartialtostring = serializepartialtostring

  def internalserialize(self, write_bytes):
    for field_descriptor, field_value in self.listfields():
      field_descriptor._encoder(write_bytes, field_value)
    for tag_bytes, value_bytes in self._unknown_fields:
      write_bytes(tag_bytes)
      write_bytes(value_bytes)
  cls._internalserialize = internalserialize


def _addmergefromstringmethod(message_descriptor, cls):
  """helper for _addmessagemethods()."""
  def mergefromstring(self, serialized):
    length = len(serialized)
    try:
      if self._internalparse(serialized, 0, length) != length:
        # the only reason _internalparse would return early is if it
        # encountered an end-group tag.
        raise message_mod.decodeerror('unexpected end-group tag.')
    except indexerror:
      raise message_mod.decodeerror('truncated message.')
    except struct.error, e:
      raise message_mod.decodeerror(e)
    return length   # return this for legacy reasons.
  cls.mergefromstring = mergefromstring

  local_readtag = decoder.readtag
  local_skipfield = decoder.skipfield
  decoders_by_tag = cls._decoders_by_tag

  def internalparse(self, buffer, pos, end):
    self._modified()
    field_dict = self._fields
    unknown_field_list = self._unknown_fields
    while pos != end:
      (tag_bytes, new_pos) = local_readtag(buffer, pos)
      field_decoder = decoders_by_tag.get(tag_bytes)
      if field_decoder is none:
        value_start_pos = new_pos
        new_pos = local_skipfield(buffer, new_pos, end, tag_bytes)
        if new_pos == -1:
          return pos
        if not unknown_field_list:
          unknown_field_list = self._unknown_fields = []
        unknown_field_list.append((tag_bytes, buffer[value_start_pos:new_pos]))
        pos = new_pos
      else:
        pos = field_decoder(buffer, new_pos, end, self, field_dict)
    return pos
  cls._internalparse = internalparse


def _addisinitializedmethod(message_descriptor, cls):
  """adds the isinitialized and findinitializationerror methods to the
  protocol message class."""

  required_fields = [field for field in message_descriptor.fields
                           if field.label == _fielddescriptor.label_required]

  def isinitialized(self, errors=none):
    """checks if all required fields of a message are set.

    args:
      errors:  a list which, if provided, will be populated with the field
               paths of all missing required fields.

    returns:
      true iff the specified message has all required fields set.
    """

    # performance is critical so we avoid hasfield() and listfields().

    for field in required_fields:
      if (field not in self._fields or
          (field.cpp_type == _fielddescriptor.cpptype_message and
           not self._fields[field]._is_present_in_parent)):
        if errors is not none:
          errors.extend(self.findinitializationerrors())
        return false

    for field, value in self._fields.iteritems():
      if field.cpp_type == _fielddescriptor.cpptype_message:
        if field.label == _fielddescriptor.label_repeated:
          for element in value:
            if not element.isinitialized():
              if errors is not none:
                errors.extend(self.findinitializationerrors())
              return false
        elif value._is_present_in_parent and not value.isinitialized():
          if errors is not none:
            errors.extend(self.findinitializationerrors())
          return false

    return true

  cls.isinitialized = isinitialized

  def findinitializationerrors(self):
    """finds required fields which are not initialized.

    returns:
      a list of strings.  each string is a path to an uninitialized field from
      the top-level message, e.g. "foo.bar[5].baz".
    """

    errors = []  # simplify things

    for field in required_fields:
      if not self.hasfield(field.name):
        errors.append(field.name)

    for field, value in self.listfields():
      if field.cpp_type == _fielddescriptor.cpptype_message:
        if field.is_extension:
          name = "(%s)" % field.full_name
        else:
          name = field.name

        if field.label == _fielddescriptor.label_repeated:
          for i in xrange(len(value)):
            element = value[i]
            prefix = "%s[%d]." % (name, i)
            sub_errors = element.findinitializationerrors()
            errors += [ prefix + error for error in sub_errors ]
        else:
          prefix = name + "."
          sub_errors = value.findinitializationerrors()
          errors += [ prefix + error for error in sub_errors ]

    return errors

  cls.findinitializationerrors = findinitializationerrors


def _addmergefrommethod(cls):
  label_repeated = _fielddescriptor.label_repeated
  cpptype_message = _fielddescriptor.cpptype_message

  def mergefrom(self, msg):
    if not isinstance(msg, cls):
      raise typeerror(
          "parameter to mergefrom() must be instance of same class: "
          "expected %s got %s." % (cls.__name__, type(msg).__name__))

    assert msg is not self
    self._modified()

    fields = self._fields

    for field, value in msg._fields.iteritems():
      if field.label == label_repeated:
        field_value = fields.get(field)
        if field_value is none:
          # construct a new object to represent this field.
          field_value = field._default_constructor(self)
          fields[field] = field_value
        field_value.mergefrom(value)
      elif field.cpp_type == cpptype_message:
        if value._is_present_in_parent:
          field_value = fields.get(field)
          if field_value is none:
            # construct a new object to represent this field.
            field_value = field._default_constructor(self)
            fields[field] = field_value
          field_value.mergefrom(value)
      else:
        self._fields[field] = value

    if msg._unknown_fields:
      if not self._unknown_fields:
        self._unknown_fields = []
      self._unknown_fields.extend(msg._unknown_fields)

  cls.mergefrom = mergefrom


def _addmessagemethods(message_descriptor, cls):
  """adds implementations of all message methods to cls."""
  _addlistfieldsmethod(message_descriptor, cls)
  _addhasfieldmethod(message_descriptor, cls)
  _addclearfieldmethod(message_descriptor, cls)
  if message_descriptor.is_extendable:
    _addclearextensionmethod(cls)
    _addhasextensionmethod(cls)
  _addclearmethod(message_descriptor, cls)
  _addequalsmethod(message_descriptor, cls)
  _addstrmethod(message_descriptor, cls)
  _addunicodemethod(message_descriptor, cls)
  _addsetlistenermethod(cls)
  _addbytesizemethod(message_descriptor, cls)
  _addserializetostringmethod(message_descriptor, cls)
  _addserializepartialtostringmethod(message_descriptor, cls)
  _addmergefromstringmethod(message_descriptor, cls)
  _addisinitializedmethod(message_descriptor, cls)
  _addmergefrommethod(cls)


def _addprivatehelpermethods(cls):
  """adds implementation of private helper methods to cls."""

  def modified(self):
    """sets the _cached_byte_size_dirty bit to true,
    and propagates this to our listener iff this was a state change.
    """

    # note:  some callers check _cached_byte_size_dirty before calling
    #   _modified() as an extra optimization.  so, if this method is ever
    #   changed such that it does stuff even when _cached_byte_size_dirty is
    #   already true, the callers need to be updated.
    if not self._cached_byte_size_dirty:
      self._cached_byte_size_dirty = true
      self._listener_for_children.dirty = true
      self._is_present_in_parent = true
      self._listener.modified()

  cls._modified = modified
  cls.setinparent = modified


class _listener(object):

  """messagelistener implementation that a parent message registers with its
  child message.

  in order to support semantics like:

    foo.bar.baz.qux = 23
    assert foo.hasfield('bar')

  ...child objects must have back references to their parents.
  this helper class is at the heart of this support.
  """

  def __init__(self, parent_message):
    """args:
      parent_message: the message whose _modified() method we should call when
        we receive modified() messages.
    """
    # this listener establishes a back reference from a child (contained) object
    # to its parent (containing) object.  we make this a weak reference to avoid
    # creating cyclic garbage when the client finishes with the 'parent' object
    # in the tree.
    if isinstance(parent_message, weakref.proxytype):
      self._parent_message_weakref = parent_message
    else:
      self._parent_message_weakref = weakref.proxy(parent_message)

    # as an optimization, we also indicate directly on the listener whether
    # or not the parent message is dirty.  this way we can avoid traversing
    # up the tree in the common case.
    self.dirty = false

  def modified(self):
    if self.dirty:
      return
    try:
      # propagate the signal to our parents iff this is the first field set.
      self._parent_message_weakref._modified()
    except referenceerror:
      # we can get here if a client has kept a reference to a child object,
      # and is now setting a field on it, but the child's parent has been
      # garbage-collected.  this is not an error.
      pass


# todo(robinson): move elsewhere?  this file is getting pretty ridiculous...
# todo(robinson): unify error handling of "unknown extension" crap.
# todo(robinson): support iteritems()-style iteration over all
# extensions with the "has" bits turned on?
class _extensiondict(object):

  """dict-like container for supporting an indexable "extensions"
  field on proto instances.

  note that in all cases we expect extension handles to be
  fielddescriptors.
  """

  def __init__(self, extended_message):
    """extended_message: message instance for which we are the extensions dict.
    """

    self._extended_message = extended_message

  def __getitem__(self, extension_handle):
    """returns the current value of the given extension handle."""

    _verifyextensionhandle(self._extended_message, extension_handle)

    result = self._extended_message._fields.get(extension_handle)
    if result is not none:
      return result

    if extension_handle.label == _fielddescriptor.label_repeated:
      result = extension_handle._default_constructor(self._extended_message)
    elif extension_handle.cpp_type == _fielddescriptor.cpptype_message:
      result = extension_handle.message_type._concrete_class()
      try:
        result._setlistener(self._extended_message._listener_for_children)
      except referenceerror:
        pass
    else:
      # singular scalar -- just return the default without inserting into the
      # dict.
      return extension_handle.default_value

    # atomically check if another thread has preempted us and, if not, swap
    # in the new object we just created.  if someone has preempted us, we
    # take that object and discard ours.
    # warning:  we are relying on setdefault() being atomic.  this is true
    #   in cpython but we haven't investigated others.  this warning appears
    #   in several other locations in this file.
    result = self._extended_message._fields.setdefault(
        extension_handle, result)

    return result

  def __eq__(self, other):
    if not isinstance(other, self.__class__):
      return false

    my_fields = self._extended_message.listfields()
    other_fields = other._extended_message.listfields()

    # get rid of non-extension fields.
    my_fields    = [ field for field in my_fields    if field.is_extension ]
    other_fields = [ field for field in other_fields if field.is_extension ]

    return my_fields == other_fields

  def __ne__(self, other):
    return not self == other

  def __hash__(self):
    raise typeerror('unhashable object')

  # note that this is only meaningful for non-repeated, scalar extension
  # fields.  note also that we may have to call _modified() when we do
  # successfully set a field this way, to set any necssary "has" bits in the
  # ancestors of the extended message.
  def __setitem__(self, extension_handle, value):
    """if extension_handle specifies a non-repeated, scalar extension
    field, sets the value of that field.
    """

    _verifyextensionhandle(self._extended_message, extension_handle)

    if (extension_handle.label == _fielddescriptor.label_repeated or
        extension_handle.cpp_type == _fielddescriptor.cpptype_message):
      raise typeerror(
          'cannot assign to extension "%s" because it is a repeated or '
          'composite type.' % extension_handle.full_name)

    # it's slightly wasteful to lookup the type checker each time,
    # but we expect this to be a vanishingly uncommon case anyway.
    type_checker = type_checkers.gettypechecker(
        extension_handle.cpp_type, extension_handle.type)
    type_checker.checkvalue(value)
    self._extended_message._fields[extension_handle] = value
    self._extended_message._modified()

  def _findextensionbyname(self, name):
    """tries to find a known extension with the specified name.

    args:
      name: extension full name.

    returns:
      extension field descriptor.
    """
    return self._extended_message._extensions_by_name.get(name, none)
