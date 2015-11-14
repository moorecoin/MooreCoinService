from __future__ import absolute_import, division, print_function, unicode_literals

from ripple.util.decimal import decimal

from unittest import testcase

class test_decimal(testcase):
    def test_construct(self):
        self.assertequals(str(decimal('')), '0')
        self.assertequals(str(decimal('0')), '0')
        self.assertequals(str(decimal('0.2')), '0.2')
        self.assertequals(str(decimal('-0.2')), '-0.2')
        self.assertequals(str(decimal('3.1416')), '3.1416')

    def test_accumulate(self):
        d = decimal()
        d.accumulate('0.5')
        d.accumulate('3.1416')
        d.accumulate('-23.34234')
        self.assertequals(str(d), '-19.70074')
