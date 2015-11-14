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

"""a simple wrapper around enum types to expose utility functions.

instances are created as properties with the same name as the enum they wrap
on proto classes.  for usage, see:
  reflection_test.py
"""

__author__ = 'rabsatt@google.com (kevin rabsatt)'


class enumtypewrapper(object):
  """a utility for finding the names of enum values."""

  descriptor = none

  def __init__(self, enum_type):
    """inits enumtypewrapper with an enumdescriptor."""
    self._enum_type = enum_type
    self.descriptor = enum_type;

  def name(self, number):
    """returns a string containing the name of an enum value."""
    if number in self._enum_type.values_by_number:
      return self._enum_type.values_by_number[number].name
    raise valueerror('enum %s has no name defined for value %d' % (
        self._enum_type.name, number))

  def value(self, name):
    """returns the value coresponding to the given enum name."""
    if name in self._enum_type.values_by_name:
      return self._enum_type.values_by_name[name].number
    raise valueerror('enum %s has no value defined for name %s' % (
        self._enum_type.name, name))

  def keys(self):
    """return a list of the string names in the enum.

    these are returned in the order they were defined in the .proto file.
    """

    return [value_descriptor.name
            for value_descriptor in self._enum_type.values]

  def values(self):
    """return a list of the integer values in the enum.

    these are returned in the order they were defined in the .proto file.
    """

    return [value_descriptor.number
            for value_descriptor in self._enum_type.values]

  def items(self):
    """return a list of the (name, value) pairs of the enum.

    these are returned in the order they were defined in the .proto file.
    """
    return [(value_descriptor.name, value_descriptor.number)
            for value_descriptor in self._enum_type.values]
