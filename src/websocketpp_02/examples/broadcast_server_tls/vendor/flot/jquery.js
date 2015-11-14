/*!
 * jquery javascript library v1.5.1
 * http://jquery.com/
 *
 * copyright 2011, john resig
 * dual licensed under the mit or gpl version 2 licenses.
 * http://jquery.org/license
 *
 * includes sizzle.js
 * http://sizzlejs.com/
 * copyright 2011, the dojo foundation
 * released under the mit, bsd, and gpl licenses.
 *
 * date: wed feb 23 13:55:29 2011 -0500
 */
(function( window, undefined ) {

// use the correct document accordingly with window argument (sandbox)
var document = window.document;
var jquery = (function() {

// define a local copy of jquery
var jquery = function( selector, context ) {
		// the jquery object is actually just the init constructor 'enhanced'
		return new jquery.fn.init( selector, context, rootjquery );
	},

	// map over jquery in case of overwrite
	_jquery = window.jquery,

	// map over the $ in case of overwrite
	_$ = window.$,

	// a central reference to the root jquery(document)
	rootjquery,

	// a simple way to check for html strings or id strings
	// (both of which we optimize for)
	quickexpr = /^(?:[^<]*(<[\w\w]+>)[^>]*$|#([\w\-]+)$)/,

	// check if a string has a non-whitespace character in it
	rnotwhite = /\s/,

	// used for trimming whitespace
	trimleft = /^\s+/,
	trimright = /\s+$/,

	// check for digits
	rdigit = /\d/,

	// match a standalone tag
	rsingletag = /^<(\w+)\s*\/?>(?:<\/\1>)?$/,

	// json regexp
	rvalidchars = /^[\],:{}\s]*$/,
	rvalidescape = /\\(?:["\\\/bfnrt]|u[0-9a-fa-f]{4})/g,
	rvalidtokens = /"[^"\\\n\r]*"|true|false|null|-?\d+(?:\.\d*)?(?:[ee][+\-]?\d+)?/g,
	rvalidbraces = /(?:^|:|,)(?:\s*\[)+/g,

	// useragent regexp
	rwebkit = /(webkit)[ \/]([\w.]+)/,
	ropera = /(opera)(?:.*version)?[ \/]([\w.]+)/,
	rmsie = /(msie) ([\w.]+)/,
	rmozilla = /(mozilla)(?:.*? rv:([\w.]+))?/,

	// keep a useragent string for use with jquery.browser
	useragent = navigator.useragent,

	// for matching the engine and version of the browser
	browsermatch,

	// has the ready events already been bound?
	readybound = false,

	// the deferred used on dom ready
	readylist,

	// promise methods
	promisemethods = "then done fail isresolved isrejected promise".split( " " ),

	// the ready event handler
	domcontentloaded,

	// save a reference to some core methods
	tostring = object.prototype.tostring,
	hasown = object.prototype.hasownproperty,
	push = array.prototype.push,
	slice = array.prototype.slice,
	trim = string.prototype.trim,
	indexof = array.prototype.indexof,

	// [[class]] -> type pairs
	class2type = {};

jquery.fn = jquery.prototype = {
	constructor: jquery,
	init: function( selector, context, rootjquery ) {
		var match, elem, ret, doc;

		// handle $(""), $(null), or $(undefined)
		if ( !selector ) {
			return this;
		}

		// handle $(domelement)
		if ( selector.nodetype ) {
			this.context = this[0] = selector;
			this.length = 1;
			return this;
		}

		// the body element only exists once, optimize finding it
		if ( selector === "body" && !context && document.body ) {
			this.context = document;
			this[0] = document.body;
			this.selector = "body";
			this.length = 1;
			return this;
		}

		// handle html strings
		if ( typeof selector === "string" ) {
			// are we dealing with html string or an id?
			match = quickexpr.exec( selector );

			// verify a match, and that no context was specified for #id
			if ( match && (match[1] || !context) ) {

				// handle: $(html) -> $(array)
				if ( match[1] ) {
					context = context instanceof jquery ? context[0] : context;
					doc = (context ? context.ownerdocument || context : document);

					// if a single string is passed in and it's a single tag
					// just do a createelement and skip the rest
					ret = rsingletag.exec( selector );

					if ( ret ) {
						if ( jquery.isplainobject( context ) ) {
							selector = [ document.createelement( ret[1] ) ];
							jquery.fn.attr.call( selector, context, true );

						} else {
							selector = [ doc.createelement( ret[1] ) ];
						}

					} else {
						ret = jquery.buildfragment( [ match[1] ], [ doc ] );
						selector = (ret.cacheable ? jquery.clone(ret.fragment) : ret.fragment).childnodes;
					}

					return jquery.merge( this, selector );

				// handle: $("#id")
				} else {
					elem = document.getelementbyid( match[2] );

					// check parentnode to catch when blackberry 4.6 returns
					// nodes that are no longer in the document #6963
					if ( elem && elem.parentnode ) {
						// handle the case where ie and opera return items
						// by name instead of id
						if ( elem.id !== match[2] ) {
							return rootjquery.find( selector );
						}

						// otherwise, we inject the element directly into the jquery object
						this.length = 1;
						this[0] = elem;
					}

					this.context = document;
					this.selector = selector;
					return this;
				}

			// handle: $(expr, $(...))
			} else if ( !context || context.jquery ) {
				return (context || rootjquery).find( selector );

			// handle: $(expr, context)
			// (which is just equivalent to: $(context).find(expr)
			} else {
				return this.constructor( context ).find( selector );
			}

		// handle: $(function)
		// shortcut for document ready
		} else if ( jquery.isfunction( selector ) ) {
			return rootjquery.ready( selector );
		}

		if (selector.selector !== undefined) {
			this.selector = selector.selector;
			this.context = selector.context;
		}

		return jquery.makearray( selector, this );
	},

	// start with an empty selector
	selector: "",

	// the current version of jquery being used
	jquery: "1.5.1",

	// the default length of a jquery object is 0
	length: 0,

	// the number of elements contained in the matched element set
	size: function() {
		return this.length;
	},

	toarray: function() {
		return slice.call( this, 0 );
	},

	// get the nth element in the matched element set or
	// get the whole matched element set as a clean array
	get: function( num ) {
		return num == null ?

			// return a 'clean' array
			this.toarray() :

			// return just the object
			( num < 0 ? this[ this.length + num ] : this[ num ] );
	},

	// take an array of elements and push it onto the stack
	// (returning the new matched element set)
	pushstack: function( elems, name, selector ) {
		// build a new jquery matched element set
		var ret = this.constructor();

		if ( jquery.isarray( elems ) ) {
			push.apply( ret, elems );

		} else {
			jquery.merge( ret, elems );
		}

		// add the old object onto the stack (as a reference)
		ret.prevobject = this;

		ret.context = this.context;

		if ( name === "find" ) {
			ret.selector = this.selector + (this.selector ? " " : "") + selector;
		} else if ( name ) {
			ret.selector = this.selector + "." + name + "(" + selector + ")";
		}

		// return the newly-formed element set
		return ret;
	},

	// execute a callback for every element in the matched set.
	// (you can seed the arguments with an array of args, but this is
	// only used internally.)
	each: function( callback, args ) {
		return jquery.each( this, callback, args );
	},

	ready: function( fn ) {
		// attach the listeners
		jquery.bindready();

		// add the callback
		readylist.done( fn );

		return this;
	},

	eq: function( i ) {
		return i === -1 ?
			this.slice( i ) :
			this.slice( i, +i + 1 );
	},

	first: function() {
		return this.eq( 0 );
	},

	last: function() {
		return this.eq( -1 );
	},

	slice: function() {
		return this.pushstack( slice.apply( this, arguments ),
			"slice", slice.call(arguments).join(",") );
	},

	map: function( callback ) {
		return this.pushstack( jquery.map(this, function( elem, i ) {
			return callback.call( elem, i, elem );
		}));
	},

	end: function() {
		return this.prevobject || this.constructor(null);
	},

	// for internal use only.
	// behaves like an array's method, not like a jquery method.
	push: push,
	sort: [].sort,
	splice: [].splice
};

// give the init function the jquery prototype for later instantiation
jquery.fn.init.prototype = jquery.fn;

jquery.extend = jquery.fn.extend = function() {
	var options, name, src, copy, copyisarray, clone,
		target = arguments[0] || {},
		i = 1,
		length = arguments.length,
		deep = false;

	// handle a deep copy situation
	if ( typeof target === "boolean" ) {
		deep = target;
		target = arguments[1] || {};
		// skip the boolean and the target
		i = 2;
	}

	// handle case when target is a string or something (possible in deep copy)
	if ( typeof target !== "object" && !jquery.isfunction(target) ) {
		target = {};
	}

	// extend jquery itself if only one argument is passed
	if ( length === i ) {
		target = this;
		--i;
	}

	for ( ; i < length; i++ ) {
		// only deal with non-null/undefined values
		if ( (options = arguments[ i ]) != null ) {
			// extend the base object
			for ( name in options ) {
				src = target[ name ];
				copy = options[ name ];

				// prevent never-ending loop
				if ( target === copy ) {
					continue;
				}

				// recurse if we're merging plain objects or arrays
				if ( deep && copy && ( jquery.isplainobject(copy) || (copyisarray = jquery.isarray(copy)) ) ) {
					if ( copyisarray ) {
						copyisarray = false;
						clone = src && jquery.isarray(src) ? src : [];

					} else {
						clone = src && jquery.isplainobject(src) ? src : {};
					}

					// never move original objects, clone them
					target[ name ] = jquery.extend( deep, clone, copy );

				// don't bring in undefined values
				} else if ( copy !== undefined ) {
					target[ name ] = copy;
				}
			}
		}
	}

	// return the modified object
	return target;
};

jquery.extend({
	noconflict: function( deep ) {
		window.$ = _$;

		if ( deep ) {
			window.jquery = _jquery;
		}

		return jquery;
	},

	// is the dom ready to be used? set to true once it occurs.
	isready: false,

	// a counter to track how many items to wait for before
	// the ready event fires. see #6781
	readywait: 1,

	// handle when the dom is ready
	ready: function( wait ) {
		// a third-party is pushing the ready event forwards
		if ( wait === true ) {
			jquery.readywait--;
		}

		// make sure that the dom is not already loaded
		if ( !jquery.readywait || (wait !== true && !jquery.isready) ) {
			// make sure body exists, at least, in case ie gets a little overzealous (ticket #5443).
			if ( !document.body ) {
				return settimeout( jquery.ready, 1 );
			}

			// remember that the dom is ready
			jquery.isready = true;

			// if a normal dom ready event fired, decrement, and wait if need be
			if ( wait !== true && --jquery.readywait > 0 ) {
				return;
			}

			// if there are functions bound, to execute
			readylist.resolvewith( document, [ jquery ] );

			// trigger any bound ready events
			if ( jquery.fn.trigger ) {
				jquery( document ).trigger( "ready" ).unbind( "ready" );
			}
		}
	},

	bindready: function() {
		if ( readybound ) {
			return;
		}

		readybound = true;

		// catch cases where $(document).ready() is called after the
		// browser event has already occurred.
		if ( document.readystate === "complete" ) {
			// handle it asynchronously to allow scripts the opportunity to delay ready
			return settimeout( jquery.ready, 1 );
		}

		// mozilla, opera and webkit nightlies currently support this event
		if ( document.addeventlistener ) {
			// use the handy event callback
			document.addeventlistener( "domcontentloaded", domcontentloaded, false );

			// a fallback to window.onload, that will always work
			window.addeventlistener( "load", jquery.ready, false );

		// if ie event model is used
		} else if ( document.attachevent ) {
			// ensure firing before onload,
			// maybe late but safe also for iframes
			document.attachevent("onreadystatechange", domcontentloaded);

			// a fallback to window.onload, that will always work
			window.attachevent( "onload", jquery.ready );

			// if ie and not a frame
			// continually check to see if the document is ready
			var toplevel = false;

			try {
				toplevel = window.frameelement == null;
			} catch(e) {}

			if ( document.documentelement.doscroll && toplevel ) {
				doscrollcheck();
			}
		}
	},

	// see test/unit/core.js for details concerning isfunction.
	// since version 1.3, dom methods and functions like alert
	// aren't supported. they return false on ie (#2968).
	isfunction: function( obj ) {
		return jquery.type(obj) === "function";
	},

	isarray: array.isarray || function( obj ) {
		return jquery.type(obj) === "array";
	},

	// a crude way of determining if an object is a window
	iswindow: function( obj ) {
		return obj && typeof obj === "object" && "setinterval" in obj;
	},

	isnan: function( obj ) {
		return obj == null || !rdigit.test( obj ) || isnan( obj );
	},

	type: function( obj ) {
		return obj == null ?
			string( obj ) :
			class2type[ tostring.call(obj) ] || "object";
	},

	isplainobject: function( obj ) {
		// must be an object.
		// because of ie, we also have to check the presence of the constructor property.
		// make sure that dom nodes and window objects don't pass through, as well
		if ( !obj || jquery.type(obj) !== "object" || obj.nodetype || jquery.iswindow( obj ) ) {
			return false;
		}

		// not own constructor property must be object
		if ( obj.constructor &&
			!hasown.call(obj, "constructor") &&
			!hasown.call(obj.constructor.prototype, "isprototypeof") ) {
			return false;
		}

		// own properties are enumerated firstly, so to speed up,
		// if last one is own, then all properties are own.

		var key;
		for ( key in obj ) {}

		return key === undefined || hasown.call( obj, key );
	},

	isemptyobject: function( obj ) {
		for ( var name in obj ) {
			return false;
		}
		return true;
	},

	error: function( msg ) {
		throw msg;
	},

	parsejson: function( data ) {
		if ( typeof data !== "string" || !data ) {
			return null;
		}

		// make sure leading/trailing whitespace is removed (ie can't handle it)
		data = jquery.trim( data );

		// make sure the incoming data is actual json
		// logic borrowed from http://json.org/json2.js
		if ( rvalidchars.test(data.replace(rvalidescape, "@")
			.replace(rvalidtokens, "]")
			.replace(rvalidbraces, "")) ) {

			// try to use the native json parser first
			return window.json && window.json.parse ?
				window.json.parse( data ) :
				(new function("return " + data))();

		} else {
			jquery.error( "invalid json: " + data );
		}
	},

	// cross-browser xml parsing
	// (xml & tmp used internally)
	parsexml: function( data , xml , tmp ) {

		if ( window.domparser ) { // standard
			tmp = new domparser();
			xml = tmp.parsefromstring( data , "text/xml" );
		} else { // ie
			xml = new activexobject( "microsoft.xmldom" );
			xml.async = "false";
			xml.loadxml( data );
		}

		tmp = xml.documentelement;

		if ( ! tmp || ! tmp.nodename || tmp.nodename === "parsererror" ) {
			jquery.error( "invalid xml: " + data );
		}

		return xml;
	},

	noop: function() {},

	// evalulates a script in a global context
	globaleval: function( data ) {
		if ( data && rnotwhite.test(data) ) {
			// inspired by code by andrea giammarchi
			// http://webreflection.blogspot.com/2007/08/global-scope-evaluation-and-dom.html
			var head = document.head || document.getelementsbytagname( "head" )[0] || document.documentelement,
				script = document.createelement( "script" );

			if ( jquery.support.scripteval() ) {
				script.appendchild( document.createtextnode( data ) );
			} else {
				script.text = data;
			}

			// use insertbefore instead of appendchild to circumvent an ie6 bug.
			// this arises when a base node is used (#2709).
			head.insertbefore( script, head.firstchild );
			head.removechild( script );
		}
	},

	nodename: function( elem, name ) {
		return elem.nodename && elem.nodename.touppercase() === name.touppercase();
	},

	// args is for internal usage only
	each: function( object, callback, args ) {
		var name, i = 0,
			length = object.length,
			isobj = length === undefined || jquery.isfunction(object);

		if ( args ) {
			if ( isobj ) {
				for ( name in object ) {
					if ( callback.apply( object[ name ], args ) === false ) {
						break;
					}
				}
			} else {
				for ( ; i < length; ) {
					if ( callback.apply( object[ i++ ], args ) === false ) {
						break;
					}
				}
			}

		// a special, fast, case for the most common use of each
		} else {
			if ( isobj ) {
				for ( name in object ) {
					if ( callback.call( object[ name ], name, object[ name ] ) === false ) {
						break;
					}
				}
			} else {
				for ( var value = object[0];
					i < length && callback.call( value, i, value ) !== false; value = object[++i] ) {}
			}
		}

		return object;
	},

	// use native string.trim function wherever possible
	trim: trim ?
		function( text ) {
			return text == null ?
				"" :
				trim.call( text );
		} :

		// otherwise use our own trimming functionality
		function( text ) {
			return text == null ?
				"" :
				text.tostring().replace( trimleft, "" ).replace( trimright, "" );
		},

	// results is for internal usage only
	makearray: function( array, results ) {
		var ret = results || [];

		if ( array != null ) {
			// the window, strings (and functions) also have 'length'
			// the extra typeof function check is to prevent crashes
			// in safari 2 (see: #3039)
			// tweaked logic slightly to handle blackberry 4.7 regexp issues #6930
			var type = jquery.type(array);

			if ( array.length == null || type === "string" || type === "function" || type === "regexp" || jquery.iswindow( array ) ) {
				push.call( ret, array );
			} else {
				jquery.merge( ret, array );
			}
		}

		return ret;
	},

	inarray: function( elem, array ) {
		if ( array.indexof ) {
			return array.indexof( elem );
		}

		for ( var i = 0, length = array.length; i < length; i++ ) {
			if ( array[ i ] === elem ) {
				return i;
			}
		}

		return -1;
	},

	merge: function( first, second ) {
		var i = first.length,
			j = 0;

		if ( typeof second.length === "number" ) {
			for ( var l = second.length; j < l; j++ ) {
				first[ i++ ] = second[ j ];
			}

		} else {
			while ( second[j] !== undefined ) {
				first[ i++ ] = second[ j++ ];
			}
		}

		first.length = i;

		return first;
	},

	grep: function( elems, callback, inv ) {
		var ret = [], retval;
		inv = !!inv;

		// go through the array, only saving the items
		// that pass the validator function
		for ( var i = 0, length = elems.length; i < length; i++ ) {
			retval = !!callback( elems[ i ], i );
			if ( inv !== retval ) {
				ret.push( elems[ i ] );
			}
		}

		return ret;
	},

	// arg is for internal usage only
	map: function( elems, callback, arg ) {
		var ret = [], value;

		// go through the array, translating each of the items to their
		// new value (or values).
		for ( var i = 0, length = elems.length; i < length; i++ ) {
			value = callback( elems[ i ], i, arg );

			if ( value != null ) {
				ret[ ret.length ] = value;
			}
		}

		// flatten any nested arrays
		return ret.concat.apply( [], ret );
	},

	// a global guid counter for objects
	guid: 1,

	proxy: function( fn, proxy, thisobject ) {
		if ( arguments.length === 2 ) {
			if ( typeof proxy === "string" ) {
				thisobject = fn;
				fn = thisobject[ proxy ];
				proxy = undefined;

			} else if ( proxy && !jquery.isfunction( proxy ) ) {
				thisobject = proxy;
				proxy = undefined;
			}
		}

		if ( !proxy && fn ) {
			proxy = function() {
				return fn.apply( thisobject || this, arguments );
			};
		}

		// set the guid of unique handler to the same of original handler, so it can be removed
		if ( fn ) {
			proxy.guid = fn.guid = fn.guid || proxy.guid || jquery.guid++;
		}

		// so proxy can be declared as an argument
		return proxy;
	},

	// mutifunctional method to get and set values to a collection
	// the value/s can be optionally by executed if its a function
	access: function( elems, key, value, exec, fn, pass ) {
		var length = elems.length;

		// setting many attributes
		if ( typeof key === "object" ) {
			for ( var k in key ) {
				jquery.access( elems, k, key[k], exec, fn, value );
			}
			return elems;
		}

		// setting one attribute
		if ( value !== undefined ) {
			// optionally, function values get executed if exec is true
			exec = !pass && exec && jquery.isfunction(value);

			for ( var i = 0; i < length; i++ ) {
				fn( elems[i], key, exec ? value.call( elems[i], i, fn( elems[i], key ) ) : value, pass );
			}

			return elems;
		}

		// getting an attribute
		return length ? fn( elems[0], key ) : undefined;
	},

	now: function() {
		return (new date()).gettime();
	},

	// create a simple deferred (one callbacks list)
	_deferred: function() {
		var // callbacks list
			callbacks = [],
			// stored [ context , args ]
			fired,
			// to avoid firing when already doing so
			firing,
			// flag to know if the deferred has been cancelled
			cancelled,
			// the deferred itself
			deferred  = {

				// done( f1, f2, ...)
				done: function() {
					if ( !cancelled ) {
						var args = arguments,
							i,
							length,
							elem,
							type,
							_fired;
						if ( fired ) {
							_fired = fired;
							fired = 0;
						}
						for ( i = 0, length = args.length; i < length; i++ ) {
							elem = args[ i ];
							type = jquery.type( elem );
							if ( type === "array" ) {
								deferred.done.apply( deferred, elem );
							} else if ( type === "function" ) {
								callbacks.push( elem );
							}
						}
						if ( _fired ) {
							deferred.resolvewith( _fired[ 0 ], _fired[ 1 ] );
						}
					}
					return this;
				},

				// resolve with given context and args
				resolvewith: function( context, args ) {
					if ( !cancelled && !fired && !firing ) {
						firing = 1;
						try {
							while( callbacks[ 0 ] ) {
								callbacks.shift().apply( context, args );
							}
						}
						// we have to add a catch block for
						// ie prior to 8 or else the finally
						// block will never get executed
						catch (e) {
							throw e;
						}
						finally {
							fired = [ context, args ];
							firing = 0;
						}
					}
					return this;
				},

				// resolve with this as context and given arguments
				resolve: function() {
					deferred.resolvewith( jquery.isfunction( this.promise ) ? this.promise() : this, arguments );
					return this;
				},

				// has this deferred been resolved?
				isresolved: function() {
					return !!( firing || fired );
				},

				// cancel
				cancel: function() {
					cancelled = 1;
					callbacks = [];
					return this;
				}
			};

		return deferred;
	},

	// full fledged deferred (two callbacks list)
	deferred: function( func ) {
		var deferred = jquery._deferred(),
			faildeferred = jquery._deferred(),
			promise;
		// add errordeferred methods, then and promise
		jquery.extend( deferred, {
			then: function( donecallbacks, failcallbacks ) {
				deferred.done( donecallbacks ).fail( failcallbacks );
				return this;
			},
			fail: faildeferred.done,
			rejectwith: faildeferred.resolvewith,
			reject: faildeferred.resolve,
			isrejected: faildeferred.isresolved,
			// get a promise for this deferred
			// if obj is provided, the promise aspect is added to the object
			promise: function( obj ) {
				if ( obj == null ) {
					if ( promise ) {
						return promise;
					}
					promise = obj = {};
				}
				var i = promisemethods.length;
				while( i-- ) {
					obj[ promisemethods[i] ] = deferred[ promisemethods[i] ];
				}
				return obj;
			}
		} );
		// make sure only one callback list will be used
		deferred.done( faildeferred.cancel ).fail( deferred.cancel );
		// unexpose cancel
		delete deferred.cancel;
		// call given func if any
		if ( func ) {
			func.call( deferred, deferred );
		}
		return deferred;
	},

	// deferred helper
	when: function( object ) {
		var lastindex = arguments.length,
			deferred = lastindex <= 1 && object && jquery.isfunction( object.promise ) ?
				object :
				jquery.deferred(),
			promise = deferred.promise();

		if ( lastindex > 1 ) {
			var array = slice.call( arguments, 0 ),
				count = lastindex,
				icallback = function( index ) {
					return function( value ) {
						array[ index ] = arguments.length > 1 ? slice.call( arguments, 0 ) : value;
						if ( !( --count ) ) {
							deferred.resolvewith( promise, array );
						}
					};
				};
			while( ( lastindex-- ) ) {
				object = array[ lastindex ];
				if ( object && jquery.isfunction( object.promise ) ) {
					object.promise().then( icallback(lastindex), deferred.reject );
				} else {
					--count;
				}
			}
			if ( !count ) {
				deferred.resolvewith( promise, array );
			}
		} else if ( deferred !== object ) {
			deferred.resolve( object );
		}
		return promise;
	},

	// use of jquery.browser is frowned upon.
	// more details: http://docs.jquery.com/utilities/jquery.browser
	uamatch: function( ua ) {
		ua = ua.tolowercase();

		var match = rwebkit.exec( ua ) ||
			ropera.exec( ua ) ||
			rmsie.exec( ua ) ||
			ua.indexof("compatible") < 0 && rmozilla.exec( ua ) ||
			[];

		return { browser: match[1] || "", version: match[2] || "0" };
	},

	sub: function() {
		function jquerysubclass( selector, context ) {
			return new jquerysubclass.fn.init( selector, context );
		}
		jquery.extend( true, jquerysubclass, this );
		jquerysubclass.superclass = this;
		jquerysubclass.fn = jquerysubclass.prototype = this();
		jquerysubclass.fn.constructor = jquerysubclass;
		jquerysubclass.subclass = this.subclass;
		jquerysubclass.fn.init = function init( selector, context ) {
			if ( context && context instanceof jquery && !(context instanceof jquerysubclass) ) {
				context = jquerysubclass(context);
			}

			return jquery.fn.init.call( this, selector, context, rootjquerysubclass );
		};
		jquerysubclass.fn.init.prototype = jquerysubclass.fn;
		var rootjquerysubclass = jquerysubclass(document);
		return jquerysubclass;
	},

	browser: {}
});

// create readylist deferred
readylist = jquery._deferred();

// populate the class2type map
jquery.each("boolean number string function array date regexp object".split(" "), function(i, name) {
	class2type[ "[object " + name + "]" ] = name.tolowercase();
});

browsermatch = jquery.uamatch( useragent );
if ( browsermatch.browser ) {
	jquery.browser[ browsermatch.browser ] = true;
	jquery.browser.version = browsermatch.version;
}

// deprecated, use jquery.browser.webkit instead
if ( jquery.browser.webkit ) {
	jquery.browser.safari = true;
}

if ( indexof ) {
	jquery.inarray = function( elem, array ) {
		return indexof.call( array, elem );
	};
}

// ie doesn't match non-breaking spaces with \s
if ( rnotwhite.test( "\xa0" ) ) {
	trimleft = /^[\s\xa0]+/;
	trimright = /[\s\xa0]+$/;
}

// all jquery objects should point back to these
rootjquery = jquery(document);

// cleanup functions for the document ready method
if ( document.addeventlistener ) {
	domcontentloaded = function() {
		document.removeeventlistener( "domcontentloaded", domcontentloaded, false );
		jquery.ready();
	};

} else if ( document.attachevent ) {
	domcontentloaded = function() {
		// make sure body exists, at least, in case ie gets a little overzealous (ticket #5443).
		if ( document.readystate === "complete" ) {
			document.detachevent( "onreadystatechange", domcontentloaded );
			jquery.ready();
		}
	};
}

// the dom ready check for internet explorer
function doscrollcheck() {
	if ( jquery.isready ) {
		return;
	}

	try {
		// if ie is used, use the trick by diego perini
		// http://javascript.nwbox.com/iecontentloaded/
		document.documentelement.doscroll("left");
	} catch(e) {
		settimeout( doscrollcheck, 1 );
		return;
	}

	// and execute any waiting functions
	jquery.ready();
}

// expose jquery to the global object
return jquery;

})();


(function() {

	jquery.support = {};

	var div = document.createelement("div");

	div.style.display = "none";
	div.innerhtml = "   <link/><table></table><a href='/a' style='color:red;float:left;opacity:.55;'>a</a><input type='checkbox'/>";

	var all = div.getelementsbytagname("*"),
		a = div.getelementsbytagname("a")[0],
		select = document.createelement("select"),
		opt = select.appendchild( document.createelement("option") ),
		input = div.getelementsbytagname("input")[0];

	// can't get basic test support
	if ( !all || !all.length || !a ) {
		return;
	}

	jquery.support = {
		// ie strips leading whitespace when .innerhtml is used
		leadingwhitespace: div.firstchild.nodetype === 3,

		// make sure that tbody elements aren't automatically inserted
		// ie will insert them into empty tables
		tbody: !div.getelementsbytagname("tbody").length,

		// make sure that link elements get serialized correctly by innerhtml
		// this requires a wrapper element in ie
		htmlserialize: !!div.getelementsbytagname("link").length,

		// get the style information from getattribute
		// (ie uses .csstext insted)
		style: /red/.test( a.getattribute("style") ),

		// make sure that urls aren't manipulated
		// (ie normalizes it by default)
		hrefnormalized: a.getattribute("href") === "/a",

		// make sure that element opacity exists
		// (ie uses filter instead)
		// use a regex to work around a webkit issue. see #5145
		opacity: /^0.55$/.test( a.style.opacity ),

		// verify style float existence
		// (ie uses stylefloat instead of cssfloat)
		cssfloat: !!a.style.cssfloat,

		// make sure that if no value is specified for a checkbox
		// that it defaults to "on".
		// (webkit defaults to "" instead)
		checkon: input.value === "on",

		// make sure that a selected-by-default option has a working selected property.
		// (webkit defaults to false instead of true, ie too, if it's in an optgroup)
		optselected: opt.selected,

		// will be defined later
		deleteexpando: true,
		optdisabled: false,
		checkclone: false,
		nocloneevent: true,
		noclonechecked: true,
		boxmodel: null,
		inlineblockneedslayout: false,
		shrinkwrapblocks: false,
		reliablehiddenoffsets: true
	};

	input.checked = true;
	jquery.support.noclonechecked = input.clonenode( true ).checked;

	// make sure that the options inside disabled selects aren't marked as disabled
	// (webkit marks them as diabled)
	select.disabled = true;
	jquery.support.optdisabled = !opt.disabled;

	var _scripteval = null;
	jquery.support.scripteval = function() {
		if ( _scripteval === null ) {
			var root = document.documentelement,
				script = document.createelement("script"),
				id = "script" + jquery.now();

			try {
				script.appendchild( document.createtextnode( "window." + id + "=1;" ) );
			} catch(e) {}

			root.insertbefore( script, root.firstchild );

			// make sure that the execution of code works by injecting a script
			// tag with appendchild/createtextnode
			// (ie doesn't support this, fails, and uses .text instead)
			if ( window[ id ] ) {
				_scripteval = true;
				delete window[ id ];
			} else {
				_scripteval = false;
			}

			root.removechild( script );
			// release memory in ie
			root = script = id  = null;
		}

		return _scripteval;
	};

	// test to see if it's possible to delete an expando from an element
	// fails in internet explorer
	try {
		delete div.test;

	} catch(e) {
		jquery.support.deleteexpando = false;
	}

	if ( !div.addeventlistener && div.attachevent && div.fireevent ) {
		div.attachevent("onclick", function click() {
			// cloning a node shouldn't copy over any
			// bound event handlers (ie does this)
			jquery.support.nocloneevent = false;
			div.detachevent("onclick", click);
		});
		div.clonenode(true).fireevent("onclick");
	}

	div = document.createelement("div");
	div.innerhtml = "<input type='radio' name='radiotest' checked='checked'/>";

	var fragment = document.createdocumentfragment();
	fragment.appendchild( div.firstchild );

	// webkit doesn't clone checked state correctly in fragments
	jquery.support.checkclone = fragment.clonenode(true).clonenode(true).lastchild.checked;

	// figure out if the w3c box model works as expected
	// document.body must exist before we can do this
	jquery(function() {
		var div = document.createelement("div"),
			body = document.getelementsbytagname("body")[0];

		// frameset documents with no body should not run this code
		if ( !body ) {
			return;
		}

		div.style.width = div.style.paddingleft = "1px";
		body.appendchild( div );
		jquery.boxmodel = jquery.support.boxmodel = div.offsetwidth === 2;

		if ( "zoom" in div.style ) {
			// check if natively block-level elements act like inline-block
			// elements when setting their display to 'inline' and giving
			// them layout
			// (ie < 8 does this)
			div.style.display = "inline";
			div.style.zoom = 1;
			jquery.support.inlineblockneedslayout = div.offsetwidth === 2;

			// check if elements with layout shrink-wrap their children
			// (ie 6 does this)
			div.style.display = "";
			div.innerhtml = "<div style='width:4px;'></div>";
			jquery.support.shrinkwrapblocks = div.offsetwidth !== 2;
		}

		div.innerhtml = "<table><tr><td style='padding:0;border:0;display:none'></td><td>t</td></tr></table>";
		var tds = div.getelementsbytagname("td");

		// check if table cells still have offsetwidth/height when they are set
		// to display:none and there are still other visible table cells in a
		// table row; if so, offsetwidth/height are not reliable for use when
		// determining if an element has been hidden directly using
		// display:none (it is still safe to use offsets if a parent element is
		// hidden; don safety goggles and see bug #4512 for more information).
		// (only ie 8 fails this test)
		jquery.support.reliablehiddenoffsets = tds[0].offsetheight === 0;

		tds[0].style.display = "";
		tds[1].style.display = "none";

		// check if empty table cells still have offsetwidth/height
		// (ie < 8 fail this test)
		jquery.support.reliablehiddenoffsets = jquery.support.reliablehiddenoffsets && tds[0].offsetheight === 0;
		div.innerhtml = "";

		body.removechild( div ).style.display = "none";
		div = tds = null;
	});

	// technique from juriy zaytsev
	// http://thinkweb2.com/projects/prototype/detecting-event-support-without-browser-sniffing/
	var eventsupported = function( eventname ) {
		var el = document.createelement("div");
		eventname = "on" + eventname;

		// we only care about the case where non-standard event systems
		// are used, namely in ie. short-circuiting here helps us to
		// avoid an eval call (in setattribute) which can cause csp
		// to go haywire. see: https://developer.mozilla.org/en/security/csp
		if ( !el.attachevent ) {
			return true;
		}

		var issupported = (eventname in el);
		if ( !issupported ) {
			el.setattribute(eventname, "return;");
			issupported = typeof el[eventname] === "function";
		}
		el = null;

		return issupported;
	};

	jquery.support.submitbubbles = eventsupported("submit");
	jquery.support.changebubbles = eventsupported("change");

	// release memory in ie
	div = all = a = null;
})();



var rbrace = /^(?:\{.*\}|\[.*\])$/;

jquery.extend({
	cache: {},

	// please use with caution
	uuid: 0,

	// unique for each copy of jquery on the page
	// non-digits removed to match rinlinejquery
	expando: "jquery" + ( jquery.fn.jquery + math.random() ).replace( /\d/g, "" ),

	// the following elements throw uncatchable exceptions if you
	// attempt to add expando properties to them.
	nodata: {
		"embed": true,
		// ban all objects except for flash (which handle expandos)
		"object": "clsid:d27cdb6e-ae6d-11cf-96b8-444553540000",
		"applet": true
	},

	hasdata: function( elem ) {
		elem = elem.nodetype ? jquery.cache[ elem[jquery.expando] ] : elem[ jquery.expando ];

		return !!elem && !isemptydataobject( elem );
	},

	data: function( elem, name, data, pvt /* internal use only */ ) {
		if ( !jquery.acceptdata( elem ) ) {
			return;
		}

		var internalkey = jquery.expando, getbyname = typeof name === "string", thiscache,

			// we have to handle dom nodes and js objects differently because ie6-7
			// can't gc object references properly across the dom-js boundary
			isnode = elem.nodetype,

			// only dom nodes need the global jquery cache; js object data is
			// attached directly to the object so gc can occur automatically
			cache = isnode ? jquery.cache : elem,

			// only defining an id for js objects if its cache already exists allows
			// the code to shortcut on the same path as a dom node with no cache
			id = isnode ? elem[ jquery.expando ] : elem[ jquery.expando ] && jquery.expando;

		// avoid doing any more work than we need to when trying to get data on an
		// object that has no data at all
		if ( (!id || (pvt && id && !cache[ id ][ internalkey ])) && getbyname && data === undefined ) {
			return;
		}

		if ( !id ) {
			// only dom nodes need a new unique id for each element since their data
			// ends up in the global cache
			if ( isnode ) {
				elem[ jquery.expando ] = id = ++jquery.uuid;
			} else {
				id = jquery.expando;
			}
		}

		if ( !cache[ id ] ) {
			cache[ id ] = {};

			// todo: this is a hack for 1.5 only. avoids exposing jquery
			// metadata on plain js objects when the object is serialized using
			// json.stringify
			if ( !isnode ) {
				cache[ id ].tojson = jquery.noop;
			}
		}

		// an object can be passed to jquery.data instead of a key/value pair; this gets
		// shallow copied over onto the existing cache
		if ( typeof name === "object" || typeof name === "function" ) {
			if ( pvt ) {
				cache[ id ][ internalkey ] = jquery.extend(cache[ id ][ internalkey ], name);
			} else {
				cache[ id ] = jquery.extend(cache[ id ], name);
			}
		}

		thiscache = cache[ id ];

		// internal jquery data is stored in a separate object inside the object's data
		// cache in order to avoid key collisions between internal data and user-defined
		// data
		if ( pvt ) {
			if ( !thiscache[ internalkey ] ) {
				thiscache[ internalkey ] = {};
			}

			thiscache = thiscache[ internalkey ];
		}

		if ( data !== undefined ) {
			thiscache[ name ] = data;
		}

		// todo: this is a hack for 1.5 only. it will be removed in 1.6. users should
		// not attempt to inspect the internal events object using jquery.data, as this
		// internal data object is undocumented and subject to change.
		if ( name === "events" && !thiscache[name] ) {
			return thiscache[ internalkey ] && thiscache[ internalkey ].events;
		}

		return getbyname ? thiscache[ name ] : thiscache;
	},

	removedata: function( elem, name, pvt /* internal use only */ ) {
		if ( !jquery.acceptdata( elem ) ) {
			return;
		}

		var internalkey = jquery.expando, isnode = elem.nodetype,

			// see jquery.data for more information
			cache = isnode ? jquery.cache : elem,

			// see jquery.data for more information
			id = isnode ? elem[ jquery.expando ] : jquery.expando;

		// if there is already no cache entry for this object, there is no
		// purpose in continuing
		if ( !cache[ id ] ) {
			return;
		}

		if ( name ) {
			var thiscache = pvt ? cache[ id ][ internalkey ] : cache[ id ];

			if ( thiscache ) {
				delete thiscache[ name ];

				// if there is no data left in the cache, we want to continue
				// and let the cache object itself get destroyed
				if ( !isemptydataobject(thiscache) ) {
					return;
				}
			}
		}

		// see jquery.data for more information
		if ( pvt ) {
			delete cache[ id ][ internalkey ];

			// don't destroy the parent cache unless the internal data object
			// had been the only thing left in it
			if ( !isemptydataobject(cache[ id ]) ) {
				return;
			}
		}

		var internalcache = cache[ id ][ internalkey ];

		// browsers that fail expando deletion also refuse to delete expandos on
		// the window, but it will allow it on all other js objects; other browsers
		// don't care
		if ( jquery.support.deleteexpando || cache != window ) {
			delete cache[ id ];
		} else {
			cache[ id ] = null;
		}

		// we destroyed the entire user cache at once because it's faster than
		// iterating through each key, but we need to continue to persist internal
		// data if it existed
		if ( internalcache ) {
			cache[ id ] = {};
			// todo: this is a hack for 1.5 only. avoids exposing jquery
			// metadata on plain js objects when the object is serialized using
			// json.stringify
			if ( !isnode ) {
				cache[ id ].tojson = jquery.noop;
			}

			cache[ id ][ internalkey ] = internalcache;

		// otherwise, we need to eliminate the expando on the node to avoid
		// false lookups in the cache for entries that no longer exist
		} else if ( isnode ) {
			// ie does not allow us to delete expando properties from nodes,
			// nor does it have a removeattribute function on document nodes;
			// we must handle all of these cases
			if ( jquery.support.deleteexpando ) {
				delete elem[ jquery.expando ];
			} else if ( elem.removeattribute ) {
				elem.removeattribute( jquery.expando );
			} else {
				elem[ jquery.expando ] = null;
			}
		}
	},

	// for internal use only.
	_data: function( elem, name, data ) {
		return jquery.data( elem, name, data, true );
	},

	// a method for determining if a dom node can handle the data expando
	acceptdata: function( elem ) {
		if ( elem.nodename ) {
			var match = jquery.nodata[ elem.nodename.tolowercase() ];

			if ( match ) {
				return !(match === true || elem.getattribute("classid") !== match);
			}
		}

		return true;
	}
});

jquery.fn.extend({
	data: function( key, value ) {
		var data = null;

		if ( typeof key === "undefined" ) {
			if ( this.length ) {
				data = jquery.data( this[0] );

				if ( this[0].nodetype === 1 ) {
					var attr = this[0].attributes, name;
					for ( var i = 0, l = attr.length; i < l; i++ ) {
						name = attr[i].name;

						if ( name.indexof( "data-" ) === 0 ) {
							name = name.substr( 5 );
							dataattr( this[0], name, data[ name ] );
						}
					}
				}
			}

			return data;

		} else if ( typeof key === "object" ) {
			return this.each(function() {
				jquery.data( this, key );
			});
		}

		var parts = key.split(".");
		parts[1] = parts[1] ? "." + parts[1] : "";

		if ( value === undefined ) {
			data = this.triggerhandler("getdata" + parts[1] + "!", [parts[0]]);

			// try to fetch any internally stored data first
			if ( data === undefined && this.length ) {
				data = jquery.data( this[0], key );
				data = dataattr( this[0], key, data );
			}

			return data === undefined && parts[1] ?
				this.data( parts[0] ) :
				data;

		} else {
			return this.each(function() {
				var $this = jquery( this ),
					args = [ parts[0], value ];

				$this.triggerhandler( "setdata" + parts[1] + "!", args );
				jquery.data( this, key, value );
				$this.triggerhandler( "changedata" + parts[1] + "!", args );
			});
		}
	},

	removedata: function( key ) {
		return this.each(function() {
			jquery.removedata( this, key );
		});
	}
});

function dataattr( elem, key, data ) {
	// if nothing was found internally, try to fetch any
	// data from the html5 data-* attribute
	if ( data === undefined && elem.nodetype === 1 ) {
		data = elem.getattribute( "data-" + key );

		if ( typeof data === "string" ) {
			try {
				data = data === "true" ? true :
				data === "false" ? false :
				data === "null" ? null :
				!jquery.isnan( data ) ? parsefloat( data ) :
					rbrace.test( data ) ? jquery.parsejson( data ) :
					data;
			} catch( e ) {}

			// make sure we set the data so it isn't changed later
			jquery.data( elem, key, data );

		} else {
			data = undefined;
		}
	}

	return data;
}

// todo: this is a hack for 1.5 only to allow objects with a single tojson
// property to be considered empty objects; this property always exists in
// order to make sure json.stringify does not expose internal metadata
function isemptydataobject( obj ) {
	for ( var name in obj ) {
		if ( name !== "tojson" ) {
			return false;
		}
	}

	return true;
}




jquery.extend({
	queue: function( elem, type, data ) {
		if ( !elem ) {
			return;
		}

		type = (type || "fx") + "queue";
		var q = jquery._data( elem, type );

		// speed up dequeue by getting out quickly if this is just a lookup
		if ( !data ) {
			return q || [];
		}

		if ( !q || jquery.isarray(data) ) {
			q = jquery._data( elem, type, jquery.makearray(data) );

		} else {
			q.push( data );
		}

		return q;
	},

	dequeue: function( elem, type ) {
		type = type || "fx";

		var queue = jquery.queue( elem, type ),
			fn = queue.shift();

		// if the fx queue is dequeued, always remove the progress sentinel
		if ( fn === "inprogress" ) {
			fn = queue.shift();
		}

		if ( fn ) {
			// add a progress sentinel to prevent the fx queue from being
			// automatically dequeued
			if ( type === "fx" ) {
				queue.unshift("inprogress");
			}

			fn.call(elem, function() {
				jquery.dequeue(elem, type);
			});
		}

		if ( !queue.length ) {
			jquery.removedata( elem, type + "queue", true );
		}
	}
});

jquery.fn.extend({
	queue: function( type, data ) {
		if ( typeof type !== "string" ) {
			data = type;
			type = "fx";
		}

		if ( data === undefined ) {
			return jquery.queue( this[0], type );
		}
		return this.each(function( i ) {
			var queue = jquery.queue( this, type, data );

			if ( type === "fx" && queue[0] !== "inprogress" ) {
				jquery.dequeue( this, type );
			}
		});
	},
	dequeue: function( type ) {
		return this.each(function() {
			jquery.dequeue( this, type );
		});
	},

	// based off of the plugin by clint helfers, with permission.
	// http://blindsignals.com/index.php/2009/07/jquery-delay/
	delay: function( time, type ) {
		time = jquery.fx ? jquery.fx.speeds[time] || time : time;
		type = type || "fx";

		return this.queue( type, function() {
			var elem = this;
			settimeout(function() {
				jquery.dequeue( elem, type );
			}, time );
		});
	},

	clearqueue: function( type ) {
		return this.queue( type || "fx", [] );
	}
});




var rclass = /[\n\t\r]/g,
	rspaces = /\s+/,
	rreturn = /\r/g,
	rspecialurl = /^(?:href|src|style)$/,
	rtype = /^(?:button|input)$/i,
	rfocusable = /^(?:button|input|object|select|textarea)$/i,
	rclickable = /^a(?:rea)?$/i,
	rradiocheck = /^(?:radio|checkbox)$/i;

jquery.props = {
	"for": "htmlfor",
	"class": "classname",
	readonly: "readonly",
	maxlength: "maxlength",
	cellspacing: "cellspacing",
	rowspan: "rowspan",
	colspan: "colspan",
	tabindex: "tabindex",
	usemap: "usemap",
	frameborder: "frameborder"
};

jquery.fn.extend({
	attr: function( name, value ) {
		return jquery.access( this, name, value, true, jquery.attr );
	},

	removeattr: function( name, fn ) {
		return this.each(function(){
			jquery.attr( this, name, "" );
			if ( this.nodetype === 1 ) {
				this.removeattribute( name );
			}
		});
	},

	addclass: function( value ) {
		if ( jquery.isfunction(value) ) {
			return this.each(function(i) {
				var self = jquery(this);
				self.addclass( value.call(this, i, self.attr("class")) );
			});
		}

		if ( value && typeof value === "string" ) {
			var classnames = (value || "").split( rspaces );

			for ( var i = 0, l = this.length; i < l; i++ ) {
				var elem = this[i];

				if ( elem.nodetype === 1 ) {
					if ( !elem.classname ) {
						elem.classname = value;

					} else {
						var classname = " " + elem.classname + " ",
							setclass = elem.classname;

						for ( var c = 0, cl = classnames.length; c < cl; c++ ) {
							if ( classname.indexof( " " + classnames[c] + " " ) < 0 ) {
								setclass += " " + classnames[c];
							}
						}
						elem.classname = jquery.trim( setclass );
					}
				}
			}
		}

		return this;
	},

	removeclass: function( value ) {
		if ( jquery.isfunction(value) ) {
			return this.each(function(i) {
				var self = jquery(this);
				self.removeclass( value.call(this, i, self.attr("class")) );
			});
		}

		if ( (value && typeof value === "string") || value === undefined ) {
			var classnames = (value || "").split( rspaces );

			for ( var i = 0, l = this.length; i < l; i++ ) {
				var elem = this[i];

				if ( elem.nodetype === 1 && elem.classname ) {
					if ( value ) {
						var classname = (" " + elem.classname + " ").replace(rclass, " ");
						for ( var c = 0, cl = classnames.length; c < cl; c++ ) {
							classname = classname.replace(" " + classnames[c] + " ", " ");
						}
						elem.classname = jquery.trim( classname );

					} else {
						elem.classname = "";
					}
				}
			}
		}

		return this;
	},

	toggleclass: function( value, stateval ) {
		var type = typeof value,
			isbool = typeof stateval === "boolean";

		if ( jquery.isfunction( value ) ) {
			return this.each(function(i) {
				var self = jquery(this);
				self.toggleclass( value.call(this, i, self.attr("class"), stateval), stateval );
			});
		}

		return this.each(function() {
			if ( type === "string" ) {
				// toggle individual class names
				var classname,
					i = 0,
					self = jquery( this ),
					state = stateval,
					classnames = value.split( rspaces );

				while ( (classname = classnames[ i++ ]) ) {
					// check each classname given, space seperated list
					state = isbool ? state : !self.hasclass( classname );
					self[ state ? "addclass" : "removeclass" ]( classname );
				}

			} else if ( type === "undefined" || type === "boolean" ) {
				if ( this.classname ) {
					// store classname if set
					jquery._data( this, "__classname__", this.classname );
				}

				// toggle whole classname
				this.classname = this.classname || value === false ? "" : jquery._data( this, "__classname__" ) || "";
			}
		});
	},

	hasclass: function( selector ) {
		var classname = " " + selector + " ";
		for ( var i = 0, l = this.length; i < l; i++ ) {
			if ( (" " + this[i].classname + " ").replace(rclass, " ").indexof( classname ) > -1 ) {
				return true;
			}
		}

		return false;
	},

	val: function( value ) {
		if ( !arguments.length ) {
			var elem = this[0];

			if ( elem ) {
				if ( jquery.nodename( elem, "option" ) ) {
					// attributes.value is undefined in blackberry 4.7 but
					// uses .value. see #6932
					var val = elem.attributes.value;
					return !val || val.specified ? elem.value : elem.text;
				}

				// we need to handle select boxes special
				if ( jquery.nodename( elem, "select" ) ) {
					var index = elem.selectedindex,
						values = [],
						options = elem.options,
						one = elem.type === "select-one";

					// nothing was selected
					if ( index < 0 ) {
						return null;
					}

					// loop through all the selected options
					for ( var i = one ? index : 0, max = one ? index + 1 : options.length; i < max; i++ ) {
						var option = options[ i ];

						// don't return options that are disabled or in a disabled optgroup
						if ( option.selected && (jquery.support.optdisabled ? !option.disabled : option.getattribute("disabled") === null) &&
								(!option.parentnode.disabled || !jquery.nodename( option.parentnode, "optgroup" )) ) {

							// get the specific value for the option
							value = jquery(option).val();

							// we don't need an array for one selects
							if ( one ) {
								return value;
							}

							// multi-selects return an array
							values.push( value );
						}
					}

					// fixes bug #2551 -- select.val() broken in ie after form.reset()
					if ( one && !values.length && options.length ) {
						return jquery( options[ index ] ).val();
					}

					return values;
				}

				// handle the case where in webkit "" is returned instead of "on" if a value isn't specified
				if ( rradiocheck.test( elem.type ) && !jquery.support.checkon ) {
					return elem.getattribute("value") === null ? "on" : elem.value;
				}

				// everything else, we just grab the value
				return (elem.value || "").replace(rreturn, "");

			}

			return undefined;
		}

		var isfunction = jquery.isfunction(value);

		return this.each(function(i) {
			var self = jquery(this), val = value;

			if ( this.nodetype !== 1 ) {
				return;
			}

			if ( isfunction ) {
				val = value.call(this, i, self.val());
			}

			// treat null/undefined as ""; convert numbers to string
			if ( val == null ) {
				val = "";
			} else if ( typeof val === "number" ) {
				val += "";
			} else if ( jquery.isarray(val) ) {
				val = jquery.map(val, function (value) {
					return value == null ? "" : value + "";
				});
			}

			if ( jquery.isarray(val) && rradiocheck.test( this.type ) ) {
				this.checked = jquery.inarray( self.val(), val ) >= 0;

			} else if ( jquery.nodename( this, "select" ) ) {
				var values = jquery.makearray(val);

				jquery( "option", this ).each(function() {
					this.selected = jquery.inarray( jquery(this).val(), values ) >= 0;
				});

				if ( !values.length ) {
					this.selectedindex = -1;
				}

			} else {
				this.value = val;
			}
		});
	}
});

jquery.extend({
	attrfn: {
		val: true,
		css: true,
		html: true,
		text: true,
		data: true,
		width: true,
		height: true,
		offset: true
	},

	attr: function( elem, name, value, pass ) {
		// don't get/set attributes on text, comment and attribute nodes
		if ( !elem || elem.nodetype === 3 || elem.nodetype === 8 || elem.nodetype === 2 ) {
			return undefined;
		}

		if ( pass && name in jquery.attrfn ) {
			return jquery(elem)[name](value);
		}

		var notxml = elem.nodetype !== 1 || !jquery.isxmldoc( elem ),
			// whether we are setting (or getting)
			set = value !== undefined;

		// try to normalize/fix the name
		name = notxml && jquery.props[ name ] || name;

		// only do all the following if this is a node (faster for style)
		if ( elem.nodetype === 1 ) {
			// these attributes require special treatment
			var special = rspecialurl.test( name );

			// safari mis-reports the default selected property of an option
			// accessing the parent's selectedindex property fixes it
			if ( name === "selected" && !jquery.support.optselected ) {
				var parent = elem.parentnode;
				if ( parent ) {
					parent.selectedindex;

					// make sure that it also works with optgroups, see #5701
					if ( parent.parentnode ) {
						parent.parentnode.selectedindex;
					}
				}
			}

			// if applicable, access the attribute via the dom 0 way
			// 'in' checks fail in blackberry 4.7 #6931
			if ( (name in elem || elem[ name ] !== undefined) && notxml && !special ) {
				if ( set ) {
					// we can't allow the type property to be changed (since it causes problems in ie)
					if ( name === "type" && rtype.test( elem.nodename ) && elem.parentnode ) {
						jquery.error( "type property can't be changed" );
					}

					if ( value === null ) {
						if ( elem.nodetype === 1 ) {
							elem.removeattribute( name );
						}

					} else {
						elem[ name ] = value;
					}
				}

				// browsers index elements by id/name on forms, give priority to attributes.
				if ( jquery.nodename( elem, "form" ) && elem.getattributenode(name) ) {
					return elem.getattributenode( name ).nodevalue;
				}

				// elem.tabindex doesn't always return the correct value when it hasn't been explicitly set
				// http://fluidproject.org/blog/2008/01/09/getting-setting-and-removing-tabindex-values-with-javascript/
				if ( name === "tabindex" ) {
					var attributenode = elem.getattributenode( "tabindex" );

					return attributenode && attributenode.specified ?
						attributenode.value :
						rfocusable.test( elem.nodename ) || rclickable.test( elem.nodename ) && elem.href ?
							0 :
							undefined;
				}

				return elem[ name ];
			}

			if ( !jquery.support.style && notxml && name === "style" ) {
				if ( set ) {
					elem.style.csstext = "" + value;
				}

				return elem.style.csstext;
			}

			if ( set ) {
				// convert the value to a string (all browsers do this but ie) see #1070
				elem.setattribute( name, "" + value );
			}

			// ensure that missing attributes return undefined
			// blackberry 4.7 returns "" from getattribute #6938
			if ( !elem.attributes[ name ] && (elem.hasattribute && !elem.hasattribute( name )) ) {
				return undefined;
			}

			var attr = !jquery.support.hrefnormalized && notxml && special ?
					// some attributes require a special call on ie
					elem.getattribute( name, 2 ) :
					elem.getattribute( name );

			// non-existent attributes return null, we normalize to undefined
			return attr === null ? undefined : attr;
		}
		// handle everything which isn't a dom element node
		if ( set ) {
			elem[ name ] = value;
		}
		return elem[ name ];
	}
});




var rnamespaces = /\.(.*)$/,
	rformelems = /^(?:textarea|input|select)$/i,
	rperiod = /\./g,
	rspace = / /g,
	rescape = /[^\w\s.|`]/g,
	fcleanup = function( nm ) {
		return nm.replace(rescape, "\\$&");
	};

/*
 * a number of helper functions used for managing events.
 * many of the ideas behind this code originated from
 * dean edwards' addevent library.
 */
jquery.event = {

	// bind an event to an element
	// original by dean edwards
	add: function( elem, types, handler, data ) {
		if ( elem.nodetype === 3 || elem.nodetype === 8 ) {
			return;
		}

		// todo :: use a try/catch until it's safe to pull this out (likely 1.6)
		// minor release fix for bug #8018
		try {
			// for whatever reason, ie has trouble passing the window object
			// around, causing it to be cloned in the process
			if ( jquery.iswindow( elem ) && ( elem !== window && !elem.frameelement ) ) {
				elem = window;
			}
		}
		catch ( e ) {}

		if ( handler === false ) {
			handler = returnfalse;
		} else if ( !handler ) {
			// fixes bug #7229. fix recommended by jdalton
			return;
		}

		var handleobjin, handleobj;

		if ( handler.handler ) {
			handleobjin = handler;
			handler = handleobjin.handler;
		}

		// make sure that the function being executed has a unique id
		if ( !handler.guid ) {
			handler.guid = jquery.guid++;
		}

		// init the element's event structure
		var elemdata = jquery._data( elem );

		// if no elemdata is found then we must be trying to bind to one of the
		// banned nodata elements
		if ( !elemdata ) {
			return;
		}

		var events = elemdata.events,
			eventhandle = elemdata.handle;

		if ( !events ) {
			elemdata.events = events = {};
		}

		if ( !eventhandle ) {
			elemdata.handle = eventhandle = function() {
				// handle the second event of a trigger and when
				// an event is called after a page has unloaded
				return typeof jquery !== "undefined" && !jquery.event.triggered ?
					jquery.event.handle.apply( eventhandle.elem, arguments ) :
					undefined;
			};
		}

		// add elem as a property of the handle function
		// this is to prevent a memory leak with non-native events in ie.
		eventhandle.elem = elem;

		// handle multiple events separated by a space
		// jquery(...).bind("mouseover mouseout", fn);
		types = types.split(" ");

		var type, i = 0, namespaces;

		while ( (type = types[ i++ ]) ) {
			handleobj = handleobjin ?
				jquery.extend({}, handleobjin) :
				{ handler: handler, data: data };

			// namespaced event handlers
			if ( type.indexof(".") > -1 ) {
				namespaces = type.split(".");
				type = namespaces.shift();
				handleobj.namespace = namespaces.slice(0).sort().join(".");

			} else {
				namespaces = [];
				handleobj.namespace = "";
			}

			handleobj.type = type;
			if ( !handleobj.guid ) {
				handleobj.guid = handler.guid;
			}

			// get the current list of functions bound to this event
			var handlers = events[ type ],
				special = jquery.event.special[ type ] || {};

			// init the event handler queue
			if ( !handlers ) {
				handlers = events[ type ] = [];

				// check for a special event handler
				// only use addeventlistener/attachevent if the special
				// events handler returns false
				if ( !special.setup || special.setup.call( elem, data, namespaces, eventhandle ) === false ) {
					// bind the global event handler to the element
					if ( elem.addeventlistener ) {
						elem.addeventlistener( type, eventhandle, false );

					} else if ( elem.attachevent ) {
						elem.attachevent( "on" + type, eventhandle );
					}
				}
			}

			if ( special.add ) {
				special.add.call( elem, handleobj );

				if ( !handleobj.handler.guid ) {
					handleobj.handler.guid = handler.guid;
				}
			}

			// add the function to the element's handler list
			handlers.push( handleobj );

			// keep track of which events have been used, for global triggering
			jquery.event.global[ type ] = true;
		}

		// nullify elem to prevent memory leaks in ie
		elem = null;
	},

	global: {},

	// detach an event or set of events from an element
	remove: function( elem, types, handler, pos ) {
		// don't do events on text and comment nodes
		if ( elem.nodetype === 3 || elem.nodetype === 8 ) {
			return;
		}

		if ( handler === false ) {
			handler = returnfalse;
		}

		var ret, type, fn, j, i = 0, all, namespaces, namespace, special, eventtype, handleobj, origtype,
			elemdata = jquery.hasdata( elem ) && jquery._data( elem ),
			events = elemdata && elemdata.events;

		if ( !elemdata || !events ) {
			return;
		}

		// types is actually an event object here
		if ( types && types.type ) {
			handler = types.handler;
			types = types.type;
		}

		// unbind all events for the element
		if ( !types || typeof types === "string" && types.charat(0) === "." ) {
			types = types || "";

			for ( type in events ) {
				jquery.event.remove( elem, type + types );
			}

			return;
		}

		// handle multiple events separated by a space
		// jquery(...).unbind("mouseover mouseout", fn);
		types = types.split(" ");

		while ( (type = types[ i++ ]) ) {
			origtype = type;
			handleobj = null;
			all = type.indexof(".") < 0;
			namespaces = [];

			if ( !all ) {
				// namespaced event handlers
				namespaces = type.split(".");
				type = namespaces.shift();

				namespace = new regexp("(^|\\.)" +
					jquery.map( namespaces.slice(0).sort(), fcleanup ).join("\\.(?:.*\\.)?") + "(\\.|$)");
			}

			eventtype = events[ type ];

			if ( !eventtype ) {
				continue;
			}

			if ( !handler ) {
				for ( j = 0; j < eventtype.length; j++ ) {
					handleobj = eventtype[ j ];

					if ( all || namespace.test( handleobj.namespace ) ) {
						jquery.event.remove( elem, origtype, handleobj.handler, j );
						eventtype.splice( j--, 1 );
					}
				}

				continue;
			}

			special = jquery.event.special[ type ] || {};

			for ( j = pos || 0; j < eventtype.length; j++ ) {
				handleobj = eventtype[ j ];

				if ( handler.guid === handleobj.guid ) {
					// remove the given handler for the given type
					if ( all || namespace.test( handleobj.namespace ) ) {
						if ( pos == null ) {
							eventtype.splice( j--, 1 );
						}

						if ( special.remove ) {
							special.remove.call( elem, handleobj );
						}
					}

					if ( pos != null ) {
						break;
					}
				}
			}

			// remove generic event handler if no more handlers exist
			if ( eventtype.length === 0 || pos != null && eventtype.length === 1 ) {
				if ( !special.teardown || special.teardown.call( elem, namespaces ) === false ) {
					jquery.removeevent( elem, type, elemdata.handle );
				}

				ret = null;
				delete events[ type ];
			}
		}

		// remove the expando if it's no longer used
		if ( jquery.isemptyobject( events ) ) {
			var handle = elemdata.handle;
			if ( handle ) {
				handle.elem = null;
			}

			delete elemdata.events;
			delete elemdata.handle;

			if ( jquery.isemptyobject( elemdata ) ) {
				jquery.removedata( elem, undefined, true );
			}
		}
	},

	// bubbling is internal
	trigger: function( event, data, elem /*, bubbling */ ) {
		// event object or event type
		var type = event.type || event,
			bubbling = arguments[3];

		if ( !bubbling ) {
			event = typeof event === "object" ?
				// jquery.event object
				event[ jquery.expando ] ? event :
				// object literal
				jquery.extend( jquery.event(type), event ) :
				// just the event type (string)
				jquery.event(type);

			if ( type.indexof("!") >= 0 ) {
				event.type = type = type.slice(0, -1);
				event.exclusive = true;
			}

			// handle a global trigger
			if ( !elem ) {
				// don't bubble custom events when global (to avoid too much overhead)
				event.stoppropagation();

				// only trigger if we've ever bound an event for it
				if ( jquery.event.global[ type ] ) {
					// xxx this code smells terrible. event.js should not be directly
					// inspecting the data cache
					jquery.each( jquery.cache, function() {
						// internalkey variable is just used to make it easier to find
						// and potentially change this stuff later; currently it just
						// points to jquery.expando
						var internalkey = jquery.expando,
							internalcache = this[ internalkey ];
						if ( internalcache && internalcache.events && internalcache.events[ type ] ) {
							jquery.event.trigger( event, data, internalcache.handle.elem );
						}
					});
				}
			}

			// handle triggering a single element

			// don't do events on text and comment nodes
			if ( !elem || elem.nodetype === 3 || elem.nodetype === 8 ) {
				return undefined;
			}

			// clean up in case it is reused
			event.result = undefined;
			event.target = elem;

			// clone the incoming data, if any
			data = jquery.makearray( data );
			data.unshift( event );
		}

		event.currenttarget = elem;

		// trigger the event, it is assumed that "handle" is a function
		var handle = jquery._data( elem, "handle" );

		if ( handle ) {
			handle.apply( elem, data );
		}

		var parent = elem.parentnode || elem.ownerdocument;

		// trigger an inline bound script
		try {
			if ( !(elem && elem.nodename && jquery.nodata[elem.nodename.tolowercase()]) ) {
				if ( elem[ "on" + type ] && elem[ "on" + type ].apply( elem, data ) === false ) {
					event.result = false;
					event.preventdefault();
				}
			}

		// prevent ie from throwing an error for some elements with some event types, see #3533
		} catch (inlineerror) {}

		if ( !event.ispropagationstopped() && parent ) {
			jquery.event.trigger( event, data, parent, true );

		} else if ( !event.isdefaultprevented() ) {
			var old,
				target = event.target,
				targettype = type.replace( rnamespaces, "" ),
				isclick = jquery.nodename( target, "a" ) && targettype === "click",
				special = jquery.event.special[ targettype ] || {};

			if ( (!special._default || special._default.call( elem, event ) === false) &&
				!isclick && !(target && target.nodename && jquery.nodata[target.nodename.tolowercase()]) ) {

				try {
					if ( target[ targettype ] ) {
						// make sure that we don't accidentally re-trigger the onfoo events
						old = target[ "on" + targettype ];

						if ( old ) {
							target[ "on" + targettype ] = null;
						}

						jquery.event.triggered = true;
						target[ targettype ]();
					}

				// prevent ie from throwing an error for some elements with some event types, see #3533
				} catch (triggererror) {}

				if ( old ) {
					target[ "on" + targettype ] = old;
				}

				jquery.event.triggered = false;
			}
		}
	},

	handle: function( event ) {
		var all, handlers, namespaces, namespace_re, events,
			namespace_sort = [],
			args = jquery.makearray( arguments );

		event = args[0] = jquery.event.fix( event || window.event );
		event.currenttarget = this;

		// namespaced event handlers
		all = event.type.indexof(".") < 0 && !event.exclusive;

		if ( !all ) {
			namespaces = event.type.split(".");
			event.type = namespaces.shift();
			namespace_sort = namespaces.slice(0).sort();
			namespace_re = new regexp("(^|\\.)" + namespace_sort.join("\\.(?:.*\\.)?") + "(\\.|$)");
		}

		event.namespace = event.namespace || namespace_sort.join(".");

		events = jquery._data(this, "events");

		handlers = (events || {})[ event.type ];

		if ( events && handlers ) {
			// clone the handlers to prevent manipulation
			handlers = handlers.slice(0);

			for ( var j = 0, l = handlers.length; j < l; j++ ) {
				var handleobj = handlers[ j ];

				// filter the functions by class
				if ( all || namespace_re.test( handleobj.namespace ) ) {
					// pass in a reference to the handler function itself
					// so that we can later remove it
					event.handler = handleobj.handler;
					event.data = handleobj.data;
					event.handleobj = handleobj;

					var ret = handleobj.handler.apply( this, args );

					if ( ret !== undefined ) {
						event.result = ret;
						if ( ret === false ) {
							event.preventdefault();
							event.stoppropagation();
						}
					}

					if ( event.isimmediatepropagationstopped() ) {
						break;
					}
				}
			}
		}

		return event.result;
	},

	props: "altkey attrchange attrname bubbles button cancelable charcode clientx clienty ctrlkey currenttarget data detail eventphase fromelement handler keycode layerx layery metakey newvalue offsetx offsety pagex pagey prevvalue relatednode relatedtarget screenx screeny shiftkey srcelement target toelement view wheeldelta which".split(" "),

	fix: function( event ) {
		if ( event[ jquery.expando ] ) {
			return event;
		}

		// store a copy of the original event object
		// and "clone" to set read-only properties
		var originalevent = event;
		event = jquery.event( originalevent );

		for ( var i = this.props.length, prop; i; ) {
			prop = this.props[ --i ];
			event[ prop ] = originalevent[ prop ];
		}

		// fix target property, if necessary
		if ( !event.target ) {
			// fixes #1925 where srcelement might not be defined either
			event.target = event.srcelement || document;
		}

		// check if target is a textnode (safari)
		if ( event.target.nodetype === 3 ) {
			event.target = event.target.parentnode;
		}

		// add relatedtarget, if necessary
		if ( !event.relatedtarget && event.fromelement ) {
			event.relatedtarget = event.fromelement === event.target ? event.toelement : event.fromelement;
		}

		// calculate pagex/y if missing and clientx/y available
		if ( event.pagex == null && event.clientx != null ) {
			var doc = document.documentelement,
				body = document.body;

			event.pagex = event.clientx + (doc && doc.scrollleft || body && body.scrollleft || 0) - (doc && doc.clientleft || body && body.clientleft || 0);
			event.pagey = event.clienty + (doc && doc.scrolltop  || body && body.scrolltop  || 0) - (doc && doc.clienttop  || body && body.clienttop  || 0);
		}

		// add which for key events
		if ( event.which == null && (event.charcode != null || event.keycode != null) ) {
			event.which = event.charcode != null ? event.charcode : event.keycode;
		}

		// add metakey to non-mac browsers (use ctrl for pc's and meta for macs)
		if ( !event.metakey && event.ctrlkey ) {
			event.metakey = event.ctrlkey;
		}

		// add which for click: 1 === left; 2 === middle; 3 === right
		// note: button is not normalized, so don't use it
		if ( !event.which && event.button !== undefined ) {
			event.which = (event.button & 1 ? 1 : ( event.button & 2 ? 3 : ( event.button & 4 ? 2 : 0 ) ));
		}

		return event;
	},

	// deprecated, use jquery.guid instead
	guid: 1e8,

	// deprecated, use jquery.proxy instead
	proxy: jquery.proxy,

	special: {
		ready: {
			// make sure the ready event is setup
			setup: jquery.bindready,
			teardown: jquery.noop
		},

		live: {
			add: function( handleobj ) {
				jquery.event.add( this,
					liveconvert( handleobj.origtype, handleobj.selector ),
					jquery.extend({}, handleobj, {handler: livehandler, guid: handleobj.handler.guid}) );
			},

			remove: function( handleobj ) {
				jquery.event.remove( this, liveconvert( handleobj.origtype, handleobj.selector ), handleobj );
			}
		},

		beforeunload: {
			setup: function( data, namespaces, eventhandle ) {
				// we only want to do this special case on windows
				if ( jquery.iswindow( this ) ) {
					this.onbeforeunload = eventhandle;
				}
			},

			teardown: function( namespaces, eventhandle ) {
				if ( this.onbeforeunload === eventhandle ) {
					this.onbeforeunload = null;
				}
			}
		}
	}
};

jquery.removeevent = document.removeeventlistener ?
	function( elem, type, handle ) {
		if ( elem.removeeventlistener ) {
			elem.removeeventlistener( type, handle, false );
		}
	} :
	function( elem, type, handle ) {
		if ( elem.detachevent ) {
			elem.detachevent( "on" + type, handle );
		}
	};

jquery.event = function( src ) {
	// allow instantiation without the 'new' keyword
	if ( !this.preventdefault ) {
		return new jquery.event( src );
	}

	// event object
	if ( src && src.type ) {
		this.originalevent = src;
		this.type = src.type;

		// events bubbling up the document may have been marked as prevented
		// by a handler lower down the tree; reflect the correct value.
		this.isdefaultprevented = (src.defaultprevented || src.returnvalue === false ||
			src.getpreventdefault && src.getpreventdefault()) ? returntrue : returnfalse;

	// event type
	} else {
		this.type = src;
	}

	// timestamp is buggy for some events on firefox(#3843)
	// so we won't rely on the native value
	this.timestamp = jquery.now();

	// mark it as fixed
	this[ jquery.expando ] = true;
};

function returnfalse() {
	return false;
}
function returntrue() {
	return true;
}

// jquery.event is based on dom3 events as specified by the ecmascript language binding
// http://www.w3.org/tr/2003/wd-dom-level-3-events-20030331/ecma-script-binding.html
jquery.event.prototype = {
	preventdefault: function() {
		this.isdefaultprevented = returntrue;

		var e = this.originalevent;
		if ( !e ) {
			return;
		}

		// if preventdefault exists run it on the original event
		if ( e.preventdefault ) {
			e.preventdefault();

		// otherwise set the returnvalue property of the original event to false (ie)
		} else {
			e.returnvalue = false;
		}
	},
	stoppropagation: function() {
		this.ispropagationstopped = returntrue;

		var e = this.originalevent;
		if ( !e ) {
			return;
		}
		// if stoppropagation exists run it on the original event
		if ( e.stoppropagation ) {
			e.stoppropagation();
		}
		// otherwise set the cancelbubble property of the original event to true (ie)
		e.cancelbubble = true;
	},
	stopimmediatepropagation: function() {
		this.isimmediatepropagationstopped = returntrue;
		this.stoppropagation();
	},
	isdefaultprevented: returnfalse,
	ispropagationstopped: returnfalse,
	isimmediatepropagationstopped: returnfalse
};

// checks if an event happened on an element within another element
// used in jquery.event.special.mouseenter and mouseleave handlers
var withinelement = function( event ) {
	// check if mouse(over|out) are still within the same parent element
	var parent = event.relatedtarget;

	// firefox sometimes assigns relatedtarget a xul element
	// which we cannot access the parentnode property of
	try {

		// chrome does something similar, the parentnode property
		// can be accessed but is null.
		if ( parent !== document && !parent.parentnode ) {
			return;
		}
		// traverse up the tree
		while ( parent && parent !== this ) {
			parent = parent.parentnode;
		}

		if ( parent !== this ) {
			// set the correct event type
			event.type = event.data;

			// handle event if we actually just moused on to a non sub-element
			jquery.event.handle.apply( this, arguments );
		}

	// assuming we've left the element since we most likely mousedover a xul element
	} catch(e) { }
},

// in case of event delegation, we only need to rename the event.type,
// livehandler will take care of the rest.
delegate = function( event ) {
	event.type = event.data;
	jquery.event.handle.apply( this, arguments );
};

// create mouseenter and mouseleave events
jquery.each({
	mouseenter: "mouseover",
	mouseleave: "mouseout"
}, function( orig, fix ) {
	jquery.event.special[ orig ] = {
		setup: function( data ) {
			jquery.event.add( this, fix, data && data.selector ? delegate : withinelement, orig );
		},
		teardown: function( data ) {
			jquery.event.remove( this, fix, data && data.selector ? delegate : withinelement );
		}
	};
});

// submit delegation
if ( !jquery.support.submitbubbles ) {

	jquery.event.special.submit = {
		setup: function( data, namespaces ) {
			if ( this.nodename && this.nodename.tolowercase() !== "form" ) {
				jquery.event.add(this, "click.specialsubmit", function( e ) {
					var elem = e.target,
						type = elem.type;

					if ( (type === "submit" || type === "image") && jquery( elem ).closest("form").length ) {
						trigger( "submit", this, arguments );
					}
				});

				jquery.event.add(this, "keypress.specialsubmit", function( e ) {
					var elem = e.target,
						type = elem.type;

					if ( (type === "text" || type === "password") && jquery( elem ).closest("form").length && e.keycode === 13 ) {
						trigger( "submit", this, arguments );
					}
				});

			} else {
				return false;
			}
		},

		teardown: function( namespaces ) {
			jquery.event.remove( this, ".specialsubmit" );
		}
	};

}

// change delegation, happens here so we have bind.
if ( !jquery.support.changebubbles ) {

	var changefilters,

	getval = function( elem ) {
		var type = elem.type, val = elem.value;

		if ( type === "radio" || type === "checkbox" ) {
			val = elem.checked;

		} else if ( type === "select-multiple" ) {
			val = elem.selectedindex > -1 ?
				jquery.map( elem.options, function( elem ) {
					return elem.selected;
				}).join("-") :
				"";

		} else if ( elem.nodename.tolowercase() === "select" ) {
			val = elem.selectedindex;
		}

		return val;
	},

	testchange = function testchange( e ) {
		var elem = e.target, data, val;

		if ( !rformelems.test( elem.nodename ) || elem.readonly ) {
			return;
		}

		data = jquery._data( elem, "_change_data" );
		val = getval(elem);

		// the current data will be also retrieved by beforeactivate
		if ( e.type !== "focusout" || elem.type !== "radio" ) {
			jquery._data( elem, "_change_data", val );
		}

		if ( data === undefined || val === data ) {
			return;
		}

		if ( data != null || val ) {
			e.type = "change";
			e.livefired = undefined;
			jquery.event.trigger( e, arguments[1], elem );
		}
	};

	jquery.event.special.change = {
		filters: {
			focusout: testchange,

			beforedeactivate: testchange,

			click: function( e ) {
				var elem = e.target, type = elem.type;

				if ( type === "radio" || type === "checkbox" || elem.nodename.tolowercase() === "select" ) {
					testchange.call( this, e );
				}
			},

			// change has to be called before submit
			// keydown will be called before keypress, which is used in submit-event delegation
			keydown: function( e ) {
				var elem = e.target, type = elem.type;

				if ( (e.keycode === 13 && elem.nodename.tolowercase() !== "textarea") ||
					(e.keycode === 32 && (type === "checkbox" || type === "radio")) ||
					type === "select-multiple" ) {
					testchange.call( this, e );
				}
			},

			// beforeactivate happens also before the previous element is blurred
			// with this event you can't trigger a change event, but you can store
			// information
			beforeactivate: function( e ) {
				var elem = e.target;
				jquery._data( elem, "_change_data", getval(elem) );
			}
		},

		setup: function( data, namespaces ) {
			if ( this.type === "file" ) {
				return false;
			}

			for ( var type in changefilters ) {
				jquery.event.add( this, type + ".specialchange", changefilters[type] );
			}

			return rformelems.test( this.nodename );
		},

		teardown: function( namespaces ) {
			jquery.event.remove( this, ".specialchange" );

			return rformelems.test( this.nodename );
		}
	};

	changefilters = jquery.event.special.change.filters;

	// handle when the input is .focus()'d
	changefilters.focus = changefilters.beforeactivate;
}

function trigger( type, elem, args ) {
	// piggyback on a donor event to simulate a different one.
	// fake originalevent to avoid donor's stoppropagation, but if the
	// simulated event prevents default then we do the same on the donor.
	// don't pass args or remember livefired; they apply to the donor event.
	var event = jquery.extend( {}, args[ 0 ] );
	event.type = type;
	event.originalevent = {};
	event.livefired = undefined;
	jquery.event.handle.call( elem, event );
	if ( event.isdefaultprevented() ) {
		args[ 0 ].preventdefault();
	}
}

// create "bubbling" focus and blur events
if ( document.addeventlistener ) {
	jquery.each({ focus: "focusin", blur: "focusout" }, function( orig, fix ) {
		jquery.event.special[ fix ] = {
			setup: function() {
				this.addeventlistener( orig, handler, true );
			},
			teardown: function() {
				this.removeeventlistener( orig, handler, true );
			}
		};

		function handler( e ) {
			e = jquery.event.fix( e );
			e.type = fix;
			return jquery.event.handle.call( this, e );
		}
	});
}

jquery.each(["bind", "one"], function( i, name ) {
	jquery.fn[ name ] = function( type, data, fn ) {
		// handle object literals
		if ( typeof type === "object" ) {
			for ( var key in type ) {
				this[ name ](key, data, type[key], fn);
			}
			return this;
		}

		if ( jquery.isfunction( data ) || data === false ) {
			fn = data;
			data = undefined;
		}

		var handler = name === "one" ? jquery.proxy( fn, function( event ) {
			jquery( this ).unbind( event, handler );
			return fn.apply( this, arguments );
		}) : fn;

		if ( type === "unload" && name !== "one" ) {
			this.one( type, data, fn );

		} else {
			for ( var i = 0, l = this.length; i < l; i++ ) {
				jquery.event.add( this[i], type, handler, data );
			}
		}

		return this;
	};
});

jquery.fn.extend({
	unbind: function( type, fn ) {
		// handle object literals
		if ( typeof type === "object" && !type.preventdefault ) {
			for ( var key in type ) {
				this.unbind(key, type[key]);
			}

		} else {
			for ( var i = 0, l = this.length; i < l; i++ ) {
				jquery.event.remove( this[i], type, fn );
			}
		}

		return this;
	},

	delegate: function( selector, types, data, fn ) {
		return this.live( types, data, fn, selector );
	},

	undelegate: function( selector, types, fn ) {
		if ( arguments.length === 0 ) {
				return this.unbind( "live" );

		} else {
			return this.die( types, null, fn, selector );
		}
	},

	trigger: function( type, data ) {
		return this.each(function() {
			jquery.event.trigger( type, data, this );
		});
	},

	triggerhandler: function( type, data ) {
		if ( this[0] ) {
			var event = jquery.event( type );
			event.preventdefault();
			event.stoppropagation();
			jquery.event.trigger( event, data, this[0] );
			return event.result;
		}
	},

	toggle: function( fn ) {
		// save reference to arguments for access in closure
		var args = arguments,
			i = 1;

		// link all the functions, so any of them can unbind this click handler
		while ( i < args.length ) {
			jquery.proxy( fn, args[ i++ ] );
		}

		return this.click( jquery.proxy( fn, function( event ) {
			// figure out which function to execute
			var lasttoggle = ( jquery._data( this, "lasttoggle" + fn.guid ) || 0 ) % i;
			jquery._data( this, "lasttoggle" + fn.guid, lasttoggle + 1 );

			// make sure that clicks stop
			event.preventdefault();

			// and execute the function
			return args[ lasttoggle ].apply( this, arguments ) || false;
		}));
	},

	hover: function( fnover, fnout ) {
		return this.mouseenter( fnover ).mouseleave( fnout || fnover );
	}
});

var livemap = {
	focus: "focusin",
	blur: "focusout",
	mouseenter: "mouseover",
	mouseleave: "mouseout"
};

jquery.each(["live", "die"], function( i, name ) {
	jquery.fn[ name ] = function( types, data, fn, origselector /* internal use only */ ) {
		var type, i = 0, match, namespaces, pretype,
			selector = origselector || this.selector,
			context = origselector ? this : jquery( this.context );

		if ( typeof types === "object" && !types.preventdefault ) {
			for ( var key in types ) {
				context[ name ]( key, data, types[key], selector );
			}

			return this;
		}

		if ( jquery.isfunction( data ) ) {
			fn = data;
			data = undefined;
		}

		types = (types || "").split(" ");

		while ( (type = types[ i++ ]) != null ) {
			match = rnamespaces.exec( type );
			namespaces = "";

			if ( match )  {
				namespaces = match[0];
				type = type.replace( rnamespaces, "" );
			}

			if ( type === "hover" ) {
				types.push( "mouseenter" + namespaces, "mouseleave" + namespaces );
				continue;
			}

			pretype = type;

			if ( type === "focus" || type === "blur" ) {
				types.push( livemap[ type ] + namespaces );
				type = type + namespaces;

			} else {
				type = (livemap[ type ] || type) + namespaces;
			}

			if ( name === "live" ) {
				// bind live handler
				for ( var j = 0, l = context.length; j < l; j++ ) {
					jquery.event.add( context[j], "live." + liveconvert( type, selector ),
						{ data: data, selector: selector, handler: fn, origtype: type, orighandler: fn, pretype: pretype } );
				}

			} else {
				// unbind live handler
				context.unbind( "live." + liveconvert( type, selector ), fn );
			}
		}

		return this;
	};
});

function livehandler( event ) {
	var stop, maxlevel, related, match, handleobj, elem, j, i, l, data, close, namespace, ret,
		elems = [],
		selectors = [],
		events = jquery._data( this, "events" );

	// make sure we avoid non-left-click bubbling in firefox (#3861) and disabled elements in ie (#6911)
	if ( event.livefired === this || !events || !events.live || event.target.disabled || event.button && event.type === "click" ) {
		return;
	}

	if ( event.namespace ) {
		namespace = new regexp("(^|\\.)" + event.namespace.split(".").join("\\.(?:.*\\.)?") + "(\\.|$)");
	}

	event.livefired = this;

	var live = events.live.slice(0);

	for ( j = 0; j < live.length; j++ ) {
		handleobj = live[j];

		if ( handleobj.origtype.replace( rnamespaces, "" ) === event.type ) {
			selectors.push( handleobj.selector );

		} else {
			live.splice( j--, 1 );
		}
	}

	match = jquery( event.target ).closest( selectors, event.currenttarget );

	for ( i = 0, l = match.length; i < l; i++ ) {
		close = match[i];

		for ( j = 0; j < live.length; j++ ) {
			handleobj = live[j];

			if ( close.selector === handleobj.selector && (!namespace || namespace.test( handleobj.namespace )) && !close.elem.disabled ) {
				elem = close.elem;
				related = null;

				// those two events require additional checking
				if ( handleobj.pretype === "mouseenter" || handleobj.pretype === "mouseleave" ) {
					event.type = handleobj.pretype;
					related = jquery( event.relatedtarget ).closest( handleobj.selector )[0];
				}

				if ( !related || related !== elem ) {
					elems.push({ elem: elem, handleobj: handleobj, level: close.level });
				}
			}
		}
	}

	for ( i = 0, l = elems.length; i < l; i++ ) {
		match = elems[i];

		if ( maxlevel && match.level > maxlevel ) {
			break;
		}

		event.currenttarget = match.elem;
		event.data = match.handleobj.data;
		event.handleobj = match.handleobj;

		ret = match.handleobj.orighandler.apply( match.elem, arguments );

		if ( ret === false || event.ispropagationstopped() ) {
			maxlevel = match.level;

			if ( ret === false ) {
				stop = false;
			}
			if ( event.isimmediatepropagationstopped() ) {
				break;
			}
		}
	}

	return stop;
}

function liveconvert( type, selector ) {
	return (type && type !== "*" ? type + "." : "") + selector.replace(rperiod, "`").replace(rspace, "&");
}

jquery.each( ("blur focus focusin focusout load resize scroll unload click dblclick " +
	"mousedown mouseup mousemove mouseover mouseout mouseenter mouseleave " +
	"change select submit keydown keypress keyup error").split(" "), function( i, name ) {

	// handle event binding
	jquery.fn[ name ] = function( data, fn ) {
		if ( fn == null ) {
			fn = data;
			data = null;
		}

		return arguments.length > 0 ?
			this.bind( name, data, fn ) :
			this.trigger( name );
	};

	if ( jquery.attrfn ) {
		jquery.attrfn[ name ] = true;
	}
});


/*!
 * sizzle css selector engine
 *  copyright 2011, the dojo foundation
 *  released under the mit, bsd, and gpl licenses.
 *  more information: http://sizzlejs.com/
 */
(function(){

var chunker = /((?:\((?:\([^()]+\)|[^()]+)+\)|\[(?:\[[^\[\]]*\]|['"][^'"]*['"]|[^\[\]'"]+)+\]|\\.|[^ >+~,(\[\\]+)+|[>+~])(\s*,\s*)?((?:.|\r|\n)*)/g,
	done = 0,
	tostring = object.prototype.tostring,
	hasduplicate = false,
	basehasduplicate = true,
	rbackslash = /\\/g,
	rnonword = /\w/;

// here we check if the javascript engine is using some sort of
// optimization where it does not always call our comparision
// function. if that is the case, discard the hasduplicate value.
//   thus far that includes google chrome.
[0, 0].sort(function() {
	basehasduplicate = false;
	return 0;
});

var sizzle = function( selector, context, results, seed ) {
	results = results || [];
	context = context || document;

	var origcontext = context;

	if ( context.nodetype !== 1 && context.nodetype !== 9 ) {
		return [];
	}
	
	if ( !selector || typeof selector !== "string" ) {
		return results;
	}

	var m, set, checkset, extra, ret, cur, pop, i,
		prune = true,
		contextxml = sizzle.isxml( context ),
		parts = [],
		sofar = selector;
	
	// reset the position of the chunker regexp (start from head)
	do {
		chunker.exec( "" );
		m = chunker.exec( sofar );

		if ( m ) {
			sofar = m[3];
		
			parts.push( m[1] );
		
			if ( m[2] ) {
				extra = m[3];
				break;
			}
		}
	} while ( m );

	if ( parts.length > 1 && origpos.exec( selector ) ) {

		if ( parts.length === 2 && expr.relative[ parts[0] ] ) {
			set = posprocess( parts[0] + parts[1], context );

		} else {
			set = expr.relative[ parts[0] ] ?
				[ context ] :
				sizzle( parts.shift(), context );

			while ( parts.length ) {
				selector = parts.shift();

				if ( expr.relative[ selector ] ) {
					selector += parts.shift();
				}
				
				set = posprocess( selector, set );
			}
		}

	} else {
		// take a shortcut and set the context if the root selector is an id
		// (but not if it'll be faster if the inner selector is an id)
		if ( !seed && parts.length > 1 && context.nodetype === 9 && !contextxml &&
				expr.match.id.test(parts[0]) && !expr.match.id.test(parts[parts.length - 1]) ) {

			ret = sizzle.find( parts.shift(), context, contextxml );
			context = ret.expr ?
				sizzle.filter( ret.expr, ret.set )[0] :
				ret.set[0];
		}

		if ( context ) {
			ret = seed ?
				{ expr: parts.pop(), set: makearray(seed) } :
				sizzle.find( parts.pop(), parts.length === 1 && (parts[0] === "~" || parts[0] === "+") && context.parentnode ? context.parentnode : context, contextxml );

			set = ret.expr ?
				sizzle.filter( ret.expr, ret.set ) :
				ret.set;

			if ( parts.length > 0 ) {
				checkset = makearray( set );

			} else {
				prune = false;
			}

			while ( parts.length ) {
				cur = parts.pop();
				pop = cur;

				if ( !expr.relative[ cur ] ) {
					cur = "";
				} else {
					pop = parts.pop();
				}

				if ( pop == null ) {
					pop = context;
				}

				expr.relative[ cur ]( checkset, pop, contextxml );
			}

		} else {
			checkset = parts = [];
		}
	}

	if ( !checkset ) {
		checkset = set;
	}

	if ( !checkset ) {
		sizzle.error( cur || selector );
	}

	if ( tostring.call(checkset) === "[object array]" ) {
		if ( !prune ) {
			results.push.apply( results, checkset );

		} else if ( context && context.nodetype === 1 ) {
			for ( i = 0; checkset[i] != null; i++ ) {
				if ( checkset[i] && (checkset[i] === true || checkset[i].nodetype === 1 && sizzle.contains(context, checkset[i])) ) {
					results.push( set[i] );
				}
			}

		} else {
			for ( i = 0; checkset[i] != null; i++ ) {
				if ( checkset[i] && checkset[i].nodetype === 1 ) {
					results.push( set[i] );
				}
			}
		}

	} else {
		makearray( checkset, results );
	}

	if ( extra ) {
		sizzle( extra, origcontext, results, seed );
		sizzle.uniquesort( results );
	}

	return results;
};

sizzle.uniquesort = function( results ) {
	if ( sortorder ) {
		hasduplicate = basehasduplicate;
		results.sort( sortorder );

		if ( hasduplicate ) {
			for ( var i = 1; i < results.length; i++ ) {
				if ( results[i] === results[ i - 1 ] ) {
					results.splice( i--, 1 );
				}
			}
		}
	}

	return results;
};

sizzle.matches = function( expr, set ) {
	return sizzle( expr, null, null, set );
};

sizzle.matchesselector = function( node, expr ) {
	return sizzle( expr, null, null, [node] ).length > 0;
};

sizzle.find = function( expr, context, isxml ) {
	var set;

	if ( !expr ) {
		return [];
	}

	for ( var i = 0, l = expr.order.length; i < l; i++ ) {
		var match,
			type = expr.order[i];
		
		if ( (match = expr.leftmatch[ type ].exec( expr )) ) {
			var left = match[1];
			match.splice( 1, 1 );

			if ( left.substr( left.length - 1 ) !== "\\" ) {
				match[1] = (match[1] || "").replace( rbackslash, "" );
				set = expr.find[ type ]( match, context, isxml );

				if ( set != null ) {
					expr = expr.replace( expr.match[ type ], "" );
					break;
				}
			}
		}
	}

	if ( !set ) {
		set = typeof context.getelementsbytagname !== "undefined" ?
			context.getelementsbytagname( "*" ) :
			[];
	}

	return { set: set, expr: expr };
};

sizzle.filter = function( expr, set, inplace, not ) {
	var match, anyfound,
		old = expr,
		result = [],
		curloop = set,
		isxmlfilter = set && set[0] && sizzle.isxml( set[0] );

	while ( expr && set.length ) {
		for ( var type in expr.filter ) {
			if ( (match = expr.leftmatch[ type ].exec( expr )) != null && match[2] ) {
				var found, item,
					filter = expr.filter[ type ],
					left = match[1];

				anyfound = false;

				match.splice(1,1);

				if ( left.substr( left.length - 1 ) === "\\" ) {
					continue;
				}

				if ( curloop === result ) {
					result = [];
				}

				if ( expr.prefilter[ type ] ) {
					match = expr.prefilter[ type ]( match, curloop, inplace, result, not, isxmlfilter );

					if ( !match ) {
						anyfound = found = true;

					} else if ( match === true ) {
						continue;
					}
				}

				if ( match ) {
					for ( var i = 0; (item = curloop[i]) != null; i++ ) {
						if ( item ) {
							found = filter( item, match, i, curloop );
							var pass = not ^ !!found;

							if ( inplace && found != null ) {
								if ( pass ) {
									anyfound = true;

								} else {
									curloop[i] = false;
								}

							} else if ( pass ) {
								result.push( item );
								anyfound = true;
							}
						}
					}
				}

				if ( found !== undefined ) {
					if ( !inplace ) {
						curloop = result;
					}

					expr = expr.replace( expr.match[ type ], "" );

					if ( !anyfound ) {
						return [];
					}

					break;
				}
			}
		}

		// improper expression
		if ( expr === old ) {
			if ( anyfound == null ) {
				sizzle.error( expr );

			} else {
				break;
			}
		}

		old = expr;
	}

	return curloop;
};

sizzle.error = function( msg ) {
	throw "syntax error, unrecognized expression: " + msg;
};

var expr = sizzle.selectors = {
	order: [ "id", "name", "tag" ],

	match: {
		id: /#((?:[\w\u00c0-\uffff\-]|\\.)+)/,
		class: /\.((?:[\w\u00c0-\uffff\-]|\\.)+)/,
		name: /\[name=['"]*((?:[\w\u00c0-\uffff\-]|\\.)+)['"]*\]/,
		attr: /\[\s*((?:[\w\u00c0-\uffff\-]|\\.)+)\s*(?:(\s?=)\s*(?:(['"])(.*?)\3|(#?(?:[\w\u00c0-\uffff\-]|\\.)*)|)|)\s*\]/,
		tag: /^((?:[\w\u00c0-\uffff\*\-]|\\.)+)/,
		child: /:(only|nth|last|first)-child(?:\(\s*(even|odd|(?:[+\-]?\d+|(?:[+\-]?\d*)?n\s*(?:[+\-]\s*\d+)?))\s*\))?/,
		pos: /:(nth|eq|gt|lt|first|last|even|odd)(?:\((\d*)\))?(?=[^\-]|$)/,
		pseudo: /:((?:[\w\u00c0-\uffff\-]|\\.)+)(?:\((['"]?)((?:\([^\)]+\)|[^\(\)]*)+)\2\))?/
	},

	leftmatch: {},

	attrmap: {
		"class": "classname",
		"for": "htmlfor"
	},

	attrhandle: {
		href: function( elem ) {
			return elem.getattribute( "href" );
		},
		type: function( elem ) {
			return elem.getattribute( "type" );
		}
	},

	relative: {
		"+": function(checkset, part){
			var ispartstr = typeof part === "string",
				istag = ispartstr && !rnonword.test( part ),
				ispartstrnottag = ispartstr && !istag;

			if ( istag ) {
				part = part.tolowercase();
			}

			for ( var i = 0, l = checkset.length, elem; i < l; i++ ) {
				if ( (elem = checkset[i]) ) {
					while ( (elem = elem.previoussibling) && elem.nodetype !== 1 ) {}

					checkset[i] = ispartstrnottag || elem && elem.nodename.tolowercase() === part ?
						elem || false :
						elem === part;
				}
			}

			if ( ispartstrnottag ) {
				sizzle.filter( part, checkset, true );
			}
		},

		">": function( checkset, part ) {
			var elem,
				ispartstr = typeof part === "string",
				i = 0,
				l = checkset.length;

			if ( ispartstr && !rnonword.test( part ) ) {
				part = part.tolowercase();

				for ( ; i < l; i++ ) {
					elem = checkset[i];

					if ( elem ) {
						var parent = elem.parentnode;
						checkset[i] = parent.nodename.tolowercase() === part ? parent : false;
					}
				}

			} else {
				for ( ; i < l; i++ ) {
					elem = checkset[i];

					if ( elem ) {
						checkset[i] = ispartstr ?
							elem.parentnode :
							elem.parentnode === part;
					}
				}

				if ( ispartstr ) {
					sizzle.filter( part, checkset, true );
				}
			}
		},

		"": function(checkset, part, isxml){
			var nodecheck,
				donename = done++,
				checkfn = dircheck;

			if ( typeof part === "string" && !rnonword.test( part ) ) {
				part = part.tolowercase();
				nodecheck = part;
				checkfn = dirnodecheck;
			}

			checkfn( "parentnode", part, donename, checkset, nodecheck, isxml );
		},

		"~": function( checkset, part, isxml ) {
			var nodecheck,
				donename = done++,
				checkfn = dircheck;

			if ( typeof part === "string" && !rnonword.test( part ) ) {
				part = part.tolowercase();
				nodecheck = part;
				checkfn = dirnodecheck;
			}

			checkfn( "previoussibling", part, donename, checkset, nodecheck, isxml );
		}
	},

	find: {
		id: function( match, context, isxml ) {
			if ( typeof context.getelementbyid !== "undefined" && !isxml ) {
				var m = context.getelementbyid(match[1]);
				// check parentnode to catch when blackberry 4.6 returns
				// nodes that are no longer in the document #6963
				return m && m.parentnode ? [m] : [];
			}
		},

		name: function( match, context ) {
			if ( typeof context.getelementsbyname !== "undefined" ) {
				var ret = [],
					results = context.getelementsbyname( match[1] );

				for ( var i = 0, l = results.length; i < l; i++ ) {
					if ( results[i].getattribute("name") === match[1] ) {
						ret.push( results[i] );
					}
				}

				return ret.length === 0 ? null : ret;
			}
		},

		tag: function( match, context ) {
			if ( typeof context.getelementsbytagname !== "undefined" ) {
				return context.getelementsbytagname( match[1] );
			}
		}
	},
	prefilter: {
		class: function( match, curloop, inplace, result, not, isxml ) {
			match = " " + match[1].replace( rbackslash, "" ) + " ";

			if ( isxml ) {
				return match;
			}

			for ( var i = 0, elem; (elem = curloop[i]) != null; i++ ) {
				if ( elem ) {
					if ( not ^ (elem.classname && (" " + elem.classname + " ").replace(/[\t\n\r]/g, " ").indexof(match) >= 0) ) {
						if ( !inplace ) {
							result.push( elem );
						}

					} else if ( inplace ) {
						curloop[i] = false;
					}
				}
			}

			return false;
		},

		id: function( match ) {
			return match[1].replace( rbackslash, "" );
		},

		tag: function( match, curloop ) {
			return match[1].replace( rbackslash, "" ).tolowercase();
		},

		child: function( match ) {
			if ( match[1] === "nth" ) {
				if ( !match[2] ) {
					sizzle.error( match[0] );
				}

				match[2] = match[2].replace(/^\+|\s*/g, '');

				// parse equations like 'even', 'odd', '5', '2n', '3n+2', '4n-1', '-n+6'
				var test = /(-?)(\d*)(?:n([+\-]?\d*))?/.exec(
					match[2] === "even" && "2n" || match[2] === "odd" && "2n+1" ||
					!/\d/.test( match[2] ) && "0n+" + match[2] || match[2]);

				// calculate the numbers (first)n+(last) including if they are negative
				match[2] = (test[1] + (test[2] || 1)) - 0;
				match[3] = test[3] - 0;
			}
			else if ( match[2] ) {
				sizzle.error( match[0] );
			}

			// todo: move to normal caching system
			match[0] = done++;

			return match;
		},

		attr: function( match, curloop, inplace, result, not, isxml ) {
			var name = match[1] = match[1].replace( rbackslash, "" );
			
			if ( !isxml && expr.attrmap[name] ) {
				match[1] = expr.attrmap[name];
			}

			// handle if an un-quoted value was used
			match[4] = ( match[4] || match[5] || "" ).replace( rbackslash, "" );

			if ( match[2] === "~=" ) {
				match[4] = " " + match[4] + " ";
			}

			return match;
		},

		pseudo: function( match, curloop, inplace, result, not ) {
			if ( match[1] === "not" ) {
				// if we're dealing with a complex expression, or a simple one
				if ( ( chunker.exec(match[3]) || "" ).length > 1 || /^\w/.test(match[3]) ) {
					match[3] = sizzle(match[3], null, null, curloop);

				} else {
					var ret = sizzle.filter(match[3], curloop, inplace, true ^ not);

					if ( !inplace ) {
						result.push.apply( result, ret );
					}

					return false;
				}

			} else if ( expr.match.pos.test( match[0] ) || expr.match.child.test( match[0] ) ) {
				return true;
			}
			
			return match;
		},

		pos: function( match ) {
			match.unshift( true );

			return match;
		}
	},
	
	filters: {
		enabled: function( elem ) {
			return elem.disabled === false && elem.type !== "hidden";
		},

		disabled: function( elem ) {
			return elem.disabled === true;
		},

		checked: function( elem ) {
			return elem.checked === true;
		},
		
		selected: function( elem ) {
			// accessing this property makes selected-by-default
			// options in safari work properly
			if ( elem.parentnode ) {
				elem.parentnode.selectedindex;
			}
			
			return elem.selected === true;
		},

		parent: function( elem ) {
			return !!elem.firstchild;
		},

		empty: function( elem ) {
			return !elem.firstchild;
		},

		has: function( elem, i, match ) {
			return !!sizzle( match[3], elem ).length;
		},

		header: function( elem ) {
			return (/h\d/i).test( elem.nodename );
		},

		text: function( elem ) {
			// ie6 and 7 will map elem.type to 'text' for new html5 types (search, etc) 
			// use getattribute instead to test this case
			return "text" === elem.getattribute( 'type' );
		},
		radio: function( elem ) {
			return "radio" === elem.type;
		},

		checkbox: function( elem ) {
			return "checkbox" === elem.type;
		},

		file: function( elem ) {
			return "file" === elem.type;
		},
		password: function( elem ) {
			return "password" === elem.type;
		},

		submit: function( elem ) {
			return "submit" === elem.type;
		},

		image: function( elem ) {
			return "image" === elem.type;
		},

		reset: function( elem ) {
			return "reset" === elem.type;
		},

		button: function( elem ) {
			return "button" === elem.type || elem.nodename.tolowercase() === "button";
		},

		input: function( elem ) {
			return (/input|select|textarea|button/i).test( elem.nodename );
		}
	},
	setfilters: {
		first: function( elem, i ) {
			return i === 0;
		},

		last: function( elem, i, match, array ) {
			return i === array.length - 1;
		},

		even: function( elem, i ) {
			return i % 2 === 0;
		},

		odd: function( elem, i ) {
			return i % 2 === 1;
		},

		lt: function( elem, i, match ) {
			return i < match[3] - 0;
		},

		gt: function( elem, i, match ) {
			return i > match[3] - 0;
		},

		nth: function( elem, i, match ) {
			return match[3] - 0 === i;
		},

		eq: function( elem, i, match ) {
			return match[3] - 0 === i;
		}
	},
	filter: {
		pseudo: function( elem, match, i, array ) {
			var name = match[1],
				filter = expr.filters[ name ];

			if ( filter ) {
				return filter( elem, i, match, array );

			} else if ( name === "contains" ) {
				return (elem.textcontent || elem.innertext || sizzle.gettext([ elem ]) || "").indexof(match[3]) >= 0;

			} else if ( name === "not" ) {
				var not = match[3];

				for ( var j = 0, l = not.length; j < l; j++ ) {
					if ( not[j] === elem ) {
						return false;
					}
				}

				return true;

			} else {
				sizzle.error( name );
			}
		},

		child: function( elem, match ) {
			var type = match[1],
				node = elem;

			switch ( type ) {
				case "only":
				case "first":
					while ( (node = node.previoussibling) )	 {
						if ( node.nodetype === 1 ) { 
							return false; 
						}
					}

					if ( type === "first" ) { 
						return true; 
					}

					node = elem;

				case "last":
					while ( (node = node.nextsibling) )	 {
						if ( node.nodetype === 1 ) { 
							return false; 
						}
					}

					return true;

				case "nth":
					var first = match[2],
						last = match[3];

					if ( first === 1 && last === 0 ) {
						return true;
					}
					
					var donename = match[0],
						parent = elem.parentnode;
	
					if ( parent && (parent.sizcache !== donename || !elem.nodeindex) ) {
						var count = 0;
						
						for ( node = parent.firstchild; node; node = node.nextsibling ) {
							if ( node.nodetype === 1 ) {
								node.nodeindex = ++count;
							}
						} 

						parent.sizcache = donename;
					}
					
					var diff = elem.nodeindex - last;

					if ( first === 0 ) {
						return diff === 0;

					} else {
						return ( diff % first === 0 && diff / first >= 0 );
					}
			}
		},

		id: function( elem, match ) {
			return elem.nodetype === 1 && elem.getattribute("id") === match;
		},

		tag: function( elem, match ) {
			return (match === "*" && elem.nodetype === 1) || elem.nodename.tolowercase() === match;
		},
		
		class: function( elem, match ) {
			return (" " + (elem.classname || elem.getattribute("class")) + " ")
				.indexof( match ) > -1;
		},

		attr: function( elem, match ) {
			var name = match[1],
				result = expr.attrhandle[ name ] ?
					expr.attrhandle[ name ]( elem ) :
					elem[ name ] != null ?
						elem[ name ] :
						elem.getattribute( name ),
				value = result + "",
				type = match[2],
				check = match[4];

			return result == null ?
				type === "!=" :
				type === "=" ?
				value === check :
				type === "*=" ?
				value.indexof(check) >= 0 :
				type === "~=" ?
				(" " + value + " ").indexof(check) >= 0 :
				!check ?
				value && result !== false :
				type === "!=" ?
				value !== check :
				type === "^=" ?
				value.indexof(check) === 0 :
				type === "$=" ?
				value.substr(value.length - check.length) === check :
				type === "|=" ?
				value === check || value.substr(0, check.length + 1) === check + "-" :
				false;
		},

		pos: function( elem, match, i, array ) {
			var name = match[2],
				filter = expr.setfilters[ name ];

			if ( filter ) {
				return filter( elem, i, match, array );
			}
		}
	}
};

var origpos = expr.match.pos,
	fescape = function(all, num){
		return "\\" + (num - 0 + 1);
	};

for ( var type in expr.match ) {
	expr.match[ type ] = new regexp( expr.match[ type ].source + (/(?![^\[]*\])(?![^\(]*\))/.source) );
	expr.leftmatch[ type ] = new regexp( /(^(?:.|\r|\n)*?)/.source + expr.match[ type ].source.replace(/\\(\d+)/g, fescape) );
}

var makearray = function( array, results ) {
	array = array.prototype.slice.call( array, 0 );

	if ( results ) {
		results.push.apply( results, array );
		return results;
	}
	
	return array;
};

// perform a simple check to determine if the browser is capable of
// converting a nodelist to an array using builtin methods.
// also verifies that the returned array holds dom nodes
// (which is not the case in the blackberry browser)
try {
	array.prototype.slice.call( document.documentelement.childnodes, 0 )[0].nodetype;

// provide a fallback method if it does not work
} catch( e ) {
	makearray = function( array, results ) {
		var i = 0,
			ret = results || [];

		if ( tostring.call(array) === "[object array]" ) {
			array.prototype.push.apply( ret, array );

		} else {
			if ( typeof array.length === "number" ) {
				for ( var l = array.length; i < l; i++ ) {
					ret.push( array[i] );
				}

			} else {
				for ( ; array[i]; i++ ) {
					ret.push( array[i] );
				}
			}
		}

		return ret;
	};
}

var sortorder, siblingcheck;

if ( document.documentelement.comparedocumentposition ) {
	sortorder = function( a, b ) {
		if ( a === b ) {
			hasduplicate = true;
			return 0;
		}

		if ( !a.comparedocumentposition || !b.comparedocumentposition ) {
			return a.comparedocumentposition ? -1 : 1;
		}

		return a.comparedocumentposition(b) & 4 ? -1 : 1;
	};

} else {
	sortorder = function( a, b ) {
		var al, bl,
			ap = [],
			bp = [],
			aup = a.parentnode,
			bup = b.parentnode,
			cur = aup;

		// the nodes are identical, we can exit early
		if ( a === b ) {
			hasduplicate = true;
			return 0;

		// if the nodes are siblings (or identical) we can do a quick check
		} else if ( aup === bup ) {
			return siblingcheck( a, b );

		// if no parents were found then the nodes are disconnected
		} else if ( !aup ) {
			return -1;

		} else if ( !bup ) {
			return 1;
		}

		// otherwise they're somewhere else in the tree so we need
		// to build up a full list of the parentnodes for comparison
		while ( cur ) {
			ap.unshift( cur );
			cur = cur.parentnode;
		}

		cur = bup;

		while ( cur ) {
			bp.unshift( cur );
			cur = cur.parentnode;
		}

		al = ap.length;
		bl = bp.length;

		// start walking down the tree looking for a discrepancy
		for ( var i = 0; i < al && i < bl; i++ ) {
			if ( ap[i] !== bp[i] ) {
				return siblingcheck( ap[i], bp[i] );
			}
		}

		// we ended someplace up the tree so do a sibling check
		return i === al ?
			siblingcheck( a, bp[i], -1 ) :
			siblingcheck( ap[i], b, 1 );
	};

	siblingcheck = function( a, b, ret ) {
		if ( a === b ) {
			return ret;
		}

		var cur = a.nextsibling;

		while ( cur ) {
			if ( cur === b ) {
				return -1;
			}

			cur = cur.nextsibling;
		}

		return 1;
	};
}

// utility function for retreiving the text value of an array of dom nodes
sizzle.gettext = function( elems ) {
	var ret = "", elem;

	for ( var i = 0; elems[i]; i++ ) {
		elem = elems[i];

		// get the text from text nodes and cdata nodes
		if ( elem.nodetype === 3 || elem.nodetype === 4 ) {
			ret += elem.nodevalue;

		// traverse everything else, except comment nodes
		} else if ( elem.nodetype !== 8 ) {
			ret += sizzle.gettext( elem.childnodes );
		}
	}

	return ret;
};

// check to see if the browser returns elements by name when
// querying by getelementbyid (and provide a workaround)
(function(){
	// we're going to inject a fake input element with a specified name
	var form = document.createelement("div"),
		id = "script" + (new date()).gettime(),
		root = document.documentelement;

	form.innerhtml = "<a name='" + id + "'/>";

	// inject it into the root element, check its status, and remove it quickly
	root.insertbefore( form, root.firstchild );

	// the workaround has to do additional checks after a getelementbyid
	// which slows things down for other browsers (hence the branching)
	if ( document.getelementbyid( id ) ) {
		expr.find.id = function( match, context, isxml ) {
			if ( typeof context.getelementbyid !== "undefined" && !isxml ) {
				var m = context.getelementbyid(match[1]);

				return m ?
					m.id === match[1] || typeof m.getattributenode !== "undefined" && m.getattributenode("id").nodevalue === match[1] ?
						[m] :
						undefined :
					[];
			}
		};

		expr.filter.id = function( elem, match ) {
			var node = typeof elem.getattributenode !== "undefined" && elem.getattributenode("id");

			return elem.nodetype === 1 && node && node.nodevalue === match;
		};
	}

	root.removechild( form );

	// release memory in ie
	root = form = null;
})();

(function(){
	// check to see if the browser returns only elements
	// when doing getelementsbytagname("*")

	// create a fake element
	var div = document.createelement("div");
	div.appendchild( document.createcomment("") );

	// make sure no comments are found
	if ( div.getelementsbytagname("*").length > 0 ) {
		expr.find.tag = function( match, context ) {
			var results = context.getelementsbytagname( match[1] );

			// filter out possible comments
			if ( match[1] === "*" ) {
				var tmp = [];

				for ( var i = 0; results[i]; i++ ) {
					if ( results[i].nodetype === 1 ) {
						tmp.push( results[i] );
					}
				}

				results = tmp;
			}

			return results;
		};
	}

	// check to see if an attribute returns normalized href attributes
	div.innerhtml = "<a href='#'></a>";

	if ( div.firstchild && typeof div.firstchild.getattribute !== "undefined" &&
			div.firstchild.getattribute("href") !== "#" ) {

		expr.attrhandle.href = function( elem ) {
			return elem.getattribute( "href", 2 );
		};
	}

	// release memory in ie
	div = null;
})();

if ( document.queryselectorall ) {
	(function(){
		var oldsizzle = sizzle,
			div = document.createelement("div"),
			id = "__sizzle__";

		div.innerhtml = "<p class='test'></p>";

		// safari can't handle uppercase or unicode characters when
		// in quirks mode.
		if ( div.queryselectorall && div.queryselectorall(".test").length === 0 ) {
			return;
		}
	
		sizzle = function( query, context, extra, seed ) {
			context = context || document;

			// only use queryselectorall on non-xml documents
			// (id selectors don't work in non-html documents)
			if ( !seed && !sizzle.isxml(context) ) {
				// see if we find a selector to speed up
				var match = /^(\w+$)|^\.([\w\-]+$)|^#([\w\-]+$)/.exec( query );
				
				if ( match && (context.nodetype === 1 || context.nodetype === 9) ) {
					// speed-up: sizzle("tag")
					if ( match[1] ) {
						return makearray( context.getelementsbytagname( query ), extra );
					
					// speed-up: sizzle(".class")
					} else if ( match[2] && expr.find.class && context.getelementsbyclassname ) {
						return makearray( context.getelementsbyclassname( match[2] ), extra );
					}
				}
				
				if ( context.nodetype === 9 ) {
					// speed-up: sizzle("body")
					// the body element only exists once, optimize finding it
					if ( query === "body" && context.body ) {
						return makearray( [ context.body ], extra );
						
					// speed-up: sizzle("#id")
					} else if ( match && match[3] ) {
						var elem = context.getelementbyid( match[3] );

						// check parentnode to catch when blackberry 4.6 returns
						// nodes that are no longer in the document #6963
						if ( elem && elem.parentnode ) {
							// handle the case where ie and opera return items
							// by name instead of id
							if ( elem.id === match[3] ) {
								return makearray( [ elem ], extra );
							}
							
						} else {
							return makearray( [], extra );
						}
					}
					
					try {
						return makearray( context.queryselectorall(query), extra );
					} catch(qsaerror) {}

				// qsa works strangely on element-rooted queries
				// we can work around this by specifying an extra id on the root
				// and working up from there (thanks to andrew dupont for the technique)
				// ie 8 doesn't work on object elements
				} else if ( context.nodetype === 1 && context.nodename.tolowercase() !== "object" ) {
					var oldcontext = context,
						old = context.getattribute( "id" ),
						nid = old || id,
						hasparent = context.parentnode,
						relativehierarchyselector = /^\s*[+~]/.test( query );

					if ( !old ) {
						context.setattribute( "id", nid );
					} else {
						nid = nid.replace( /'/g, "\\$&" );
					}
					if ( relativehierarchyselector && hasparent ) {
						context = context.parentnode;
					}

					try {
						if ( !relativehierarchyselector || hasparent ) {
							return makearray( context.queryselectorall( "[id='" + nid + "'] " + query ), extra );
						}

					} catch(pseudoerror) {
					} finally {
						if ( !old ) {
							oldcontext.removeattribute( "id" );
						}
					}
				}
			}
		
			return oldsizzle(query, context, extra, seed);
		};

		for ( var prop in oldsizzle ) {
			sizzle[ prop ] = oldsizzle[ prop ];
		}

		// release memory in ie
		div = null;
	})();
}

(function(){
	var html = document.documentelement,
		matches = html.matchesselector || html.mozmatchesselector || html.webkitmatchesselector || html.msmatchesselector,
		pseudoworks = false;

	try {
		// this should fail with an exception
		// gecko does not error, returns false instead
		matches.call( document.documentelement, "[test!='']:sizzle" );
	
	} catch( pseudoerror ) {
		pseudoworks = true;
	}

	if ( matches ) {
		sizzle.matchesselector = function( node, expr ) {
			// make sure that attribute selectors are quoted
			expr = expr.replace(/\=\s*([^'"\]]*)\s*\]/g, "='$1']");

			if ( !sizzle.isxml( node ) ) {
				try { 
					if ( pseudoworks || !expr.match.pseudo.test( expr ) && !/!=/.test( expr ) ) {
						return matches.call( node, expr );
					}
				} catch(e) {}
			}

			return sizzle(expr, null, null, [node]).length > 0;
		};
	}
})();

(function(){
	var div = document.createelement("div");

	div.innerhtml = "<div class='test e'></div><div class='test'></div>";

	// opera can't find a second classname (in 9.6)
	// also, make sure that getelementsbyclassname actually exists
	if ( !div.getelementsbyclassname || div.getelementsbyclassname("e").length === 0 ) {
		return;
	}

	// safari caches class attributes, doesn't catch changes (in 3.2)
	div.lastchild.classname = "e";

	if ( div.getelementsbyclassname("e").length === 1 ) {
		return;
	}
	
	expr.order.splice(1, 0, "class");
	expr.find.class = function( match, context, isxml ) {
		if ( typeof context.getelementsbyclassname !== "undefined" && !isxml ) {
			return context.getelementsbyclassname(match[1]);
		}
	};

	// release memory in ie
	div = null;
})();

function dirnodecheck( dir, cur, donename, checkset, nodecheck, isxml ) {
	for ( var i = 0, l = checkset.length; i < l; i++ ) {
		var elem = checkset[i];

		if ( elem ) {
			var match = false;

			elem = elem[dir];

			while ( elem ) {
				if ( elem.sizcache === donename ) {
					match = checkset[elem.sizset];
					break;
				}

				if ( elem.nodetype === 1 && !isxml ){
					elem.sizcache = donename;
					elem.sizset = i;
				}

				if ( elem.nodename.tolowercase() === cur ) {
					match = elem;
					break;
				}

				elem = elem[dir];
			}

			checkset[i] = match;
		}
	}
}

function dircheck( dir, cur, donename, checkset, nodecheck, isxml ) {
	for ( var i = 0, l = checkset.length; i < l; i++ ) {
		var elem = checkset[i];

		if ( elem ) {
			var match = false;
			
			elem = elem[dir];

			while ( elem ) {
				if ( elem.sizcache === donename ) {
					match = checkset[elem.sizset];
					break;
				}

				if ( elem.nodetype === 1 ) {
					if ( !isxml ) {
						elem.sizcache = donename;
						elem.sizset = i;
					}

					if ( typeof cur !== "string" ) {
						if ( elem === cur ) {
							match = true;
							break;
						}

					} else if ( sizzle.filter( cur, [elem] ).length > 0 ) {
						match = elem;
						break;
					}
				}

				elem = elem[dir];
			}

			checkset[i] = match;
		}
	}
}

if ( document.documentelement.contains ) {
	sizzle.contains = function( a, b ) {
		return a !== b && (a.contains ? a.contains(b) : true);
	};

} else if ( document.documentelement.comparedocumentposition ) {
	sizzle.contains = function( a, b ) {
		return !!(a.comparedocumentposition(b) & 16);
	};

} else {
	sizzle.contains = function() {
		return false;
	};
}

sizzle.isxml = function( elem ) {
	// documentelement is verified for cases where it doesn't yet exist
	// (such as loading iframes in ie - #4833) 
	var documentelement = (elem ? elem.ownerdocument || elem : 0).documentelement;

	return documentelement ? documentelement.nodename !== "html" : false;
};

var posprocess = function( selector, context ) {
	var match,
		tmpset = [],
		later = "",
		root = context.nodetype ? [context] : context;

	// position selectors must be done after the filter
	// and so must :not(positional) so we move all pseudos to the end
	while ( (match = expr.match.pseudo.exec( selector )) ) {
		later += match[0];
		selector = selector.replace( expr.match.pseudo, "" );
	}

	selector = expr.relative[selector] ? selector + "*" : selector;

	for ( var i = 0, l = root.length; i < l; i++ ) {
		sizzle( selector, root[i], tmpset );
	}

	return sizzle.filter( later, tmpset );
};

// expose
jquery.find = sizzle;
jquery.expr = sizzle.selectors;
jquery.expr[":"] = jquery.expr.filters;
jquery.unique = sizzle.uniquesort;
jquery.text = sizzle.gettext;
jquery.isxmldoc = sizzle.isxml;
jquery.contains = sizzle.contains;


})();


var runtil = /until$/,
	rparentsprev = /^(?:parents|prevuntil|prevall)/,
	// note: this regexp should be improved, or likely pulled from sizzle
	rmultiselector = /,/,
	issimple = /^.[^:#\[\.,]*$/,
	slice = array.prototype.slice,
	pos = jquery.expr.match.pos,
	// methods guaranteed to produce a unique set when starting from a unique set
	guaranteedunique = {
		children: true,
		contents: true,
		next: true,
		prev: true
	};

jquery.fn.extend({
	find: function( selector ) {
		var ret = this.pushstack( "", "find", selector ),
			length = 0;

		for ( var i = 0, l = this.length; i < l; i++ ) {
			length = ret.length;
			jquery.find( selector, this[i], ret );

			if ( i > 0 ) {
				// make sure that the results are unique
				for ( var n = length; n < ret.length; n++ ) {
					for ( var r = 0; r < length; r++ ) {
						if ( ret[r] === ret[n] ) {
							ret.splice(n--, 1);
							break;
						}
					}
				}
			}
		}

		return ret;
	},

	has: function( target ) {
		var targets = jquery( target );
		return this.filter(function() {
			for ( var i = 0, l = targets.length; i < l; i++ ) {
				if ( jquery.contains( this, targets[i] ) ) {
					return true;
				}
			}
		});
	},

	not: function( selector ) {
		return this.pushstack( winnow(this, selector, false), "not", selector);
	},

	filter: function( selector ) {
		return this.pushstack( winnow(this, selector, true), "filter", selector );
	},

	is: function( selector ) {
		return !!selector && jquery.filter( selector, this ).length > 0;
	},

	closest: function( selectors, context ) {
		var ret = [], i, l, cur = this[0];

		if ( jquery.isarray( selectors ) ) {
			var match, selector,
				matches = {},
				level = 1;

			if ( cur && selectors.length ) {
				for ( i = 0, l = selectors.length; i < l; i++ ) {
					selector = selectors[i];

					if ( !matches[selector] ) {
						matches[selector] = jquery.expr.match.pos.test( selector ) ?
							jquery( selector, context || this.context ) :
							selector;
					}
				}

				while ( cur && cur.ownerdocument && cur !== context ) {
					for ( selector in matches ) {
						match = matches[selector];

						if ( match.jquery ? match.index(cur) > -1 : jquery(cur).is(match) ) {
							ret.push({ selector: selector, elem: cur, level: level });
						}
					}

					cur = cur.parentnode;
					level++;
				}
			}

			return ret;
		}

		var pos = pos.test( selectors ) ?
			jquery( selectors, context || this.context ) : null;

		for ( i = 0, l = this.length; i < l; i++ ) {
			cur = this[i];

			while ( cur ) {
				if ( pos ? pos.index(cur) > -1 : jquery.find.matchesselector(cur, selectors) ) {
					ret.push( cur );
					break;

				} else {
					cur = cur.parentnode;
					if ( !cur || !cur.ownerdocument || cur === context ) {
						break;
					}
				}
			}
		}

		ret = ret.length > 1 ? jquery.unique(ret) : ret;

		return this.pushstack( ret, "closest", selectors );
	},

	// determine the position of an element within
	// the matched set of elements
	index: function( elem ) {
		if ( !elem || typeof elem === "string" ) {
			return jquery.inarray( this[0],
				// if it receives a string, the selector is used
				// if it receives nothing, the siblings are used
				elem ? jquery( elem ) : this.parent().children() );
		}
		// locate the position of the desired element
		return jquery.inarray(
			// if it receives a jquery object, the first element is used
			elem.jquery ? elem[0] : elem, this );
	},

	add: function( selector, context ) {
		var set = typeof selector === "string" ?
				jquery( selector, context ) :
				jquery.makearray( selector ),
			all = jquery.merge( this.get(), set );

		return this.pushstack( isdisconnected( set[0] ) || isdisconnected( all[0] ) ?
			all :
			jquery.unique( all ) );
	},

	andself: function() {
		return this.add( this.prevobject );
	}
});

// a painfully simple check to see if an element is disconnected
// from a document (should be improved, where feasible).
function isdisconnected( node ) {
	return !node || !node.parentnode || node.parentnode.nodetype === 11;
}

jquery.each({
	parent: function( elem ) {
		var parent = elem.parentnode;
		return parent && parent.nodetype !== 11 ? parent : null;
	},
	parents: function( elem ) {
		return jquery.dir( elem, "parentnode" );
	},
	parentsuntil: function( elem, i, until ) {
		return jquery.dir( elem, "parentnode", until );
	},
	next: function( elem ) {
		return jquery.nth( elem, 2, "nextsibling" );
	},
	prev: function( elem ) {
		return jquery.nth( elem, 2, "previoussibling" );
	},
	nextall: function( elem ) {
		return jquery.dir( elem, "nextsibling" );
	},
	prevall: function( elem ) {
		return jquery.dir( elem, "previoussibling" );
	},
	nextuntil: function( elem, i, until ) {
		return jquery.dir( elem, "nextsibling", until );
	},
	prevuntil: function( elem, i, until ) {
		return jquery.dir( elem, "previoussibling", until );
	},
	siblings: function( elem ) {
		return jquery.sibling( elem.parentnode.firstchild, elem );
	},
	children: function( elem ) {
		return jquery.sibling( elem.firstchild );
	},
	contents: function( elem ) {
		return jquery.nodename( elem, "iframe" ) ?
			elem.contentdocument || elem.contentwindow.document :
			jquery.makearray( elem.childnodes );
	}
}, function( name, fn ) {
	jquery.fn[ name ] = function( until, selector ) {
		var ret = jquery.map( this, fn, until ),
			// the variable 'args' was introduced in
			// https://github.com/jquery/jquery/commit/52a0238
			// to work around a bug in chrome 10 (dev) and should be removed when the bug is fixed.
			// http://code.google.com/p/v8/issues/detail?id=1050
			args = slice.call(arguments);

		if ( !runtil.test( name ) ) {
			selector = until;
		}

		if ( selector && typeof selector === "string" ) {
			ret = jquery.filter( selector, ret );
		}

		ret = this.length > 1 && !guaranteedunique[ name ] ? jquery.unique( ret ) : ret;

		if ( (this.length > 1 || rmultiselector.test( selector )) && rparentsprev.test( name ) ) {
			ret = ret.reverse();
		}

		return this.pushstack( ret, name, args.join(",") );
	};
});

jquery.extend({
	filter: function( expr, elems, not ) {
		if ( not ) {
			expr = ":not(" + expr + ")";
		}

		return elems.length === 1 ?
			jquery.find.matchesselector(elems[0], expr) ? [ elems[0] ] : [] :
			jquery.find.matches(expr, elems);
	},

	dir: function( elem, dir, until ) {
		var matched = [],
			cur = elem[ dir ];

		while ( cur && cur.nodetype !== 9 && (until === undefined || cur.nodetype !== 1 || !jquery( cur ).is( until )) ) {
			if ( cur.nodetype === 1 ) {
				matched.push( cur );
			}
			cur = cur[dir];
		}
		return matched;
	},

	nth: function( cur, result, dir, elem ) {
		result = result || 1;
		var num = 0;

		for ( ; cur; cur = cur[dir] ) {
			if ( cur.nodetype === 1 && ++num === result ) {
				break;
			}
		}

		return cur;
	},

	sibling: function( n, elem ) {
		var r = [];

		for ( ; n; n = n.nextsibling ) {
			if ( n.nodetype === 1 && n !== elem ) {
				r.push( n );
			}
		}

		return r;
	}
});

// implement the identical functionality for filter and not
function winnow( elements, qualifier, keep ) {
	if ( jquery.isfunction( qualifier ) ) {
		return jquery.grep(elements, function( elem, i ) {
			var retval = !!qualifier.call( elem, i, elem );
			return retval === keep;
		});

	} else if ( qualifier.nodetype ) {
		return jquery.grep(elements, function( elem, i ) {
			return (elem === qualifier) === keep;
		});

	} else if ( typeof qualifier === "string" ) {
		var filtered = jquery.grep(elements, function( elem ) {
			return elem.nodetype === 1;
		});

		if ( issimple.test( qualifier ) ) {
			return jquery.filter(qualifier, filtered, !keep);
		} else {
			qualifier = jquery.filter( qualifier, filtered );
		}
	}

	return jquery.grep(elements, function( elem, i ) {
		return (jquery.inarray( elem, qualifier ) >= 0) === keep;
	});
}




var rinlinejquery = / jquery\d+="(?:\d+|null)"/g,
	rleadingwhitespace = /^\s+/,
	rxhtmltag = /<(?!area|br|col|embed|hr|img|input|link|meta|param)(([\w:]+)[^>]*)\/>/ig,
	rtagname = /<([\w:]+)/,
	rtbody = /<tbody/i,
	rhtml = /<|&#?\w+;/,
	rnocache = /<(?:script|object|embed|option|style)/i,
	// checked="checked" or checked
	rchecked = /checked\s*(?:[^=]|=\s*.checked.)/i,
	wrapmap = {
		option: [ 1, "<select multiple='multiple'>", "</select>" ],
		legend: [ 1, "<fieldset>", "</fieldset>" ],
		thead: [ 1, "<table>", "</table>" ],
		tr: [ 2, "<table><tbody>", "</tbody></table>" ],
		td: [ 3, "<table><tbody><tr>", "</tr></tbody></table>" ],
		col: [ 2, "<table><tbody></tbody><colgroup>", "</colgroup></table>" ],
		area: [ 1, "<map>", "</map>" ],
		_default: [ 0, "", "" ]
	};

wrapmap.optgroup = wrapmap.option;
wrapmap.tbody = wrapmap.tfoot = wrapmap.colgroup = wrapmap.caption = wrapmap.thead;
wrapmap.th = wrapmap.td;

// ie can't serialize <link> and <script> tags normally
if ( !jquery.support.htmlserialize ) {
	wrapmap._default = [ 1, "div<div>", "</div>" ];
}

jquery.fn.extend({
	text: function( text ) {
		if ( jquery.isfunction(text) ) {
			return this.each(function(i) {
				var self = jquery( this );

				self.text( text.call(this, i, self.text()) );
			});
		}

		if ( typeof text !== "object" && text !== undefined ) {
			return this.empty().append( (this[0] && this[0].ownerdocument || document).createtextnode( text ) );
		}

		return jquery.text( this );
	},

	wrapall: function( html ) {
		if ( jquery.isfunction( html ) ) {
			return this.each(function(i) {
				jquery(this).wrapall( html.call(this, i) );
			});
		}

		if ( this[0] ) {
			// the elements to wrap the target around
			var wrap = jquery( html, this[0].ownerdocument ).eq(0).clone(true);

			if ( this[0].parentnode ) {
				wrap.insertbefore( this[0] );
			}

			wrap.map(function() {
				var elem = this;

				while ( elem.firstchild && elem.firstchild.nodetype === 1 ) {
					elem = elem.firstchild;
				}

				return elem;
			}).append(this);
		}

		return this;
	},

	wrapinner: function( html ) {
		if ( jquery.isfunction( html ) ) {
			return this.each(function(i) {
				jquery(this).wrapinner( html.call(this, i) );
			});
		}

		return this.each(function() {
			var self = jquery( this ),
				contents = self.contents();

			if ( contents.length ) {
				contents.wrapall( html );

			} else {
				self.append( html );
			}
		});
	},

	wrap: function( html ) {
		return this.each(function() {
			jquery( this ).wrapall( html );
		});
	},

	unwrap: function() {
		return this.parent().each(function() {
			if ( !jquery.nodename( this, "body" ) ) {
				jquery( this ).replacewith( this.childnodes );
			}
		}).end();
	},

	append: function() {
		return this.dommanip(arguments, true, function( elem ) {
			if ( this.nodetype === 1 ) {
				this.appendchild( elem );
			}
		});
	},

	prepend: function() {
		return this.dommanip(arguments, true, function( elem ) {
			if ( this.nodetype === 1 ) {
				this.insertbefore( elem, this.firstchild );
			}
		});
	},

	before: function() {
		if ( this[0] && this[0].parentnode ) {
			return this.dommanip(arguments, false, function( elem ) {
				this.parentnode.insertbefore( elem, this );
			});
		} else if ( arguments.length ) {
			var set = jquery(arguments[0]);
			set.push.apply( set, this.toarray() );
			return this.pushstack( set, "before", arguments );
		}
	},

	after: function() {
		if ( this[0] && this[0].parentnode ) {
			return this.dommanip(arguments, false, function( elem ) {
				this.parentnode.insertbefore( elem, this.nextsibling );
			});
		} else if ( arguments.length ) {
			var set = this.pushstack( this, "after", arguments );
			set.push.apply( set, jquery(arguments[0]).toarray() );
			return set;
		}
	},

	// keepdata is for internal use only--do not document
	remove: function( selector, keepdata ) {
		for ( var i = 0, elem; (elem = this[i]) != null; i++ ) {
			if ( !selector || jquery.filter( selector, [ elem ] ).length ) {
				if ( !keepdata && elem.nodetype === 1 ) {
					jquery.cleandata( elem.getelementsbytagname("*") );
					jquery.cleandata( [ elem ] );
				}

				if ( elem.parentnode ) {
					elem.parentnode.removechild( elem );
				}
			}
		}

		return this;
	},

	empty: function() {
		for ( var i = 0, elem; (elem = this[i]) != null; i++ ) {
			// remove element nodes and prevent memory leaks
			if ( elem.nodetype === 1 ) {
				jquery.cleandata( elem.getelementsbytagname("*") );
			}

			// remove any remaining nodes
			while ( elem.firstchild ) {
				elem.removechild( elem.firstchild );
			}
		}

		return this;
	},

	clone: function( dataandevents, deepdataandevents ) {
		dataandevents = dataandevents == null ? false : dataandevents;
		deepdataandevents = deepdataandevents == null ? dataandevents : deepdataandevents;

		return this.map( function () {
			return jquery.clone( this, dataandevents, deepdataandevents );
		});
	},

	html: function( value ) {
		if ( value === undefined ) {
			return this[0] && this[0].nodetype === 1 ?
				this[0].innerhtml.replace(rinlinejquery, "") :
				null;

		// see if we can take a shortcut and just use innerhtml
		} else if ( typeof value === "string" && !rnocache.test( value ) &&
			(jquery.support.leadingwhitespace || !rleadingwhitespace.test( value )) &&
			!wrapmap[ (rtagname.exec( value ) || ["", ""])[1].tolowercase() ] ) {

			value = value.replace(rxhtmltag, "<$1></$2>");

			try {
				for ( var i = 0, l = this.length; i < l; i++ ) {
					// remove element nodes and prevent memory leaks
					if ( this[i].nodetype === 1 ) {
						jquery.cleandata( this[i].getelementsbytagname("*") );
						this[i].innerhtml = value;
					}
				}

			// if using innerhtml throws an exception, use the fallback method
			} catch(e) {
				this.empty().append( value );
			}

		} else if ( jquery.isfunction( value ) ) {
			this.each(function(i){
				var self = jquery( this );

				self.html( value.call(this, i, self.html()) );
			});

		} else {
			this.empty().append( value );
		}

		return this;
	},

	replacewith: function( value ) {
		if ( this[0] && this[0].parentnode ) {
			// make sure that the elements are removed from the dom before they are inserted
			// this can help fix replacing a parent with child elements
			if ( jquery.isfunction( value ) ) {
				return this.each(function(i) {
					var self = jquery(this), old = self.html();
					self.replacewith( value.call( this, i, old ) );
				});
			}

			if ( typeof value !== "string" ) {
				value = jquery( value ).detach();
			}

			return this.each(function() {
				var next = this.nextsibling,
					parent = this.parentnode;

				jquery( this ).remove();

				if ( next ) {
					jquery(next).before( value );
				} else {
					jquery(parent).append( value );
				}
			});
		} else {
			return this.pushstack( jquery(jquery.isfunction(value) ? value() : value), "replacewith", value );
		}
	},

	detach: function( selector ) {
		return this.remove( selector, true );
	},

	dommanip: function( args, table, callback ) {
		var results, first, fragment, parent,
			value = args[0],
			scripts = [];

		// we can't clonenode fragments that contain checked, in webkit
		if ( !jquery.support.checkclone && arguments.length === 3 && typeof value === "string" && rchecked.test( value ) ) {
			return this.each(function() {
				jquery(this).dommanip( args, table, callback, true );
			});
		}

		if ( jquery.isfunction(value) ) {
			return this.each(function(i) {
				var self = jquery(this);
				args[0] = value.call(this, i, table ? self.html() : undefined);
				self.dommanip( args, table, callback );
			});
		}

		if ( this[0] ) {
			parent = value && value.parentnode;

			// if we're in a fragment, just use that instead of building a new one
			if ( jquery.support.parentnode && parent && parent.nodetype === 11 && parent.childnodes.length === this.length ) {
				results = { fragment: parent };

			} else {
				results = jquery.buildfragment( args, this, scripts );
			}

			fragment = results.fragment;

			if ( fragment.childnodes.length === 1 ) {
				first = fragment = fragment.firstchild;
			} else {
				first = fragment.firstchild;
			}

			if ( first ) {
				table = table && jquery.nodename( first, "tr" );

				for ( var i = 0, l = this.length, lastindex = l - 1; i < l; i++ ) {
					callback.call(
						table ?
							root(this[i], first) :
							this[i],
						// make sure that we do not leak memory by inadvertently discarding
						// the original fragment (which might have attached data) instead of
						// using it; in addition, use the original fragment object for the last
						// item instead of first because it can end up being emptied incorrectly
						// in certain situations (bug #8070).
						// fragments from the fragment cache must always be cloned and never used
						// in place.
						results.cacheable || (l > 1 && i < lastindex) ?
							jquery.clone( fragment, true, true ) :
							fragment
					);
				}
			}

			if ( scripts.length ) {
				jquery.each( scripts, evalscript );
			}
		}

		return this;
	}
});

function root( elem, cur ) {
	return jquery.nodename(elem, "table") ?
		(elem.getelementsbytagname("tbody")[0] ||
		elem.appendchild(elem.ownerdocument.createelement("tbody"))) :
		elem;
}

function clonecopyevent( src, dest ) {

	if ( dest.nodetype !== 1 || !jquery.hasdata( src ) ) {
		return;
	}

	var internalkey = jquery.expando,
		olddata = jquery.data( src ),
		curdata = jquery.data( dest, olddata );

	// switch to use the internal data object, if it exists, for the next
	// stage of data copying
	if ( (olddata = olddata[ internalkey ]) ) {
		var events = olddata.events;
				curdata = curdata[ internalkey ] = jquery.extend({}, olddata);

		if ( events ) {
			delete curdata.handle;
			curdata.events = {};

			for ( var type in events ) {
				for ( var i = 0, l = events[ type ].length; i < l; i++ ) {
					jquery.event.add( dest, type + ( events[ type ][ i ].namespace ? "." : "" ) + events[ type ][ i ].namespace, events[ type ][ i ], events[ type ][ i ].data );
				}
			}
		}
	}
}

function clonefixattributes(src, dest) {
	// we do not need to do anything for non-elements
	if ( dest.nodetype !== 1 ) {
		return;
	}

	var nodename = dest.nodename.tolowercase();

	// clearattributes removes the attributes, which we don't want,
	// but also removes the attachevent events, which we *do* want
	dest.clearattributes();

	// mergeattributes, in contrast, only merges back on the
	// original attributes, not the events
	dest.mergeattributes(src);

	// ie6-8 fail to clone children inside object elements that use
	// the proprietary classid attribute value (rather than the type
	// attribute) to identify the type of content to display
	if ( nodename === "object" ) {
		dest.outerhtml = src.outerhtml;

	} else if ( nodename === "input" && (src.type === "checkbox" || src.type === "radio") ) {
		// ie6-8 fails to persist the checked state of a cloned checkbox
		// or radio button. worse, ie6-7 fail to give the cloned element
		// a checked appearance if the defaultchecked value isn't also set
		if ( src.checked ) {
			dest.defaultchecked = dest.checked = src.checked;
		}

		// ie6-7 get confused and end up setting the value of a cloned
		// checkbox/radio button to an empty string instead of "on"
		if ( dest.value !== src.value ) {
			dest.value = src.value;
		}

	// ie6-8 fails to return the selected option to the default selected
	// state when cloning options
	} else if ( nodename === "option" ) {
		dest.selected = src.defaultselected;

	// ie6-8 fails to set the defaultvalue to the correct value when
	// cloning other types of input fields
	} else if ( nodename === "input" || nodename === "textarea" ) {
		dest.defaultvalue = src.defaultvalue;
	}

	// event data gets referenced instead of copied if the expando
	// gets copied too
	dest.removeattribute( jquery.expando );
}

jquery.buildfragment = function( args, nodes, scripts ) {
	var fragment, cacheable, cacheresults,
		doc = (nodes && nodes[0] ? nodes[0].ownerdocument || nodes[0] : document);

	// only cache "small" (1/2 kb) html strings that are associated with the main document
	// cloning options loses the selected state, so don't cache them
	// ie 6 doesn't like it when you put <object> or <embed> elements in a fragment
	// also, webkit does not clone 'checked' attributes on clonenode, so don't cache
	if ( args.length === 1 && typeof args[0] === "string" && args[0].length < 512 && doc === document &&
		args[0].charat(0) === "<" && !rnocache.test( args[0] ) && (jquery.support.checkclone || !rchecked.test( args[0] )) ) {

		cacheable = true;
		cacheresults = jquery.fragments[ args[0] ];
		if ( cacheresults ) {
			if ( cacheresults !== 1 ) {
				fragment = cacheresults;
			}
		}
	}

	if ( !fragment ) {
		fragment = doc.createdocumentfragment();
		jquery.clean( args, doc, fragment, scripts );
	}

	if ( cacheable ) {
		jquery.fragments[ args[0] ] = cacheresults ? fragment : 1;
	}

	return { fragment: fragment, cacheable: cacheable };
};

jquery.fragments = {};

jquery.each({
	appendto: "append",
	prependto: "prepend",
	insertbefore: "before",
	insertafter: "after",
	replaceall: "replacewith"
}, function( name, original ) {
	jquery.fn[ name ] = function( selector ) {
		var ret = [],
			insert = jquery( selector ),
			parent = this.length === 1 && this[0].parentnode;

		if ( parent && parent.nodetype === 11 && parent.childnodes.length === 1 && insert.length === 1 ) {
			insert[ original ]( this[0] );
			return this;

		} else {
			for ( var i = 0, l = insert.length; i < l; i++ ) {
				var elems = (i > 0 ? this.clone(true) : this).get();
				jquery( insert[i] )[ original ]( elems );
				ret = ret.concat( elems );
			}

			return this.pushstack( ret, name, insert.selector );
		}
	};
});

function getall( elem ) {
	if ( "getelementsbytagname" in elem ) {
		return elem.getelementsbytagname( "*" );
	
	} else if ( "queryselectorall" in elem ) {
		return elem.queryselectorall( "*" );

	} else {
		return [];
	}
}

jquery.extend({
	clone: function( elem, dataandevents, deepdataandevents ) {
		var clone = elem.clonenode(true),
				srcelements,
				destelements,
				i;

		if ( (!jquery.support.nocloneevent || !jquery.support.noclonechecked) &&
				(elem.nodetype === 1 || elem.nodetype === 11) && !jquery.isxmldoc(elem) ) {
			// ie copies events bound via attachevent when using clonenode.
			// calling detachevent on the clone will also remove the events
			// from the original. in order to get around this, we use some
			// proprietary methods to clear the events. thanks to mootools
			// guys for this hotness.

			clonefixattributes( elem, clone );

			// using sizzle here is crazy slow, so we use getelementsbytagname
			// instead
			srcelements = getall( elem );
			destelements = getall( clone );

			// weird iteration because ie will replace the length property
			// with an element if you are cloning the body and one of the
			// elements on the page has a name or id of "length"
			for ( i = 0; srcelements[i]; ++i ) {
				clonefixattributes( srcelements[i], destelements[i] );
			}
		}

		// copy the events from the original to the clone
		if ( dataandevents ) {
			clonecopyevent( elem, clone );

			if ( deepdataandevents ) {
				srcelements = getall( elem );
				destelements = getall( clone );

				for ( i = 0; srcelements[i]; ++i ) {
					clonecopyevent( srcelements[i], destelements[i] );
				}
			}
		}

		// return the cloned set
		return clone;
},
	clean: function( elems, context, fragment, scripts ) {
		context = context || document;

		// !context.createelement fails in ie with an error but returns typeof 'object'
		if ( typeof context.createelement === "undefined" ) {
			context = context.ownerdocument || context[0] && context[0].ownerdocument || document;
		}

		var ret = [];

		for ( var i = 0, elem; (elem = elems[i]) != null; i++ ) {
			if ( typeof elem === "number" ) {
				elem += "";
			}

			if ( !elem ) {
				continue;
			}

			// convert html string into dom nodes
			if ( typeof elem === "string" && !rhtml.test( elem ) ) {
				elem = context.createtextnode( elem );

			} else if ( typeof elem === "string" ) {
				// fix "xhtml"-style tags in all browsers
				elem = elem.replace(rxhtmltag, "<$1></$2>");

				// trim whitespace, otherwise indexof won't work as expected
				var tag = (rtagname.exec( elem ) || ["", ""])[1].tolowercase(),
					wrap = wrapmap[ tag ] || wrapmap._default,
					depth = wrap[0],
					div = context.createelement("div");

				// go to html and back, then peel off extra wrappers
				div.innerhtml = wrap[1] + elem + wrap[2];

				// move to the right depth
				while ( depth-- ) {
					div = div.lastchild;
				}

				// remove ie's autoinserted <tbody> from table fragments
				if ( !jquery.support.tbody ) {

					// string was a <table>, *may* have spurious <tbody>
					var hasbody = rtbody.test(elem),
						tbody = tag === "table" && !hasbody ?
							div.firstchild && div.firstchild.childnodes :

							// string was a bare <thead> or <tfoot>
							wrap[1] === "<table>" && !hasbody ?
								div.childnodes :
								[];

					for ( var j = tbody.length - 1; j >= 0 ; --j ) {
						if ( jquery.nodename( tbody[ j ], "tbody" ) && !tbody[ j ].childnodes.length ) {
							tbody[ j ].parentnode.removechild( tbody[ j ] );
						}
					}

				}

				// ie completely kills leading whitespace when innerhtml is used
				if ( !jquery.support.leadingwhitespace && rleadingwhitespace.test( elem ) ) {
					div.insertbefore( context.createtextnode( rleadingwhitespace.exec(elem)[0] ), div.firstchild );
				}

				elem = div.childnodes;
			}

			if ( elem.nodetype ) {
				ret.push( elem );
			} else {
				ret = jquery.merge( ret, elem );
			}
		}

		if ( fragment ) {
			for ( i = 0; ret[i]; i++ ) {
				if ( scripts && jquery.nodename( ret[i], "script" ) && (!ret[i].type || ret[i].type.tolowercase() === "text/javascript") ) {
					scripts.push( ret[i].parentnode ? ret[i].parentnode.removechild( ret[i] ) : ret[i] );

				} else {
					if ( ret[i].nodetype === 1 ) {
						ret.splice.apply( ret, [i + 1, 0].concat(jquery.makearray(ret[i].getelementsbytagname("script"))) );
					}
					fragment.appendchild( ret[i] );
				}
			}
		}

		return ret;
	},

	cleandata: function( elems ) {
		var data, id, cache = jquery.cache, internalkey = jquery.expando, special = jquery.event.special,
			deleteexpando = jquery.support.deleteexpando;

		for ( var i = 0, elem; (elem = elems[i]) != null; i++ ) {
			if ( elem.nodename && jquery.nodata[elem.nodename.tolowercase()] ) {
				continue;
			}

			id = elem[ jquery.expando ];

			if ( id ) {
				data = cache[ id ] && cache[ id ][ internalkey ];

				if ( data && data.events ) {
					for ( var type in data.events ) {
						if ( special[ type ] ) {
							jquery.event.remove( elem, type );

						// this is a shortcut to avoid jquery.event.remove's overhead
						} else {
							jquery.removeevent( elem, type, data.handle );
						}
					}

					// null the dom reference to avoid ie6/7/8 leak (#7054)
					if ( data.handle ) {
						data.handle.elem = null;
					}
				}

				if ( deleteexpando ) {
					delete elem[ jquery.expando ];

				} else if ( elem.removeattribute ) {
					elem.removeattribute( jquery.expando );
				}

				delete cache[ id ];
			}
		}
	}
});

function evalscript( i, elem ) {
	if ( elem.src ) {
		jquery.ajax({
			url: elem.src,
			async: false,
			datatype: "script"
		});
	} else {
		jquery.globaleval( elem.text || elem.textcontent || elem.innerhtml || "" );
	}

	if ( elem.parentnode ) {
		elem.parentnode.removechild( elem );
	}
}




var ralpha = /alpha\([^)]*\)/i,
	ropacity = /opacity=([^)]*)/,
	rdashalpha = /-([a-z])/ig,
	rupper = /([a-z])/g,
	rnumpx = /^-?\d+(?:px)?$/i,
	rnum = /^-?\d/,

	cssshow = { position: "absolute", visibility: "hidden", display: "block" },
	csswidth = [ "left", "right" ],
	cssheight = [ "top", "bottom" ],
	curcss,

	getcomputedstyle,
	currentstyle,

	fcamelcase = function( all, letter ) {
		return letter.touppercase();
	};

jquery.fn.css = function( name, value ) {
	// setting 'undefined' is a no-op
	if ( arguments.length === 2 && value === undefined ) {
		return this;
	}

	return jquery.access( this, name, value, true, function( elem, name, value ) {
		return value !== undefined ?
			jquery.style( elem, name, value ) :
			jquery.css( elem, name );
	});
};

jquery.extend({
	// add in style property hooks for overriding the default
	// behavior of getting and setting a style property
	csshooks: {
		opacity: {
			get: function( elem, computed ) {
				if ( computed ) {
					// we should always get a number back from opacity
					var ret = curcss( elem, "opacity", "opacity" );
					return ret === "" ? "1" : ret;

				} else {
					return elem.style.opacity;
				}
			}
		}
	},

	// exclude the following css properties to add px
	cssnumber: {
		"zindex": true,
		"fontweight": true,
		"opacity": true,
		"zoom": true,
		"lineheight": true
	},

	// add in properties whose names you wish to fix before
	// setting or getting the value
	cssprops: {
		// normalize float css property
		"float": jquery.support.cssfloat ? "cssfloat" : "stylefloat"
	},

	// get and set the style property on a dom node
	style: function( elem, name, value, extra ) {
		// don't set styles on text and comment nodes
		if ( !elem || elem.nodetype === 3 || elem.nodetype === 8 || !elem.style ) {
			return;
		}

		// make sure that we're working with the right name
		var ret, origname = jquery.camelcase( name ),
			style = elem.style, hooks = jquery.csshooks[ origname ];

		name = jquery.cssprops[ origname ] || origname;

		// check if we're setting a value
		if ( value !== undefined ) {
			// make sure that nan and null values aren't set. see: #7116
			if ( typeof value === "number" && isnan( value ) || value == null ) {
				return;
			}

			// if a number was passed in, add 'px' to the (except for certain css properties)
			if ( typeof value === "number" && !jquery.cssnumber[ origname ] ) {
				value += "px";
			}

			// if a hook was provided, use that value, otherwise just set the specified value
			if ( !hooks || !("set" in hooks) || (value = hooks.set( elem, value )) !== undefined ) {
				// wrapped to prevent ie from throwing errors when 'invalid' values are provided
				// fixes bug #5509
				try {
					style[ name ] = value;
				} catch(e) {}
			}

		} else {
			// if a hook was provided get the non-computed value from there
			if ( hooks && "get" in hooks && (ret = hooks.get( elem, false, extra )) !== undefined ) {
				return ret;
			}

			// otherwise just get the value from the style object
			return style[ name ];
		}
	},

	css: function( elem, name, extra ) {
		// make sure that we're working with the right name
		var ret, origname = jquery.camelcase( name ),
			hooks = jquery.csshooks[ origname ];

		name = jquery.cssprops[ origname ] || origname;

		// if a hook was provided get the computed value from there
		if ( hooks && "get" in hooks && (ret = hooks.get( elem, true, extra )) !== undefined ) {
			return ret;

		// otherwise, if a way to get the computed value exists, use that
		} else if ( curcss ) {
			return curcss( elem, name, origname );
		}
	},

	// a method for quickly swapping in/out css properties to get correct calculations
	swap: function( elem, options, callback ) {
		var old = {};

		// remember the old values, and insert the new ones
		for ( var name in options ) {
			old[ name ] = elem.style[ name ];
			elem.style[ name ] = options[ name ];
		}

		callback.call( elem );

		// revert the old values
		for ( name in options ) {
			elem.style[ name ] = old[ name ];
		}
	},

	camelcase: function( string ) {
		return string.replace( rdashalpha, fcamelcase );
	}
});

// deprecated, use jquery.css() instead
jquery.curcss = jquery.css;

jquery.each(["height", "width"], function( i, name ) {
	jquery.csshooks[ name ] = {
		get: function( elem, computed, extra ) {
			var val;

			if ( computed ) {
				if ( elem.offsetwidth !== 0 ) {
					val = getwh( elem, name, extra );

				} else {
					jquery.swap( elem, cssshow, function() {
						val = getwh( elem, name, extra );
					});
				}

				if ( val <= 0 ) {
					val = curcss( elem, name, name );

					if ( val === "0px" && currentstyle ) {
						val = currentstyle( elem, name, name );
					}

					if ( val != null ) {
						// should return "auto" instead of 0, use 0 for
						// temporary backwards-compat
						return val === "" || val === "auto" ? "0px" : val;
					}
				}

				if ( val < 0 || val == null ) {
					val = elem.style[ name ];

					// should return "auto" instead of 0, use 0 for
					// temporary backwards-compat
					return val === "" || val === "auto" ? "0px" : val;
				}

				return typeof val === "string" ? val : val + "px";
			}
		},

		set: function( elem, value ) {
			if ( rnumpx.test( value ) ) {
				// ignore negative width and height values #1599
				value = parsefloat(value);

				if ( value >= 0 ) {
					return value + "px";
				}

			} else {
				return value;
			}
		}
	};
});

if ( !jquery.support.opacity ) {
	jquery.csshooks.opacity = {
		get: function( elem, computed ) {
			// ie uses filters for opacity
			return ropacity.test((computed && elem.currentstyle ? elem.currentstyle.filter : elem.style.filter) || "") ?
				(parsefloat(regexp.$1) / 100) + "" :
				computed ? "1" : "";
		},

		set: function( elem, value ) {
			var style = elem.style;

			// ie has trouble with opacity if it does not have layout
			// force it by setting the zoom level
			style.zoom = 1;

			// set the alpha filter to set the opacity
			var opacity = jquery.isnan(value) ?
				"" :
				"alpha(opacity=" + value * 100 + ")",
				filter = style.filter || "";

			style.filter = ralpha.test(filter) ?
				filter.replace(ralpha, opacity) :
				style.filter + ' ' + opacity;
		}
	};
}

if ( document.defaultview && document.defaultview.getcomputedstyle ) {
	getcomputedstyle = function( elem, newname, name ) {
		var ret, defaultview, computedstyle;

		name = name.replace( rupper, "-$1" ).tolowercase();

		if ( !(defaultview = elem.ownerdocument.defaultview) ) {
			return undefined;
		}

		if ( (computedstyle = defaultview.getcomputedstyle( elem, null )) ) {
			ret = computedstyle.getpropertyvalue( name );
			if ( ret === "" && !jquery.contains( elem.ownerdocument.documentelement, elem ) ) {
				ret = jquery.style( elem, name );
			}
		}

		return ret;
	};
}

if ( document.documentelement.currentstyle ) {
	currentstyle = function( elem, name ) {
		var left,
			ret = elem.currentstyle && elem.currentstyle[ name ],
			rsleft = elem.runtimestyle && elem.runtimestyle[ name ],
			style = elem.style;

		// from the awesome hack by dean edwards
		// http://erik.eae.net/archives/2007/07/27/18.54.15/#comment-102291

		// if we're not dealing with a regular pixel number
		// but a number that has a weird ending, we need to convert it to pixels
		if ( !rnumpx.test( ret ) && rnum.test( ret ) ) {
			// remember the original values
			left = style.left;

			// put in the new values to get a computed value out
			if ( rsleft ) {
				elem.runtimestyle.left = elem.currentstyle.left;
			}
			style.left = name === "fontsize" ? "1em" : (ret || 0);
			ret = style.pixelleft + "px";

			// revert the changed values
			style.left = left;
			if ( rsleft ) {
				elem.runtimestyle.left = rsleft;
			}
		}

		return ret === "" ? "auto" : ret;
	};
}

curcss = getcomputedstyle || currentstyle;

function getwh( elem, name, extra ) {
	var which = name === "width" ? csswidth : cssheight,
		val = name === "width" ? elem.offsetwidth : elem.offsetheight;

	if ( extra === "border" ) {
		return val;
	}

	jquery.each( which, function() {
		if ( !extra ) {
			val -= parsefloat(jquery.css( elem, "padding" + this )) || 0;
		}

		if ( extra === "margin" ) {
			val += parsefloat(jquery.css( elem, "margin" + this )) || 0;

		} else {
			val -= parsefloat(jquery.css( elem, "border" + this + "width" )) || 0;
		}
	});

	return val;
}

if ( jquery.expr && jquery.expr.filters ) {
	jquery.expr.filters.hidden = function( elem ) {
		var width = elem.offsetwidth,
			height = elem.offsetheight;

		return (width === 0 && height === 0) || (!jquery.support.reliablehiddenoffsets && (elem.style.display || jquery.css( elem, "display" )) === "none");
	};

	jquery.expr.filters.visible = function( elem ) {
		return !jquery.expr.filters.hidden( elem );
	};
}




var r20 = /%20/g,
	rbracket = /\[\]$/,
	rcrlf = /\r?\n/g,
	rhash = /#.*$/,
	rheaders = /^(.*?):[ \t]*([^\r\n]*)\r?$/mg, // ie leaves an \r character at eol
	rinput = /^(?:color|date|datetime|email|hidden|month|number|password|range|search|tel|text|time|url|week)$/i,
	// #7653, #8125, #8152: local protocol detection
	rlocalprotocol = /(?:^file|^widget|\-extension):$/,
	rnocontent = /^(?:get|head)$/,
	rprotocol = /^\/\//,
	rquery = /\?/,
	rscript = /<script\b[^<]*(?:(?!<\/script>)<[^<]*)*<\/script>/gi,
	rselecttextarea = /^(?:select|textarea)/i,
	rspacesajax = /\s+/,
	rts = /([?&])_=[^&]*/,
	rucheaders = /(^|\-)([a-z])/g,
	rucheadersfunc = function( _, $1, $2 ) {
		return $1 + $2.touppercase();
	},
	rurl = /^([\w\+\.\-]+:)\/\/([^\/?#:]*)(?::(\d+))?/,

	// keep a copy of the old load method
	_load = jquery.fn.load,

	/* prefilters
	 * 1) they are useful to introduce custom datatypes (see ajax/jsonp.js for an example)
	 * 2) these are called:
	 *    - before asking for a transport
	 *    - after param serialization (s.data is a string if s.processdata is true)
	 * 3) key is the datatype
	 * 4) the catchall symbol "*" can be used
	 * 5) execution will start with transport datatype and then continue down to "*" if needed
	 */
	prefilters = {},

	/* transports bindings
	 * 1) key is the datatype
	 * 2) the catchall symbol "*" can be used
	 * 3) selection will start with transport datatype and then go to "*" if needed
	 */
	transports = {},

	// document location
	ajaxlocation,

	// document location segments
	ajaxlocparts;

// #8138, ie may throw an exception when accessing
// a field from document.location if document.domain has been set
try {
	ajaxlocation = document.location.href;
} catch( e ) {
	// use the href attribute of an a element
	// since ie will modify it given document.location
	ajaxlocation = document.createelement( "a" );
	ajaxlocation.href = "";
	ajaxlocation = ajaxlocation.href;
}

// segment location into parts
ajaxlocparts = rurl.exec( ajaxlocation.tolowercase() );

// base "constructor" for jquery.ajaxprefilter and jquery.ajaxtransport
function addtoprefiltersortransports( structure ) {

	// datatypeexpression is optional and defaults to "*"
	return function( datatypeexpression, func ) {

		if ( typeof datatypeexpression !== "string" ) {
			func = datatypeexpression;
			datatypeexpression = "*";
		}

		if ( jquery.isfunction( func ) ) {
			var datatypes = datatypeexpression.tolowercase().split( rspacesajax ),
				i = 0,
				length = datatypes.length,
				datatype,
				list,
				placebefore;

			// for each datatype in the datatypeexpression
			for(; i < length; i++ ) {
				datatype = datatypes[ i ];
				// we control if we're asked to add before
				// any existing element
				placebefore = /^\+/.test( datatype );
				if ( placebefore ) {
					datatype = datatype.substr( 1 ) || "*";
				}
				list = structure[ datatype ] = structure[ datatype ] || [];
				// then we add to the structure accordingly
				list[ placebefore ? "unshift" : "push" ]( func );
			}
		}
	};
}

//base inspection function for prefilters and transports
function inspectprefiltersortransports( structure, options, originaloptions, jqxhr,
		datatype /* internal */, inspected /* internal */ ) {

	datatype = datatype || options.datatypes[ 0 ];
	inspected = inspected || {};

	inspected[ datatype ] = true;

	var list = structure[ datatype ],
		i = 0,
		length = list ? list.length : 0,
		executeonly = ( structure === prefilters ),
		selection;

	for(; i < length && ( executeonly || !selection ); i++ ) {
		selection = list[ i ]( options, originaloptions, jqxhr );
		// if we got redirected to another datatype
		// we try there if executing only and not done already
		if ( typeof selection === "string" ) {
			if ( !executeonly || inspected[ selection ] ) {
				selection = undefined;
			} else {
				options.datatypes.unshift( selection );
				selection = inspectprefiltersortransports(
						structure, options, originaloptions, jqxhr, selection, inspected );
			}
		}
	}
	// if we're only executing or nothing was selected
	// we try the catchall datatype if not done already
	if ( ( executeonly || !selection ) && !inspected[ "*" ] ) {
		selection = inspectprefiltersortransports(
				structure, options, originaloptions, jqxhr, "*", inspected );
	}
	// unnecessary when only executing (prefilters)
	// but it'll be ignored by the caller in that case
	return selection;
}

jquery.fn.extend({
	load: function( url, params, callback ) {
		if ( typeof url !== "string" && _load ) {
			return _load.apply( this, arguments );

		// don't do a request if no elements are being requested
		} else if ( !this.length ) {
			return this;
		}

		var off = url.indexof( " " );
		if ( off >= 0 ) {
			var selector = url.slice( off, url.length );
			url = url.slice( 0, off );
		}

		// default to a get request
		var type = "get";

		// if the second parameter was provided
		if ( params ) {
			// if it's a function
			if ( jquery.isfunction( params ) ) {
				// we assume that it's the callback
				callback = params;
				params = undefined;

			// otherwise, build a param string
			} else if ( typeof params === "object" ) {
				params = jquery.param( params, jquery.ajaxsettings.traditional );
				type = "post";
			}
		}

		var self = this;

		// request the remote document
		jquery.ajax({
			url: url,
			type: type,
			datatype: "html",
			data: params,
			// complete callback (responsetext is used internally)
			complete: function( jqxhr, status, responsetext ) {
				// store the response as specified by the jqxhr object
				responsetext = jqxhr.responsetext;
				// if successful, inject the html into all the matched elements
				if ( jqxhr.isresolved() ) {
					// #4825: get the actual response in case
					// a datafilter is present in ajaxsettings
					jqxhr.done(function( r ) {
						responsetext = r;
					});
					// see if a selector was specified
					self.html( selector ?
						// create a dummy div to hold the results
						jquery("<div>")
							// inject the contents of the document in, removing the scripts
							// to avoid any 'permission denied' errors in ie
							.append(responsetext.replace(rscript, ""))

							// locate the specified elements
							.find(selector) :

						// if not, just inject the full result
						responsetext );
				}

				if ( callback ) {
					self.each( callback, [ responsetext, status, jqxhr ] );
				}
			}
		});

		return this;
	},

	serialize: function() {
		return jquery.param( this.serializearray() );
	},

	serializearray: function() {
		return this.map(function(){
			return this.elements ? jquery.makearray( this.elements ) : this;
		})
		.filter(function(){
			return this.name && !this.disabled &&
				( this.checked || rselecttextarea.test( this.nodename ) ||
					rinput.test( this.type ) );
		})
		.map(function( i, elem ){
			var val = jquery( this ).val();

			return val == null ?
				null :
				jquery.isarray( val ) ?
					jquery.map( val, function( val, i ){
						return { name: elem.name, value: val.replace( rcrlf, "\r\n" ) };
					}) :
					{ name: elem.name, value: val.replace( rcrlf, "\r\n" ) };
		}).get();
	}
});

// attach a bunch of functions for handling common ajax events
jquery.each( "ajaxstart ajaxstop ajaxcomplete ajaxerror ajaxsuccess ajaxsend".split( " " ), function( i, o ){
	jquery.fn[ o ] = function( f ){
		return this.bind( o, f );
	};
} );

jquery.each( [ "get", "post" ], function( i, method ) {
	jquery[ method ] = function( url, data, callback, type ) {
		// shift arguments if data argument was omitted
		if ( jquery.isfunction( data ) ) {
			type = type || callback;
			callback = data;
			data = undefined;
		}

		return jquery.ajax({
			type: method,
			url: url,
			data: data,
			success: callback,
			datatype: type
		});
	};
} );

jquery.extend({

	getscript: function( url, callback ) {
		return jquery.get( url, undefined, callback, "script" );
	},

	getjson: function( url, data, callback ) {
		return jquery.get( url, data, callback, "json" );
	},

	// creates a full fledged settings object into target
	// with both ajaxsettings and settings fields.
	// if target is omitted, writes into ajaxsettings.
	ajaxsetup: function ( target, settings ) {
		if ( !settings ) {
			// only one parameter, we extend ajaxsettings
			settings = target;
			target = jquery.extend( true, jquery.ajaxsettings, settings );
		} else {
			// target was provided, we extend into it
			jquery.extend( true, target, jquery.ajaxsettings, settings );
		}
		// flatten fields we don't want deep extended
		for( var field in { context: 1, url: 1 } ) {
			if ( field in settings ) {
				target[ field ] = settings[ field ];
			} else if( field in jquery.ajaxsettings ) {
				target[ field ] = jquery.ajaxsettings[ field ];
			}
		}
		return target;
	},

	ajaxsettings: {
		url: ajaxlocation,
		islocal: rlocalprotocol.test( ajaxlocparts[ 1 ] ),
		global: true,
		type: "get",
		contenttype: "application/x-www-form-urlencoded",
		processdata: true,
		async: true,
		/*
		timeout: 0,
		data: null,
		datatype: null,
		username: null,
		password: null,
		cache: null,
		traditional: false,
		headers: {},
		crossdomain: null,
		*/

		accepts: {
			xml: "application/xml, text/xml",
			html: "text/html",
			text: "text/plain",
			json: "application/json, text/javascript",
			"*": "*/*"
		},

		contents: {
			xml: /xml/,
			html: /html/,
			json: /json/
		},

		responsefields: {
			xml: "responsexml",
			text: "responsetext"
		},

		// list of data converters
		// 1) key format is "source_type destination_type" (a single space in-between)
		// 2) the catchall symbol "*" can be used for source_type
		converters: {

			// convert anything to text
			"* text": window.string,

			// text to html (true = no transformation)
			"text html": true,

			// evaluate text as a json expression
			"text json": jquery.parsejson,

			// parse text as xml
			"text xml": jquery.parsexml
		}
	},

	ajaxprefilter: addtoprefiltersortransports( prefilters ),
	ajaxtransport: addtoprefiltersortransports( transports ),

	// main method
	ajax: function( url, options ) {

		// if url is an object, simulate pre-1.5 signature
		if ( typeof url === "object" ) {
			options = url;
			url = undefined;
		}

		// force options to be an object
		options = options || {};

		var // create the final options object
			s = jquery.ajaxsetup( {}, options ),
			// callbacks context
			callbackcontext = s.context || s,
			// context for global events
			// it's the callbackcontext if one was provided in the options
			// and if it's a dom node or a jquery collection
			globaleventcontext = callbackcontext !== s &&
				( callbackcontext.nodetype || callbackcontext instanceof jquery ) ?
						jquery( callbackcontext ) : jquery.event,
			// deferreds
			deferred = jquery.deferred(),
			completedeferred = jquery._deferred(),
			// status-dependent callbacks
			statuscode = s.statuscode || {},
			// ifmodified key
			ifmodifiedkey,
			// headers (they are sent all at once)
			requestheaders = {},
			// response headers
			responseheadersstring,
			responseheaders,
			// transport
			transport,
			// timeout handle
			timeouttimer,
			// cross-domain detection vars
			parts,
			// the jqxhr state
			state = 0,
			// to know if global events are to be dispatched
			fireglobals,
			// loop variable
			i,
			// fake xhr
			jqxhr = {

				readystate: 0,

				// caches the header
				setrequestheader: function( name, value ) {
					if ( !state ) {
						requestheaders[ name.tolowercase().replace( rucheaders, rucheadersfunc ) ] = value;
					}
					return this;
				},

				// raw string
				getallresponseheaders: function() {
					return state === 2 ? responseheadersstring : null;
				},

				// builds headers hashtable if needed
				getresponseheader: function( key ) {
					var match;
					if ( state === 2 ) {
						if ( !responseheaders ) {
							responseheaders = {};
							while( ( match = rheaders.exec( responseheadersstring ) ) ) {
								responseheaders[ match[1].tolowercase() ] = match[ 2 ];
							}
						}
						match = responseheaders[ key.tolowercase() ];
					}
					return match === undefined ? null : match;
				},

				// overrides response content-type header
				overridemimetype: function( type ) {
					if ( !state ) {
						s.mimetype = type;
					}
					return this;
				},

				// cancel the request
				abort: function( statustext ) {
					statustext = statustext || "abort";
					if ( transport ) {
						transport.abort( statustext );
					}
					done( 0, statustext );
					return this;
				}
			};

		// callback for when everything is done
		// it is defined here because jslint complains if it is declared
		// at the end of the function (which would be more logical and readable)
		function done( status, statustext, responses, headers ) {

			// called once
			if ( state === 2 ) {
				return;
			}

			// state is "done" now
			state = 2;

			// clear timeout if it exists
			if ( timeouttimer ) {
				cleartimeout( timeouttimer );
			}

			// dereference transport for early garbage collection
			// (no matter how long the jqxhr object will be used)
			transport = undefined;

			// cache response headers
			responseheadersstring = headers || "";

			// set readystate
			jqxhr.readystate = status ? 4 : 0;

			var issuccess,
				success,
				error,
				response = responses ? ajaxhandleresponses( s, jqxhr, responses ) : undefined,
				lastmodified,
				etag;

			// if successful, handle type chaining
			if ( status >= 200 && status < 300 || status === 304 ) {

				// set the if-modified-since and/or if-none-match header, if in ifmodified mode.
				if ( s.ifmodified ) {

					if ( ( lastmodified = jqxhr.getresponseheader( "last-modified" ) ) ) {
						jquery.lastmodified[ ifmodifiedkey ] = lastmodified;
					}
					if ( ( etag = jqxhr.getresponseheader( "etag" ) ) ) {
						jquery.etag[ ifmodifiedkey ] = etag;
					}
				}

				// if not modified
				if ( status === 304 ) {

					statustext = "notmodified";
					issuccess = true;

				// if we have data
				} else {

					try {
						success = ajaxconvert( s, response );
						statustext = "success";
						issuccess = true;
					} catch(e) {
						// we have a parsererror
						statustext = "parsererror";
						error = e;
					}
				}
			} else {
				// we extract error from statustext
				// then normalize statustext and status for non-aborts
				error = statustext;
				if( !statustext || status ) {
					statustext = "error";
					if ( status < 0 ) {
						status = 0;
					}
				}
			}

			// set data for the fake xhr object
			jqxhr.status = status;
			jqxhr.statustext = statustext;

			// success/error
			if ( issuccess ) {
				deferred.resolvewith( callbackcontext, [ success, statustext, jqxhr ] );
			} else {
				deferred.rejectwith( callbackcontext, [ jqxhr, statustext, error ] );
			}

			// status-dependent callbacks
			jqxhr.statuscode( statuscode );
			statuscode = undefined;

			if ( fireglobals ) {
				globaleventcontext.trigger( "ajax" + ( issuccess ? "success" : "error" ),
						[ jqxhr, s, issuccess ? success : error ] );
			}

			// complete
			completedeferred.resolvewith( callbackcontext, [ jqxhr, statustext ] );

			if ( fireglobals ) {
				globaleventcontext.trigger( "ajaxcomplete", [ jqxhr, s] );
				// handle the global ajax counter
				if ( !( --jquery.active ) ) {
					jquery.event.trigger( "ajaxstop" );
				}
			}
		}

		// attach deferreds
		deferred.promise( jqxhr );
		jqxhr.success = jqxhr.done;
		jqxhr.error = jqxhr.fail;
		jqxhr.complete = completedeferred.done;

		// status-dependent callbacks
		jqxhr.statuscode = function( map ) {
			if ( map ) {
				var tmp;
				if ( state < 2 ) {
					for( tmp in map ) {
						statuscode[ tmp ] = [ statuscode[tmp], map[tmp] ];
					}
				} else {
					tmp = map[ jqxhr.status ];
					jqxhr.then( tmp, tmp );
				}
			}
			return this;
		};

		// remove hash character (#7531: and string promotion)
		// add protocol if not provided (#5866: ie7 issue with protocol-less urls)
		// we also use the url parameter if available
		s.url = ( ( url || s.url ) + "" ).replace( rhash, "" ).replace( rprotocol, ajaxlocparts[ 1 ] + "//" );

		// extract datatypes list
		s.datatypes = jquery.trim( s.datatype || "*" ).tolowercase().split( rspacesajax );

		// determine if a cross-domain request is in order
		if ( !s.crossdomain ) {
			parts = rurl.exec( s.url.tolowercase() );
			s.crossdomain = !!( parts &&
				( parts[ 1 ] != ajaxlocparts[ 1 ] || parts[ 2 ] != ajaxlocparts[ 2 ] ||
					( parts[ 3 ] || ( parts[ 1 ] === "http:" ? 80 : 443 ) ) !=
						( ajaxlocparts[ 3 ] || ( ajaxlocparts[ 1 ] === "http:" ? 80 : 443 ) ) )
			);
		}

		// convert data if not already a string
		if ( s.data && s.processdata && typeof s.data !== "string" ) {
			s.data = jquery.param( s.data, s.traditional );
		}

		// apply prefilters
		inspectprefiltersortransports( prefilters, s, options, jqxhr );

		// if request was aborted inside a prefiler, stop there
		if ( state === 2 ) {
			return false;
		}

		// we can fire global events as of now if asked to
		fireglobals = s.global;

		// uppercase the type
		s.type = s.type.touppercase();

		// determine if request has content
		s.hascontent = !rnocontent.test( s.type );

		// watch for a new set of requests
		if ( fireglobals && jquery.active++ === 0 ) {
			jquery.event.trigger( "ajaxstart" );
		}

		// more options handling for requests with no content
		if ( !s.hascontent ) {

			// if data is available, append data to url
			if ( s.data ) {
				s.url += ( rquery.test( s.url ) ? "&" : "?" ) + s.data;
			}

			// get ifmodifiedkey before adding the anti-cache parameter
			ifmodifiedkey = s.url;

			// add anti-cache in url if needed
			if ( s.cache === false ) {

				var ts = jquery.now(),
					// try replacing _= if it is there
					ret = s.url.replace( rts, "$1_=" + ts );

				// if nothing was replaced, add timestamp to the end
				s.url = ret + ( (ret === s.url ) ? ( rquery.test( s.url ) ? "&" : "?" ) + "_=" + ts : "" );
			}
		}

		// set the correct header, if data is being sent
		if ( s.data && s.hascontent && s.contenttype !== false || options.contenttype ) {
			requestheaders[ "content-type" ] = s.contenttype;
		}

		// set the if-modified-since and/or if-none-match header, if in ifmodified mode.
		if ( s.ifmodified ) {
			ifmodifiedkey = ifmodifiedkey || s.url;
			if ( jquery.lastmodified[ ifmodifiedkey ] ) {
				requestheaders[ "if-modified-since" ] = jquery.lastmodified[ ifmodifiedkey ];
			}
			if ( jquery.etag[ ifmodifiedkey ] ) {
				requestheaders[ "if-none-match" ] = jquery.etag[ ifmodifiedkey ];
			}
		}

		// set the accepts header for the server, depending on the datatype
		requestheaders.accept = s.datatypes[ 0 ] && s.accepts[ s.datatypes[0] ] ?
			s.accepts[ s.datatypes[0] ] + ( s.datatypes[ 0 ] !== "*" ? ", */*; q=0.01" : "" ) :
			s.accepts[ "*" ];

		// check for headers option
		for ( i in s.headers ) {
			jqxhr.setrequestheader( i, s.headers[ i ] );
		}

		// allow custom headers/mimetypes and early abort
		if ( s.beforesend && ( s.beforesend.call( callbackcontext, jqxhr, s ) === false || state === 2 ) ) {
				// abort if not done already
				jqxhr.abort();
				return false;

		}

		// install callbacks on deferreds
		for ( i in { success: 1, error: 1, complete: 1 } ) {
			jqxhr[ i ]( s[ i ] );
		}

		// get transport
		transport = inspectprefiltersortransports( transports, s, options, jqxhr );

		// if no transport, we auto-abort
		if ( !transport ) {
			done( -1, "no transport" );
		} else {
			jqxhr.readystate = 1;
			// send global event
			if ( fireglobals ) {
				globaleventcontext.trigger( "ajaxsend", [ jqxhr, s ] );
			}
			// timeout
			if ( s.async && s.timeout > 0 ) {
				timeouttimer = settimeout( function(){
					jqxhr.abort( "timeout" );
				}, s.timeout );
			}

			try {
				state = 1;
				transport.send( requestheaders, done );
			} catch (e) {
				// propagate exception as error if not done
				if ( status < 2 ) {
					done( -1, e );
				// simply rethrow otherwise
				} else {
					jquery.error( e );
				}
			}
		}

		return jqxhr;
	},

	// serialize an array of form elements or a set of
	// key/values into a query string
	param: function( a, traditional ) {
		var s = [],
			add = function( key, value ) {
				// if value is a function, invoke it and return its value
				value = jquery.isfunction( value ) ? value() : value;
				s[ s.length ] = encodeuricomponent( key ) + "=" + encodeuricomponent( value );
			};

		// set traditional to true for jquery <= 1.3.2 behavior.
		if ( traditional === undefined ) {
			traditional = jquery.ajaxsettings.traditional;
		}

		// if an array was passed in, assume that it is an array of form elements.
		if ( jquery.isarray( a ) || ( a.jquery && !jquery.isplainobject( a ) ) ) {
			// serialize the form elements
			jquery.each( a, function() {
				add( this.name, this.value );
			} );

		} else {
			// if traditional, encode the "old" way (the way 1.3.2 or older
			// did it), otherwise encode params recursively.
			for ( var prefix in a ) {
				buildparams( prefix, a[ prefix ], traditional, add );
			}
		}

		// return the resulting serialization
		return s.join( "&" ).replace( r20, "+" );
	}
});

function buildparams( prefix, obj, traditional, add ) {
	if ( jquery.isarray( obj ) && obj.length ) {
		// serialize array item.
		jquery.each( obj, function( i, v ) {
			if ( traditional || rbracket.test( prefix ) ) {
				// treat each array item as a scalar.
				add( prefix, v );

			} else {
				// if array item is non-scalar (array or object), encode its
				// numeric index to resolve deserialization ambiguity issues.
				// note that rack (as of 1.0.0) can't currently deserialize
				// nested arrays properly, and attempting to do so may cause
				// a server error. possible fixes are to modify rack's
				// deserialization algorithm or to provide an option or flag
				// to force array serialization to be shallow.
				buildparams( prefix + "[" + ( typeof v === "object" || jquery.isarray(v) ? i : "" ) + "]", v, traditional, add );
			}
		});

	} else if ( !traditional && obj != null && typeof obj === "object" ) {
		// if we see an array here, it is empty and should be treated as an empty
		// object
		if ( jquery.isarray( obj ) || jquery.isemptyobject( obj ) ) {
			add( prefix, "" );

		// serialize object item.
		} else {
			for ( var name in obj ) {
				buildparams( prefix + "[" + name + "]", obj[ name ], traditional, add );
			}
		}

	} else {
		// serialize scalar item.
		add( prefix, obj );
	}
}

// this is still on the jquery object... for now
// want to move this to jquery.ajax some day
jquery.extend({

	// counter for holding the number of active queries
	active: 0,

	// last-modified header cache for next request
	lastmodified: {},
	etag: {}

});

/* handles responses to an ajax request:
 * - sets all responsexxx fields accordingly
 * - finds the right datatype (mediates between content-type and expected datatype)
 * - returns the corresponding response
 */
function ajaxhandleresponses( s, jqxhr, responses ) {

	var contents = s.contents,
		datatypes = s.datatypes,
		responsefields = s.responsefields,
		ct,
		type,
		finaldatatype,
		firstdatatype;

	// fill responsexxx fields
	for( type in responsefields ) {
		if ( type in responses ) {
			jqxhr[ responsefields[type] ] = responses[ type ];
		}
	}

	// remove auto datatype and get content-type in the process
	while( datatypes[ 0 ] === "*" ) {
		datatypes.shift();
		if ( ct === undefined ) {
			ct = s.mimetype || jqxhr.getresponseheader( "content-type" );
		}
	}

	// check if we're dealing with a known content-type
	if ( ct ) {
		for ( type in contents ) {
			if ( contents[ type ] && contents[ type ].test( ct ) ) {
				datatypes.unshift( type );
				break;
			}
		}
	}

	// check to see if we have a response for the expected datatype
	if ( datatypes[ 0 ] in responses ) {
		finaldatatype = datatypes[ 0 ];
	} else {
		// try convertible datatypes
		for ( type in responses ) {
			if ( !datatypes[ 0 ] || s.converters[ type + " " + datatypes[0] ] ) {
				finaldatatype = type;
				break;
			}
			if ( !firstdatatype ) {
				firstdatatype = type;
			}
		}
		// or just use first one
		finaldatatype = finaldatatype || firstdatatype;
	}

	// if we found a datatype
	// we add the datatype to the list if needed
	// and return the corresponding response
	if ( finaldatatype ) {
		if ( finaldatatype !== datatypes[ 0 ] ) {
			datatypes.unshift( finaldatatype );
		}
		return responses[ finaldatatype ];
	}
}

// chain conversions given the request and the original response
function ajaxconvert( s, response ) {

	// apply the datafilter if provided
	if ( s.datafilter ) {
		response = s.datafilter( response, s.datatype );
	}

	var datatypes = s.datatypes,
		converters = {},
		i,
		key,
		length = datatypes.length,
		tmp,
		// current and previous datatypes
		current = datatypes[ 0 ],
		prev,
		// conversion expression
		conversion,
		// conversion function
		conv,
		// conversion functions (transitive conversion)
		conv1,
		conv2;

	// for each datatype in the chain
	for( i = 1; i < length; i++ ) {

		// create converters map
		// with lowercased keys
		if ( i === 1 ) {
			for( key in s.converters ) {
				if( typeof key === "string" ) {
					converters[ key.tolowercase() ] = s.converters[ key ];
				}
			}
		}

		// get the datatypes
		prev = current;
		current = datatypes[ i ];

		// if current is auto datatype, update it to prev
		if( current === "*" ) {
			current = prev;
		// if no auto and datatypes are actually different
		} else if ( prev !== "*" && prev !== current ) {

			// get the converter
			conversion = prev + " " + current;
			conv = converters[ conversion ] || converters[ "* " + current ];

			// if there is no direct converter, search transitively
			if ( !conv ) {
				conv2 = undefined;
				for( conv1 in converters ) {
					tmp = conv1.split( " " );
					if ( tmp[ 0 ] === prev || tmp[ 0 ] === "*" ) {
						conv2 = converters[ tmp[1] + " " + current ];
						if ( conv2 ) {
							conv1 = converters[ conv1 ];
							if ( conv1 === true ) {
								conv = conv2;
							} else if ( conv2 === true ) {
								conv = conv1;
							}
							break;
						}
					}
				}
			}
			// if we found no converter, dispatch an error
			if ( !( conv || conv2 ) ) {
				jquery.error( "no conversion from " + conversion.replace(" "," to ") );
			}
			// if found converter is not an equivalence
			if ( conv !== true ) {
				// convert with 1 or 2 converters accordingly
				response = conv ? conv( response ) : conv2( conv1(response) );
			}
		}
	}
	return response;
}




var jsc = jquery.now(),
	jsre = /(\=)\?(&|$)|()\?\?()/i;

// default jsonp settings
jquery.ajaxsetup({
	jsonp: "callback",
	jsonpcallback: function() {
		return jquery.expando + "_" + ( jsc++ );
	}
});

// detect, normalize options and install callbacks for jsonp requests
jquery.ajaxprefilter( "json jsonp", function( s, originalsettings, jqxhr ) {

	var dataisstring = ( typeof s.data === "string" );

	if ( s.datatypes[ 0 ] === "jsonp" ||
		originalsettings.jsonpcallback ||
		originalsettings.jsonp != null ||
		s.jsonp !== false && ( jsre.test( s.url ) ||
				dataisstring && jsre.test( s.data ) ) ) {

		var responsecontainer,
			jsonpcallback = s.jsonpcallback =
				jquery.isfunction( s.jsonpcallback ) ? s.jsonpcallback() : s.jsonpcallback,
			previous = window[ jsonpcallback ],
			url = s.url,
			data = s.data,
			replace = "$1" + jsonpcallback + "$2",
			cleanup = function() {
				// set callback back to previous value
				window[ jsonpcallback ] = previous;
				// call if it was a function and we have a response
				if ( responsecontainer && jquery.isfunction( previous ) ) {
					window[ jsonpcallback ]( responsecontainer[ 0 ] );
				}
			};

		if ( s.jsonp !== false ) {
			url = url.replace( jsre, replace );
			if ( s.url === url ) {
				if ( dataisstring ) {
					data = data.replace( jsre, replace );
				}
				if ( s.data === data ) {
					// add callback manually
					url += (/\?/.test( url ) ? "&" : "?") + s.jsonp + "=" + jsonpcallback;
				}
			}
		}

		s.url = url;
		s.data = data;

		// install callback
		window[ jsonpcallback ] = function( response ) {
			responsecontainer = [ response ];
		};

		// install cleanup function
		jqxhr.then( cleanup, cleanup );

		// use data converter to retrieve json after script execution
		s.converters["script json"] = function() {
			if ( !responsecontainer ) {
				jquery.error( jsonpcallback + " was not called" );
			}
			return responsecontainer[ 0 ];
		};

		// force json datatype
		s.datatypes[ 0 ] = "json";

		// delegate to script
		return "script";
	}
} );




// install script datatype
jquery.ajaxsetup({
	accepts: {
		script: "text/javascript, application/javascript, application/ecmascript, application/x-ecmascript"
	},
	contents: {
		script: /javascript|ecmascript/
	},
	converters: {
		"text script": function( text ) {
			jquery.globaleval( text );
			return text;
		}
	}
});

// handle cache's special case and global
jquery.ajaxprefilter( "script", function( s ) {
	if ( s.cache === undefined ) {
		s.cache = false;
	}
	if ( s.crossdomain ) {
		s.type = "get";
		s.global = false;
	}
} );

// bind script tag hack transport
jquery.ajaxtransport( "script", function(s) {

	// this transport only deals with cross domain requests
	if ( s.crossdomain ) {

		var script,
			head = document.head || document.getelementsbytagname( "head" )[0] || document.documentelement;

		return {

			send: function( _, callback ) {

				script = document.createelement( "script" );

				script.async = "async";

				if ( s.scriptcharset ) {
					script.charset = s.scriptcharset;
				}

				script.src = s.url;

				// attach handlers for all browsers
				script.onload = script.onreadystatechange = function( _, isabort ) {

					if ( !script.readystate || /loaded|complete/.test( script.readystate ) ) {

						// handle memory leak in ie
						script.onload = script.onreadystatechange = null;

						// remove the script
						if ( head && script.parentnode ) {
							head.removechild( script );
						}

						// dereference the script
						script = undefined;

						// callback if not abort
						if ( !isabort ) {
							callback( 200, "success" );
						}
					}
				};
				// use insertbefore instead of appendchild  to circumvent an ie6 bug.
				// this arises when a base node is used (#2709 and #4378).
				head.insertbefore( script, head.firstchild );
			},

			abort: function() {
				if ( script ) {
					script.onload( 0, 1 );
				}
			}
		};
	}
} );




var // #5280: next active xhr id and list of active xhrs' callbacks
	xhrid = jquery.now(),
	xhrcallbacks,

	// xhr used to determine supports properties
	testxhr;

// #5280: internet explorer will keep connections alive if we don't abort on unload
function xhronunloadabort() {
	jquery( window ).unload(function() {
		// abort all pending requests
		for ( var key in xhrcallbacks ) {
			xhrcallbacks[ key ]( 0, 1 );
		}
	});
}

// functions to create xhrs
function createstandardxhr() {
	try {
		return new window.xmlhttprequest();
	} catch( e ) {}
}

function createactivexhr() {
	try {
		return new window.activexobject( "microsoft.xmlhttp" );
	} catch( e ) {}
}

// create the request object
// (this is still attached to ajaxsettings for backward compatibility)
jquery.ajaxsettings.xhr = window.activexobject ?
	/* microsoft failed to properly
	 * implement the xmlhttprequest in ie7 (can't request local files),
	 * so we use the activexobject when it is available
	 * additionally xmlhttprequest can be disabled in ie7/ie8 so
	 * we need a fallback.
	 */
	function() {
		return !this.islocal && createstandardxhr() || createactivexhr();
	} :
	// for all other browsers, use the standard xmlhttprequest object
	createstandardxhr;

// test if we can create an xhr object
testxhr = jquery.ajaxsettings.xhr();
jquery.support.ajax = !!testxhr;

// does this browser support crossdomain xhr requests
jquery.support.cors = testxhr && ( "withcredentials" in testxhr );

// no need for the temporary xhr anymore
testxhr = undefined;

// create transport if the browser can provide an xhr
if ( jquery.support.ajax ) {

	jquery.ajaxtransport(function( s ) {
		// cross domain only allowed if supported through xmlhttprequest
		if ( !s.crossdomain || jquery.support.cors ) {

			var callback;

			return {
				send: function( headers, complete ) {

					// get a new xhr
					var xhr = s.xhr(),
						handle,
						i;

					// open the socket
					// passing null username, generates a login popup on opera (#2865)
					if ( s.username ) {
						xhr.open( s.type, s.url, s.async, s.username, s.password );
					} else {
						xhr.open( s.type, s.url, s.async );
					}

					// apply custom fields if provided
					if ( s.xhrfields ) {
						for ( i in s.xhrfields ) {
							xhr[ i ] = s.xhrfields[ i ];
						}
					}

					// override mime type if needed
					if ( s.mimetype && xhr.overridemimetype ) {
						xhr.overridemimetype( s.mimetype );
					}

					// requested-with header
					// not set for crossdomain requests with no content
					// (see why at http://trac.dojotoolkit.org/ticket/9486)
					// won't change header if already provided
					if ( !( s.crossdomain && !s.hascontent ) && !headers["x-requested-with"] ) {
						headers[ "x-requested-with" ] = "xmlhttprequest";
					}

					// need an extra try/catch for cross domain requests in firefox 3
					try {
						for ( i in headers ) {
							xhr.setrequestheader( i, headers[ i ] );
						}
					} catch( _ ) {}

					// do send the request
					// this may raise an exception which is actually
					// handled in jquery.ajax (so no try/catch here)
					xhr.send( ( s.hascontent && s.data ) || null );

					// listener
					callback = function( _, isabort ) {

						var status,
							statustext,
							responseheaders,
							responses,
							xml;

						// firefox throws exceptions when accessing properties
						// of an xhr when a network error occured
						// http://helpful.knobs-dials.com/index.php/component_returned_failure_code:_0x80040111_(ns_error_not_available)
						try {

							// was never called and is aborted or complete
							if ( callback && ( isabort || xhr.readystate === 4 ) ) {

								// only called once
								callback = undefined;

								// do not keep as active anymore
								if ( handle ) {
									xhr.onreadystatechange = jquery.noop;
									delete xhrcallbacks[ handle ];
								}

								// if it's an abort
								if ( isabort ) {
									// abort it manually if needed
									if ( xhr.readystate !== 4 ) {
										xhr.abort();
									}
								} else {
									status = xhr.status;
									responseheaders = xhr.getallresponseheaders();
									responses = {};
									xml = xhr.responsexml;

									// construct response list
									if ( xml && xml.documentelement /* #4958 */ ) {
										responses.xml = xml;
									}
									responses.text = xhr.responsetext;

									// firefox throws an exception when accessing
									// statustext for faulty cross-domain requests
									try {
										statustext = xhr.statustext;
									} catch( e ) {
										// we normalize with webkit giving an empty statustext
										statustext = "";
									}

									// filter status for non standard behaviors

									// if the request is local and we have data: assume a success
									// (success with no data won't get notified, that's the best we
									// can do given current implementations)
									if ( !status && s.islocal && !s.crossdomain ) {
										status = responses.text ? 200 : 404;
									// ie - #1450: sometimes returns 1223 when it should be 204
									} else if ( status === 1223 ) {
										status = 204;
									}
								}
							}
						} catch( firefoxaccessexception ) {
							if ( !isabort ) {
								complete( -1, firefoxaccessexception );
							}
						}

						// call complete if needed
						if ( responses ) {
							complete( status, statustext, responses, responseheaders );
						}
					};

					// if we're in sync mode or it's in cache
					// and has been retrieved directly (ie6 & ie7)
					// we need to manually fire the callback
					if ( !s.async || xhr.readystate === 4 ) {
						callback();
					} else {
						// create the active xhrs callbacks list if needed
						// and attach the unload handler
						if ( !xhrcallbacks ) {
							xhrcallbacks = {};
							xhronunloadabort();
						}
						// add to list of active xhrs callbacks
						handle = xhrid++;
						xhr.onreadystatechange = xhrcallbacks[ handle ] = callback;
					}
				},

				abort: function() {
					if ( callback ) {
						callback(0,1);
					}
				}
			};
		}
	});
}




var elemdisplay = {},
	rfxtypes = /^(?:toggle|show|hide)$/,
	rfxnum = /^([+\-]=)?([\d+.\-]+)([a-z%]*)$/i,
	timerid,
	fxattrs = [
		// height animations
		[ "height", "margintop", "marginbottom", "paddingtop", "paddingbottom" ],
		// width animations
		[ "width", "marginleft", "marginright", "paddingleft", "paddingright" ],
		// opacity animations
		[ "opacity" ]
	];

jquery.fn.extend({
	show: function( speed, easing, callback ) {
		var elem, display;

		if ( speed || speed === 0 ) {
			return this.animate( genfx("show", 3), speed, easing, callback);

		} else {
			for ( var i = 0, j = this.length; i < j; i++ ) {
				elem = this[i];
				display = elem.style.display;

				// reset the inline display of this element to learn if it is
				// being hidden by cascaded rules or not
				if ( !jquery._data(elem, "olddisplay") && display === "none" ) {
					display = elem.style.display = "";
				}

				// set elements which have been overridden with display: none
				// in a stylesheet to whatever the default browser style is
				// for such an element
				if ( display === "" && jquery.css( elem, "display" ) === "none" ) {
					jquery._data(elem, "olddisplay", defaultdisplay(elem.nodename));
				}
			}

			// set the display of most of the elements in a second loop
			// to avoid the constant reflow
			for ( i = 0; i < j; i++ ) {
				elem = this[i];
				display = elem.style.display;

				if ( display === "" || display === "none" ) {
					elem.style.display = jquery._data(elem, "olddisplay") || "";
				}
			}

			return this;
		}
	},

	hide: function( speed, easing, callback ) {
		if ( speed || speed === 0 ) {
			return this.animate( genfx("hide", 3), speed, easing, callback);

		} else {
			for ( var i = 0, j = this.length; i < j; i++ ) {
				var display = jquery.css( this[i], "display" );

				if ( display !== "none" && !jquery._data( this[i], "olddisplay" ) ) {
					jquery._data( this[i], "olddisplay", display );
				}
			}

			// set the display of the elements in a second loop
			// to avoid the constant reflow
			for ( i = 0; i < j; i++ ) {
				this[i].style.display = "none";
			}

			return this;
		}
	},

	// save the old toggle function
	_toggle: jquery.fn.toggle,

	toggle: function( fn, fn2, callback ) {
		var bool = typeof fn === "boolean";

		if ( jquery.isfunction(fn) && jquery.isfunction(fn2) ) {
			this._toggle.apply( this, arguments );

		} else if ( fn == null || bool ) {
			this.each(function() {
				var state = bool ? fn : jquery(this).is(":hidden");
				jquery(this)[ state ? "show" : "hide" ]();
			});

		} else {
			this.animate(genfx("toggle", 3), fn, fn2, callback);
		}

		return this;
	},

	fadeto: function( speed, to, easing, callback ) {
		return this.filter(":hidden").css("opacity", 0).show().end()
					.animate({opacity: to}, speed, easing, callback);
	},

	animate: function( prop, speed, easing, callback ) {
		var optall = jquery.speed(speed, easing, callback);

		if ( jquery.isemptyobject( prop ) ) {
			return this.each( optall.complete );
		}

		return this[ optall.queue === false ? "each" : "queue" ](function() {
			// xxx 'this' does not always have a nodename when running the
			// test suite

			var opt = jquery.extend({}, optall), p,
				iselement = this.nodetype === 1,
				hidden = iselement && jquery(this).is(":hidden"),
				self = this;

			for ( p in prop ) {
				var name = jquery.camelcase( p );

				if ( p !== name ) {
					prop[ name ] = prop[ p ];
					delete prop[ p ];
					p = name;
				}

				if ( prop[p] === "hide" && hidden || prop[p] === "show" && !hidden ) {
					return opt.complete.call(this);
				}

				if ( iselement && ( p === "height" || p === "width" ) ) {
					// make sure that nothing sneaks out
					// record all 3 overflow attributes because ie does not
					// change the overflow attribute when overflowx and
					// overflowy are set to the same value
					opt.overflow = [ this.style.overflow, this.style.overflowx, this.style.overflowy ];

					// set display property to inline-block for height/width
					// animations on inline elements that are having width/height
					// animated
					if ( jquery.css( this, "display" ) === "inline" &&
							jquery.css( this, "float" ) === "none" ) {
						if ( !jquery.support.inlineblockneedslayout ) {
							this.style.display = "inline-block";

						} else {
							var display = defaultdisplay(this.nodename);

							// inline-level elements accept inline-block;
							// block-level elements need to be inline with layout
							if ( display === "inline" ) {
								this.style.display = "inline-block";

							} else {
								this.style.display = "inline";
								this.style.zoom = 1;
							}
						}
					}
				}

				if ( jquery.isarray( prop[p] ) ) {
					// create (if needed) and add to specialeasing
					(opt.specialeasing = opt.specialeasing || {})[p] = prop[p][1];
					prop[p] = prop[p][0];
				}
			}

			if ( opt.overflow != null ) {
				this.style.overflow = "hidden";
			}

			opt.curanim = jquery.extend({}, prop);

			jquery.each( prop, function( name, val ) {
				var e = new jquery.fx( self, opt, name );

				if ( rfxtypes.test(val) ) {
					e[ val === "toggle" ? hidden ? "show" : "hide" : val ]( prop );

				} else {
					var parts = rfxnum.exec(val),
						start = e.cur();

					if ( parts ) {
						var end = parsefloat( parts[2] ),
							unit = parts[3] || ( jquery.cssnumber[ name ] ? "" : "px" );

						// we need to compute starting value
						if ( unit !== "px" ) {
							jquery.style( self, name, (end || 1) + unit);
							start = ((end || 1) / e.cur()) * start;
							jquery.style( self, name, start + unit);
						}

						// if a +=/-= token was provided, we're doing a relative animation
						if ( parts[1] ) {
							end = ((parts[1] === "-=" ? -1 : 1) * end) + start;
						}

						e.custom( start, end, unit );

					} else {
						e.custom( start, val, "" );
					}
				}
			});

			// for js strict compliance
			return true;
		});
	},

	stop: function( clearqueue, gotoend ) {
		var timers = jquery.timers;

		if ( clearqueue ) {
			this.queue([]);
		}

		this.each(function() {
			// go in reverse order so anything added to the queue during the loop is ignored
			for ( var i = timers.length - 1; i >= 0; i-- ) {
				if ( timers[i].elem === this ) {
					if (gotoend) {
						// force the next step to be the last
						timers[i](true);
					}

					timers.splice(i, 1);
				}
			}
		});

		// start the next in the queue if the last step wasn't forced
		if ( !gotoend ) {
			this.dequeue();
		}

		return this;
	}

});

function genfx( type, num ) {
	var obj = {};

	jquery.each( fxattrs.concat.apply([], fxattrs.slice(0,num)), function() {
		obj[ this ] = type;
	});

	return obj;
}

// generate shortcuts for custom animations
jquery.each({
	slidedown: genfx("show", 1),
	slideup: genfx("hide", 1),
	slidetoggle: genfx("toggle", 1),
	fadein: { opacity: "show" },
	fadeout: { opacity: "hide" },
	fadetoggle: { opacity: "toggle" }
}, function( name, props ) {
	jquery.fn[ name ] = function( speed, easing, callback ) {
		return this.animate( props, speed, easing, callback );
	};
});

jquery.extend({
	speed: function( speed, easing, fn ) {
		var opt = speed && typeof speed === "object" ? jquery.extend({}, speed) : {
			complete: fn || !fn && easing ||
				jquery.isfunction( speed ) && speed,
			duration: speed,
			easing: fn && easing || easing && !jquery.isfunction(easing) && easing
		};

		opt.duration = jquery.fx.off ? 0 : typeof opt.duration === "number" ? opt.duration :
			opt.duration in jquery.fx.speeds ? jquery.fx.speeds[opt.duration] : jquery.fx.speeds._default;

		// queueing
		opt.old = opt.complete;
		opt.complete = function() {
			if ( opt.queue !== false ) {
				jquery(this).dequeue();
			}
			if ( jquery.isfunction( opt.old ) ) {
				opt.old.call( this );
			}
		};

		return opt;
	},

	easing: {
		linear: function( p, n, firstnum, diff ) {
			return firstnum + diff * p;
		},
		swing: function( p, n, firstnum, diff ) {
			return ((-math.cos(p*math.pi)/2) + 0.5) * diff + firstnum;
		}
	},

	timers: [],

	fx: function( elem, options, prop ) {
		this.options = options;
		this.elem = elem;
		this.prop = prop;

		if ( !options.orig ) {
			options.orig = {};
		}
	}

});

jquery.fx.prototype = {
	// simple function for setting a style value
	update: function() {
		if ( this.options.step ) {
			this.options.step.call( this.elem, this.now, this );
		}

		(jquery.fx.step[this.prop] || jquery.fx.step._default)( this );
	},

	// get the current size
	cur: function() {
		if ( this.elem[this.prop] != null && (!this.elem.style || this.elem.style[this.prop] == null) ) {
			return this.elem[ this.prop ];
		}

		var parsed,
			r = jquery.css( this.elem, this.prop );
		// empty strings, null, undefined and "auto" are converted to 0,
		// complex values such as "rotate(1rad)" are returned as is,
		// simple values such as "10px" are parsed to float.
		return isnan( parsed = parsefloat( r ) ) ? !r || r === "auto" ? 0 : r : parsed;
	},

	// start an animation from one number to another
	custom: function( from, to, unit ) {
		var self = this,
			fx = jquery.fx;

		this.starttime = jquery.now();
		this.start = from;
		this.end = to;
		this.unit = unit || this.unit || ( jquery.cssnumber[ this.prop ] ? "" : "px" );
		this.now = this.start;
		this.pos = this.state = 0;

		function t( gotoend ) {
			return self.step(gotoend);
		}

		t.elem = this.elem;

		if ( t() && jquery.timers.push(t) && !timerid ) {
			timerid = setinterval(fx.tick, fx.interval);
		}
	},

	// simple 'show' function
	show: function() {
		// remember where we started, so that we can go back to it later
		this.options.orig[this.prop] = jquery.style( this.elem, this.prop );
		this.options.show = true;

		// begin the animation
		// make sure that we start at a small width/height to avoid any
		// flash of content
		this.custom(this.prop === "width" || this.prop === "height" ? 1 : 0, this.cur());

		// start by showing the element
		jquery( this.elem ).show();
	},

	// simple 'hide' function
	hide: function() {
		// remember where we started, so that we can go back to it later
		this.options.orig[this.prop] = jquery.style( this.elem, this.prop );
		this.options.hide = true;

		// begin the animation
		this.custom(this.cur(), 0);
	},

	// each step of an animation
	step: function( gotoend ) {
		var t = jquery.now(), done = true;

		if ( gotoend || t >= this.options.duration + this.starttime ) {
			this.now = this.end;
			this.pos = this.state = 1;
			this.update();

			this.options.curanim[ this.prop ] = true;

			for ( var i in this.options.curanim ) {
				if ( this.options.curanim[i] !== true ) {
					done = false;
				}
			}

			if ( done ) {
				// reset the overflow
				if ( this.options.overflow != null && !jquery.support.shrinkwrapblocks ) {
					var elem = this.elem,
						options = this.options;

					jquery.each( [ "", "x", "y" ], function (index, value) {
						elem.style[ "overflow" + value ] = options.overflow[index];
					} );
				}

				// hide the element if the "hide" operation was done
				if ( this.options.hide ) {
					jquery(this.elem).hide();
				}

				// reset the properties, if the item has been hidden or shown
				if ( this.options.hide || this.options.show ) {
					for ( var p in this.options.curanim ) {
						jquery.style( this.elem, p, this.options.orig[p] );
					}
				}

				// execute the complete function
				this.options.complete.call( this.elem );
			}

			return false;

		} else {
			var n = t - this.starttime;
			this.state = n / this.options.duration;

			// perform the easing function, defaults to swing
			var specialeasing = this.options.specialeasing && this.options.specialeasing[this.prop];
			var defaulteasing = this.options.easing || (jquery.easing.swing ? "swing" : "linear");
			this.pos = jquery.easing[specialeasing || defaulteasing](this.state, n, 0, 1, this.options.duration);
			this.now = this.start + ((this.end - this.start) * this.pos);

			// perform the next step of the animation
			this.update();
		}

		return true;
	}
};

jquery.extend( jquery.fx, {
	tick: function() {
		var timers = jquery.timers;

		for ( var i = 0; i < timers.length; i++ ) {
			if ( !timers[i]() ) {
				timers.splice(i--, 1);
			}
		}

		if ( !timers.length ) {
			jquery.fx.stop();
		}
	},

	interval: 13,

	stop: function() {
		clearinterval( timerid );
		timerid = null;
	},

	speeds: {
		slow: 600,
		fast: 200,
		// default speed
		_default: 400
	},

	step: {
		opacity: function( fx ) {
			jquery.style( fx.elem, "opacity", fx.now );
		},

		_default: function( fx ) {
			if ( fx.elem.style && fx.elem.style[ fx.prop ] != null ) {
				fx.elem.style[ fx.prop ] = (fx.prop === "width" || fx.prop === "height" ? math.max(0, fx.now) : fx.now) + fx.unit;
			} else {
				fx.elem[ fx.prop ] = fx.now;
			}
		}
	}
});

if ( jquery.expr && jquery.expr.filters ) {
	jquery.expr.filters.animated = function( elem ) {
		return jquery.grep(jquery.timers, function( fn ) {
			return elem === fn.elem;
		}).length;
	};
}

function defaultdisplay( nodename ) {
	if ( !elemdisplay[ nodename ] ) {
		var elem = jquery("<" + nodename + ">").appendto("body"),
			display = elem.css("display");

		elem.remove();

		if ( display === "none" || display === "" ) {
			display = "block";
		}

		elemdisplay[ nodename ] = display;
	}

	return elemdisplay[ nodename ];
}




var rtable = /^t(?:able|d|h)$/i,
	rroot = /^(?:body|html)$/i;

if ( "getboundingclientrect" in document.documentelement ) {
	jquery.fn.offset = function( options ) {
		var elem = this[0], box;

		if ( options ) {
			return this.each(function( i ) {
				jquery.offset.setoffset( this, options, i );
			});
		}

		if ( !elem || !elem.ownerdocument ) {
			return null;
		}

		if ( elem === elem.ownerdocument.body ) {
			return jquery.offset.bodyoffset( elem );
		}

		try {
			box = elem.getboundingclientrect();
		} catch(e) {}

		var doc = elem.ownerdocument,
			docelem = doc.documentelement;

		// make sure we're not dealing with a disconnected dom node
		if ( !box || !jquery.contains( docelem, elem ) ) {
			return box ? { top: box.top, left: box.left } : { top: 0, left: 0 };
		}

		var body = doc.body,
			win = getwindow(doc),
			clienttop  = docelem.clienttop  || body.clienttop  || 0,
			clientleft = docelem.clientleft || body.clientleft || 0,
			scrolltop  = (win.pageyoffset || jquery.support.boxmodel && docelem.scrolltop  || body.scrolltop ),
			scrollleft = (win.pagexoffset || jquery.support.boxmodel && docelem.scrollleft || body.scrollleft),
			top  = box.top  + scrolltop  - clienttop,
			left = box.left + scrollleft - clientleft;

		return { top: top, left: left };
	};

} else {
	jquery.fn.offset = function( options ) {
		var elem = this[0];

		if ( options ) {
			return this.each(function( i ) {
				jquery.offset.setoffset( this, options, i );
			});
		}

		if ( !elem || !elem.ownerdocument ) {
			return null;
		}

		if ( elem === elem.ownerdocument.body ) {
			return jquery.offset.bodyoffset( elem );
		}

		jquery.offset.initialize();

		var computedstyle,
			offsetparent = elem.offsetparent,
			prevoffsetparent = elem,
			doc = elem.ownerdocument,
			docelem = doc.documentelement,
			body = doc.body,
			defaultview = doc.defaultview,
			prevcomputedstyle = defaultview ? defaultview.getcomputedstyle( elem, null ) : elem.currentstyle,
			top = elem.offsettop,
			left = elem.offsetleft;

		while ( (elem = elem.parentnode) && elem !== body && elem !== docelem ) {
			if ( jquery.offset.supportsfixedposition && prevcomputedstyle.position === "fixed" ) {
				break;
			}

			computedstyle = defaultview ? defaultview.getcomputedstyle(elem, null) : elem.currentstyle;
			top  -= elem.scrolltop;
			left -= elem.scrollleft;

			if ( elem === offsetparent ) {
				top  += elem.offsettop;
				left += elem.offsetleft;

				if ( jquery.offset.doesnotaddborder && !(jquery.offset.doesaddborderfortableandcells && rtable.test(elem.nodename)) ) {
					top  += parsefloat( computedstyle.bordertopwidth  ) || 0;
					left += parsefloat( computedstyle.borderleftwidth ) || 0;
				}

				prevoffsetparent = offsetparent;
				offsetparent = elem.offsetparent;
			}

			if ( jquery.offset.subtractsborderforoverflownotvisible && computedstyle.overflow !== "visible" ) {
				top  += parsefloat( computedstyle.bordertopwidth  ) || 0;
				left += parsefloat( computedstyle.borderleftwidth ) || 0;
			}

			prevcomputedstyle = computedstyle;
		}

		if ( prevcomputedstyle.position === "relative" || prevcomputedstyle.position === "static" ) {
			top  += body.offsettop;
			left += body.offsetleft;
		}

		if ( jquery.offset.supportsfixedposition && prevcomputedstyle.position === "fixed" ) {
			top  += math.max( docelem.scrolltop, body.scrolltop );
			left += math.max( docelem.scrollleft, body.scrollleft );
		}

		return { top: top, left: left };
	};
}

jquery.offset = {
	initialize: function() {
		var body = document.body, container = document.createelement("div"), innerdiv, checkdiv, table, td, bodymargintop = parsefloat( jquery.css(body, "margintop") ) || 0,
			html = "<div style='position:absolute;top:0;left:0;margin:0;border:5px solid #000;padding:0;width:1px;height:1px;'><div></div></div><table style='position:absolute;top:0;left:0;margin:0;border:5px solid #000;padding:0;width:1px;height:1px;' cellpadding='0' cellspacing='0'><tr><td></td></tr></table>";

		jquery.extend( container.style, { position: "absolute", top: 0, left: 0, margin: 0, border: 0, width: "1px", height: "1px", visibility: "hidden" } );

		container.innerhtml = html;
		body.insertbefore( container, body.firstchild );
		innerdiv = container.firstchild;
		checkdiv = innerdiv.firstchild;
		td = innerdiv.nextsibling.firstchild.firstchild;

		this.doesnotaddborder = (checkdiv.offsettop !== 5);
		this.doesaddborderfortableandcells = (td.offsettop === 5);

		checkdiv.style.position = "fixed";
		checkdiv.style.top = "20px";

		// safari subtracts parent border width here which is 5px
		this.supportsfixedposition = (checkdiv.offsettop === 20 || checkdiv.offsettop === 15);
		checkdiv.style.position = checkdiv.style.top = "";

		innerdiv.style.overflow = "hidden";
		innerdiv.style.position = "relative";

		this.subtractsborderforoverflownotvisible = (checkdiv.offsettop === -5);

		this.doesnotincludemargininbodyoffset = (body.offsettop !== bodymargintop);

		body.removechild( container );
		body = container = innerdiv = checkdiv = table = td = null;
		jquery.offset.initialize = jquery.noop;
	},

	bodyoffset: function( body ) {
		var top = body.offsettop,
			left = body.offsetleft;

		jquery.offset.initialize();

		if ( jquery.offset.doesnotincludemargininbodyoffset ) {
			top  += parsefloat( jquery.css(body, "margintop") ) || 0;
			left += parsefloat( jquery.css(body, "marginleft") ) || 0;
		}

		return { top: top, left: left };
	},

	setoffset: function( elem, options, i ) {
		var position = jquery.css( elem, "position" );

		// set position first, in-case top/left are set even on static elem
		if ( position === "static" ) {
			elem.style.position = "relative";
		}

		var curelem = jquery( elem ),
			curoffset = curelem.offset(),
			curcsstop = jquery.css( elem, "top" ),
			curcssleft = jquery.css( elem, "left" ),
			calculateposition = (position === "absolute" && jquery.inarray('auto', [curcsstop, curcssleft]) > -1),
			props = {}, curposition = {}, curtop, curleft;

		// need to be able to calculate position if either top or left is auto and position is absolute
		if ( calculateposition ) {
			curposition = curelem.position();
		}

		curtop  = calculateposition ? curposition.top  : parseint( curcsstop,  10 ) || 0;
		curleft = calculateposition ? curposition.left : parseint( curcssleft, 10 ) || 0;

		if ( jquery.isfunction( options ) ) {
			options = options.call( elem, i, curoffset );
		}

		if (options.top != null) {
			props.top = (options.top - curoffset.top) + curtop;
		}
		if (options.left != null) {
			props.left = (options.left - curoffset.left) + curleft;
		}

		if ( "using" in options ) {
			options.using.call( elem, props );
		} else {
			curelem.css( props );
		}
	}
};


jquery.fn.extend({
	position: function() {
		if ( !this[0] ) {
			return null;
		}

		var elem = this[0],

		// get *real* offsetparent
		offsetparent = this.offsetparent(),

		// get correct offsets
		offset       = this.offset(),
		parentoffset = rroot.test(offsetparent[0].nodename) ? { top: 0, left: 0 } : offsetparent.offset();

		// subtract element margins
		// note: when an element has margin: auto the offsetleft and marginleft
		// are the same in safari causing offset.left to incorrectly be 0
		offset.top  -= parsefloat( jquery.css(elem, "margintop") ) || 0;
		offset.left -= parsefloat( jquery.css(elem, "marginleft") ) || 0;

		// add offsetparent borders
		parentoffset.top  += parsefloat( jquery.css(offsetparent[0], "bordertopwidth") ) || 0;
		parentoffset.left += parsefloat( jquery.css(offsetparent[0], "borderleftwidth") ) || 0;

		// subtract the two offsets
		return {
			top:  offset.top  - parentoffset.top,
			left: offset.left - parentoffset.left
		};
	},

	offsetparent: function() {
		return this.map(function() {
			var offsetparent = this.offsetparent || document.body;
			while ( offsetparent && (!rroot.test(offsetparent.nodename) && jquery.css(offsetparent, "position") === "static") ) {
				offsetparent = offsetparent.offsetparent;
			}
			return offsetparent;
		});
	}
});


// create scrollleft and scrolltop methods
jquery.each( ["left", "top"], function( i, name ) {
	var method = "scroll" + name;

	jquery.fn[ method ] = function(val) {
		var elem = this[0], win;

		if ( !elem ) {
			return null;
		}

		if ( val !== undefined ) {
			// set the scroll offset
			return this.each(function() {
				win = getwindow( this );

				if ( win ) {
					win.scrollto(
						!i ? val : jquery(win).scrollleft(),
						i ? val : jquery(win).scrolltop()
					);

				} else {
					this[ method ] = val;
				}
			});
		} else {
			win = getwindow( elem );

			// return the scroll offset
			return win ? ("pagexoffset" in win) ? win[ i ? "pageyoffset" : "pagexoffset" ] :
				jquery.support.boxmodel && win.document.documentelement[ method ] ||
					win.document.body[ method ] :
				elem[ method ];
		}
	};
});

function getwindow( elem ) {
	return jquery.iswindow( elem ) ?
		elem :
		elem.nodetype === 9 ?
			elem.defaultview || elem.parentwindow :
			false;
}




// create innerheight, innerwidth, outerheight and outerwidth methods
jquery.each([ "height", "width" ], function( i, name ) {

	var type = name.tolowercase();

	// innerheight and innerwidth
	jquery.fn["inner" + name] = function() {
		return this[0] ?
			parsefloat( jquery.css( this[0], type, "padding" ) ) :
			null;
	};

	// outerheight and outerwidth
	jquery.fn["outer" + name] = function( margin ) {
		return this[0] ?
			parsefloat( jquery.css( this[0], type, margin ? "margin" : "border" ) ) :
			null;
	};

	jquery.fn[ type ] = function( size ) {
		// get window width or height
		var elem = this[0];
		if ( !elem ) {
			return size == null ? null : this;
		}

		if ( jquery.isfunction( size ) ) {
			return this.each(function( i ) {
				var self = jquery( this );
				self[ type ]( size.call( this, i, self[ type ]() ) );
			});
		}

		if ( jquery.iswindow( elem ) ) {
			// everyone else use document.documentelement or document.body depending on quirks vs standards mode
			// 3rd condition allows nokia support, as it supports the docelem prop but not css1compat
			var docelemprop = elem.document.documentelement[ "client" + name ];
			return elem.document.compatmode === "css1compat" && docelemprop ||
				elem.document.body[ "client" + name ] || docelemprop;

		// get document width or height
		} else if ( elem.nodetype === 9 ) {
			// either scroll[width/height] or offset[width/height], whichever is greater
			return math.max(
				elem.documentelement["client" + name],
				elem.body["scroll" + name], elem.documentelement["scroll" + name],
				elem.body["offset" + name], elem.documentelement["offset" + name]
			);

		// get or set width or height on the element
		} else if ( size === undefined ) {
			var orig = jquery.css( elem, type ),
				ret = parsefloat( orig );

			return jquery.isnan( ret ) ? orig : ret;

		// set the width or height on the element (default to pixels if value is unitless)
		} else {
			return this.css( type, typeof size === "string" ? size : size + "px" );
		}
	};

});


window.jquery = window.$ = jquery;
})(window);
