from __future__ import absolute_import, division, print_function, unicode_literals

from ripple.util import prettyprint

from unittest import testcase

class test_prettyprint(testcase):
    def setup(self):
        self._results = []
        self.printer = prettyprint.streamer(printer=self.printer)

    def printer(self, *args, **kwds):
        self._results.extend(args)

    def run_test(self, expected, *args):
        for i in range(0, len(args), 2):
            self.printer.add(args[i], args[i + 1])
        self.printer.finish()
        self.assertequals(''.join(self._results), expected)

    def test_simple_printer(self):
        self.run_test(
            '{\n    "foo": "bar"\n}',
            'foo', 'bar')

    def test_multiple_lines(self):
        self.run_test(
            '{\n    "foo": "bar",\n    "baz": 5\n}',
            'foo', 'bar', 'baz', 5)

    def test_multiple_lines(self):
        self.run_test(
            """
{
    "foo": {
        "bar": 1,
        "baz": true
    },
    "bang": "bing"
}
        """.strip(), 'foo', {'bar': 1, 'baz': true}, 'bang', 'bing')

    def test_multiple_lines_with_list(self):
        self.run_test(
            """
{
    "foo": [
        "bar",
        1
    ],
    "baz": [
        23,
        42
    ]
}
        """.strip(), 'foo', ['bar', 1], 'baz', [23, 42])
