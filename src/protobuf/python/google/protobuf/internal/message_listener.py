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

"""defines a listener interface for observing certain
state transitions on message objects.

also defines a null implementation of this interface.
"""

__author__ = 'robinson@google.com (will robinson)'


class messagelistener(object):

  """listens for modifications made to a message.  meant to be registered via
  message._setlistener().

  attributes:
    dirty:  if true, then calling modified() would be a no-op.  this can be
            used to avoid these calls entirely in the common case.
  """

  def modified(self):
    """called every time the message is modified in such a way that the parent
    message may need to be updated.  this currently means either:
    (a) the message was modified for the first time, so the parent message
        should henceforth mark the message as present.
    (b) the message's cached byte size became dirty -- i.e. the message was
        modified for the first time after a previous call to bytesize().
        therefore the parent should also mark its byte size as dirty.
    note that (a) implies (b), since new objects start out with a client cached
    size (zero).  however, we document (a) explicitly because it is important.

    modified() will *only* be called in response to one of these two events --
    not every time the sub-message is modified.

    note that if the listener's |dirty| attribute is true, then calling
    modified at the moment would be a no-op, so it can be skipped.  performance-
    sensitive callers should check this attribute directly before calling since
    it will be true most of the time.
    """

    raise notimplementederror


class nullmessagelistener(object):

  """no-op messagelistener implementation."""

  def modified(self):
    pass
