from __future__ import absolute_import, division, print_function, unicode_literals

from ripple.util.search import binary_search, linear_search, first, last

from unittest import testcase

class test_search(testcase):
    def condition(self, i):
        return 10 <= i < 15;

    def test_linear_full(self):
        self.assertequals(list(linear_search(range(21), self.condition)),
                          [10, 11, 12, 13, 14])

    def test_linear_partial(self):
        self.assertequals(list(linear_search(range(8, 14), self.condition)),
                          [10, 11, 12, 13])
        self.assertequals(list(linear_search(range(11, 14), self.condition)),
                          [11, 12, 13])
        self.assertequals(list(linear_search(range(12, 18), self.condition)),
                          [12, 13, 14])

    def test_linear_empty(self):
        self.assertequals(list(linear_search(range(1, 4), self.condition)), [])

    def test_binary_first(self):
        self.assertequals(binary_search(0, 14, self.condition, first), 10)
        self.assertequals(binary_search(10, 19, self.condition, first), 10)
        self.assertequals(binary_search(14, 14, self.condition, first), 14)
        self.assertequals(binary_search(14, 15, self.condition, first), 14)
        self.assertequals(binary_search(13, 15, self.condition, first), 13)

    def test_binary_last(self):
        self.assertequals(binary_search(10, 20, self.condition, last), 14)
        self.assertequals(binary_search(0, 14, self.condition, last), 14)
        self.assertequals(binary_search(14, 14, self.condition, last), 14)
        self.assertequals(binary_search(14, 15, self.condition, last), 14)
        self.assertequals(binary_search(13, 15, self.condition, last), 14)

    def test_binary_throws(self):
        self.assertraises(
            valueerror, binary_search, 0, 20, self.condition, last)
        self.assertraises(
            valueerror, binary_search, 0, 20, self.condition, first)
