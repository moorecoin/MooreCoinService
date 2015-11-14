from __future__ import absolute_import, division, print_function, unicode_literals

from ripple.util.cache import namedcache

from unittest import testcase

class test_cache(testcase):
    def setup(self):
        self.cache = namedcache()

    def test_trivial(self):
        pass
