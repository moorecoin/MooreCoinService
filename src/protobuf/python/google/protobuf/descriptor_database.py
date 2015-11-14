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

"""provides a container for descriptorprotos."""

__author__ = 'matthewtoia@google.com (matt toia)'


class descriptordatabase(object):
  """a container accepting filedescriptorprotos and maps descriptorprotos."""

  def __init__(self):
    self._file_desc_protos_by_file = {}
    self._file_desc_protos_by_symbol = {}

  def add(self, file_desc_proto):
    """adds the filedescriptorproto and its types to this database.

    args:
      file_desc_proto: the filedescriptorproto to add.
    """

    self._file_desc_protos_by_file[file_desc_proto.name] = file_desc_proto
    package = file_desc_proto.package
    for message in file_desc_proto.message_type:
      self._file_desc_protos_by_symbol.update(
          (name, file_desc_proto) for name in _extractsymbols(message, package))
    for enum in file_desc_proto.enum_type:
      self._file_desc_protos_by_symbol[
          '.'.join((package, enum.name))] = file_desc_proto

  def findfilebyname(self, name):
    """finds the file descriptor proto by file name.

    typically the file name is a relative path ending to a .proto file. the
    proto with the given name will have to have been added to this database
    using the add method or else an error will be raised.

    args:
      name: the file name to find.

    returns:
      the file descriptor proto matching the name.

    raises:
      keyerror if no file by the given name was added.
    """

    return self._file_desc_protos_by_file[name]

  def findfilecontainingsymbol(self, symbol):
    """finds the file descriptor proto containing the specified symbol.

    the symbol should be a fully qualified name including the file descriptor's
    package and any containing messages. some examples:

    'some.package.name.message'
    'some.package.name.message.nestedenum'

    the file descriptor proto containing the specified symbol must be added to
    this database using the add method or else an error will be raised.

    args:
      symbol: the fully qualified symbol name.

    returns:
      the file descriptor proto containing the symbol.

    raises:
      keyerror if no file contains the specified symbol.
    """

    return self._file_desc_protos_by_symbol[symbol]


def _extractsymbols(desc_proto, package):
  """pulls out all the symbols from a descriptor proto.

  args:
    desc_proto: the proto to extract symbols from.
    package: the package containing the descriptor type.

  yields:
    the fully qualified name found in the descriptor.
  """

  message_name = '.'.join((package, desc_proto.name))
  yield message_name
  for nested_type in desc_proto.nested_type:
    for symbol in _extractsymbols(nested_type, message_name):
      yield symbol
    for enum_type in desc_proto.enum_type:
      yield '.'.join((message_name, enum_type.name))
