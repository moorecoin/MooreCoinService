from __future__ import absolute_import, division, print_function, unicode_literals

"""a function that can be specified at the command line, with an argument."""

import importlib
import re
import tokenize

from stringio import stringio

matcher = re.compile(r'([\w.]+)(.*)')

remappings = {
    'false': false,
    'true': true,
    'null': none,
    'false': false,
    'true': true,
    'none': none,
}

def eval_arguments(args):
    args = args.strip()
    if not args or (args == '()'):
        return ()
    tokens = list(tokenize.generate_tokens(stringio(args).readline))
    def remap():
        for type, name, _, _, _ in tokens:
            if type == tokenize.name and name not in remappings:
                yield tokenize.string, '"%s"' % name
            else:
                yield type, name
    untok = tokenize.untokenize(remap())
    if untok[1:-1].strip():
        untok = untok[:-1] + ',)'  # force a tuple.
    try:
        return eval(untok, remappings)
    except exception as e:
        raise valueerror('couldn\'t evaluate expression "%s" (became "%s"), '
                         'error "%s"' % (args, untok, str(e)))

class function(object):
    def __init__(self, desc='', default_path=''):
        self.desc = desc.strip()
        if not self.desc:
            # make an empty function that does nothing.
            self.args = ()
            self.function = lambda *args, **kwds: none
            return

        m = matcher.match(desc)
        if not m:
            raise valueerror('"%s" is not a function' % desc)
        self.function, self.args = (g.strip() for g in m.groups())
        self.args = eval_arguments(self.args)

        if '.' not in self.function:
            if default_path and not default_path.endswith('.'):
                default_path += '.'
            self.function = default_path + self.function
        p, m = self.function.rsplit('.', 1)
        mod = importlib.import_module(p)
        # errors in modules are swallowed here.
        # except:
        #    raise valueerror('can\'t find python module "%s"' % p)

        try:
            self.function = getattr(mod, m)
        except:
            raise valueerror('no function "%s" in module "%s"' % (m, p))

    def __str__(self):
        return self.desc

    def __call__(self, *args, **kwds):
        return self.function(*(args + self.args), **kwds)

    def __eq__(self, other):
        return self.function == other.function and self.args == other.args

    def __ne__(self, other):
        return not (self == other)
