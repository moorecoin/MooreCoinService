//     underscore.js 1.3.1
//     (c) 2009-2012 jeremy ashkenas, documentcloud inc.
//     underscore is freely distributable under the mit license.
//     portions of underscore are inspired or borrowed from prototype,
//     oliver steele's functional, and john resig's micro-templating.
//     for all details and documentation:
//     http://documentcloud.github.com/underscore

(function() {

  // baseline setup
  // --------------

  // establish the root object, `window` in the browser, or `global` on the server.
  var root = this;

  // save the previous value of the `_` variable.
  var previousunderscore = root._;

  // establish the object that gets returned to break out of a loop iteration.
  var breaker = {};

  // save bytes in the minified (but not gzipped) version:
  var arrayproto = array.prototype, objproto = object.prototype, funcproto = function.prototype;

  // create quick reference variables for speed access to core prototypes.
  var slice            = arrayproto.slice,
      unshift          = arrayproto.unshift,
      tostring         = objproto.tostring,
      hasownproperty   = objproto.hasownproperty;

  // all **ecmascript 5** native function implementations that we hope to use
  // are declared here.
  var
    nativeforeach      = arrayproto.foreach,
    nativemap          = arrayproto.map,
    nativereduce       = arrayproto.reduce,
    nativereduceright  = arrayproto.reduceright,
    nativefilter       = arrayproto.filter,
    nativeevery        = arrayproto.every,
    nativesome         = arrayproto.some,
    nativeindexof      = arrayproto.indexof,
    nativelastindexof  = arrayproto.lastindexof,
    nativeisarray      = array.isarray,
    nativekeys         = object.keys,
    nativebind         = funcproto.bind;

  // create a safe reference to the underscore object for use below.
  var _ = function(obj) { return new wrapper(obj); };

  // export the underscore object for **node.js**, with
  // backwards-compatibility for the old `require()` api. if we're in
  // the browser, add `_` as a global object via a string identifier,
  // for closure compiler "advanced" mode.
  if (typeof exports !== 'undefined') {
    if (typeof module !== 'undefined' && module.exports) {
      exports = module.exports = _;
    }
    exports._ = _;
  } else {
    root['_'] = _;
  }

  // current version.
  _.version = '1.3.1';

  // collection functions
  // --------------------

  // the cornerstone, an `each` implementation, aka `foreach`.
  // handles objects with the built-in `foreach`, arrays, and raw objects.
  // delegates to **ecmascript 5**'s native `foreach` if available.
  var each = _.each = _.foreach = function(obj, iterator, context) {
    if (obj == null) return;
    if (nativeforeach && obj.foreach === nativeforeach) {
      obj.foreach(iterator, context);
    } else if (obj.length === +obj.length) {
      for (var i = 0, l = obj.length; i < l; i++) {
        if (i in obj && iterator.call(context, obj[i], i, obj) === breaker) return;
      }
    } else {
      for (var key in obj) {
        if (_.has(obj, key)) {
          if (iterator.call(context, obj[key], key, obj) === breaker) return;
        }
      }
    }
  };

  // return the results of applying the iterator to each element.
  // delegates to **ecmascript 5**'s native `map` if available.
  _.map = _.collect = function(obj, iterator, context) {
    var results = [];
    if (obj == null) return results;
    if (nativemap && obj.map === nativemap) return obj.map(iterator, context);
    each(obj, function(value, index, list) {
      results[results.length] = iterator.call(context, value, index, list);
    });
    if (obj.length === +obj.length) results.length = obj.length;
    return results;
  };

  // **reduce** builds up a single result from a list of values, aka `inject`,
  // or `foldl`. delegates to **ecmascript 5**'s native `reduce` if available.
  _.reduce = _.foldl = _.inject = function(obj, iterator, memo, context) {
    var initial = arguments.length > 2;
    if (obj == null) obj = [];
    if (nativereduce && obj.reduce === nativereduce) {
      if (context) iterator = _.bind(iterator, context);
      return initial ? obj.reduce(iterator, memo) : obj.reduce(iterator);
    }
    each(obj, function(value, index, list) {
      if (!initial) {
        memo = value;
        initial = true;
      } else {
        memo = iterator.call(context, memo, value, index, list);
      }
    });
    if (!initial) throw new typeerror('reduce of empty array with no initial value');
    return memo;
  };

  // the right-associative version of reduce, also known as `foldr`.
  // delegates to **ecmascript 5**'s native `reduceright` if available.
  _.reduceright = _.foldr = function(obj, iterator, memo, context) {
    var initial = arguments.length > 2;
    if (obj == null) obj = [];
    if (nativereduceright && obj.reduceright === nativereduceright) {
      if (context) iterator = _.bind(iterator, context);
      return initial ? obj.reduceright(iterator, memo) : obj.reduceright(iterator);
    }
    var reversed = _.toarray(obj).reverse();
    if (context && !initial) iterator = _.bind(iterator, context);
    return initial ? _.reduce(reversed, iterator, memo, context) : _.reduce(reversed, iterator);
  };

  // return the first value which passes a truth test. aliased as `detect`.
  _.find = _.detect = function(obj, iterator, context) {
    var result;
    any(obj, function(value, index, list) {
      if (iterator.call(context, value, index, list)) {
        result = value;
        return true;
      }
    });
    return result;
  };

  // return all the elements that pass a truth test.
  // delegates to **ecmascript 5**'s native `filter` if available.
  // aliased as `select`.
  _.filter = _.select = function(obj, iterator, context) {
    var results = [];
    if (obj == null) return results;
    if (nativefilter && obj.filter === nativefilter) return obj.filter(iterator, context);
    each(obj, function(value, index, list) {
      if (iterator.call(context, value, index, list)) results[results.length] = value;
    });
    return results;
  };

  // return all the elements for which a truth test fails.
  _.reject = function(obj, iterator, context) {
    var results = [];
    if (obj == null) return results;
    each(obj, function(value, index, list) {
      if (!iterator.call(context, value, index, list)) results[results.length] = value;
    });
    return results;
  };

  // determine whether all of the elements match a truth test.
  // delegates to **ecmascript 5**'s native `every` if available.
  // aliased as `all`.
  _.every = _.all = function(obj, iterator, context) {
    var result = true;
    if (obj == null) return result;
    if (nativeevery && obj.every === nativeevery) return obj.every(iterator, context);
    each(obj, function(value, index, list) {
      if (!(result = result && iterator.call(context, value, index, list))) return breaker;
    });
    return result;
  };

  // determine if at least one element in the object matches a truth test.
  // delegates to **ecmascript 5**'s native `some` if available.
  // aliased as `any`.
  var any = _.some = _.any = function(obj, iterator, context) {
    iterator || (iterator = _.identity);
    var result = false;
    if (obj == null) return result;
    if (nativesome && obj.some === nativesome) return obj.some(iterator, context);
    each(obj, function(value, index, list) {
      if (result || (result = iterator.call(context, value, index, list))) return breaker;
    });
    return !!result;
  };

  // determine if a given value is included in the array or object using `===`.
  // aliased as `contains`.
  _.include = _.contains = function(obj, target) {
    var found = false;
    if (obj == null) return found;
    if (nativeindexof && obj.indexof === nativeindexof) return obj.indexof(target) != -1;
    found = any(obj, function(value) {
      return value === target;
    });
    return found;
  };

  // invoke a method (with arguments) on every item in a collection.
  _.invoke = function(obj, method) {
    var args = slice.call(arguments, 2);
    return _.map(obj, function(value) {
      return (_.isfunction(method) ? method || value : value[method]).apply(value, args);
    });
  };

  // convenience version of a common use case of `map`: fetching a property.
  _.pluck = function(obj, key) {
    return _.map(obj, function(value){ return value[key]; });
  };

  // return the maximum element or (element-based computation).
  _.max = function(obj, iterator, context) {
    if (!iterator && _.isarray(obj)) return math.max.apply(math, obj);
    if (!iterator && _.isempty(obj)) return -infinity;
    var result = {computed : -infinity};
    each(obj, function(value, index, list) {
      var computed = iterator ? iterator.call(context, value, index, list) : value;
      computed >= result.computed && (result = {value : value, computed : computed});
    });
    return result.value;
  };

  // return the minimum element (or element-based computation).
  _.min = function(obj, iterator, context) {
    if (!iterator && _.isarray(obj)) return math.min.apply(math, obj);
    if (!iterator && _.isempty(obj)) return infinity;
    var result = {computed : infinity};
    each(obj, function(value, index, list) {
      var computed = iterator ? iterator.call(context, value, index, list) : value;
      computed < result.computed && (result = {value : value, computed : computed});
    });
    return result.value;
  };

  // shuffle an array.
  _.shuffle = function(obj) {
    var shuffled = [], rand;
    each(obj, function(value, index, list) {
      if (index == 0) {
        shuffled[0] = value;
      } else {
        rand = math.floor(math.random() * (index + 1));
        shuffled[index] = shuffled[rand];
        shuffled[rand] = value;
      }
    });
    return shuffled;
  };

  // sort the object's values by a criterion produced by an iterator.
  _.sortby = function(obj, iterator, context) {
    return _.pluck(_.map(obj, function(value, index, list) {
      return {
        value : value,
        criteria : iterator.call(context, value, index, list)
      };
    }).sort(function(left, right) {
      var a = left.criteria, b = right.criteria;
      return a < b ? -1 : a > b ? 1 : 0;
    }), 'value');
  };

  // groups the object's values by a criterion. pass either a string attribute
  // to group by, or a function that returns the criterion.
  _.groupby = function(obj, val) {
    var result = {};
    var iterator = _.isfunction(val) ? val : function(obj) { return obj[val]; };
    each(obj, function(value, index) {
      var key = iterator(value, index);
      (result[key] || (result[key] = [])).push(value);
    });
    return result;
  };

  // use a comparator function to figure out at what index an object should
  // be inserted so as to maintain order. uses binary search.
  _.sortedindex = function(array, obj, iterator) {
    iterator || (iterator = _.identity);
    var low = 0, high = array.length;
    while (low < high) {
      var mid = (low + high) >> 1;
      iterator(array[mid]) < iterator(obj) ? low = mid + 1 : high = mid;
    }
    return low;
  };

  // safely convert anything iterable into a real, live array.
  _.toarray = function(iterable) {
    if (!iterable)                return [];
    if (iterable.toarray)         return iterable.toarray();
    if (_.isarray(iterable))      return slice.call(iterable);
    if (_.isarguments(iterable))  return slice.call(iterable);
    return _.values(iterable);
  };

  // return the number of elements in an object.
  _.size = function(obj) {
    return _.toarray(obj).length;
  };

  // array functions
  // ---------------

  // get the first element of an array. passing **n** will return the first n
  // values in the array. aliased as `head`. the **guard** check allows it to work
  // with `_.map`.
  _.first = _.head = function(array, n, guard) {
    return (n != null) && !guard ? slice.call(array, 0, n) : array[0];
  };

  // returns everything but the last entry of the array. especcialy useful on
  // the arguments object. passing **n** will return all the values in
  // the array, excluding the last n. the **guard** check allows it to work with
  // `_.map`.
  _.initial = function(array, n, guard) {
    return slice.call(array, 0, array.length - ((n == null) || guard ? 1 : n));
  };

  // get the last element of an array. passing **n** will return the last n
  // values in the array. the **guard** check allows it to work with `_.map`.
  _.last = function(array, n, guard) {
    if ((n != null) && !guard) {
      return slice.call(array, math.max(array.length - n, 0));
    } else {
      return array[array.length - 1];
    }
  };

  // returns everything but the first entry of the array. aliased as `tail`.
  // especially useful on the arguments object. passing an **index** will return
  // the rest of the values in the array from that index onward. the **guard**
  // check allows it to work with `_.map`.
  _.rest = _.tail = function(array, index, guard) {
    return slice.call(array, (index == null) || guard ? 1 : index);
  };

  // trim out all falsy values from an array.
  _.compact = function(array) {
    return _.filter(array, function(value){ return !!value; });
  };

  // return a completely flattened version of an array.
  _.flatten = function(array, shallow) {
    return _.reduce(array, function(memo, value) {
      if (_.isarray(value)) return memo.concat(shallow ? value : _.flatten(value));
      memo[memo.length] = value;
      return memo;
    }, []);
  };

  // return a version of the array that does not contain the specified value(s).
  _.without = function(array) {
    return _.difference(array, slice.call(arguments, 1));
  };

  // produce a duplicate-free version of the array. if the array has already
  // been sorted, you have the option of using a faster algorithm.
  // aliased as `unique`.
  _.uniq = _.unique = function(array, issorted, iterator) {
    var initial = iterator ? _.map(array, iterator) : array;
    var result = [];
    _.reduce(initial, function(memo, el, i) {
      if (0 == i || (issorted === true ? _.last(memo) != el : !_.include(memo, el))) {
        memo[memo.length] = el;
        result[result.length] = array[i];
      }
      return memo;
    }, []);
    return result;
  };

  // produce an array that contains the union: each distinct element from all of
  // the passed-in arrays.
  _.union = function() {
    return _.uniq(_.flatten(arguments, true));
  };

  // produce an array that contains every item shared between all the
  // passed-in arrays. (aliased as "intersect" for back-compat.)
  _.intersection = _.intersect = function(array) {
    var rest = slice.call(arguments, 1);
    return _.filter(_.uniq(array), function(item) {
      return _.every(rest, function(other) {
        return _.indexof(other, item) >= 0;
      });
    });
  };

  // take the difference between one array and a number of other arrays.
  // only the elements present in just the first array will remain.
  _.difference = function(array) {
    var rest = _.flatten(slice.call(arguments, 1));
    return _.filter(array, function(value){ return !_.include(rest, value); });
  };

  // zip together multiple lists into a single array -- elements that share
  // an index go together.
  _.zip = function() {
    var args = slice.call(arguments);
    var length = _.max(_.pluck(args, 'length'));
    var results = new array(length);
    for (var i = 0; i < length; i++) results[i] = _.pluck(args, "" + i);
    return results;
  };

  // if the browser doesn't supply us with indexof (i'm looking at you, **msie**),
  // we need this function. return the position of the first occurrence of an
  // item in an array, or -1 if the item is not included in the array.
  // delegates to **ecmascript 5**'s native `indexof` if available.
  // if the array is large and already in sort order, pass `true`
  // for **issorted** to use binary search.
  _.indexof = function(array, item, issorted) {
    if (array == null) return -1;
    var i, l;
    if (issorted) {
      i = _.sortedindex(array, item);
      return array[i] === item ? i : -1;
    }
    if (nativeindexof && array.indexof === nativeindexof) return array.indexof(item);
    for (i = 0, l = array.length; i < l; i++) if (i in array && array[i] === item) return i;
    return -1;
  };

  // delegates to **ecmascript 5**'s native `lastindexof` if available.
  _.lastindexof = function(array, item) {
    if (array == null) return -1;
    if (nativelastindexof && array.lastindexof === nativelastindexof) return array.lastindexof(item);
    var i = array.length;
    while (i--) if (i in array && array[i] === item) return i;
    return -1;
  };

  // generate an integer array containing an arithmetic progression. a port of
  // the native python `range()` function. see
  // [the python documentation](http://docs.python.org/library/functions.html#range).
  _.range = function(start, stop, step) {
    if (arguments.length <= 1) {
      stop = start || 0;
      start = 0;
    }
    step = arguments[2] || 1;

    var len = math.max(math.ceil((stop - start) / step), 0);
    var idx = 0;
    var range = new array(len);

    while(idx < len) {
      range[idx++] = start;
      start += step;
    }

    return range;
  };

  // function (ahem) functions
  // ------------------

  // reusable constructor function for prototype setting.
  var ctor = function(){};

  // create a function bound to a given object (assigning `this`, and arguments,
  // optionally). binding with arguments is also known as `curry`.
  // delegates to **ecmascript 5**'s native `function.bind` if available.
  // we check for `func.bind` first, to fail fast when `func` is undefined.
  _.bind = function bind(func, context) {
    var bound, args;
    if (func.bind === nativebind && nativebind) return nativebind.apply(func, slice.call(arguments, 1));
    if (!_.isfunction(func)) throw new typeerror;
    args = slice.call(arguments, 2);
    return bound = function() {
      if (!(this instanceof bound)) return func.apply(context, args.concat(slice.call(arguments)));
      ctor.prototype = func.prototype;
      var self = new ctor;
      var result = func.apply(self, args.concat(slice.call(arguments)));
      if (object(result) === result) return result;
      return self;
    };
  };

  // bind all of an object's methods to that object. useful for ensuring that
  // all callbacks defined on an object belong to it.
  _.bindall = function(obj) {
    var funcs = slice.call(arguments, 1);
    if (funcs.length == 0) funcs = _.functions(obj);
    each(funcs, function(f) { obj[f] = _.bind(obj[f], obj); });
    return obj;
  };

  // memoize an expensive function by storing its results.
  _.memoize = function(func, hasher) {
    var memo = {};
    hasher || (hasher = _.identity);
    return function() {
      var key = hasher.apply(this, arguments);
      return _.has(memo, key) ? memo[key] : (memo[key] = func.apply(this, arguments));
    };
  };

  // delays a function for the given number of milliseconds, and then calls
  // it with the arguments supplied.
  _.delay = function(func, wait) {
    var args = slice.call(arguments, 2);
    return settimeout(function(){ return func.apply(func, args); }, wait);
  };

  // defers a function, scheduling it to run after the current call stack has
  // cleared.
  _.defer = function(func) {
    return _.delay.apply(_, [func, 1].concat(slice.call(arguments, 1)));
  };

  // returns a function, that, when invoked, will only be triggered at most once
  // during a given window of time.
  _.throttle = function(func, wait) {
    var context, args, timeout, throttling, more;
    var whendone = _.debounce(function(){ more = throttling = false; }, wait);
    return function() {
      context = this; args = arguments;
      var later = function() {
        timeout = null;
        if (more) func.apply(context, args);
        whendone();
      };
      if (!timeout) timeout = settimeout(later, wait);
      if (throttling) {
        more = true;
      } else {
        func.apply(context, args);
      }
      whendone();
      throttling = true;
    };
  };

  // returns a function, that, as long as it continues to be invoked, will not
  // be triggered. the function will be called after it stops being called for
  // n milliseconds.
  _.debounce = function(func, wait) {
    var timeout;
    return function() {
      var context = this, args = arguments;
      var later = function() {
        timeout = null;
        func.apply(context, args);
      };
      cleartimeout(timeout);
      timeout = settimeout(later, wait);
    };
  };

  // returns a function that will be executed at most one time, no matter how
  // often you call it. useful for lazy initialization.
  _.once = function(func) {
    var ran = false, memo;
    return function() {
      if (ran) return memo;
      ran = true;
      return memo = func.apply(this, arguments);
    };
  };

  // returns the first function passed as an argument to the second,
  // allowing you to adjust arguments, run code before and after, and
  // conditionally execute the original function.
  _.wrap = function(func, wrapper) {
    return function() {
      var args = [func].concat(slice.call(arguments, 0));
      return wrapper.apply(this, args);
    };
  };

  // returns a function that is the composition of a list of functions, each
  // consuming the return value of the function that follows.
  _.compose = function() {
    var funcs = arguments;
    return function() {
      var args = arguments;
      for (var i = funcs.length - 1; i >= 0; i--) {
        args = [funcs[i].apply(this, args)];
      }
      return args[0];
    };
  };

  // returns a function that will only be executed after being called n times.
  _.after = function(times, func) {
    if (times <= 0) return func();
    return function() {
      if (--times < 1) { return func.apply(this, arguments); }
    };
  };

  // object functions
  // ----------------

  // retrieve the names of an object's properties.
  // delegates to **ecmascript 5**'s native `object.keys`
  _.keys = nativekeys || function(obj) {
    if (obj !== object(obj)) throw new typeerror('invalid object');
    var keys = [];
    for (var key in obj) if (_.has(obj, key)) keys[keys.length] = key;
    return keys;
  };

  // retrieve the values of an object's properties.
  _.values = function(obj) {
    return _.map(obj, _.identity);
  };

  // return a sorted list of the function names available on the object.
  // aliased as `methods`
  _.functions = _.methods = function(obj) {
    var names = [];
    for (var key in obj) {
      if (_.isfunction(obj[key])) names.push(key);
    }
    return names.sort();
  };

  // extend a given object with all the properties in passed-in object(s).
  _.extend = function(obj) {
    each(slice.call(arguments, 1), function(source) {
      for (var prop in source) {
        obj[prop] = source[prop];
      }
    });
    return obj;
  };

  // fill in a given object with default properties.
  _.defaults = function(obj) {
    each(slice.call(arguments, 1), function(source) {
      for (var prop in source) {
        if (obj[prop] == null) obj[prop] = source[prop];
      }
    });
    return obj;
  };

  // create a (shallow-cloned) duplicate of an object.
  _.clone = function(obj) {
    if (!_.isobject(obj)) return obj;
    return _.isarray(obj) ? obj.slice() : _.extend({}, obj);
  };

  // invokes interceptor with the obj, and then returns obj.
  // the primary purpose of this method is to "tap into" a method chain, in
  // order to perform operations on intermediate results within the chain.
  _.tap = function(obj, interceptor) {
    interceptor(obj);
    return obj;
  };

  // internal recursive comparison function.
  function eq(a, b, stack) {
    // identical objects are equal. `0 === -0`, but they aren't identical.
    // see the harmony `egal` proposal: http://wiki.ecmascript.org/doku.php?id=harmony:egal.
    if (a === b) return a !== 0 || 1 / a == 1 / b;
    // a strict comparison is necessary because `null == undefined`.
    if (a == null || b == null) return a === b;
    // unwrap any wrapped objects.
    if (a._chain) a = a._wrapped;
    if (b._chain) b = b._wrapped;
    // invoke a custom `isequal` method if one is provided.
    if (a.isequal && _.isfunction(a.isequal)) return a.isequal(b);
    if (b.isequal && _.isfunction(b.isequal)) return b.isequal(a);
    // compare `[[class]]` names.
    var classname = tostring.call(a);
    if (classname != tostring.call(b)) return false;
    switch (classname) {
      // strings, numbers, dates, and booleans are compared by value.
      case '[object string]':
        // primitives and their corresponding object wrappers are equivalent; thus, `"5"` is
        // equivalent to `new string("5")`.
        return a == string(b);
      case '[object number]':
        // `nan`s are equivalent, but non-reflexive. an `egal` comparison is performed for
        // other numeric values.
        return a != +a ? b != +b : (a == 0 ? 1 / a == 1 / b : a == +b);
      case '[object date]':
      case '[object boolean]':
        // coerce dates and booleans to numeric primitive values. dates are compared by their
        // millisecond representations. note that invalid dates with millisecond representations
        // of `nan` are not equivalent.
        return +a == +b;
      // regexps are compared by their source patterns and flags.
      case '[object regexp]':
        return a.source == b.source &&
               a.global == b.global &&
               a.multiline == b.multiline &&
               a.ignorecase == b.ignorecase;
    }
    if (typeof a != 'object' || typeof b != 'object') return false;
    // assume equality for cyclic structures. the algorithm for detecting cyclic
    // structures is adapted from es 5.1 section 15.12.3, abstract operation `jo`.
    var length = stack.length;
    while (length--) {
      // linear search. performance is inversely proportional to the number of
      // unique nested structures.
      if (stack[length] == a) return true;
    }
    // add the first object to the stack of traversed objects.
    stack.push(a);
    var size = 0, result = true;
    // recursively compare objects and arrays.
    if (classname == '[object array]') {
      // compare array lengths to determine if a deep comparison is necessary.
      size = a.length;
      result = size == b.length;
      if (result) {
        // deep compare the contents, ignoring non-numeric properties.
        while (size--) {
          // ensure commutative equality for sparse arrays.
          if (!(result = size in a == size in b && eq(a[size], b[size], stack))) break;
        }
      }
    } else {
      // objects with different constructors are not equivalent.
      if ('constructor' in a != 'constructor' in b || a.constructor != b.constructor) return false;
      // deep compare objects.
      for (var key in a) {
        if (_.has(a, key)) {
          // count the expected number of properties.
          size++;
          // deep compare each member.
          if (!(result = _.has(b, key) && eq(a[key], b[key], stack))) break;
        }
      }
      // ensure that both objects contain the same number of properties.
      if (result) {
        for (key in b) {
          if (_.has(b, key) && !(size--)) break;
        }
        result = !size;
      }
    }
    // remove the first object from the stack of traversed objects.
    stack.pop();
    return result;
  }

  // perform a deep comparison to check if two objects are equal.
  _.isequal = function(a, b) {
    return eq(a, b, []);
  };

  // is a given array, string, or object empty?
  // an "empty" object has no enumerable own-properties.
  _.isempty = function(obj) {
    if (_.isarray(obj) || _.isstring(obj)) return obj.length === 0;
    for (var key in obj) if (_.has(obj, key)) return false;
    return true;
  };

  // is a given value a dom element?
  _.iselement = function(obj) {
    return !!(obj && obj.nodetype == 1);
  };

  // is a given value an array?
  // delegates to ecma5's native array.isarray
  _.isarray = nativeisarray || function(obj) {
    return tostring.call(obj) == '[object array]';
  };

  // is a given variable an object?
  _.isobject = function(obj) {
    return obj === object(obj);
  };

  // is a given variable an arguments object?
  _.isarguments = function(obj) {
    return tostring.call(obj) == '[object arguments]';
  };
  if (!_.isarguments(arguments)) {
    _.isarguments = function(obj) {
      return !!(obj && _.has(obj, 'callee'));
    };
  }

  // is a given value a function?
  _.isfunction = function(obj) {
    return tostring.call(obj) == '[object function]';
  };

  // is a given value a string?
  _.isstring = function(obj) {
    return tostring.call(obj) == '[object string]';
  };

  // is a given value a number?
  _.isnumber = function(obj) {
    return tostring.call(obj) == '[object number]';
  };

  // is the given value `nan`?
  _.isnan = function(obj) {
    // `nan` is the only value for which `===` is not reflexive.
    return obj !== obj;
  };

  // is a given value a boolean?
  _.isboolean = function(obj) {
    return obj === true || obj === false || tostring.call(obj) == '[object boolean]';
  };

  // is a given value a date?
  _.isdate = function(obj) {
    return tostring.call(obj) == '[object date]';
  };

  // is the given value a regular expression?
  _.isregexp = function(obj) {
    return tostring.call(obj) == '[object regexp]';
  };

  // is a given value equal to null?
  _.isnull = function(obj) {
    return obj === null;
  };

  // is a given variable undefined?
  _.isundefined = function(obj) {
    return obj === void 0;
  };

  // has own property?
  _.has = function(obj, key) {
    return hasownproperty.call(obj, key);
  };

  // utility functions
  // -----------------

  // run underscore.js in *noconflict* mode, returning the `_` variable to its
  // previous owner. returns a reference to the underscore object.
  _.noconflict = function() {
    root._ = previousunderscore;
    return this;
  };

  // keep the identity function around for default iterators.
  _.identity = function(value) {
    return value;
  };

  // run a function **n** times.
  _.times = function (n, iterator, context) {
    for (var i = 0; i < n; i++) iterator.call(context, i);
  };

  // escape a string for html interpolation.
  _.escape = function(string) {
    return (''+string).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;').replace(/'/g, '&#x27;').replace(/\//g,'&#x2f;');
  };

  // add your own custom functions to the underscore object, ensuring that
  // they're correctly added to the oop wrapper as well.
  _.mixin = function(obj) {
    each(_.functions(obj), function(name){
      addtowrapper(name, _[name] = obj[name]);
    });
  };

  // generate a unique integer id (unique within the entire client session).
  // useful for temporary dom ids.
  var idcounter = 0;
  _.uniqueid = function(prefix) {
    var id = idcounter++;
    return prefix ? prefix + id : id;
  };

  // by default, underscore uses erb-style template delimiters, change the
  // following template settings to use alternative delimiters.
  _.templatesettings = {
    evaluate    : /<%([\s\s]+?)%>/g,
    interpolate : /<%=([\s\s]+?)%>/g,
    escape      : /<%-([\s\s]+?)%>/g
  };

  // when customizing `templatesettings`, if you don't want to define an
  // interpolation, evaluation or escaping regex, we need one that is
  // guaranteed not to match.
  var nomatch = /.^/;

  // within an interpolation, evaluation, or escaping, remove html escaping
  // that had been previously added.
  var unescape = function(code) {
    return code.replace(/\\\\/g, '\\').replace(/\\'/g, "'");
  };

  // javascript micro-templating, similar to john resig's implementation.
  // underscore templating handles arbitrary delimiters, preserves whitespace,
  // and correctly escapes quotes within interpolated code.
  _.template = function(str, data) {
    var c  = _.templatesettings;
    var tmpl = 'var __p=[],print=function(){__p.push.apply(__p,arguments);};' +
      'with(obj||{}){__p.push(\'' +
      str.replace(/\\/g, '\\\\')
         .replace(/'/g, "\\'")
         .replace(c.escape || nomatch, function(match, code) {
           return "',_.escape(" + unescape(code) + "),'";
         })
         .replace(c.interpolate || nomatch, function(match, code) {
           return "'," + unescape(code) + ",'";
         })
         .replace(c.evaluate || nomatch, function(match, code) {
           return "');" + unescape(code).replace(/[\r\n\t]/g, ' ') + ";__p.push('";
         })
         .replace(/\r/g, '\\r')
         .replace(/\n/g, '\\n')
         .replace(/\t/g, '\\t')
         + "');}return __p.join('');";
    var func = new function('obj', '_', tmpl);
    if (data) return func(data, _);
    return function(data) {
      return func.call(this, data, _);
    };
  };

  // add a "chain" function, which will delegate to the wrapper.
  _.chain = function(obj) {
    return _(obj).chain();
  };

  // the oop wrapper
  // ---------------

  // if underscore is called as a function, it returns a wrapped object that
  // can be used oo-style. this wrapper holds altered versions of all the
  // underscore functions. wrapped objects may be chained.
  var wrapper = function(obj) { this._wrapped = obj; };

  // expose `wrapper.prototype` as `_.prototype`
  _.prototype = wrapper.prototype;

  // helper function to continue chaining intermediate results.
  var result = function(obj, chain) {
    return chain ? _(obj).chain() : obj;
  };

  // a method to easily add functions to the oop wrapper.
  var addtowrapper = function(name, func) {
    wrapper.prototype[name] = function() {
      var args = slice.call(arguments);
      unshift.call(args, this._wrapped);
      return result(func.apply(_, args), this._chain);
    };
  };

  // add all of the underscore functions to the wrapper object.
  _.mixin(_);

  // add all mutator array functions to the wrapper.
  each(['pop', 'push', 'reverse', 'shift', 'sort', 'splice', 'unshift'], function(name) {
    var method = arrayproto[name];
    wrapper.prototype[name] = function() {
      var wrapped = this._wrapped;
      method.apply(wrapped, arguments);
      var length = wrapped.length;
      if ((name == 'shift' || name == 'splice') && length === 0) delete wrapped[0];
      return result(wrapped, this._chain);
    };
  });

  // add all accessor array functions to the wrapper.
  each(['concat', 'join', 'slice'], function(name) {
    var method = arrayproto[name];
    wrapper.prototype[name] = function() {
      return result(method.apply(this._wrapped, arguments), this._chain);
    };
  });

  // start chaining a wrapped underscore object.
  wrapper.prototype.chain = function() {
    this._chain = true;
    return this;
  };

  // extracts the result from a wrapped and chained object.
  wrapper.prototype.value = function() {
    return this._wrapped;
  };

}).call(this);
