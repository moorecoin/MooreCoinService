##########################     licence     ###############################

# copyright (c) 2005-2012, michele simionato
# all rights reserved.

# redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:

#   redistributions of source code must retain the above copyright 
#   notice, this list of conditions and the following disclaimer.
#   redistributions in bytecode form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in
#   the documentation and/or other materials provided with the
#   distribution. 

# this software is provided by the copyright holders and contributors
# "as is" and any express or implied warranties, including, but not
# limited to, the implied warranties of merchantability and fitness for
# a particular purpose are disclaimed. in no event shall the copyright
# holders or contributors be liable for any direct, indirect,
# incidental, special, exemplary, or consequential damages (including,
# but not limited to, procurement of substitute goods or services; loss
# of use, data, or profits; or business interruption) however caused and
# on any theory of liability, whether in contract, strict liability, or
# tort (including negligence or otherwise) arising in any way out of the
# use of this software, even if advised of the possibility of such
# damage.

"""
decorator module, see http://pypi.python.org/pypi/decorator
for the documentation.
"""

__version__ = '3.4.0'

__all__ = ["decorator", "functionmaker", "contextmanager"]

import sys, re, inspect
if sys.version >= '3':
    from inspect import getfullargspec
    def get_init(cls):
        return cls.__init__
else:
    class getfullargspec(object):
        "a quick and dirty replacement for getfullargspec for python 2.x"
        def __init__(self, f):
            self.args, self.varargs, self.varkw, self.defaults = \
                inspect.getargspec(f)
            self.kwonlyargs = []
            self.kwonlydefaults = none
        def __iter__(self):
            yield self.args
            yield self.varargs
            yield self.varkw
            yield self.defaults
    def get_init(cls):
        return cls.__init__.im_func

def = re.compile('\s*def\s*([_\w][_\w\d]*)\s*\(')

# basic functionality
class functionmaker(object):
    """
    an object with the ability to create functions with a given signature.
    it has attributes name, doc, module, signature, defaults, dict and
    methods update and make.
    """
    def __init__(self, func=none, name=none, signature=none,
                 defaults=none, doc=none, module=none, funcdict=none):
        self.shortsignature = signature
        if func:
            # func can be a class or a callable, but not an instance method
            self.name = func.__name__
            if self.name == '<lambda>': # small hack for lambda functions
                self.name = '_lambda_' 
            self.doc = func.__doc__
            self.module = func.__module__
            if inspect.isfunction(func):
                argspec = getfullargspec(func)
                self.annotations = getattr(func, '__annotations__', {})
                for a in ('args', 'varargs', 'varkw', 'defaults', 'kwonlyargs',
                          'kwonlydefaults'):
                    setattr(self, a, getattr(argspec, a))
                for i, arg in enumerate(self.args):
                    setattr(self, 'arg%d' % i, arg)
                if sys.version < '3': # easy way
                    self.shortsignature = self.signature = \
                        inspect.formatargspec(
                        formatvalue=lambda val: "", *argspec)[1:-1]
                else: # python 3 way
                    allargs = list(self.args)
                    allshortargs = list(self.args)
                    if self.varargs:
                        allargs.append('*' + self.varargs)
                        allshortargs.append('*' + self.varargs)
                    elif self.kwonlyargs:
                        allargs.append('*') # single star syntax
                    for a in self.kwonlyargs:
                        allargs.append('%s=none' % a)
                        allshortargs.append('%s=%s' % (a, a))
                    if self.varkw:
                        allargs.append('**' + self.varkw)
                        allshortargs.append('**' + self.varkw)
                    self.signature = ', '.join(allargs)
                    self.shortsignature = ', '.join(allshortargs)
                self.dict = func.__dict__.copy()
        # func=none happens when decorating a caller
        if name:
            self.name = name
        if signature is not none:
            self.signature = signature
        if defaults:
            self.defaults = defaults
        if doc:
            self.doc = doc
        if module:
            self.module = module
        if funcdict:
            self.dict = funcdict
        # check existence required attributes
        assert hasattr(self, 'name')
        if not hasattr(self, 'signature'):
            raise typeerror('you are decorating a non function: %s' % func)

    def update(self, func, **kw):
        "update the signature of func with the data in self"
        func.__name__ = self.name
        func.__doc__ = getattr(self, 'doc', none)
        func.__dict__ = getattr(self, 'dict', {})
        func.func_defaults = getattr(self, 'defaults', ())
        func.__kwdefaults__ = getattr(self, 'kwonlydefaults', none)
        func.__annotations__ = getattr(self, 'annotations', none)
        callermodule = sys._getframe(3).f_globals.get('__name__', '?')
        func.__module__ = getattr(self, 'module', callermodule)
        func.__dict__.update(kw)

    def make(self, src_templ, evaldict=none, addsource=false, **attrs):
        "make a new function from a given template and update the signature"
        src = src_templ % vars(self) # expand name and signature
        evaldict = evaldict or {}
        mo = def.match(src)
        if mo is none:
            raise syntaxerror('not a valid function template\n%s' % src)
        name = mo.group(1) # extract the function name
        names = set([name] + [arg.strip(' *') for arg in 
                             self.shortsignature.split(',')])
        for n in names:
            if n in ('_func_', '_call_'):
                raise nameerror('%s is overridden in\n%s' % (n, src))
        if not src.endswith('\n'): # add a newline just for safety
            src += '\n' # this is needed in old versions of python
        try:
            code = compile(src, '<string>', 'single')
            # print >> sys.stderr, 'compiling %s' % src
            exec code in evaldict
        except:
            print >> sys.stderr, 'error in generated code:'
            print >> sys.stderr, src
            raise
        func = evaldict[name]
        if addsource:
            attrs['__source__'] = src
        self.update(func, **attrs)
        return func

    @classmethod
    def create(cls, obj, body, evaldict, defaults=none,
               doc=none, module=none, addsource=true, **attrs):
        """
        create a function from the strings name, signature and body.
        evaldict is the evaluation dictionary. if addsource is true an attribute
        __source__ is added to the result. the attributes attrs are added,
        if any.
        """
        if isinstance(obj, str): # "name(signature)"
            name, rest = obj.strip().split('(', 1)
            signature = rest[:-1] #strip a right parens            
            func = none
        else: # a function
            name = none
            signature = none
            func = obj
        self = cls(func, name, signature, defaults, doc, module)
        ibody = '\n'.join('    ' + line for line in body.splitlines())
        return self.make('def %(name)s(%(signature)s):\n' + ibody, 
                        evaldict, addsource, **attrs)
  
def decorator(caller, func=none):
    """
    decorator(caller) converts a caller function into a decorator;
    decorator(caller, func) decorates a function using a caller.
    """
    if func is not none: # returns a decorated function
        evaldict = func.func_globals.copy()
        evaldict['_call_'] = caller
        evaldict['_func_'] = func
        return functionmaker.create(
            func, "return _call_(_func_, %(shortsignature)s)",
            evaldict, undecorated=func, __wrapped__=func)
    else: # returns a decorator
        if inspect.isclass(caller):
            name = caller.__name__.lower()
            callerfunc = get_init(caller)
            doc = 'decorator(%s) converts functions/generators into ' \
                'factories of %s objects' % (caller.__name__, caller.__name__)
            fun = getfullargspec(callerfunc).args[1] # second arg
        elif inspect.isfunction(caller):
            name = '_lambda_' if caller.__name__ == '<lambda>' \
                else caller.__name__
            callerfunc = caller
            doc = caller.__doc__
            fun = getfullargspec(callerfunc).args[0] # first arg
        else: # assume caller is an object with a __call__ method
            name = caller.__class__.__name__.lower()
            callerfunc = caller.__call__.im_func
            doc = caller.__call__.__doc__
            fun = getfullargspec(callerfunc).args[1] # second arg
        evaldict = callerfunc.func_globals.copy()
        evaldict['_call_'] = caller
        evaldict['decorator'] = decorator
        return functionmaker.create(
            '%s(%s)' % (name, fun), 
            'return decorator(_call_, %s)' % fun,
            evaldict, undecorated=caller, __wrapped__=caller,
            doc=doc, module=caller.__module__)

######################### contextmanager ########################

def __call__(self, func):
    'context manager decorator'
    return functionmaker.create(
        func, "with _self_: return _func_(%(shortsignature)s)",
        dict(_self_=self, _func_=func), __wrapped__=func)

try: # python >= 3.2

    from contextlib import _generatorcontextmanager 
    contextmanager = type(
        'contextmanager', (_generatorcontextmanager,), dict(__call__=__call__))

except importerror: # python >= 2.5

    from contextlib import generatorcontextmanager
    def __init__(self, f, *a, **k):
        return generatorcontextmanager.__init__(self, f(*a, **k))
    contextmanager = type(
        'contextmanager', (generatorcontextmanager,), 
        dict(__call__=__call__, __init__=__init__))
    
contextmanager = decorator(contextmanager)
