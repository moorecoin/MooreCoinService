from __future__ import absolute_import, division, print_function, unicode_literals

from ripple.util import range

from unittest import testcase

class test_range(testcase):
    def round_trip(self, s, *items):
        self.assertequals(range.from_string(s), set(items))
        self.assertequals(range.to_string(items), s)

    def test_complete(self):
        self.round_trip('10,19', 10, 19)
        self.round_trip('10', 10)
        self.round_trip('10-12', 10, 11, 12)
        self.round_trip('10,19,42-45', 10, 19, 42, 43, 44, 45)

    def test_names(self):
        self.assertequals(
            range.from_string('first,last,current', first=1, last=3, current=5),
            set([1, 3, 5]))

    def test_is_range(self):
        self.asserttrue(range.is_range(''))
        self.asserttrue(range.is_range('10'))
        self.asserttrue(range.is_range('10,12'))
        self.assertfalse(range.is_range('10,12,fred'))
        self.asserttrue(range.is_range('10,12,fred', 'fred'))
