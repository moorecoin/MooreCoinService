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

"""descriptors essentially contain exactly the information found in a .proto
file, in types that make this information accessible in python.
"""

__author__ = 'robinson@google.com (will robinson)'


from google.protobuf.internal import api_implementation


if api_implementation.type() == 'cpp':
  if api_implementation.version() == 2:
    from google.protobuf.internal.cpp import _message
  else:
    from google.protobuf.internal import cpp_message


class error(exception):
  """base error for this module."""


class typetransformationerror(error):
  """error transforming between python proto type and corresponding c++ type."""


class descriptorbase(object):

  """descriptors base class.

  this class is the base of all descriptor classes. it provides common options
  related functionaility.

  attributes:
    has_options:  true if the descriptor has non-default options.  usually it
        is not necessary to read this -- just call getoptions() which will
        happily return the default instance.  however, it's sometimes useful
        for efficiency, and also useful inside the protobuf implementation to
        avoid some bootstrapping issues.
  """

  def __init__(self, options, options_class_name):
    """initialize the descriptor given its options message and the name of the
    class of the options message. the name of the class is required in case
    the options message is none and has to be created.
    """
    self._options = options
    self._options_class_name = options_class_name

    # does this descriptor have non-default options?
    self.has_options = options is not none

  def _setoptions(self, options, options_class_name):
    """sets the descriptor's options

    this function is used in generated proto2 files to update descriptor
    options. it must not be used outside proto2.
    """
    self._options = options
    self._options_class_name = options_class_name

    # does this descriptor have non-default options?
    self.has_options = options is not none

  def getoptions(self):
    """retrieves descriptor options.

    this method returns the options set or creates the default options for the
    descriptor.
    """
    if self._options:
      return self._options
    from google.protobuf import descriptor_pb2
    try:
      options_class = getattr(descriptor_pb2, self._options_class_name)
    except attributeerror:
      raise runtimeerror('unknown options class name %s!' %
                         (self._options_class_name))
    self._options = options_class()
    return self._options


class _nesteddescriptorbase(descriptorbase):
  """common class for descriptors that can be nested."""

  def __init__(self, options, options_class_name, name, full_name,
               file, containing_type, serialized_start=none,
               serialized_end=none):
    """constructor.

    args:
      options: protocol message options or none
        to use default message options.
      options_class_name: (str) the class name of the above options.

      name: (str) name of this protocol message type.
      full_name: (str) fully-qualified name of this protocol message type,
        which will include protocol "package" name and the name of any
        enclosing types.
      file: (filedescriptor) reference to file info.
      containing_type: if provided, this is a nested descriptor, with this
        descriptor as parent, otherwise none.
      serialized_start: the start index (inclusive) in block in the
        file.serialized_pb that describes this descriptor.
      serialized_end: the end index (exclusive) in block in the
        file.serialized_pb that describes this descriptor.
    """
    super(_nesteddescriptorbase, self).__init__(
        options, options_class_name)

    self.name = name
    # todo(falk): add function to calculate full_name instead of having it in
    #             memory?
    self.full_name = full_name
    self.file = file
    self.containing_type = containing_type

    self._serialized_start = serialized_start
    self._serialized_end = serialized_end

  def gettoplevelcontainingtype(self):
    """returns the root if this is a nested type, or itself if its the root."""
    desc = self
    while desc.containing_type is not none:
      desc = desc.containing_type
    return desc

  def copytoproto(self, proto):
    """copies this to the matching proto in descriptor_pb2.

    args:
      proto: an empty proto instance from descriptor_pb2.

    raises:
      error: if self couldnt be serialized, due to to few constructor arguments.
    """
    if (self.file is not none and
        self._serialized_start is not none and
        self._serialized_end is not none):
      proto.parsefromstring(self.file.serialized_pb[
          self._serialized_start:self._serialized_end])
    else:
      raise error('descriptor does not contain serialization.')


class descriptor(_nesteddescriptorbase):

  """descriptor for a protocol message type.

  a descriptor instance has the following attributes:

    name: (str) name of this protocol message type.
    full_name: (str) fully-qualified name of this protocol message type,
      which will include protocol "package" name and the name of any
      enclosing types.

    containing_type: (descriptor) reference to the descriptor of the
      type containing us, or none if this is top-level.

    fields: (list of fielddescriptors) field descriptors for all
      fields in this type.
    fields_by_number: (dict int -> fielddescriptor) same fielddescriptor
      objects as in |fields|, but indexed by "number" attribute in each
      fielddescriptor.
    fields_by_name: (dict str -> fielddescriptor) same fielddescriptor
      objects as in |fields|, but indexed by "name" attribute in each
      fielddescriptor.

    nested_types: (list of descriptors) descriptor references
      for all protocol message types nested within this one.
    nested_types_by_name: (dict str -> descriptor) same descriptor
      objects as in |nested_types|, but indexed by "name" attribute
      in each descriptor.

    enum_types: (list of enumdescriptors) enumdescriptor references
      for all enums contained within this type.
    enum_types_by_name: (dict str ->enumdescriptor) same enumdescriptor
      objects as in |enum_types|, but indexed by "name" attribute
      in each enumdescriptor.
    enum_values_by_name: (dict str -> enumvaluedescriptor) dict mapping
      from enum value name to enumvaluedescriptor for that value.

    extensions: (list of fielddescriptor) all extensions defined directly
      within this message type (not within a nested type).
    extensions_by_name: (dict, string -> fielddescriptor) same fielddescriptor
      objects as |extensions|, but indexed by "name" attribute of each
      fielddescriptor.

    is_extendable:  does this type define any extension ranges?

    options: (descriptor_pb2.messageoptions) protocol message options or none
      to use default message options.

    file: (filedescriptor) reference to file descriptor.
  """

  def __init__(self, name, full_name, filename, containing_type, fields,
               nested_types, enum_types, extensions, options=none,
               is_extendable=true, extension_ranges=none, file=none,
               serialized_start=none, serialized_end=none):
    """arguments to __init__() are as described in the description
    of descriptor fields above.

    note that filename is an obsolete argument, that is not used anymore.
    please use file.name to access this as an attribute.
    """
    super(descriptor, self).__init__(
        options, 'messageoptions', name, full_name, file,
        containing_type, serialized_start=serialized_start,
        serialized_end=serialized_start)

    # we have fields in addition to fields_by_name and fields_by_number,
    # so that:
    #   1. clients can index fields by "order in which they're listed."
    #   2. clients can easily iterate over all fields with the terse
    #      syntax: for f in descriptor.fields: ...
    self.fields = fields
    for field in self.fields:
      field.containing_type = self
    self.fields_by_number = dict((f.number, f) for f in fields)
    self.fields_by_name = dict((f.name, f) for f in fields)

    self.nested_types = nested_types
    self.nested_types_by_name = dict((t.name, t) for t in nested_types)

    self.enum_types = enum_types
    for enum_type in self.enum_types:
      enum_type.containing_type = self
    self.enum_types_by_name = dict((t.name, t) for t in enum_types)
    self.enum_values_by_name = dict(
        (v.name, v) for t in enum_types for v in t.values)

    self.extensions = extensions
    for extension in self.extensions:
      extension.extension_scope = self
    self.extensions_by_name = dict((f.name, f) for f in extensions)
    self.is_extendable = is_extendable
    self.extension_ranges = extension_ranges

    self._serialized_start = serialized_start
    self._serialized_end = serialized_end

  def enumvaluename(self, enum, value):
    """returns the string name of an enum value.

    this is just a small helper method to simplify a common operation.

    args:
      enum: string name of the enum.
      value: int, value of the enum.

    returns:
      string name of the enum value.

    raises:
      keyerror if either the enum doesn't exist or the value is not a valid
        value for the enum.
    """
    return self.enum_types_by_name[enum].values_by_number[value].name

  def copytoproto(self, proto):
    """copies this to a descriptor_pb2.descriptorproto.

    args:
      proto: an empty descriptor_pb2.descriptorproto.
    """
    # this function is overriden to give a better doc comment.
    super(descriptor, self).copytoproto(proto)


# todo(robinson): we should have aggressive checking here,
# for example:
#   * if you specify a repeated field, you should not be allowed
#     to specify a default value.
#   * [other examples here as needed].
#
# todo(robinson): for this and other *descriptor classes, we
# might also want to lock things down aggressively (e.g.,
# prevent clients from setting the attributes).  having
# stronger invariants here in general will reduce the number
# of runtime checks we must do in reflection.py...
class fielddescriptor(descriptorbase):

  """descriptor for a single field in a .proto file.

  a fielddescriptor instance has the following attributes:

    name: (str) name of this field, exactly as it appears in .proto.
    full_name: (str) name of this field, including containing scope.  this is
      particularly relevant for extensions.
    index: (int) dense, 0-indexed index giving the order that this
      field textually appears within its message in the .proto file.
    number: (int) tag number declared for this field in the .proto file.

    type: (one of the type_* constants below) declared type.
    cpp_type: (one of the cpptype_* constants below) c++ type used to
      represent this field.

    label: (one of the label_* constants below) tells whether this
      field is optional, required, or repeated.
    has_default_value: (bool) true if this field has a default value defined,
      otherwise false.
    default_value: (varies) default value of this field.  only
      meaningful for non-repeated scalar fields.  repeated fields
      should always set this to [], and non-repeated composite
      fields should always set this to none.

    containing_type: (descriptor) descriptor of the protocol message
      type that contains this field.  set by the descriptor constructor
      if we're passed into one.
      somewhat confusingly, for extension fields, this is the
      descriptor of the extended message, not the descriptor
      of the message containing this field.  (see is_extension and
      extension_scope below).
    message_type: (descriptor) if a composite field, a descriptor
      of the message type contained in this field.  otherwise, this is none.
    enum_type: (enumdescriptor) if this field contains an enum, a
      descriptor of that enum.  otherwise, this is none.

    is_extension: true iff this describes an extension field.
    extension_scope: (descriptor) only meaningful if is_extension is true.
      gives the message that immediately contains this extension field.
      will be none iff we're a top-level (file-level) extension field.

    options: (descriptor_pb2.fieldoptions) protocol message field options or
      none to use default field options.
  """

  # must be consistent with c++ fielddescriptor::type enum in
  # descriptor.h.
  #
  # todo(robinson): find a way to eliminate this repetition.
  type_double         = 1
  type_float          = 2
  type_int64          = 3
  type_uint64         = 4
  type_int32          = 5
  type_fixed64        = 6
  type_fixed32        = 7
  type_bool           = 8
  type_string         = 9
  type_group          = 10
  type_message        = 11
  type_bytes          = 12
  type_uint32         = 13
  type_enum           = 14
  type_sfixed32       = 15
  type_sfixed64       = 16
  type_sint32         = 17
  type_sint64         = 18
  max_type            = 18

  # must be consistent with c++ fielddescriptor::cpptype enum in
  # descriptor.h.
  #
  # todo(robinson): find a way to eliminate this repetition.
  cpptype_int32       = 1
  cpptype_int64       = 2
  cpptype_uint32      = 3
  cpptype_uint64      = 4
  cpptype_double      = 5
  cpptype_float       = 6
  cpptype_bool        = 7
  cpptype_enum        = 8
  cpptype_string      = 9
  cpptype_message     = 10
  max_cpptype         = 10

  _python_to_cpp_proto_type_map = {
      type_double: cpptype_double,
      type_float: cpptype_float,
      type_enum: cpptype_enum,
      type_int64: cpptype_int64,
      type_sint64: cpptype_int64,
      type_sfixed64: cpptype_int64,
      type_uint64: cpptype_uint64,
      type_fixed64: cpptype_uint64,
      type_int32: cpptype_int32,
      type_sfixed32: cpptype_int32,
      type_sint32: cpptype_int32,
      type_uint32: cpptype_uint32,
      type_fixed32: cpptype_uint32,
      type_bytes: cpptype_string,
      type_string: cpptype_string,
      type_bool: cpptype_bool,
      type_message: cpptype_message,
      type_group: cpptype_message
      }

  # must be consistent with c++ fielddescriptor::label enum in
  # descriptor.h.
  #
  # todo(robinson): find a way to eliminate this repetition.
  label_optional      = 1
  label_required      = 2
  label_repeated      = 3
  max_label           = 3

  def __init__(self, name, full_name, index, number, type, cpp_type, label,
               default_value, message_type, enum_type, containing_type,
               is_extension, extension_scope, options=none,
               has_default_value=true):
    """the arguments are as described in the description of fielddescriptor
    attributes above.

    note that containing_type may be none, and may be set later if necessary
    (to deal with circular references between message types, for example).
    likewise for extension_scope.
    """
    super(fielddescriptor, self).__init__(options, 'fieldoptions')
    self.name = name
    self.full_name = full_name
    self.index = index
    self.number = number
    self.type = type
    self.cpp_type = cpp_type
    self.label = label
    self.has_default_value = has_default_value
    self.default_value = default_value
    self.containing_type = containing_type
    self.message_type = message_type
    self.enum_type = enum_type
    self.is_extension = is_extension
    self.extension_scope = extension_scope
    if api_implementation.type() == 'cpp':
      if is_extension:
        if api_implementation.version() == 2:
          self._cdescriptor = _message.getextensiondescriptor(full_name)
        else:
          self._cdescriptor = cpp_message.getextensiondescriptor(full_name)
      else:
        if api_implementation.version() == 2:
          self._cdescriptor = _message.getfielddescriptor(full_name)
        else:
          self._cdescriptor = cpp_message.getfielddescriptor(full_name)
    else:
      self._cdescriptor = none

  @staticmethod
  def prototypetocppprototype(proto_type):
    """converts from a python proto type to a c++ proto type.

    the python protocolbuffer classes specify both the 'python' datatype and the
    'c++' datatype - and they're not the same. this helper method should
    translate from one to another.

    args:
      proto_type: the python proto type (descriptor.fielddescriptor.type_*)
    returns:
      descriptor.fielddescriptor.cpptype_*, the c++ type.
    raises:
      typetransformationerror: when the python proto type isn't known.
    """
    try:
      return fielddescriptor._python_to_cpp_proto_type_map[proto_type]
    except keyerror:
      raise typetransformationerror('unknown proto_type: %s' % proto_type)


class enumdescriptor(_nesteddescriptorbase):

  """descriptor for an enum defined in a .proto file.

  an enumdescriptor instance has the following attributes:

    name: (str) name of the enum type.
    full_name: (str) full name of the type, including package name
      and any enclosing type(s).

    values: (list of enumvaluedescriptors) list of the values
      in this enum.
    values_by_name: (dict str -> enumvaluedescriptor) same as |values|,
      but indexed by the "name" field of each enumvaluedescriptor.
    values_by_number: (dict int -> enumvaluedescriptor) same as |values|,
      but indexed by the "number" field of each enumvaluedescriptor.
    containing_type: (descriptor) descriptor of the immediate containing
      type of this enum, or none if this is an enum defined at the
      top level in a .proto file.  set by descriptor's constructor
      if we're passed into one.
    file: (filedescriptor) reference to file descriptor.
    options: (descriptor_pb2.enumoptions) enum options message or
      none to use default enum options.
  """

  def __init__(self, name, full_name, filename, values,
               containing_type=none, options=none, file=none,
               serialized_start=none, serialized_end=none):
    """arguments are as described in the attribute description above.

    note that filename is an obsolete argument, that is not used anymore.
    please use file.name to access this as an attribute.
    """
    super(enumdescriptor, self).__init__(
        options, 'enumoptions', name, full_name, file,
        containing_type, serialized_start=serialized_start,
        serialized_end=serialized_start)

    self.values = values
    for value in self.values:
      value.type = self
    self.values_by_name = dict((v.name, v) for v in values)
    self.values_by_number = dict((v.number, v) for v in values)

    self._serialized_start = serialized_start
    self._serialized_end = serialized_end

  def copytoproto(self, proto):
    """copies this to a descriptor_pb2.enumdescriptorproto.

    args:
      proto: an empty descriptor_pb2.enumdescriptorproto.
    """
    # this function is overriden to give a better doc comment.
    super(enumdescriptor, self).copytoproto(proto)


class enumvaluedescriptor(descriptorbase):

  """descriptor for a single value within an enum.

    name: (str) name of this value.
    index: (int) dense, 0-indexed index giving the order that this
      value appears textually within its enum in the .proto file.
    number: (int) actual number assigned to this enum value.
    type: (enumdescriptor) enumdescriptor to which this value
      belongs.  set by enumdescriptor's constructor if we're
      passed into one.
    options: (descriptor_pb2.enumvalueoptions) enum value options message or
      none to use default enum value options options.
  """

  def __init__(self, name, index, number, type=none, options=none):
    """arguments are as described in the attribute description above."""
    super(enumvaluedescriptor, self).__init__(options, 'enumvalueoptions')
    self.name = name
    self.index = index
    self.number = number
    self.type = type


class servicedescriptor(_nesteddescriptorbase):

  """descriptor for a service.

    name: (str) name of the service.
    full_name: (str) full name of the service, including package name.
    index: (int) 0-indexed index giving the order that this services
      definition appears withing the .proto file.
    methods: (list of methoddescriptor) list of methods provided by this
      service.
    options: (descriptor_pb2.serviceoptions) service options message or
      none to use default service options.
    file: (filedescriptor) reference to file info.
  """

  def __init__(self, name, full_name, index, methods, options=none, file=none,
               serialized_start=none, serialized_end=none):
    super(servicedescriptor, self).__init__(
        options, 'serviceoptions', name, full_name, file,
        none, serialized_start=serialized_start,
        serialized_end=serialized_end)
    self.index = index
    self.methods = methods
    # set the containing service for each method in this service.
    for method in self.methods:
      method.containing_service = self

  def findmethodbyname(self, name):
    """searches for the specified method, and returns its descriptor."""
    for method in self.methods:
      if name == method.name:
        return method
    return none

  def copytoproto(self, proto):
    """copies this to a descriptor_pb2.servicedescriptorproto.

    args:
      proto: an empty descriptor_pb2.servicedescriptorproto.
    """
    # this function is overriden to give a better doc comment.
    super(servicedescriptor, self).copytoproto(proto)


class methoddescriptor(descriptorbase):

  """descriptor for a method in a service.

  name: (str) name of the method within the service.
  full_name: (str) full name of method.
  index: (int) 0-indexed index of the method inside the service.
  containing_service: (servicedescriptor) the service that contains this
    method.
  input_type: the descriptor of the message that this method accepts.
  output_type: the descriptor of the message that this method returns.
  options: (descriptor_pb2.methodoptions) method options message or
    none to use default method options.
  """

  def __init__(self, name, full_name, index, containing_service,
               input_type, output_type, options=none):
    """the arguments are as described in the description of methoddescriptor
    attributes above.

    note that containing_service may be none, and may be set later if necessary.
    """
    super(methoddescriptor, self).__init__(options, 'methodoptions')
    self.name = name
    self.full_name = full_name
    self.index = index
    self.containing_service = containing_service
    self.input_type = input_type
    self.output_type = output_type


class filedescriptor(descriptorbase):
  """descriptor for a file. mimics the descriptor_pb2.filedescriptorproto.

  name: name of file, relative to root of source tree.
  package: name of the package
  serialized_pb: (str) byte string of serialized
    descriptor_pb2.filedescriptorproto.
  """

  def __init__(self, name, package, options=none, serialized_pb=none):
    """constructor."""
    super(filedescriptor, self).__init__(options, 'fileoptions')

    self.message_types_by_name = {}
    self.name = name
    self.package = package
    self.serialized_pb = serialized_pb
    if (api_implementation.type() == 'cpp' and
        self.serialized_pb is not none):
      if api_implementation.version() == 2:
        _message.buildfile(self.serialized_pb)
      else:
        cpp_message.buildfile(self.serialized_pb)

  def copytoproto(self, proto):
    """copies this to a descriptor_pb2.filedescriptorproto.

    args:
      proto: an empty descriptor_pb2.filedescriptorproto.
    """
    proto.parsefromstring(self.serialized_pb)


def _parseoptions(message, string):
  """parses serialized options.

  this helper function is used to parse serialized options in generated
  proto2 files. it must not be used outside proto2.
  """
  message.parsefromstring(string)
  return message


def makedescriptor(desc_proto, package=''):
  """make a protobuf descriptor given a descriptorproto protobuf.

  args:
    desc_proto: the descriptor_pb2.descriptorproto protobuf message.
    package: optional package name for the new message descriptor (string).

  returns:
    a descriptor for protobuf messages.
  """
  full_message_name = [desc_proto.name]
  if package: full_message_name.insert(0, package)
  fields = []
  for field_proto in desc_proto.field:
    full_name = '.'.join(full_message_name + [field_proto.name])
    field = fielddescriptor(
        field_proto.name, full_name, field_proto.number - 1,
        field_proto.number, field_proto.type,
        fielddescriptor.prototypetocppprototype(field_proto.type),
        field_proto.label, none, none, none, none, false, none,
        has_default_value=false)
    fields.append(field)

  desc_name = '.'.join(full_message_name)
  return descriptor(desc_proto.name, desc_name, none, none, fields,
                    [], [], [])
