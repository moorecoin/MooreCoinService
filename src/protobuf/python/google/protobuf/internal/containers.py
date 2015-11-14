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

"""contains container classes to represent different protocol buffer types.

this file defines container classes which represent categories of protocol
buffer field types which need extra maintenance. currently these categories
are:
  - repeated scalar fields - these are all repeated fields which aren't
    composite (e.g. they are of simple types like int32, string, etc).
  - repeated composite fields - repeated fields which are composite. this
    includes groups and nested messages.
"""

__author__ = 'petar@google.com (petar petrov)'


class basecontainer(object):

  """base container class."""

  # minimizes memory usage and disallows assignment to other attributes.
  __slots__ = ['_message_listener', '_values']

  def __init__(self, message_listener):
    """
    args:
      message_listener: a messagelistener implementation.
        the repeatedscalarfieldcontainer will call this object's
        modified() method when it is modified.
    """
    self._message_listener = message_listener
    self._values = []

  def __getitem__(self, key):
    """retrieves item by the specified key."""
    return self._values[key]

  def __len__(self):
    """returns the number of elements in the container."""
    return len(self._values)

  def __ne__(self, other):
    """checks if another instance isn't equal to this one."""
    # the concrete classes should define __eq__.
    return not self == other

  def __hash__(self):
    raise typeerror('unhashable object')

  def __repr__(self):
    return repr(self._values)

  def sort(self, *args, **kwargs):
    # continue to support the old sort_function keyword argument.
    # this is expected to be a rare occurrence, so use lbyl to avoid
    # the overhead of actually catching keyerror.
    if 'sort_function' in kwargs:
      kwargs['cmp'] = kwargs.pop('sort_function')
    self._values.sort(*args, **kwargs)


class repeatedscalarfieldcontainer(basecontainer):

  """simple, type-checked, list-like container for holding repeated scalars."""

  # disallows assignment to other attributes.
  __slots__ = ['_type_checker']

  def __init__(self, message_listener, type_checker):
    """
    args:
      message_listener: a messagelistener implementation.
        the repeatedscalarfieldcontainer will call this object's
        modified() method when it is modified.
      type_checker: a type_checkers.valuechecker instance to run on elements
        inserted into this container.
    """
    super(repeatedscalarfieldcontainer, self).__init__(message_listener)
    self._type_checker = type_checker

  def append(self, value):
    """appends an item to the list. similar to list.append()."""
    self._type_checker.checkvalue(value)
    self._values.append(value)
    if not self._message_listener.dirty:
      self._message_listener.modified()

  def insert(self, key, value):
    """inserts the item at the specified position. similar to list.insert()."""
    self._type_checker.checkvalue(value)
    self._values.insert(key, value)
    if not self._message_listener.dirty:
      self._message_listener.modified()

  def extend(self, elem_seq):
    """extends by appending the given sequence. similar to list.extend()."""
    if not elem_seq:
      return

    new_values = []
    for elem in elem_seq:
      self._type_checker.checkvalue(elem)
      new_values.append(elem)
    self._values.extend(new_values)
    self._message_listener.modified()

  def mergefrom(self, other):
    """appends the contents of another repeated field of the same type to this
    one. we do not check the types of the individual fields.
    """
    self._values.extend(other._values)
    self._message_listener.modified()

  def remove(self, elem):
    """removes an item from the list. similar to list.remove()."""
    self._values.remove(elem)
    self._message_listener.modified()

  def __setitem__(self, key, value):
    """sets the item on the specified position."""
    self._type_checker.checkvalue(value)
    self._values[key] = value
    self._message_listener.modified()

  def __getslice__(self, start, stop):
    """retrieves the subset of items from between the specified indices."""
    return self._values[start:stop]

  def __setslice__(self, start, stop, values):
    """sets the subset of items from between the specified indices."""
    new_values = []
    for value in values:
      self._type_checker.checkvalue(value)
      new_values.append(value)
    self._values[start:stop] = new_values
    self._message_listener.modified()

  def __delitem__(self, key):
    """deletes the item at the specified position."""
    del self._values[key]
    self._message_listener.modified()

  def __delslice__(self, start, stop):
    """deletes the subset of items from between the specified indices."""
    del self._values[start:stop]
    self._message_listener.modified()

  def __eq__(self, other):
    """compares the current instance with another one."""
    if self is other:
      return true
    # special case for the same type which should be common and fast.
    if isinstance(other, self.__class__):
      return other._values == self._values
    # we are presumably comparing against some other sequence type.
    return other == self._values


class repeatedcompositefieldcontainer(basecontainer):

  """simple, list-like container for holding repeated composite fields."""

  # disallows assignment to other attributes.
  __slots__ = ['_message_descriptor']

  def __init__(self, message_listener, message_descriptor):
    """
    note that we pass in a descriptor instead of the generated directly,
    since at the time we construct a _repeatedcompositefieldcontainer we
    haven't yet necessarily initialized the type that will be contained in the
    container.

    args:
      message_listener: a messagelistener implementation.
        the repeatedcompositefieldcontainer will call this object's
        modified() method when it is modified.
      message_descriptor: a descriptor instance describing the protocol type
        that should be present in this container.  we'll use the
        _concrete_class field of this descriptor when the client calls add().
    """
    super(repeatedcompositefieldcontainer, self).__init__(message_listener)
    self._message_descriptor = message_descriptor

  def add(self, **kwargs):
    """adds a new element at the end of the list and returns it. keyword
    arguments may be used to initialize the element.
    """
    new_element = self._message_descriptor._concrete_class(**kwargs)
    new_element._setlistener(self._message_listener)
    self._values.append(new_element)
    if not self._message_listener.dirty:
      self._message_listener.modified()
    return new_element

  def extend(self, elem_seq):
    """extends by appending the given sequence of elements of the same type
    as this one, copying each individual message.
    """
    message_class = self._message_descriptor._concrete_class
    listener = self._message_listener
    values = self._values
    for message in elem_seq:
      new_element = message_class()
      new_element._setlistener(listener)
      new_element.mergefrom(message)
      values.append(new_element)
    listener.modified()

  def mergefrom(self, other):
    """appends the contents of another repeated field of the same type to this
    one, copying each individual message.
    """
    self.extend(other._values)

  def remove(self, elem):
    """removes an item from the list. similar to list.remove()."""
    self._values.remove(elem)
    self._message_listener.modified()

  def __getslice__(self, start, stop):
    """retrieves the subset of items from between the specified indices."""
    return self._values[start:stop]

  def __delitem__(self, key):
    """deletes the item at the specified position."""
    del self._values[key]
    self._message_listener.modified()

  def __delslice__(self, start, stop):
    """deletes the subset of items from between the specified indices."""
    del self._values[start:stop]
    self._message_listener.modified()

  def __eq__(self, other):
    """compares the current instance with another one."""
    if self is other:
      return true
    if not isinstance(other, self.__class__):
      raise typeerror('can only compare repeated composite fields against '
                      'other repeated composite fields.')
    return self._values == other._values
