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

"""
this module is the central entity that determines which implementation of the
api is used.
"""

__author__ = 'petar@google.com (petar petrov)'

import os
# this environment variable can be used to switch to a certain implementation
# of the python api. right now only 'python' and 'cpp' are valid values. any
# other value will be ignored.
_implementation_type = os.getenv('protocol_buffers_python_implementation',
                                 'python')


if _implementation_type != 'python':
  # for now, by default use the pure-python implementation.
  # the code below checks if the c extension is available and
  # uses it if it is available.
  _implementation_type = 'cpp'
  ## determine automatically which implementation to use.
  #try:
  #  from google.protobuf.internal import cpp_message
  #  _implementation_type = 'cpp'
  #except importerror, e:
  #  _implementation_type = 'python'


# this environment variable can be used to switch between the two
# 'cpp' implementations. right now only 1 and 2 are valid values. any
# other value will be ignored.
_implementation_version_str = os.getenv(
    'protocol_buffers_python_implementation_version',
    '1')


if _implementation_version_str not in ('1', '2'):
  raise valueerror(
      "unsupported protocol_buffers_python_implementation_version: '" +
      _implementation_version_str + "' (supported versions: 1, 2)"
      )


_implementation_version = int(_implementation_version_str)



# usage of this function is discouraged. clients shouldn't care which
# implementation of the api is in use. note that there is no guarantee
# that differences between apis will be maintained.
# please don't use this function if possible.
def type():
  return _implementation_type

# see comment on 'type' above.
def version():
  return _implementation_version
