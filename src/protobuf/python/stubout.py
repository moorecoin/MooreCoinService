#!/usr/bin/python2.4
#
# copyright 2008 google inc.
#
# licensed under the apache license, version 2.0 (the "license");
# you may not use this file except in compliance with the license.
# you may obtain a copy of the license at
#
#      http://www.apache.org/licenses/license-2.0
#
# unless required by applicable law or agreed to in writing, software
# distributed under the license is distributed on an "as is" basis,
# without warranties or conditions of any kind, either express or implied.
# see the license for the specific language governing permissions and
# limitations under the license.

# this file is used for testing.  the original is at:
#   http://code.google.com/p/pymox/

class stuboutfortesting:
  """sample usage:
     you want os.path.exists() to always return true during testing.

     stubs = stuboutfortesting()
     stubs.set(os.path, 'exists', lambda x: 1)
       ...
     stubs.unsetall()

     the above changes os.path.exists into a lambda that returns 1.  once
     the ... part of the code finishes, the unsetall() looks up the old value
     of os.path.exists and restores it.

  """
  def __init__(self):
    self.cache = []
    self.stubs = []

  def __del__(self):
    self.smartunsetall()
    self.unsetall()

  def smartset(self, obj, attr_name, new_attr):
    """replace obj.attr_name with new_attr. this method is smart and works
       at the module, class, and instance level while preserving proper
       inheritance. it will not stub out c types however unless that has been
       explicitly allowed by the type.

       this method supports the case where attr_name is a staticmethod or a
       classmethod of obj.

       notes:
      - if obj is an instance, then it is its class that will actually be
        stubbed. note that the method set() does not do that: if obj is
        an instance, it (and not its class) will be stubbed.
      - the stubbing is using the builtin getattr and setattr. so, the __get__
        and __set__ will be called when stubbing (todo: a better idea would
        probably be to manipulate obj.__dict__ instead of getattr() and
        setattr()).

       raises attributeerror if the attribute cannot be found.
    """
    if (inspect.ismodule(obj) or
        (not inspect.isclass(obj) and obj.__dict__.has_key(attr_name))):
      orig_obj = obj
      orig_attr = getattr(obj, attr_name)

    else:
      if not inspect.isclass(obj):
        mro = list(inspect.getmro(obj.__class__))
      else:
        mro = list(inspect.getmro(obj))

      mro.reverse()

      orig_attr = none

      for cls in mro:
        try:
          orig_obj = cls
          orig_attr = getattr(obj, attr_name)
        except attributeerror:
          continue

    if orig_attr is none:
      raise attributeerror("attribute not found.")

    # calling getattr() on a staticmethod transforms it to a 'normal' function.
    # we need to ensure that we put it back as a staticmethod.
    old_attribute = obj.__dict__.get(attr_name)
    if old_attribute is not none and isinstance(old_attribute, staticmethod):
      orig_attr = staticmethod(orig_attr)

    self.stubs.append((orig_obj, attr_name, orig_attr))
    setattr(orig_obj, attr_name, new_attr)

  def smartunsetall(self):
    """reverses all the smartset() calls, restoring things to their original
    definition.  its okay to call smartunsetall() repeatedly, as later calls
    have no effect if no smartset() calls have been made.

    """
    self.stubs.reverse()

    for args in self.stubs:
      setattr(*args)

    self.stubs = []

  def set(self, parent, child_name, new_child):
    """replace child_name's old definition with new_child, in the context
    of the given parent.  the parent could be a module when the child is a
    function at module scope.  or the parent could be a class when a class'
    method is being replaced.  the named child is set to new_child, while
    the prior definition is saved away for later, when unsetall() is called.

    this method supports the case where child_name is a staticmethod or a
    classmethod of parent.
    """
    old_child = getattr(parent, child_name)

    old_attribute = parent.__dict__.get(child_name)
    if old_attribute is not none and isinstance(old_attribute, staticmethod):
      old_child = staticmethod(old_child)

    self.cache.append((parent, old_child, child_name))
    setattr(parent, child_name, new_child)

  def unsetall(self):
    """reverses all the set() calls, restoring things to their original
    definition.  its okay to call unsetall() repeatedly, as later calls have
    no effect if no set() calls have been made.

    """
    # undo calls to set() in reverse order, in case set() was called on the
    # same arguments repeatedly (want the original call to be last one undone)
    self.cache.reverse()

    for (parent, old_child, child_name) in self.cache:
      setattr(parent, child_name, old_child)
    self.cache = []
