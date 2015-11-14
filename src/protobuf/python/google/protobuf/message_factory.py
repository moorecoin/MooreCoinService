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

"""provides a factory class for generating dynamic messages."""

__author__ = 'matthewtoia@google.com (matt toia)'

from google.protobuf import descriptor_database
from google.protobuf import descriptor_pool
from google.protobuf import message
from google.protobuf import reflection


class messagefactory(object):
  """factory for creating proto2 messages from descriptors in a pool."""

  def __init__(self):
    """initializes a new factory."""
    self._classes = {}

  def getprototype(self, descriptor):
    """builds a proto2 message class based on the passed in descriptor.

    passing a descriptor with a fully qualified name matching a previous
    invocation will cause the same class to be returned.

    args:
      descriptor: the descriptor to build from.

    returns:
      a class describing the passed in descriptor.
    """

    if descriptor.full_name not in self._classes:
      result_class = reflection.generatedprotocolmessagetype(
          descriptor.name.encode('ascii', 'ignore'),
          (message.message,),
          {'descriptor': descriptor})
      self._classes[descriptor.full_name] = result_class
      for field in descriptor.fields:
        if field.message_type:
          self.getprototype(field.message_type)
    return self._classes[descriptor.full_name]


_db = descriptor_database.descriptordatabase()
_pool = descriptor_pool.descriptorpool(_db)
_factory = messagefactory()


def getmessages(file_protos):
  """builds a dictionary of all the messages available in a set of files.

  args:
    file_protos: a sequence of file protos to build messages out of.

  returns:
    a dictionary containing all the message types in the files mapping the
    fully qualified name to a message subclass for the descriptor.
  """

  result = {}
  for file_proto in file_protos:
    _db.add(file_proto)
  for file_proto in file_protos:
    for desc in _getalldescriptors(file_proto.message_type, file_proto.package):
      result[desc.full_name] = _factory.getprototype(desc)
  return result


def _getalldescriptors(desc_protos, package):
  """gets all levels of nested message types as a flattened list of descriptors.

  args:
    desc_protos: the descriptor protos to process.
    package: the package where the protos are defined.

  yields:
    each message descriptor for each nested type.
  """

  for desc_proto in desc_protos:
    name = '.'.join((package, desc_proto.name))
    yield _pool.findmessagetypebyname(name)
    for nested_desc in _getalldescriptors(desc_proto.nested_type, name):
      yield nested_desc
