from __future__ import absolute_import, division, print_function, unicode_literals

from ripple.util.function import function, matcher

from unittest import testcase

def fn(*args, **kwds):
    return args, kwds

class test_function(testcase):
    def match_test(self, item, *results):
        self.assertequals(matcher.match(item).groups(), results)

    def test_simple(self):
        self.match_test('function', 'function', '')
        self.match_test('f(x)', 'f', '(x)')

    def test_empty_function(self):
        self.assertequals(function()(), none)

    def test_empty_args(self):
        f = function('ripple.util.test_function.fn()')
        self.assertequals(f(), ((), {}))

    def test_function(self):
        f = function('ripple.util.test_function.fn(true, {1: 2}, none)')
        self.assertequals(f(), ((true, {1: 2}, none), {}))
        self.assertequals(f('hello', foo='bar'),
                          (('hello', true, {1: 2}, none), {'foo':'bar'}))
        self.assertequals(
            f, function('ripple.util.test_function.fn(true, {1: 2}, null)'))

    def test_quoting(self):
        f = function('ripple.util.test_function.fn(testing)')
        self.assertequals(f(), (('testing',), {}))
        f = function('ripple.util.test_function.fn(testing, true, false, null)')
        self.assertequals(f(), (('testing', true, false, none), {}))
