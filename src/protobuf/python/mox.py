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

"""mox, an object-mocking framework for python.

mox works in the record-replay-verify paradigm.  when you first create
a mock object, it is in record mode.  you then programmatically set
the expected behavior of the mock object (what methods are to be
called on it, with what parameters, what they should return, and in
what order).

once you have set up the expected mock behavior, you put it in replay
mode.  now the mock responds to method calls just as you told it to.
if an unexpected method (or an expected method with unexpected
parameters) is called, then an exception will be raised.

once you are done interacting with the mock, you need to verify that
all the expected interactions occured.  (maybe your code exited
prematurely without calling some cleanup method!)  the verify phase
ensures that every expected method was called; otherwise, an exception
will be raised.

suggested usage / workflow:

  # create mox factory
  my_mox = mox()

  # create a mock data access object
  mock_dao = my_mox.createmock(daoclass)

  # set up expected behavior
  mock_dao.retrievepersonwithidentifier('1').andreturn(person)
  mock_dao.deleteperson(person)

  # put mocks in replay mode
  my_mox.replayall()

  # inject mock object and run test
  controller.setdao(mock_dao)
  controller.deletepersonbyid('1')

  # verify all methods were called as expected
  my_mox.verifyall()
"""

from collections import deque
import re
import types
import unittest

import stubout

class error(assertionerror):
  """base exception for this module."""

  pass


class expectedmethodcallserror(error):
  """raised when verify() is called before all expected methods have been called
  """

  def __init__(self, expected_methods):
    """init exception.

    args:
      # expected_methods: a sequence of mockmethod objects that should have been
      #   called.
      expected_methods: [mockmethod]

    raises:
      valueerror: if expected_methods contains no methods.
    """

    if not expected_methods:
      raise valueerror("there must be at least one expected method")
    error.__init__(self)
    self._expected_methods = expected_methods

  def __str__(self):
    calls = "\n".join(["%3d.  %s" % (i, m)
                       for i, m in enumerate(self._expected_methods)])
    return "verify: expected methods never called:\n%s" % (calls,)


class unexpectedmethodcallerror(error):
  """raised when an unexpected method is called.

  this can occur if a method is called with incorrect parameters, or out of the
  specified order.
  """

  def __init__(self, unexpected_method, expected):
    """init exception.

    args:
      # unexpected_method: mockmethod that was called but was not at the head of
      #   the expected_method queue.
      # expected: mockmethod or unorderedgroup the method should have
      #   been in.
      unexpected_method: mockmethod
      expected: mockmethod or unorderedgroup
    """

    error.__init__(self)
    self._unexpected_method = unexpected_method
    self._expected = expected

  def __str__(self):
    return "unexpected method call: %s.  expecting: %s" % \
      (self._unexpected_method, self._expected)


class unknownmethodcallerror(error):
  """raised if an unknown method is requested of the mock object."""

  def __init__(self, unknown_method_name):
    """init exception.

    args:
      # unknown_method_name: method call that is not part of the mocked class's
      #   public interface.
      unknown_method_name: str
    """

    error.__init__(self)
    self._unknown_method_name = unknown_method_name

  def __str__(self):
    return "method called is not a member of the object: %s" % \
      self._unknown_method_name


class mox(object):
  """mox: a factory for creating mock objects."""

  # a list of types that should be stubbed out with mockobjects (as
  # opposed to mockanythings).
  _use_mock_object = [types.classtype, types.instancetype, types.moduletype,
                      types.objecttype, types.typetype]

  def __init__(self):
    """initialize a new mox."""

    self._mock_objects = []
    self.stubs = stubout.stuboutfortesting()

  def createmock(self, class_to_mock):
    """create a new mock object.

    args:
      # class_to_mock: the class to be mocked
      class_to_mock: class

    returns:
      mockobject that can be used as the class_to_mock would be.
    """

    new_mock = mockobject(class_to_mock)
    self._mock_objects.append(new_mock)
    return new_mock

  def createmockanything(self):
    """create a mock that will accept any method calls.

    this does not enforce an interface.
    """

    new_mock = mockanything()
    self._mock_objects.append(new_mock)
    return new_mock

  def replayall(self):
    """set all mock objects to replay mode."""

    for mock_obj in self._mock_objects:
      mock_obj._replay()


  def verifyall(self):
    """call verify on all mock objects created."""

    for mock_obj in self._mock_objects:
      mock_obj._verify()

  def resetall(self):
    """call reset on all mock objects.  this does not unset stubs."""

    for mock_obj in self._mock_objects:
      mock_obj._reset()

  def stuboutwithmock(self, obj, attr_name, use_mock_anything=false):
    """replace a method, attribute, etc. with a mock.

    this will replace a class or module with a mockobject, and everything else
    (method, function, etc) with a mockanything.  this can be overridden to
    always use a mockanything by setting use_mock_anything to true.

    args:
      obj: a python object (class, module, instance, callable).
      attr_name: str.  the name of the attribute to replace with a mock.
      use_mock_anything: bool. true if a mockanything should be used regardless
        of the type of attribute.
    """

    attr_to_replace = getattr(obj, attr_name)
    if type(attr_to_replace) in self._use_mock_object and not use_mock_anything:
      stub = self.createmock(attr_to_replace)
    else:
      stub = self.createmockanything()

    self.stubs.set(obj, attr_name, stub)

  def unsetstubs(self):
    """restore stubs to their original state."""

    self.stubs.unsetall()

def replay(*args):
  """put mocks into replay mode.

  args:
    # args is any number of mocks to put into replay mode.
  """

  for mock in args:
    mock._replay()


def verify(*args):
  """verify mocks.

  args:
    # args is any number of mocks to be verified.
  """

  for mock in args:
    mock._verify()


def reset(*args):
  """reset mocks.

  args:
    # args is any number of mocks to be reset.
  """

  for mock in args:
    mock._reset()


class mockanything:
  """a mock that can be used to mock anything.

  this is helpful for mocking classes that do not provide a public interface.
  """

  def __init__(self):
    """ """
    self._reset()

  def __getattr__(self, method_name):
    """intercept method calls on this object.

     a new mockmethod is returned that is aware of the mockanything's
     state (record or replay).  the call will be recorded or replayed
     by the mockmethod's __call__.

    args:
      # method name: the name of the method being called.
      method_name: str

    returns:
      a new mockmethod aware of mockanything's state (record or replay).
    """

    return self._createmockmethod(method_name)

  def _createmockmethod(self, method_name):
    """create a new mock method call and return it.

    args:
      # method name: the name of the method being called.
      method_name: str

    returns:
      a new mockmethod aware of mockanything's state (record or replay).
    """

    return mockmethod(method_name, self._expected_calls_queue,
                      self._replay_mode)

  def __nonzero__(self):
    """return 1 for nonzero so the mock can be used as a conditional."""

    return 1

  def __eq__(self, rhs):
    """provide custom logic to compare objects."""

    return (isinstance(rhs, mockanything) and
            self._replay_mode == rhs._replay_mode and
            self._expected_calls_queue == rhs._expected_calls_queue)

  def __ne__(self, rhs):
    """provide custom logic to compare objects."""

    return not self == rhs

  def _replay(self):
    """start replaying expected method calls."""

    self._replay_mode = true

  def _verify(self):
    """verify that all of the expected calls have been made.

    raises:
      expectedmethodcallserror: if there are still more method calls in the
        expected queue.
    """

    # if the list of expected calls is not empty, raise an exception
    if self._expected_calls_queue:
      # the last multipletimesgroup is not popped from the queue.
      if (len(self._expected_calls_queue) == 1 and
          isinstance(self._expected_calls_queue[0], multipletimesgroup) and
          self._expected_calls_queue[0].issatisfied()):
        pass
      else:
        raise expectedmethodcallserror(self._expected_calls_queue)

  def _reset(self):
    """reset the state of this mock to record mode with an empty queue."""

    # maintain a list of method calls we are expecting
    self._expected_calls_queue = deque()

    # make sure we are in setup mode, not replay mode
    self._replay_mode = false


class mockobject(mockanything, object):
  """a mock object that simulates the public/protected interface of a class."""

  def __init__(self, class_to_mock):
    """initialize a mock object.

    this determines the methods and properties of the class and stores them.

    args:
      # class_to_mock: class to be mocked
      class_to_mock: class
    """

    # this is used to hack around the mixin/inheritance of mockanything, which
    # is not a proper object (it can be anything. :-)
    mockanything.__dict__['__init__'](self)

    # get a list of all the public and special methods we should mock.
    self._known_methods = set()
    self._known_vars = set()
    self._class_to_mock = class_to_mock
    for method in dir(class_to_mock):
      if callable(getattr(class_to_mock, method)):
        self._known_methods.add(method)
      else:
        self._known_vars.add(method)

  def __getattr__(self, name):
    """intercept attribute request on this object.

    if the attribute is a public class variable, it will be returned and not
    recorded as a call.

    if the attribute is not a variable, it is handled like a method
    call. the method name is checked against the set of mockable
    methods, and a new mockmethod is returned that is aware of the
    mockobject's state (record or replay).  the call will be recorded
    or replayed by the mockmethod's __call__.

    args:
      # name: the name of the attribute being requested.
      name: str

    returns:
      either a class variable or a new mockmethod that is aware of the state
      of the mock (record or replay).

    raises:
      unknownmethodcallerror if the mockobject does not mock the requested
          method.
    """

    if name in self._known_vars:
      return getattr(self._class_to_mock, name)

    if name in self._known_methods:
      return self._createmockmethod(name)

    raise unknownmethodcallerror(name)

  def __eq__(self, rhs):
    """provide custom logic to compare objects."""

    return (isinstance(rhs, mockobject) and
            self._class_to_mock == rhs._class_to_mock and
            self._replay_mode == rhs._replay_mode and
            self._expected_calls_queue == rhs._expected_calls_queue)

  def __setitem__(self, key, value):
    """provide custom logic for mocking classes that support item assignment.

    args:
      key: key to set the value for.
      value: value to set.

    returns:
      expected return value in replay mode.  a mockmethod object for the
      __setitem__ method that has already been called if not in replay mode.

    raises:
      typeerror if the underlying class does not support item assignment.
      unexpectedmethodcallerror if the object does not expect the call to
        __setitem__.

    """
    setitem = self._class_to_mock.__dict__.get('__setitem__', none)

    # verify the class supports item assignment.
    if setitem is none:
      raise typeerror('object does not support item assignment')

    # if we are in replay mode then simply call the mock __setitem__ method.
    if self._replay_mode:
      return mockmethod('__setitem__', self._expected_calls_queue,
                        self._replay_mode)(key, value)


    # otherwise, create a mock method __setitem__.
    return self._createmockmethod('__setitem__')(key, value)

  def __getitem__(self, key):
    """provide custom logic for mocking classes that are subscriptable.

    args:
      key: key to return the value for.

    returns:
      expected return value in replay mode.  a mockmethod object for the
      __getitem__ method that has already been called if not in replay mode.

    raises:
      typeerror if the underlying class is not subscriptable.
      unexpectedmethodcallerror if the object does not expect the call to
        __setitem__.

    """
    getitem = self._class_to_mock.__dict__.get('__getitem__', none)

    # verify the class supports item assignment.
    if getitem is none:
      raise typeerror('unsubscriptable object')

    # if we are in replay mode then simply call the mock __getitem__ method.
    if self._replay_mode:
      return mockmethod('__getitem__', self._expected_calls_queue,
                        self._replay_mode)(key)


    # otherwise, create a mock method __getitem__.
    return self._createmockmethod('__getitem__')(key)

  def __call__(self, *params, **named_params):
    """provide custom logic for mocking classes that are callable."""

    # verify the class we are mocking is callable
    callable = self._class_to_mock.__dict__.get('__call__', none)
    if callable is none:
      raise typeerror('not callable')

    # because the call is happening directly on this object instead of a method,
    # the call on the mock method is made right here
    mock_method = self._createmockmethod('__call__')
    return mock_method(*params, **named_params)

  @property
  def __class__(self):
    """return the class that is being mocked."""

    return self._class_to_mock


class mockmethod(object):
  """callable mock method.

  a mockmethod should act exactly like the method it mocks, accepting parameters
  and returning a value, or throwing an exception (as specified).  when this
  method is called, it can optionally verify whether the called method (name and
  signature) matches the expected method.
  """

  def __init__(self, method_name, call_queue, replay_mode):
    """construct a new mock method.

    args:
      # method_name: the name of the method
      # call_queue: deque of calls, verify this call against the head, or add
      #     this call to the queue.
      # replay_mode: false if we are recording, true if we are verifying calls
      #     against the call queue.
      method_name: str
      call_queue: list or deque
      replay_mode: bool
    """

    self._name = method_name
    self._call_queue = call_queue
    if not isinstance(call_queue, deque):
      self._call_queue = deque(self._call_queue)
    self._replay_mode = replay_mode

    self._params = none
    self._named_params = none
    self._return_value = none
    self._exception = none
    self._side_effects = none

  def __call__(self, *params, **named_params):
    """log parameters and return the specified return value.

    if the mock(anything/object) associated with this call is in record mode,
    this mockmethod will be pushed onto the expected call queue.  if the mock
    is in replay mode, this will pop a mockmethod off the top of the queue and
    verify this call is equal to the expected call.

    raises:
      unexpectedmethodcall if this call is supposed to match an expected method
        call and it does not.
    """

    self._params = params
    self._named_params = named_params

    if not self._replay_mode:
      self._call_queue.append(self)
      return self

    expected_method = self._verifymethodcall()

    if expected_method._side_effects:
      expected_method._side_effects(*params, **named_params)

    if expected_method._exception:
      raise expected_method._exception

    return expected_method._return_value

  def __getattr__(self, name):
    """raise an attributeerror with a helpful message."""

    raise attributeerror('mockmethod has no attribute "%s". '
        'did you remember to put your mocks in replay mode?' % name)

  def _popnextmethod(self):
    """pop the next method from our call queue."""
    try:
      return self._call_queue.popleft()
    except indexerror:
      raise unexpectedmethodcallerror(self, none)

  def _verifymethodcall(self):
    """verify the called method is expected.

    this can be an ordered method, or part of an unordered set.

    returns:
      the expected mock method.

    raises:
      unexpectedmethodcall if the method called was not expected.
    """

    expected = self._popnextmethod()

    # loop here, because we might have a methodgroup followed by another
    # group.
    while isinstance(expected, methodgroup):
      expected, method = expected.methodcalled(self)
      if method is not none:
        return method

    # this is a mock method, so just check equality.
    if expected != self:
      raise unexpectedmethodcallerror(self, expected)

    return expected

  def __str__(self):
    params = ', '.join(
        [repr(p) for p in self._params or []] +
        ['%s=%r' % x for x in sorted((self._named_params or {}).items())])
    desc = "%s(%s) -> %r" % (self._name, params, self._return_value)
    return desc

  def __eq__(self, rhs):
    """test whether this mockmethod is equivalent to another mockmethod.

    args:
      # rhs: the right hand side of the test
      rhs: mockmethod
    """

    return (isinstance(rhs, mockmethod) and
            self._name == rhs._name and
            self._params == rhs._params and
            self._named_params == rhs._named_params)

  def __ne__(self, rhs):
    """test whether this mockmethod is not equivalent to another mockmethod.

    args:
      # rhs: the right hand side of the test
      rhs: mockmethod
    """

    return not self == rhs

  def getpossiblegroup(self):
    """returns a possible group from the end of the call queue or none if no
    other methods are on the stack.
    """

    # remove this method from the tail of the queue so we can add it to a group.
    this_method = self._call_queue.pop()
    assert this_method == self

    # determine if the tail of the queue is a group, or just a regular ordered
    # mock method.
    group = none
    try:
      group = self._call_queue[-1]
    except indexerror:
      pass

    return group

  def _checkandcreatenewgroup(self, group_name, group_class):
    """checks if the last method (a possible group) is an instance of our
    group_class. adds the current method to this group or creates a new one.

    args:

      group_name: the name of the group.
      group_class: the class used to create instance of this new group
    """
    group = self.getpossiblegroup()

    # if this is a group, and it is the correct group, add the method.
    if isinstance(group, group_class) and group.group_name() == group_name:
      group.addmethod(self)
      return self

    # create a new group and add the method.
    new_group = group_class(group_name)
    new_group.addmethod(self)
    self._call_queue.append(new_group)
    return self

  def inanyorder(self, group_name="default"):
    """move this method into a group of unordered calls.

    a group of unordered calls must be defined together, and must be executed
    in full before the next expected method can be called.  there can be
    multiple groups that are expected serially, if they are given
    different group names.  the same group name can be reused if there is a
    standard method call, or a group with a different name, spliced between
    usages.

    args:
      group_name: the name of the unordered group.

    returns:
      self
    """
    return self._checkandcreatenewgroup(group_name, unorderedgroup)

  def multipletimes(self, group_name="default"):
    """move this method into group of calls which may be called multiple times.

    a group of repeating calls must be defined together, and must be executed in
    full before the next expected mehtod can be called.

    args:
      group_name: the name of the unordered group.

    returns:
      self
    """
    return self._checkandcreatenewgroup(group_name, multipletimesgroup)

  def andreturn(self, return_value):
    """set the value to return when this method is called.

    args:
      # return_value can be anything.
    """

    self._return_value = return_value
    return return_value

  def andraise(self, exception):
    """set the exception to raise when this method is called.

    args:
      # exception: the exception to raise when this method is called.
      exception: exception
    """

    self._exception = exception

  def withsideeffects(self, side_effects):
    """set the side effects that are simulated when this method is called.

    args:
      side_effects: a callable which modifies the parameters or other relevant
        state which a given test case depends on.

    returns:
      self for chaining with andreturn and andraise.
    """
    self._side_effects = side_effects
    return self

class comparator:
  """base class for all mox comparators.

  a comparator can be used as a parameter to a mocked method when the exact
  value is not known.  for example, the code you are testing might build up a
  long sql string that is passed to your mock dao. you're only interested that
  the in clause contains the proper primary keys, so you can set your mock
  up as follows:

  mock_dao.runquery(strcontains('in (1, 2, 4, 5)')).andreturn(mock_result)

  now whatever query is passed in must contain the string 'in (1, 2, 4, 5)'.

  a comparator may replace one or more parameters, for example:
  # return at most 10 rows
  mock_dao.runquery(strcontains('select'), 10)

  or

  # return some non-deterministic number of rows
  mock_dao.runquery(strcontains('select'), isa(int))
  """

  def equals(self, rhs):
    """special equals method that all comparators must implement.

    args:
      rhs: any python object
    """

    raise notimplementederror, 'method must be implemented by a subclass.'

  def __eq__(self, rhs):
    return self.equals(rhs)

  def __ne__(self, rhs):
    return not self.equals(rhs)


class isa(comparator):
  """this class wraps a basic python type or class.  it is used to verify
  that a parameter is of the given type or class.

  example:
  mock_dao.connect(isa(dbconnectinfo))
  """

  def __init__(self, class_name):
    """initialize isa

    args:
      class_name: basic python type or a class
    """

    self._class_name = class_name

  def equals(self, rhs):
    """check to see if the rhs is an instance of class_name.

    args:
      # rhs: the right hand side of the test
      rhs: object

    returns:
      bool
    """

    try:
      return isinstance(rhs, self._class_name)
    except typeerror:
      # check raw types if there was a type error.  this is helpful for
      # things like cstringio.stringio.
      return type(rhs) == type(self._class_name)

  def __repr__(self):
    return str(self._class_name)

class isalmost(comparator):
  """comparison class used to check whether a parameter is nearly equal
  to a given value.  generally useful for floating point numbers.

  example mock_dao.settimeout((isalmost(3.9)))
  """

  def __init__(self, float_value, places=7):
    """initialize isalmost.

    args:
      float_value: the value for making the comparison.
      places: the number of decimal places to round to.
    """

    self._float_value = float_value
    self._places = places

  def equals(self, rhs):
    """check to see if rhs is almost equal to float_value

    args:
      rhs: the value to compare to float_value

    returns:
      bool
    """

    try:
      return round(rhs-self._float_value, self._places) == 0
    except typeerror:
      # this is probably because either float_value or rhs is not a number.
      return false

  def __repr__(self):
    return str(self._float_value)

class strcontains(comparator):
  """comparison class used to check whether a substring exists in a
  string parameter.  this can be useful in mocking a database with sql
  passed in as a string parameter, for example.

  example:
  mock_dao.runquery(strcontains('in (1, 2, 4, 5)')).andreturn(mock_result)
  """

  def __init__(self, search_string):
    """initialize.

    args:
      # search_string: the string you are searching for
      search_string: str
    """

    self._search_string = search_string

  def equals(self, rhs):
    """check to see if the search_string is contained in the rhs string.

    args:
      # rhs: the right hand side of the test
      rhs: object

    returns:
      bool
    """

    try:
      return rhs.find(self._search_string) > -1
    except exception:
      return false

  def __repr__(self):
    return '<str containing \'%s\'>' % self._search_string


class regex(comparator):
  """checks if a string matches a regular expression.

  this uses a given regular expression to determine equality.
  """

  def __init__(self, pattern, flags=0):
    """initialize.

    args:
      # pattern is the regular expression to search for
      pattern: str
      # flags passed to re.compile function as the second argument
      flags: int
    """

    self.regex = re.compile(pattern, flags=flags)

  def equals(self, rhs):
    """check to see if rhs matches regular expression pattern.

    returns:
      bool
    """

    return self.regex.search(rhs) is not none

  def __repr__(self):
    s = '<regular expression \'%s\'' % self.regex.pattern
    if self.regex.flags:
      s += ', flags=%d' % self.regex.flags
    s += '>'
    return s


class in(comparator):
  """checks whether an item (or key) is in a list (or dict) parameter.

  example:
  mock_dao.getusersinfo(in('expectedusername')).andreturn(mock_result)
  """

  def __init__(self, key):
    """initialize.

    args:
      # key is any thing that could be in a list or a key in a dict
    """

    self._key = key

  def equals(self, rhs):
    """check to see whether key is in rhs.

    args:
      rhs: dict

    returns:
      bool
    """

    return self._key in rhs

  def __repr__(self):
    return '<sequence or map containing \'%s\'>' % self._key


class containskeyvalue(comparator):
  """checks whether a key/value pair is in a dict parameter.

  example:
  mock_dao.updateusers(containskeyvalue('stevepm', stevepm_user_info))
  """

  def __init__(self, key, value):
    """initialize.

    args:
      # key: a key in a dict
      # value: the corresponding value
    """

    self._key = key
    self._value = value

  def equals(self, rhs):
    """check whether the given key/value pair is in the rhs dict.

    returns:
      bool
    """

    try:
      return rhs[self._key] == self._value
    except exception:
      return false

  def __repr__(self):
    return '<map containing the entry \'%s: %s\'>' % (self._key, self._value)


class sameelementsas(comparator):
  """checks whether iterables contain the same elements (ignoring order).

  example:
  mock_dao.processusers(sameelementsas('stevepm', 'salomaki'))
  """

  def __init__(self, expected_seq):
    """initialize.

    args:
      expected_seq: a sequence
    """

    self._expected_seq = expected_seq

  def equals(self, actual_seq):
    """check to see whether actual_seq has same elements as expected_seq.

    args:
      actual_seq: sequence

    returns:
      bool
    """

    try:
      expected = dict([(element, none) for element in self._expected_seq])
      actual = dict([(element, none) for element in actual_seq])
    except typeerror:
      # fall back to slower list-compare if any of the objects are unhashable.
      expected = list(self._expected_seq)
      actual = list(actual_seq)
      expected.sort()
      actual.sort()
    return expected == actual

  def __repr__(self):
    return '<sequence with same elements as \'%s\'>' % self._expected_seq


class and(comparator):
  """evaluates one or more comparators on rhs and returns an and of the results.
  """

  def __init__(self, *args):
    """initialize.

    args:
      *args: one or more comparator
    """

    self._comparators = args

  def equals(self, rhs):
    """checks whether all comparators are equal to rhs.

    args:
      # rhs: can be anything

    returns:
      bool
    """

    for comparator in self._comparators:
      if not comparator.equals(rhs):
        return false

    return true

  def __repr__(self):
    return '<and %s>' % str(self._comparators)


class or(comparator):
  """evaluates one or more comparators on rhs and returns an or of the results.
  """

  def __init__(self, *args):
    """initialize.

    args:
      *args: one or more mox comparators
    """

    self._comparators = args

  def equals(self, rhs):
    """checks whether any comparator is equal to rhs.

    args:
      # rhs: can be anything

    returns:
      bool
    """

    for comparator in self._comparators:
      if comparator.equals(rhs):
        return true

    return false

  def __repr__(self):
    return '<or %s>' % str(self._comparators)


class func(comparator):
  """call a function that should verify the parameter passed in is correct.

  you may need the ability to perform more advanced operations on the parameter
  in order to validate it.  you can use this to have a callable validate any
  parameter. the callable should return either true or false.


  example:

  def myparamvalidator(param):
    # advanced logic here
    return true

  mock_dao.dosomething(func(myparamvalidator), true)
  """

  def __init__(self, func):
    """initialize.

    args:
      func: callable that takes one parameter and returns a bool
    """

    self._func = func

  def equals(self, rhs):
    """test whether rhs passes the function test.

    rhs is passed into func.

    args:
      rhs: any python object

    returns:
      the result of func(rhs)
    """

    return self._func(rhs)

  def __repr__(self):
    return str(self._func)


class ignorearg(comparator):
  """ignore an argument.

  this can be used when we don't care about an argument of a method call.

  example:
  # check if castmagic is called with 3 as first arg and 'disappear' as third.
  mymock.castmagic(3, ignorearg(), 'disappear')
  """

  def equals(self, unused_rhs):
    """ignores arguments and returns true.

    args:
      unused_rhs: any python object

    returns:
      always returns true
    """

    return true

  def __repr__(self):
    return '<ignorearg>'


class methodgroup(object):
  """base class containing common behaviour for methodgroups."""

  def __init__(self, group_name):
    self._group_name = group_name

  def group_name(self):
    return self._group_name

  def __str__(self):
    return '<%s "%s">' % (self.__class__.__name__, self._group_name)

  def addmethod(self, mock_method):
    raise notimplementederror

  def methodcalled(self, mock_method):
    raise notimplementederror

  def issatisfied(self):
    raise notimplementederror

class unorderedgroup(methodgroup):
  """unorderedgroup holds a set of method calls that may occur in any order.

  this construct is helpful for non-deterministic events, such as iterating
  over the keys of a dict.
  """

  def __init__(self, group_name):
    super(unorderedgroup, self).__init__(group_name)
    self._methods = []

  def addmethod(self, mock_method):
    """add a method to this group.

    args:
      mock_method: a mock method to be added to this group.
    """

    self._methods.append(mock_method)

  def methodcalled(self, mock_method):
    """remove a method call from the group.

    if the method is not in the set, an unexpectedmethodcallerror will be
    raised.

    args:
      mock_method: a mock method that should be equal to a method in the group.

    returns:
      the mock method from the group

    raises:
      unexpectedmethodcallerror if the mock_method was not in the group.
    """

    # check to see if this method exists, and if so, remove it from the set
    # and return it.
    for method in self._methods:
      if method == mock_method:
        # remove the called mock_method instead of the method in the group.
        # the called method will match any comparators when equality is checked
        # during removal.  the method in the group could pass a comparator to
        # another comparator during the equality check.
        self._methods.remove(mock_method)

        # if this group is not empty, put it back at the head of the queue.
        if not self.issatisfied():
          mock_method._call_queue.appendleft(self)

        return self, method

    raise unexpectedmethodcallerror(mock_method, self)

  def issatisfied(self):
    """return true if there are not any methods in this group."""

    return len(self._methods) == 0


class multipletimesgroup(methodgroup):
  """multipletimesgroup holds methods that may be called any number of times.

  note: each method must be called at least once.

  this is helpful, if you don't know or care how many times a method is called.
  """

  def __init__(self, group_name):
    super(multipletimesgroup, self).__init__(group_name)
    self._methods = set()
    self._methods_called = set()

  def addmethod(self, mock_method):
    """add a method to this group.

    args:
      mock_method: a mock method to be added to this group.
    """

    self._methods.add(mock_method)

  def methodcalled(self, mock_method):
    """remove a method call from the group.

    if the method is not in the set, an unexpectedmethodcallerror will be
    raised.

    args:
      mock_method: a mock method that should be equal to a method in the group.

    returns:
      the mock method from the group

    raises:
      unexpectedmethodcallerror if the mock_method was not in the group.
    """

    # check to see if this method exists, and if so add it to the set of
    # called methods.

    for method in self._methods:
      if method == mock_method:
        self._methods_called.add(mock_method)
        # always put this group back on top of the queue, because we don't know
        # when we are done.
        mock_method._call_queue.appendleft(self)
        return self, method

    if self.issatisfied():
      next_method = mock_method._popnextmethod();
      return next_method, none
    else:
      raise unexpectedmethodcallerror(mock_method, self)

  def issatisfied(self):
    """return true if all methods in this group are called at least once."""
    # note(psycho): we can't use the simple set difference here because we want
    # to match different parameters which are considered the same e.g. isa(str)
    # and some string. this solution is o(n^2) but n should be small.
    tmp = self._methods.copy()
    for called in self._methods_called:
      for expected in tmp:
        if called == expected:
          tmp.remove(expected)
          if not tmp:
            return true
          break
    return false


class moxmetatestbase(type):
  """metaclass to add mox cleanup and verification to every test.

  as the mox unit testing class is being constructed (moxtestbase or a
  subclass), this metaclass will modify all test functions to call the
  cleanupmox method of the test class after they finish. this means that
  unstubbing and verifying will happen for every test with no additional code,
  and any failures will result in test failures as opposed to errors.
  """

  def __init__(cls, name, bases, d):
    type.__init__(cls, name, bases, d)

    # also get all the attributes from the base classes to account
    # for a case when test class is not the immediate child of moxtestbase
    for base in bases:
      for attr_name in dir(base):
        d[attr_name] = getattr(base, attr_name)

    for func_name, func in d.items():
      if func_name.startswith('test') and callable(func):
        setattr(cls, func_name, moxmetatestbase.cleanuptest(cls, func))

  @staticmethod
  def cleanuptest(cls, func):
    """adds mox cleanup code to any moxtestbase method.

    always unsets stubs after a test. will verify all mocks for tests that
    otherwise pass.

    args:
      cls: moxtestbase or subclass; the class whose test method we are altering.
      func: method; the method of the moxtestbase test class we wish to alter.

    returns:
      the modified method.
    """
    def new_method(self, *args, **kwargs):
      mox_obj = getattr(self, 'mox', none)
      cleanup_mox = false
      if mox_obj and isinstance(mox_obj, mox):
        cleanup_mox = true
      try:
        func(self, *args, **kwargs)
      finally:
        if cleanup_mox:
          mox_obj.unsetstubs()
      if cleanup_mox:
        mox_obj.verifyall()
    new_method.__name__ = func.__name__
    new_method.__doc__ = func.__doc__
    new_method.__module__ = func.__module__
    return new_method


class moxtestbase(unittest.testcase):
  """convenience test class to make stubbing easier.

  sets up a "mox" attribute which is an instance of mox - any mox tests will
  want this. also automatically unsets any stubs and verifies that all mock
  methods have been called at the end of each test, eliminating boilerplate
  code.
  """

  __metaclass__ = moxmetatestbase

  def setup(self):
    self.mox = mox()
