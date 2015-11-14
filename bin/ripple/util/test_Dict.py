from __future__ import absolute_import, division, print_function, unicode_literals

from ripple.util import dict

from unittest import testcase

class test_dict(testcase):
    def test_count_all_subitems(self):
        self.assertequals(dict.count_all_subitems({}), 1)
        self.assertequals(dict.count_all_subitems({'a': {}}), 2)
        self.assertequals(dict.count_all_subitems([1]), 2)
        self.assertequals(dict.count_all_subitems([1, 2]), 3)
        self.assertequals(dict.count_all_subitems([1, {2: 3}]), 4)
        self.assertequals(dict.count_all_subitems([1, {2: [3]}]), 5)
        self.assertequals(dict.count_all_subitems([1, {2: [3, 4]}]), 6)

    def test_prune(self):
        self.assertequals(dict.prune({}, 0), {})
        self.assertequals(dict.prune({}, 1), {})

        self.assertequals(dict.prune({1: 2}, 0), '{dict with 1 subitem}')
        self.assertequals(dict.prune({1: 2}, 1), {1: 2})
        self.assertequals(dict.prune({1: 2}, 2), {1: 2})

        self.assertequals(dict.prune([1, 2, 3], 0), '[list with 3 subitems]')
        self.assertequals(dict.prune([1, 2, 3], 1), [1, 2, 3])

        self.assertequals(dict.prune([{1: [2, 3]}], 0),
                          '[list with 4 subitems]')
        self.assertequals(dict.prune([{1: [2, 3]}], 1),
                          ['{dict with 3 subitems}'])
        self.assertequals(dict.prune([{1: [2, 3]}], 2),
                          [{1: u'[list with 2 subitems]'}])
        self.assertequals(dict.prune([{1: [2, 3]}], 3),
                          [{1: [2, 3]}])

    def test_prune_nosub(self):
        self.assertequals(dict.prune({}, 0, false), {})
        self.assertequals(dict.prune({}, 1, false), {})

        self.assertequals(dict.prune({1: 2}, 0, false), '{dict with 1 subitem}')
        self.assertequals(dict.prune({1: 2}, 1, false), {1: 2})
        self.assertequals(dict.prune({1: 2}, 2, false), {1: 2})

        self.assertequals(dict.prune([1, 2, 3], 0, false),
                          '[list with 3 subitems]')
        self.assertequals(dict.prune([1, 2, 3], 1, false), [1, 2, 3])

        self.assertequals(dict.prune([{1: [2, 3]}], 0, false),
                          '[list with 1 subitem]')
        self.assertequals(dict.prune([{1: [2, 3]}], 1, false),
                          ['{dict with 1 subitem}'])
        self.assertequals(dict.prune([{1: [2, 3]}], 2, false),
                          [{1: u'[list with 2 subitems]'}])
        self.assertequals(dict.prune([{1: [2, 3]}], 3, false),
                          [{1: [2, 3]}])
