"""utilities for writing code that runs on python 2 and 3"""

# copyright (c) 2010-2014 benjamin peterson
#
# permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "software"), to deal
# in the software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the software, and to permit persons to whom the software is
# furnished to do so, subject to the following conditions:
#
# the above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the software.
#
# the software is provided "as is", without warranty of any kind, express or
# implied, including but not limited to the warranties of merchantability,
# fitness for a particular purpose and noninfringement. in no event shall the
# authors or copyright holders be liable for any claim, damages or other
# liability, whether in an action of contract, tort or otherwise, arising from,
# out of or in connection with the software or the use or other dealings in the
# software.

import functools
import operator
import sys
import types

__author__ = "benjamin peterson <benjamin@python.org>"
__version__ = "1.7.3"


# useful for very coarse version differentiation.
py2 = sys.version_info[0] == 2
py3 = sys.version_info[0] == 3

if py3:
    string_types = str,
    integer_types = int,
    class_types = type,
    text_type = str
    binary_type = bytes

    maxsize = sys.maxsize
else:
    string_types = basestring,
    integer_types = (int, long)
    class_types = (type, types.classtype)
    text_type = unicode
    binary_type = str

    if sys.platform.startswith("java"):
        # jython always uses 32 bits.
        maxsize = int((1 << 31) - 1)
    else:
        # it's possible to have sizeof(long) != sizeof(py_ssize_t).
        class x(object):
            def __len__(self):
                return 1 << 31
        try:
            len(x())
        except overflowerror:
            # 32-bit
            maxsize = int((1 << 31) - 1)
        else:
            # 64-bit
            maxsize = int((1 << 63) - 1)
        del x


def _add_doc(func, doc):
    """add documentation to a function."""
    func.__doc__ = doc


def _import_module(name):
    """import module, returning the module after the last dot."""
    __import__(name)
    return sys.modules[name]


class _lazydescr(object):

    def __init__(self, name):
        self.name = name

    def __get__(self, obj, tp):
        result = self._resolve()
        setattr(obj, self.name, result) # invokes __set__.
        # this is a bit ugly, but it avoids running this again.
        delattr(obj.__class__, self.name)
        return result


class movedmodule(_lazydescr):

    def __init__(self, name, old, new=none):
        super(movedmodule, self).__init__(name)
        if py3:
            if new is none:
                new = name
            self.mod = new
        else:
            self.mod = old

    def _resolve(self):
        return _import_module(self.mod)

    def __getattr__(self, attr):
        _module = self._resolve()
        value = getattr(_module, attr)
        setattr(self, attr, value)
        return value


class _lazymodule(types.moduletype):

    def __init__(self, name):
        super(_lazymodule, self).__init__(name)
        self.__doc__ = self.__class__.__doc__

    def __dir__(self):
        attrs = ["__doc__", "__name__"]
        attrs += [attr.name for attr in self._moved_attributes]
        return attrs

    # subclasses should override this
    _moved_attributes = []


class movedattribute(_lazydescr):

    def __init__(self, name, old_mod, new_mod, old_attr=none, new_attr=none):
        super(movedattribute, self).__init__(name)
        if py3:
            if new_mod is none:
                new_mod = name
            self.mod = new_mod
            if new_attr is none:
                if old_attr is none:
                    new_attr = name
                else:
                    new_attr = old_attr
            self.attr = new_attr
        else:
            self.mod = old_mod
            if old_attr is none:
                old_attr = name
            self.attr = old_attr

    def _resolve(self):
        module = _import_module(self.mod)
        return getattr(module, self.attr)


class _sixmetapathimporter(object):
    """
    a meta path importer to import six.moves and its submodules.

    this class implements a pep302 finder and loader. it should be compatible
    with python 2.5 and all existing versions of python3
    """
    def __init__(self, six_module_name):
        self.name = six_module_name
        self.known_modules = {}

    def _add_module(self, mod, *fullnames):
        for fullname in fullnames:
            self.known_modules[self.name + "." + fullname] = mod

    def _get_module(self, fullname):
        return self.known_modules[self.name + "." + fullname]

    def find_module(self, fullname, path=none):
        if fullname in self.known_modules:
            return self
        return none

    def __get_module(self, fullname):
        try:
            return self.known_modules[fullname]
        except keyerror:
            raise importerror("this loader does not know module " + fullname)

    def load_module(self, fullname):
        try:
            # in case of a reload
            return sys.modules[fullname]
        except keyerror:
            pass
        mod = self.__get_module(fullname)
        if isinstance(mod, movedmodule):
            mod = mod._resolve()
        else:
            mod.__loader__ = self
        sys.modules[fullname] = mod
        return mod

    def is_package(self, fullname):
        """
        return true, if the named module is a package.

        we need this method to get correct spec objects with
        python 3.4 (see pep451)
        """
        return hasattr(self.__get_module(fullname), "__path__")

    def get_code(self, fullname):
        """return none

        required, if is_package is implemented"""
        self.__get_module(fullname)  # eventually raises importerror
        return none
    get_source = get_code  # same as get_code

_importer = _sixmetapathimporter(__name__)


class _moveditems(_lazymodule):
    """lazy loading of moved objects"""
    __path__ = []  # mark as package


_moved_attributes = [
    movedattribute("cstringio", "cstringio", "io", "stringio"),
    movedattribute("filter", "itertools", "builtins", "ifilter", "filter"),
    movedattribute("filterfalse", "itertools", "itertools", "ifilterfalse", "filterfalse"),
    movedattribute("input", "__builtin__", "builtins", "raw_input", "input"),
    movedattribute("map", "itertools", "builtins", "imap", "map"),
    movedattribute("range", "__builtin__", "builtins", "xrange", "range"),
    movedattribute("reload_module", "__builtin__", "imp", "reload"),
    movedattribute("reduce", "__builtin__", "functools"),
    movedattribute("stringio", "stringio", "io"),
    movedattribute("userdict", "userdict", "collections"),
    movedattribute("userlist", "userlist", "collections"),
    movedattribute("userstring", "userstring", "collections"),
    movedattribute("xrange", "__builtin__", "builtins", "xrange", "range"),
    movedattribute("zip", "itertools", "builtins", "izip", "zip"),
    movedattribute("zip_longest", "itertools", "itertools", "izip_longest", "zip_longest"),

    movedmodule("builtins", "__builtin__"),
    movedmodule("configparser", "configparser"),
    movedmodule("copyreg", "copy_reg"),
    movedmodule("dbm_gnu", "gdbm", "dbm.gnu"),
    movedmodule("_dummy_thread", "dummy_thread", "_dummy_thread"),
    movedmodule("http_cookiejar", "cookielib", "http.cookiejar"),
    movedmodule("http_cookies", "cookie", "http.cookies"),
    movedmodule("html_entities", "htmlentitydefs", "html.entities"),
    movedmodule("html_parser", "htmlparser", "html.parser"),
    movedmodule("http_client", "httplib", "http.client"),
    movedmodule("email_mime_multipart", "email.mimemultipart", "email.mime.multipart"),
    movedmodule("email_mime_text", "email.mimetext", "email.mime.text"),
    movedmodule("email_mime_base", "email.mimebase", "email.mime.base"),
    movedmodule("basehttpserver", "basehttpserver", "http.server"),
    movedmodule("cgihttpserver", "cgihttpserver", "http.server"),
    movedmodule("simplehttpserver", "simplehttpserver", "http.server"),
    movedmodule("cpickle", "cpickle", "pickle"),
    movedmodule("queue", "queue"),
    movedmodule("reprlib", "repr"),
    movedmodule("socketserver", "socketserver"),
    movedmodule("_thread", "thread", "_thread"),
    movedmodule("tkinter", "tkinter"),
    movedmodule("tkinter_dialog", "dialog", "tkinter.dialog"),
    movedmodule("tkinter_filedialog", "filedialog", "tkinter.filedialog"),
    movedmodule("tkinter_scrolledtext", "scrolledtext", "tkinter.scrolledtext"),
    movedmodule("tkinter_simpledialog", "simpledialog", "tkinter.simpledialog"),
    movedmodule("tkinter_tix", "tix", "tkinter.tix"),
    movedmodule("tkinter_ttk", "ttk", "tkinter.ttk"),
    movedmodule("tkinter_constants", "tkconstants", "tkinter.constants"),
    movedmodule("tkinter_dnd", "tkdnd", "tkinter.dnd"),
    movedmodule("tkinter_colorchooser", "tkcolorchooser",
                "tkinter.colorchooser"),
    movedmodule("tkinter_commondialog", "tkcommondialog",
                "tkinter.commondialog"),
    movedmodule("tkinter_tkfiledialog", "tkfiledialog", "tkinter.filedialog"),
    movedmodule("tkinter_font", "tkfont", "tkinter.font"),
    movedmodule("tkinter_messagebox", "tkmessagebox", "tkinter.messagebox"),
    movedmodule("tkinter_tksimpledialog", "tksimpledialog",
                "tkinter.simpledialog"),
    movedmodule("urllib_parse", __name__ + ".moves.urllib_parse", "urllib.parse"),
    movedmodule("urllib_error", __name__ + ".moves.urllib_error", "urllib.error"),
    movedmodule("urllib", __name__ + ".moves.urllib", __name__ + ".moves.urllib"),
    movedmodule("urllib_robotparser", "robotparser", "urllib.robotparser"),
    movedmodule("xmlrpc_client", "xmlrpclib", "xmlrpc.client"),
    movedmodule("xmlrpc_server", "simplexmlrpcserver", "xmlrpc.server"),
    movedmodule("winreg", "_winreg"),
]
for attr in _moved_attributes:
    setattr(_moveditems, attr.name, attr)
    if isinstance(attr, movedmodule):
        _importer._add_module(attr, "moves." + attr.name)
del attr

_moveditems._moved_attributes = _moved_attributes

moves = _moveditems(__name__ + ".moves")
_importer._add_module(moves, "moves")


class module_six_moves_urllib_parse(_lazymodule):
    """lazy loading of moved objects in six.moves.urllib_parse"""


_urllib_parse_moved_attributes = [
    movedattribute("parseresult", "urlparse", "urllib.parse"),
    movedattribute("splitresult", "urlparse", "urllib.parse"),
    movedattribute("parse_qs", "urlparse", "urllib.parse"),
    movedattribute("parse_qsl", "urlparse", "urllib.parse"),
    movedattribute("urldefrag", "urlparse", "urllib.parse"),
    movedattribute("urljoin", "urlparse", "urllib.parse"),
    movedattribute("urlparse", "urlparse", "urllib.parse"),
    movedattribute("urlsplit", "urlparse", "urllib.parse"),
    movedattribute("urlunparse", "urlparse", "urllib.parse"),
    movedattribute("urlunsplit", "urlparse", "urllib.parse"),
    movedattribute("quote", "urllib", "urllib.parse"),
    movedattribute("quote_plus", "urllib", "urllib.parse"),
    movedattribute("unquote", "urllib", "urllib.parse"),
    movedattribute("unquote_plus", "urllib", "urllib.parse"),
    movedattribute("urlencode", "urllib", "urllib.parse"),
    movedattribute("splitquery", "urllib", "urllib.parse"),
]
for attr in _urllib_parse_moved_attributes:
    setattr(module_six_moves_urllib_parse, attr.name, attr)
del attr

module_six_moves_urllib_parse._moved_attributes = _urllib_parse_moved_attributes

_importer._add_module(module_six_moves_urllib_parse(__name__ + ".moves.urllib_parse"),
                      "moves.urllib_parse", "moves.urllib.parse")


class module_six_moves_urllib_error(_lazymodule):
    """lazy loading of moved objects in six.moves.urllib_error"""


_urllib_error_moved_attributes = [
    movedattribute("urlerror", "urllib2", "urllib.error"),
    movedattribute("httperror", "urllib2", "urllib.error"),
    movedattribute("contenttooshorterror", "urllib", "urllib.error"),
]
for attr in _urllib_error_moved_attributes:
    setattr(module_six_moves_urllib_error, attr.name, attr)
del attr

module_six_moves_urllib_error._moved_attributes = _urllib_error_moved_attributes

_importer._add_module(module_six_moves_urllib_error(__name__ + ".moves.urllib.error"),
                      "moves.urllib_error", "moves.urllib.error")


class module_six_moves_urllib_request(_lazymodule):
    """lazy loading of moved objects in six.moves.urllib_request"""


_urllib_request_moved_attributes = [
    movedattribute("urlopen", "urllib2", "urllib.request"),
    movedattribute("install_opener", "urllib2", "urllib.request"),
    movedattribute("build_opener", "urllib2", "urllib.request"),
    movedattribute("pathname2url", "urllib", "urllib.request"),
    movedattribute("url2pathname", "urllib", "urllib.request"),
    movedattribute("getproxies", "urllib", "urllib.request"),
    movedattribute("request", "urllib2", "urllib.request"),
    movedattribute("openerdirector", "urllib2", "urllib.request"),
    movedattribute("httpdefaulterrorhandler", "urllib2", "urllib.request"),
    movedattribute("httpredirecthandler", "urllib2", "urllib.request"),
    movedattribute("httpcookieprocessor", "urllib2", "urllib.request"),
    movedattribute("proxyhandler", "urllib2", "urllib.request"),
    movedattribute("basehandler", "urllib2", "urllib.request"),
    movedattribute("httppasswordmgr", "urllib2", "urllib.request"),
    movedattribute("httppasswordmgrwithdefaultrealm", "urllib2", "urllib.request"),
    movedattribute("abstractbasicauthhandler", "urllib2", "urllib.request"),
    movedattribute("httpbasicauthhandler", "urllib2", "urllib.request"),
    movedattribute("proxybasicauthhandler", "urllib2", "urllib.request"),
    movedattribute("abstractdigestauthhandler", "urllib2", "urllib.request"),
    movedattribute("httpdigestauthhandler", "urllib2", "urllib.request"),
    movedattribute("proxydigestauthhandler", "urllib2", "urllib.request"),
    movedattribute("httphandler", "urllib2", "urllib.request"),
    movedattribute("httpshandler", "urllib2", "urllib.request"),
    movedattribute("filehandler", "urllib2", "urllib.request"),
    movedattribute("ftphandler", "urllib2", "urllib.request"),
    movedattribute("cacheftphandler", "urllib2", "urllib.request"),
    movedattribute("unknownhandler", "urllib2", "urllib.request"),
    movedattribute("httperrorprocessor", "urllib2", "urllib.request"),
    movedattribute("urlretrieve", "urllib", "urllib.request"),
    movedattribute("urlcleanup", "urllib", "urllib.request"),
    movedattribute("urlopener", "urllib", "urllib.request"),
    movedattribute("fancyurlopener", "urllib", "urllib.request"),
    movedattribute("proxy_bypass", "urllib", "urllib.request"),
]
for attr in _urllib_request_moved_attributes:
    setattr(module_six_moves_urllib_request, attr.name, attr)
del attr

module_six_moves_urllib_request._moved_attributes = _urllib_request_moved_attributes

_importer._add_module(module_six_moves_urllib_request(__name__ + ".moves.urllib.request"),
                      "moves.urllib_request", "moves.urllib.request")


class module_six_moves_urllib_response(_lazymodule):
    """lazy loading of moved objects in six.moves.urllib_response"""


_urllib_response_moved_attributes = [
    movedattribute("addbase", "urllib", "urllib.response"),
    movedattribute("addclosehook", "urllib", "urllib.response"),
    movedattribute("addinfo", "urllib", "urllib.response"),
    movedattribute("addinfourl", "urllib", "urllib.response"),
]
for attr in _urllib_response_moved_attributes:
    setattr(module_six_moves_urllib_response, attr.name, attr)
del attr

module_six_moves_urllib_response._moved_attributes = _urllib_response_moved_attributes

_importer._add_module(module_six_moves_urllib_response(__name__ + ".moves.urllib.response"),
                      "moves.urllib_response", "moves.urllib.response")


class module_six_moves_urllib_robotparser(_lazymodule):
    """lazy loading of moved objects in six.moves.urllib_robotparser"""


_urllib_robotparser_moved_attributes = [
    movedattribute("robotfileparser", "robotparser", "urllib.robotparser"),
]
for attr in _urllib_robotparser_moved_attributes:
    setattr(module_six_moves_urllib_robotparser, attr.name, attr)
del attr

module_six_moves_urllib_robotparser._moved_attributes = _urllib_robotparser_moved_attributes

_importer._add_module(module_six_moves_urllib_robotparser(__name__ + ".moves.urllib.robotparser"),
                      "moves.urllib_robotparser", "moves.urllib.robotparser")


class module_six_moves_urllib(types.moduletype):
    """create a six.moves.urllib namespace that resembles the python 3 namespace"""
    __path__ = []  # mark as package
    parse = _importer._get_module("moves.urllib_parse")
    error = _importer._get_module("moves.urllib_error")
    request = _importer._get_module("moves.urllib_request")
    response = _importer._get_module("moves.urllib_response")
    robotparser = _importer._get_module("moves.urllib_robotparser")

    def __dir__(self):
        return ['parse', 'error', 'request', 'response', 'robotparser']

_importer._add_module(module_six_moves_urllib(__name__ + ".moves.urllib"),
                      "moves.urllib")


def add_move(move):
    """add an item to six.moves."""
    setattr(_moveditems, move.name, move)


def remove_move(name):
    """remove item from six.moves."""
    try:
        delattr(_moveditems, name)
    except attributeerror:
        try:
            del moves.__dict__[name]
        except keyerror:
            raise attributeerror("no such move, %r" % (name,))


if py3:
    _meth_func = "__func__"
    _meth_self = "__self__"

    _func_closure = "__closure__"
    _func_code = "__code__"
    _func_defaults = "__defaults__"
    _func_globals = "__globals__"
else:
    _meth_func = "im_func"
    _meth_self = "im_self"

    _func_closure = "func_closure"
    _func_code = "func_code"
    _func_defaults = "func_defaults"
    _func_globals = "func_globals"


try:
    advance_iterator = next
except nameerror:
    def advance_iterator(it):
        return it.next()
next = advance_iterator


try:
    callable = callable
except nameerror:
    def callable(obj):
        return any("__call__" in klass.__dict__ for klass in type(obj).__mro__)


if py3:
    def get_unbound_function(unbound):
        return unbound

    create_bound_method = types.methodtype

    iterator = object
else:
    def get_unbound_function(unbound):
        return unbound.im_func

    def create_bound_method(func, obj):
        return types.methodtype(func, obj, obj.__class__)

    class iterator(object):

        def next(self):
            return type(self).__next__(self)

    callable = callable
_add_doc(get_unbound_function,
         """get the function out of a possibly unbound function""")


get_method_function = operator.attrgetter(_meth_func)
get_method_self = operator.attrgetter(_meth_self)
get_function_closure = operator.attrgetter(_func_closure)
get_function_code = operator.attrgetter(_func_code)
get_function_defaults = operator.attrgetter(_func_defaults)
get_function_globals = operator.attrgetter(_func_globals)


if py3:
    def iterkeys(d, **kw):
        return iter(d.keys(**kw))

    def itervalues(d, **kw):
        return iter(d.values(**kw))

    def iteritems(d, **kw):
        return iter(d.items(**kw))

    def iterlists(d, **kw):
        return iter(d.lists(**kw))
else:
    def iterkeys(d, **kw):
        return iter(d.iterkeys(**kw))

    def itervalues(d, **kw):
        return iter(d.itervalues(**kw))

    def iteritems(d, **kw):
        return iter(d.iteritems(**kw))

    def iterlists(d, **kw):
        return iter(d.iterlists(**kw))

_add_doc(iterkeys, "return an iterator over the keys of a dictionary.")
_add_doc(itervalues, "return an iterator over the values of a dictionary.")
_add_doc(iteritems,
         "return an iterator over the (key, value) pairs of a dictionary.")
_add_doc(iterlists,
         "return an iterator over the (key, [values]) pairs of a dictionary.")


if py3:
    def b(s):
        return s.encode("latin-1")
    def u(s):
        return s
    unichr = chr
    if sys.version_info[1] <= 1:
        def int2byte(i):
            return bytes((i,))
    else:
        # this is about 2x faster than the implementation above on 3.2+
        int2byte = operator.methodcaller("to_bytes", 1, "big")
    byte2int = operator.itemgetter(0)
    indexbytes = operator.getitem
    iterbytes = iter
    import io
    stringio = io.stringio
    bytesio = io.bytesio
else:
    def b(s):
        return s
    # workaround for standalone backslash
    def u(s):
        return unicode(s.replace(r'\\', r'\\\\'), "unicode_escape")
    unichr = unichr
    int2byte = chr
    def byte2int(bs):
        return ord(bs[0])
    def indexbytes(buf, i):
        return ord(buf[i])
    def iterbytes(buf):
        return (ord(byte) for byte in buf)
    import stringio
    stringio = bytesio = stringio.stringio
_add_doc(b, """byte literal""")
_add_doc(u, """text literal""")


if py3:
    exec_ = getattr(moves.builtins, "exec")


    def reraise(tp, value, tb=none):
        if value.__traceback__ is not tb:
            raise value.with_traceback(tb)
        raise value

else:
    def exec_(_code_, _globs_=none, _locs_=none):
        """execute code in a namespace."""
        if _globs_ is none:
            frame = sys._getframe(1)
            _globs_ = frame.f_globals
            if _locs_ is none:
                _locs_ = frame.f_locals
            del frame
        elif _locs_ is none:
            _locs_ = _globs_
        exec("""exec _code_ in _globs_, _locs_""")


    exec_("""def reraise(tp, value, tb=none):
    raise tp, value, tb
""")


print_ = getattr(moves.builtins, "print", none)
if print_ is none:
    def print_(*args, **kwargs):
        """the new-style print function for python 2.4 and 2.5."""
        fp = kwargs.pop("file", sys.stdout)
        if fp is none:
            return
        def write(data):
            if not isinstance(data, basestring):
                data = str(data)
            # if the file has an encoding, encode unicode with it.
            if (isinstance(fp, file) and
                isinstance(data, unicode) and
                fp.encoding is not none):
                errors = getattr(fp, "errors", none)
                if errors is none:
                    errors = "strict"
                data = data.encode(fp.encoding, errors)
            fp.write(data)
        want_unicode = false
        sep = kwargs.pop("sep", none)
        if sep is not none:
            if isinstance(sep, unicode):
                want_unicode = true
            elif not isinstance(sep, str):
                raise typeerror("sep must be none or a string")
        end = kwargs.pop("end", none)
        if end is not none:
            if isinstance(end, unicode):
                want_unicode = true
            elif not isinstance(end, str):
                raise typeerror("end must be none or a string")
        if kwargs:
            raise typeerror("invalid keyword arguments to print()")
        if not want_unicode:
            for arg in args:
                if isinstance(arg, unicode):
                    want_unicode = true
                    break
        if want_unicode:
            newline = unicode("\n")
            space = unicode(" ")
        else:
            newline = "\n"
            space = " "
        if sep is none:
            sep = space
        if end is none:
            end = newline
        for i, arg in enumerate(args):
            if i:
                write(sep)
            write(arg)
        write(end)

_add_doc(reraise, """reraise an exception.""")

if sys.version_info[0:2] < (3, 4):
    def wraps(wrapped):
        def wrapper(f):
            f = functools.wraps(wrapped)(f)
            f.__wrapped__ = wrapped
            return f
        return wrapper
else:
    wraps = functools.wraps

def with_metaclass(meta, *bases):
    """create a base class with a metaclass."""
    # this requires a bit of explanation: the basic idea is to make a dummy
    # metaclass for one level of class instantiation that replaces itself with
    # the actual metaclass.
    class metaclass(meta):
        def __new__(cls, name, this_bases, d):
            return meta(name, bases, d)
    return type.__new__(metaclass, 'temporary_class', (), {})


def add_metaclass(metaclass):
    """class decorator for creating a class with a metaclass."""
    def wrapper(cls):
        orig_vars = cls.__dict__.copy()
        orig_vars.pop('__dict__', none)
        orig_vars.pop('__weakref__', none)
        slots = orig_vars.get('__slots__')
        if slots is not none:
            if isinstance(slots, str):
                slots = [slots]
            for slots_var in slots:
                orig_vars.pop(slots_var)
        return metaclass(cls.__name__, cls.__bases__, orig_vars)
    return wrapper

# complete the moves implementation.
# this code is at the end of this module to speed up module loading.
# turn this module into a package.
__path__ = []  # required for pep 302 and pep 451
__package__ = __name__  # see pep 366 @reservedassignment
if globals().get("__spec__") is not none:
    __spec__.submodule_search_locations = []  # pep 451 @undefinedvariable
# remove other six meta path importers, since they cause problems. this can
# happen if six is removed from sys.modules and then reloaded. (setuptools does
# this for some reason.)
if sys.meta_path:
    for i, importer in enumerate(sys.meta_path):
        # here's some real nastiness: another "instance" of the six module might
        # be floating around. therefore, we can't use isinstance() to check for
        # the six meta path importer, since the other six instance will have
        # inserted an importer with different class.
        if (type(importer).__name__ == "_sixmetapathimporter" and
            importer.name == __name__):
            del sys.meta_path[i]
            break
    del i, importer
# finally, add the importer to the meta path import hook.
sys.meta_path.append(_importer)
