from __future__ import unicode_literals, print_function, absolute_import, division, generators, nested_scopes
import logging
import six
from six.moves import xrange
from itertools import *

logger = logging.getlogger(__name__)

# turn on/off the automatic creation of id attributes
# ... could be a kwarg pervasively but uses are rare and simple today
auto_id_field = none

class jsonpath(object):
    """
    the base class for jsonpath abstract syntax; those
    methods stubbed here are the interface to supported 
    jsonpath semantics.
    """

    def find(self, data):
        """
        all `jsonpath` types support `find()`, which returns an iterable of `datumincontext`s.
        they keep track of the path followed to the current location, so if the calling code
        has some opinion about that, it can be passed in here as a starting point.
        """
        raise notimplementederror()

    def update(self, data, val):
        "returns `data` with the specified path replaced by `val`"
        raise notimplementederror()

    def child(self, child):
        """
        equivalent to child(self, next) but with some canonicalization
        """
        if isinstance(self, this) or isinstance(self, root):
            return child
        elif isinstance(child, this):
            return self
        elif isinstance(child, root):
            return child
        else:
            return child(self, child)

    def make_datum(self, value):
        if isinstance(value, datumincontext):
            return value
        else:
            return datumincontext(value, path=root(), context=none)

class datumincontext(object):
    """
    represents a datum along a path from a context.

    essentially a zipper but with a structure represented by jsonpath, 
    and where the context is more of a parent pointer than a proper 
    representation of the context.

    for quick-and-dirty work, this proxies any non-special attributes
    to the underlying datum, but the actual datum can (and usually should)
    be retrieved via the `value` attribute.

    to place `datum` within another, use `datum.in_context(context=..., path=...)`
    which extends the path. if the datum already has a context, it places the entire
    context within that passed in, so an object can be built from the inside
    out.
    """
    @classmethod
    def wrap(cls, data):
        if isinstance(data, cls):
            return data
        else:
            return cls(data)

    def __init__(self, value, path=none, context=none):
        self.value = value
        self.path = path or this()
        self.context = none if context is none else datumincontext.wrap(context)

    def in_context(self, context, path):
        context = datumincontext.wrap(context)

        if self.context:
            return datumincontext(value=self.value, path=self.path, context=context.in_context(path=path, context=context))
        else:
            return datumincontext(value=self.value, path=path, context=context)

    @property
    def full_path(self):
        return self.path if self.context is none else self.context.full_path.child(self.path)

    @property
    def id_pseudopath(self):
        """
        looks like a path, but with ids stuck in when available
        """
        try:
            pseudopath = fields(str(self.value[auto_id_field]))
        except (typeerror, attributeerror, keyerror): # this may not be all the interesting exceptions
            pseudopath = self.path

        if self.context:
            return self.context.id_pseudopath.child(pseudopath)
        else:
            return pseudopath

    def __repr__(self):
        return '%s(value=%r, path=%r, context=%r)' % (self.__class__.__name__, self.value, self.path, self.context)

    def __eq__(self, other):
        return isinstance(other, datumincontext) and other.value == self.value and other.path == self.path and self.context == other.context

class autoidfordatum(datumincontext):
    """
    this behaves like a datumincontext, but the value is
    always the path leading up to it, not including the "id",
    and with any "id" fields along the way replacing the prior 
    segment of the path

    for example, it will make "foo.bar.id" return a datum
    that behaves like datumincontext(value="foo.bar", path="foo.bar.id").

    this is disabled by default; it can be turned on by
    settings the `auto_id_field` global to a value other
    than `none`. 
    """
    
    def __init__(self, datum, id_field=none):
        """
        invariant is that datum.path is the path from context to datum. the auto id
        will either be the id in the datum (if present) or the id of the context
        followed by the path to the datum.

        the path to this datum is always the path to the context, the path to the
        datum, and then the auto id field.
        """
        self.datum = datum
        self.id_field = id_field or auto_id_field

    @property
    def value(self):
        return str(self.datum.id_pseudopath)

    @property
    def path(self):
        return self.id_field

    @property
    def context(self):
        return self.datum

    def __repr__(self):
        return '%s(%r)' % (self.__class__.__name__, self.datum)

    def in_context(self, context, path):
        return autoidfordatum(self.datum.in_context(context=context, path=path))

    def __eq__(self, other):
        return isinstance(other, autoidfordatum) and other.datum == self.datum and self.id_field == other.id_field


class root(jsonpath):
    """
    the jsonpath referring to the "root" object. concrete syntax is '$'.
    the root is the topmost datum without any context attached.
    """

    def find(self, data):
        if not isinstance(data, datumincontext):
            return [datumincontext(data, path=root(), context=none)]
        else:
            if data.context is none:
                return [datumincontext(data.value, context=none, path=root())]
            else:
                return root().find(data.context)

    def update(self, data, val):
        return val

    def __str__(self):
        return '$'

    def __repr__(self):
        return 'root()'

    def __eq__(self, other):
        return isinstance(other, root)

class this(jsonpath):
    """
    the jsonpath referring to the current datum. concrete syntax is '@'.
    """

    def find(self, datum):
        return [datumincontext.wrap(datum)]

    def update(self, data, val):
        return val

    def __str__(self):
        return '`this`'

    def __repr__(self):
        return 'this()'

    def __eq__(self, other):
        return isinstance(other, this)

class child(jsonpath):
    """
    jsonpath that first matches the left, then the right.
    concrete syntax is <left> '.' <right>
    """
    
    def __init__(self, left, right):
        self.left = left
        self.right = right

    def find(self, datum):
        """
        extra special case: auto ids do not have children,
        so cut it off right now rather than auto id the auto id
        """
        
        return [submatch
                for subdata in self.left.find(datum)
                if not isinstance(subdata, autoidfordatum)
                for submatch in self.right.find(subdata)]

    def __eq__(self, other):
        return isinstance(other, child) and self.left == other.left and self.right == other.right

    def __str__(self):
        return '%s.%s' % (self.left, self.right)

    def __repr__(self):
        return '%s(%r, %r)' % (self.__class__.__name__, self.left, self.right)

class parent(jsonpath):
    """
    jsonpath that matches the parent node of the current match.
    will crash if no such parent exists.
    available via named operator `parent`.
    """

    def find(self, datum):
        datum = datumincontext.wrap(datum)
        return [datum.context]

    def __eq__(self, other):
        return isinstance(other, parent)

    def __str__(self):
        return '`parent`'

    def __repr__(self):
        return 'parent()'
        

class where(jsonpath):
    """
    jsonpath that first matches the left, and then
    filters for only those nodes that have
    a match on the right.

    warning: subject to change. may want to have "contains"
    or some other better word for it.
    """
    
    def __init__(self, left, right):
        self.left = left
        self.right = right

    def find(self, data):
        return [subdata for subdata in self.left.find(data) if self.right.find(data)]

    def __str__(self):
        return '%s where %s' % (self.left, self.right)

    def __eq__(self, other):
        return isinstance(other, where) and other.left == self.left and other.right == self.right

class descendants(jsonpath):
    """
    jsonpath that matches first the left expression then any descendant
    of it which matches the right expression.
    """
    
    def __init__(self, left, right):
        self.left = left
        self.right = right

    def find(self, datum):
        # <left> .. <right> ==> <left> . (<right> | *..<right> | [*]..<right>)
        #
        # with with a wonky caveat that since slice() has funky coercions
        # we cannot just delegate to that equivalence or we'll hit an 
        # infinite loop. so right here we implement the coercion-free version.

        # get all left matches into a list
        left_matches = self.left.find(datum)
        if not isinstance(left_matches, list):
            left_matches = [left_matches]

        def match_recursively(datum):
            right_matches = self.right.find(datum)

            # manually do the * or [*] to avoid coercion and recurse just the right-hand pattern
            if isinstance(datum.value, list):
                recursive_matches = [submatch
                                     for i in range(0, len(datum.value))
                                     for submatch in match_recursively(datumincontext(datum.value[i], context=datum, path=index(i)))]

            elif isinstance(datum.value, dict):
                recursive_matches = [submatch
                                     for field in datum.value.keys()
                                     for submatch in match_recursively(datumincontext(datum.value[field], context=datum, path=fields(field)))]

            else:
                recursive_matches = []

            return right_matches + list(recursive_matches)
                
        # todo: repeatable iterator instead of list?
        return [submatch
                for left_match in left_matches
                for submatch in match_recursively(left_match)]
            
    def is_singular():
        return false

    def __str__(self):
        return '%s..%s' % (self.left, self.right)

    def __eq__(self, other):
        return isinstance(other, descendants) and self.left == other.left and self.right == other.right

class union(jsonpath):
    """
    jsonpath that returns the union of the results of each match.
    this is pretty shoddily implemented for now. the nicest semantics
    in case of mismatched bits (list vs atomic) is to put
    them all in a list, but i haven't done that yet.

    warning: any appearance of this being the _concatenation_ is
    coincidence. it may even be a bug! (or laziness)
    """
    def __init__(self, left, right):
        self.left = left
        self.right = right

    def is_singular(self):
        return false

    def find(self, data):
        return self.left.find(data) + self.right.find(data)

class intersect(jsonpath):
    """
    jsonpath for bits that match *both* patterns.

    this can be accomplished a couple of ways. the most
    efficient is to actually build the intersected
    ast as in building a state machine for matching the
    intersection of regular languages. the next
    idea is to build a filtered data and match against
    that.
    """
    def __init__(self, left, right):
        self.left = left
        self.right = right

    def is_singular(self):
        return false

    def find(self, data):
        raise notimplementederror()

class fields(jsonpath):
    """
    jsonpath referring to some field of the current object.
    concrete syntax ix comma-separated field names.

    warning: if '*' is any of the field names, then they will
    all be returned.
    """
    
    def __init__(self, *fields):
        self.fields = fields

    def get_field_datum(self, datum, field):
        if field == auto_id_field:
            return autoidfordatum(datum)
        else:
            try:
                field_value = datum.value[field] # do not use `val.get(field)` since that confuses none as a value and none due to `get`
                return datumincontext(value=field_value, path=fields(field), context=datum)
            except (typeerror, keyerror, attributeerror):
                return none

    def reified_fields(self, datum):
        if '*' not in self.fields:
            return self.fields
        else:
            try:
                fields = tuple(datum.value.keys())
                return fields if auto_id_field is none else fields + (auto_id_field,)
            except attributeerror:
                return ()

    def find(self, datum):
        datum  = datumincontext.wrap(datum)
        
        return  [field_datum
                 for field_datum in [self.get_field_datum(datum, field) for field in self.reified_fields(datum)]
                 if field_datum is not none]

    def __str__(self):
        return ','.join(self.fields)

    def __repr__(self):
        return '%s(%s)' % (self.__class__.__name__, ','.join(map(repr, self.fields)))

    def __eq__(self, other):
        return isinstance(other, fields) and tuple(self.fields) == tuple(other.fields)


class index(jsonpath):
    """
    jsonpath that matches indices of the current datum, or none if not large enough.
    concrete syntax is brackets. 

    warning: if the datum is not long enough, it will not crash but will not match anything.
    note: for the concrete syntax of `[*]`, the abstract syntax is a slice() with no parameters (equiv to `[:]`
    """

    def __init__(self, index):
        self.index = index

    def find(self, datum):
        datum = datumincontext.wrap(datum)
        
        if len(datum.value) > self.index:
            return [datumincontext(datum.value[self.index], path=self, context=datum)]
        else:
            return []

    def __eq__(self, other):
        return isinstance(other, index) and self.index == other.index

    def __str__(self):
        return '[%i]' % self.index

class slice(jsonpath):
    """
    jsonpath matching a slice of an array. 

    because of a mismatch between json and xml when schema-unaware,
    this always returns an iterable; if the incoming data
    was not a list, then it returns a one element list _containing_ that
    data.

    consider these two docs, and their schema-unaware translation to json:
    
    <a><b>hello</b></a> ==> {"a": {"b": "hello"}}
    <a><b>hello</b><b>goodbye</b></a> ==> {"a": {"b": ["hello", "goodbye"]}}

    if there were a schema, it would be known that "b" should always be an
    array (unless the schema were wonky, but that is too much to fix here)
    so when querying with json if the one writing the json knows that it
    should be an array, they can write a slice operator and it will coerce
    a non-array value to an array.

    this may be a bit unfortunate because it would be nice to always have
    an iterator, but dictionaries and other objects may also be iterable,
    so this is the compromise.
    """
    def __init__(self, start=none, end=none, step=none):
        self.start = start
        self.end = end
        self.step = step
    
    def find(self, datum):
        datum = datumincontext.wrap(datum)
        
        # here's the hack. if it is a dictionary or some kind of constant,
        # put it in a single-element list
        if (isinstance(datum.value, dict) or isinstance(datum.value, six.integer_types) or isinstance(datum.value, six.string_types)):
            return self.find(datumincontext([datum.value], path=datum.path, context=datum.context))

        # some iterators do not support slicing but we can still
        # at least work for '*'
        if self.start == none and self.end == none and self.step == none:
            return [datumincontext(datum.value[i], path=index(i), context=datum) for i in xrange(0, len(datum.value))]
        else:
            return [datumincontext(datum.value[i], path=index(i), context=datum) for i in range(0, len(datum.value))[self.start:self.end:self.step]]

    def __str__(self):
        if self.start == none and self.end == none and self.step == none:
            return '[*]'
        else:
            return '[%s%s%s]' % (self.start or '', 
                                   ':%d'%self.end if self.end else '',
                                   ':%d'%self.step if self.step else '')

    def __repr__(self):
        return '%s(start=%r,end=%r,step=%r)' % (self.__class__.__name__, self.start, self.end, self.step)

    def __eq__(self, other):
        return isinstance(other, slice) and other.start == self.start and self.end == other.end and other.step == self.step
