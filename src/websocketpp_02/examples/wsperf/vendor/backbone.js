//     backbone.js 0.9.1

//     (c) 2010-2012 jeremy ashkenas, documentcloud inc.
//     backbone may be freely distributed under the mit license.
//     for all details and documentation:
//     http://backbonejs.org

(function(){

  // initial setup
  // -------------

  // save a reference to the global object (`window` in the browser, `global`
  // on the server).
  var root = this;

  // save the previous value of the `backbone` variable, so that it can be
  // restored later on, if `noconflict` is used.
  var previousbackbone = root.backbone;

  // create a local reference to slice/splice.
  var slice = array.prototype.slice;
  var splice = array.prototype.splice;

  // the top-level namespace. all public backbone classes and modules will
  // be attached to this. exported for both commonjs and the browser.
  var backbone;
  if (typeof exports !== 'undefined') {
    backbone = exports;
  } else {
    backbone = root.backbone = {};
  }

  // current version of the library. keep in sync with `package.json`.
  backbone.version = '0.9.1';

  // require underscore, if we're on the server, and it's not already present.
  var _ = root._;
  if (!_ && (typeof require !== 'undefined')) _ = require('underscore');

  // for backbone's purposes, jquery, zepto, or ender owns the `$` variable.
  var $ = root.jquery || root.zepto || root.ender;

  // set the javascript library that will be used for dom manipulation and
  // ajax calls (a.k.a. the `$` variable). by default backbone will use: jquery,
  // zepto, or ender; but the `setdomlibrary()` method lets you inject an
  // alternate javascript library (or a mock library for testing your views
  // outside of a browser).
  backbone.setdomlibrary = function(lib) {
    $ = lib;
  };

  // runs backbone.js in *noconflict* mode, returning the `backbone` variable
  // to its previous owner. returns a reference to this backbone object.
  backbone.noconflict = function() {
    root.backbone = previousbackbone;
    return this;
  };

  // turn on `emulatehttp` to support legacy http servers. setting this option
  // will fake `"put"` and `"delete"` requests via the `_method` parameter and
  // set a `x-http-method-override` header.
  backbone.emulatehttp = false;

  // turn on `emulatejson` to support legacy servers that can't deal with direct
  // `application/json` requests ... will encode the body as
  // `application/x-www-form-urlencoded` instead and will send the model in a
  // form param named `model`.
  backbone.emulatejson = false;

  // backbone.events
  // -----------------

  // a module that can be mixed in to *any object* in order to provide it with
  // custom events. you may bind with `on` or remove with `off` callback functions
  // to an event; trigger`-ing an event fires all callbacks in succession.
  //
  //     var object = {};
  //     _.extend(object, backbone.events);
  //     object.on('expand', function(){ alert('expanded'); });
  //     object.trigger('expand');
  //
  backbone.events = {

    // bind an event, specified by a string name, `ev`, to a `callback`
    // function. passing `"all"` will bind the callback to all events fired.
    on: function(events, callback, context) {
      var ev;
      events = events.split(/\s+/);
      var calls = this._callbacks || (this._callbacks = {});
      while (ev = events.shift()) {
        // create an immutable callback list, allowing traversal during
        // modification.  the tail is an empty object that will always be used
        // as the next node.
        var list  = calls[ev] || (calls[ev] = {});
        var tail = list.tail || (list.tail = list.next = {});
        tail.callback = callback;
        tail.context = context;
        list.tail = tail.next = {};
      }
      return this;
    },

    // remove one or many callbacks. if `context` is null, removes all callbacks
    // with that function. if `callback` is null, removes all callbacks for the
    // event. if `ev` is null, removes all bound callbacks for all events.
    off: function(events, callback, context) {
      var ev, calls, node;
      if (!events) {
        delete this._callbacks;
      } else if (calls = this._callbacks) {
        events = events.split(/\s+/);
        while (ev = events.shift()) {
          node = calls[ev];
          delete calls[ev];
          if (!callback || !node) continue;
          // create a new list, omitting the indicated event/context pairs.
          while ((node = node.next) && node.next) {
            if (node.callback === callback &&
              (!context || node.context === context)) continue;
            this.on(ev, node.callback, node.context);
          }
        }
      }
      return this;
    },

    // trigger an event, firing all bound callbacks. callbacks are passed the
    // same arguments as `trigger` is, apart from the event name.
    // listening for `"all"` passes the true event name as the first argument.
    trigger: function(events) {
      var event, node, calls, tail, args, all, rest;
      if (!(calls = this._callbacks)) return this;
      all = calls['all'];
      (events = events.split(/\s+/)).push(null);
      // save references to the current heads & tails.
      while (event = events.shift()) {
        if (all) events.push({next: all.next, tail: all.tail, event: event});
        if (!(node = calls[event])) continue;
        events.push({next: node.next, tail: node.tail});
      }
      // traverse each list, stopping when the saved tail is reached.
      rest = slice.call(arguments, 1);
      while (node = events.pop()) {
        tail = node.tail;
        args = node.event ? [node.event].concat(rest) : rest;
        while ((node = node.next) !== tail) {
          node.callback.apply(node.context || this, args);
        }
      }
      return this;
    }

  };

  // aliases for backwards compatibility.
  backbone.events.bind   = backbone.events.on;
  backbone.events.unbind = backbone.events.off;

  // backbone.model
  // --------------

  // create a new model, with defined attributes. a client id (`cid`)
  // is automatically generated and assigned for you.
  backbone.model = function(attributes, options) {
    var defaults;
    attributes || (attributes = {});
    if (options && options.parse) attributes = this.parse(attributes);
    if (defaults = getvalue(this, 'defaults')) {
      attributes = _.extend({}, defaults, attributes);
    }
    if (options && options.collection) this.collection = options.collection;
    this.attributes = {};
    this._escapedattributes = {};
    this.cid = _.uniqueid('c');
    if (!this.set(attributes, {silent: true})) {
      throw new error("can't create an invalid model");
    }
    delete this._changed;
    this._previousattributes = _.clone(this.attributes);
    this.initialize.apply(this, arguments);
  };

  // attach all inheritable methods to the model prototype.
  _.extend(backbone.model.prototype, backbone.events, {

    // the default name for the json `id` attribute is `"id"`. mongodb and
    // couchdb users may want to set this to `"_id"`.
    idattribute: 'id',

    // initialize is an empty function by default. override it with your own
    // initialization logic.
    initialize: function(){},

    // return a copy of the model's `attributes` object.
    tojson: function() {
      return _.clone(this.attributes);
    },

    // get the value of an attribute.
    get: function(attr) {
      return this.attributes[attr];
    },

    // get the html-escaped value of an attribute.
    escape: function(attr) {
      var html;
      if (html = this._escapedattributes[attr]) return html;
      var val = this.attributes[attr];
      return this._escapedattributes[attr] = _.escape(val == null ? '' : '' + val);
    },

    // returns `true` if the attribute contains a value that is not null
    // or undefined.
    has: function(attr) {
      return this.attributes[attr] != null;
    },

    // set a hash of model attributes on the object, firing `"change"` unless
    // you choose to silence it.
    set: function(key, value, options) {
      var attrs, attr, val;
      if (_.isobject(key) || key == null) {
        attrs = key;
        options = value;
      } else {
        attrs = {};
        attrs[key] = value;
      }

      // extract attributes and options.
      options || (options = {});
      if (!attrs) return this;
      if (attrs instanceof backbone.model) attrs = attrs.attributes;
      if (options.unset) for (attr in attrs) attrs[attr] = void 0;

      // run validation.
      if (!this._validate(attrs, options)) return false;

      // check for changes of `id`.
      if (this.idattribute in attrs) this.id = attrs[this.idattribute];

      var now = this.attributes;
      var escaped = this._escapedattributes;
      var prev = this._previousattributes || {};
      var alreadysetting = this._setting;
      this._changed || (this._changed = {});
      this._setting = true;

      // update attributes.
      for (attr in attrs) {
        val = attrs[attr];
        if (!_.isequal(now[attr], val)) delete escaped[attr];
        options.unset ? delete now[attr] : now[attr] = val;
        if (this._changing && !_.isequal(this._changed[attr], val)) {
          this.trigger('change:' + attr, this, val, options);
          this._morechanges = true;
        }
        delete this._changed[attr];
        if (!_.isequal(prev[attr], val) || (_.has(now, attr) != _.has(prev, attr))) {
          this._changed[attr] = val;
        }
      }

      // fire the `"change"` events, if the model has been changed.
      if (!alreadysetting) {
        if (!options.silent && this.haschanged()) this.change(options);
        this._setting = false;
      }
      return this;
    },

    // remove an attribute from the model, firing `"change"` unless you choose
    // to silence it. `unset` is a noop if the attribute doesn't exist.
    unset: function(attr, options) {
      (options || (options = {})).unset = true;
      return this.set(attr, null, options);
    },

    // clear all attributes on the model, firing `"change"` unless you choose
    // to silence it.
    clear: function(options) {
      (options || (options = {})).unset = true;
      return this.set(_.clone(this.attributes), options);
    },

    // fetch the model from the server. if the server's representation of the
    // model differs from its current attributes, they will be overriden,
    // triggering a `"change"` event.
    fetch: function(options) {
      options = options ? _.clone(options) : {};
      var model = this;
      var success = options.success;
      options.success = function(resp, status, xhr) {
        if (!model.set(model.parse(resp, xhr), options)) return false;
        if (success) success(model, resp);
      };
      options.error = backbone.wraperror(options.error, model, options);
      return (this.sync || backbone.sync).call(this, 'read', this, options);
    },

    // set a hash of model attributes, and sync the model to the server.
    // if the server returns an attributes hash that differs, the model's
    // state will be `set` again.
    save: function(key, value, options) {
      var attrs, current;
      if (_.isobject(key) || key == null) {
        attrs = key;
        options = value;
      } else {
        attrs = {};
        attrs[key] = value;
      }

      options = options ? _.clone(options) : {};
      if (options.wait) current = _.clone(this.attributes);
      var silentoptions = _.extend({}, options, {silent: true});
      if (attrs && !this.set(attrs, options.wait ? silentoptions : options)) {
        return false;
      }
      var model = this;
      var success = options.success;
      options.success = function(resp, status, xhr) {
        var serverattrs = model.parse(resp, xhr);
        if (options.wait) serverattrs = _.extend(attrs || {}, serverattrs);
        if (!model.set(serverattrs, options)) return false;
        if (success) {
          success(model, resp);
        } else {
          model.trigger('sync', model, resp, options);
        }
      };
      options.error = backbone.wraperror(options.error, model, options);
      var method = this.isnew() ? 'create' : 'update';
      var xhr = (this.sync || backbone.sync).call(this, method, this, options);
      if (options.wait) this.set(current, silentoptions);
      return xhr;
    },

    // destroy this model on the server if it was already persisted.
    // optimistically removes the model from its collection, if it has one.
    // if `wait: true` is passed, waits for the server to respond before removal.
    destroy: function(options) {
      options = options ? _.clone(options) : {};
      var model = this;
      var success = options.success;

      var triggerdestroy = function() {
        model.trigger('destroy', model, model.collection, options);
      };

      if (this.isnew()) return triggerdestroy();
      options.success = function(resp) {
        if (options.wait) triggerdestroy();
        if (success) {
          success(model, resp);
        } else {
          model.trigger('sync', model, resp, options);
        }
      };
      options.error = backbone.wraperror(options.error, model, options);
      var xhr = (this.sync || backbone.sync).call(this, 'delete', this, options);
      if (!options.wait) triggerdestroy();
      return xhr;
    },

    // default url for the model's representation on the server -- if you're
    // using backbone's restful methods, override this to change the endpoint
    // that will be called.
    url: function() {
      var base = getvalue(this.collection, 'url') || getvalue(this, 'urlroot') || urlerror();
      if (this.isnew()) return base;
      return base + (base.charat(base.length - 1) == '/' ? '' : '/') + encodeuricomponent(this.id);
    },

    // **parse** converts a response into the hash of attributes to be `set` on
    // the model. the default implementation is just to pass the response along.
    parse: function(resp, xhr) {
      return resp;
    },

    // create a new model with identical attributes to this one.
    clone: function() {
      return new this.constructor(this.attributes);
    },

    // a model is new if it has never been saved to the server, and lacks an id.
    isnew: function() {
      return this.id == null;
    },

    // call this method to manually fire a `"change"` event for this model and
    // a `"change:attribute"` event for each changed attribute.
    // calling this will cause all objects observing the model to update.
    change: function(options) {
      if (this._changing || !this.haschanged()) return this;
      this._changing = true;
      this._morechanges = true;
      for (var attr in this._changed) {
        this.trigger('change:' + attr, this, this._changed[attr], options);
      }
      while (this._morechanges) {
        this._morechanges = false;
        this.trigger('change', this, options);
      }
      this._previousattributes = _.clone(this.attributes);
      delete this._changed;
      this._changing = false;
      return this;
    },

    // determine if the model has changed since the last `"change"` event.
    // if you specify an attribute name, determine if that attribute has changed.
    haschanged: function(attr) {
      if (!arguments.length) return !_.isempty(this._changed);
      return this._changed && _.has(this._changed, attr);
    },

    // return an object containing all the attributes that have changed, or
    // false if there are no changed attributes. useful for determining what
    // parts of a view need to be updated and/or what attributes need to be
    // persisted to the server. unset attributes will be set to undefined.
    // you can also pass an attributes object to diff against the model,
    // determining if there *would be* a change.
    changedattributes: function(diff) {
      if (!diff) return this.haschanged() ? _.clone(this._changed) : false;
      var val, changed = false, old = this._previousattributes;
      for (var attr in diff) {
        if (_.isequal(old[attr], (val = diff[attr]))) continue;
        (changed || (changed = {}))[attr] = val;
      }
      return changed;
    },

    // get the previous value of an attribute, recorded at the time the last
    // `"change"` event was fired.
    previous: function(attr) {
      if (!arguments.length || !this._previousattributes) return null;
      return this._previousattributes[attr];
    },

    // get all of the attributes of the model at the time of the previous
    // `"change"` event.
    previousattributes: function() {
      return _.clone(this._previousattributes);
    },

    // check if the model is currently in a valid state. it's only possible to
    // get into an *invalid* state if you're using silent changes.
    isvalid: function() {
      return !this.validate(this.attributes);
    },

    // run validation against a set of incoming attributes, returning `true`
    // if all is well. if a specific `error` callback has been passed,
    // call that instead of firing the general `"error"` event.
    _validate: function(attrs, options) {
      if (options.silent || !this.validate) return true;
      attrs = _.extend({}, this.attributes, attrs);
      var error = this.validate(attrs, options);
      if (!error) return true;
      if (options && options.error) {
        options.error(this, error, options);
      } else {
        this.trigger('error', this, error, options);
      }
      return false;
    }

  });

  // backbone.collection
  // -------------------

  // provides a standard collection class for our sets of models, ordered
  // or unordered. if a `comparator` is specified, the collection will maintain
  // its models in sort order, as they're added and removed.
  backbone.collection = function(models, options) {
    options || (options = {});
    if (options.comparator) this.comparator = options.comparator;
    this._reset();
    this.initialize.apply(this, arguments);
    if (models) this.reset(models, {silent: true, parse: options.parse});
  };

  // define the collection's inheritable methods.
  _.extend(backbone.collection.prototype, backbone.events, {

    // the default model for a collection is just a **backbone.model**.
    // this should be overridden in most cases.
    model: backbone.model,

    // initialize is an empty function by default. override it with your own
    // initialization logic.
    initialize: function(){},

    // the json representation of a collection is an array of the
    // models' attributes.
    tojson: function() {
      return this.map(function(model){ return model.tojson(); });
    },

    // add a model, or list of models to the set. pass **silent** to avoid
    // firing the `add` event for every new model.
    add: function(models, options) {
      var i, index, length, model, cid, id, cids = {}, ids = {};
      options || (options = {});
      models = _.isarray(models) ? models.slice() : [models];

      // begin by turning bare objects into model references, and preventing
      // invalid models or duplicate models from being added.
      for (i = 0, length = models.length; i < length; i++) {
        if (!(model = models[i] = this._preparemodel(models[i], options))) {
          throw new error("can't add an invalid model to a collection");
        }
        if (cids[cid = model.cid] || this._bycid[cid] ||
          (((id = model.id) != null) && (ids[id] || this._byid[id]))) {
          throw new error("can't add the same model to a collection twice");
        }
        cids[cid] = ids[id] = model;
      }

      // listen to added models' events, and index models for lookup by
      // `id` and by `cid`.
      for (i = 0; i < length; i++) {
        (model = models[i]).on('all', this._onmodelevent, this);
        this._bycid[model.cid] = model;
        if (model.id != null) this._byid[model.id] = model;
      }

      // insert models into the collection, re-sorting if needed, and triggering
      // `add` events unless silenced.
      this.length += length;
      index = options.at != null ? options.at : this.models.length;
      splice.apply(this.models, [index, 0].concat(models));
      if (this.comparator) this.sort({silent: true});
      if (options.silent) return this;
      for (i = 0, length = this.models.length; i < length; i++) {
        if (!cids[(model = this.models[i]).cid]) continue;
        options.index = i;
        model.trigger('add', model, this, options);
      }
      return this;
    },

    // remove a model, or a list of models from the set. pass silent to avoid
    // firing the `remove` event for every model removed.
    remove: function(models, options) {
      var i, l, index, model;
      options || (options = {});
      models = _.isarray(models) ? models.slice() : [models];
      for (i = 0, l = models.length; i < l; i++) {
        model = this.getbycid(models[i]) || this.get(models[i]);
        if (!model) continue;
        delete this._byid[model.id];
        delete this._bycid[model.cid];
        index = this.indexof(model);
        this.models.splice(index, 1);
        this.length--;
        if (!options.silent) {
          options.index = index;
          model.trigger('remove', model, this, options);
        }
        this._removereference(model);
      }
      return this;
    },

    // get a model from the set by id.
    get: function(id) {
      if (id == null) return null;
      return this._byid[id.id != null ? id.id : id];
    },

    // get a model from the set by client id.
    getbycid: function(cid) {
      return cid && this._bycid[cid.cid || cid];
    },

    // get the model at the given index.
    at: function(index) {
      return this.models[index];
    },

    // force the collection to re-sort itself. you don't need to call this under
    // normal circumstances, as the set will maintain sort order as each item
    // is added.
    sort: function(options) {
      options || (options = {});
      if (!this.comparator) throw new error('cannot sort a set without a comparator');
      var boundcomparator = _.bind(this.comparator, this);
      if (this.comparator.length == 1) {
        this.models = this.sortby(boundcomparator);
      } else {
        this.models.sort(boundcomparator);
      }
      if (!options.silent) this.trigger('reset', this, options);
      return this;
    },

    // pluck an attribute from each model in the collection.
    pluck: function(attr) {
      return _.map(this.models, function(model){ return model.get(attr); });
    },

    // when you have more items than you want to add or remove individually,
    // you can reset the entire set with a new list of models, without firing
    // any `add` or `remove` events. fires `reset` when finished.
    reset: function(models, options) {
      models  || (models = []);
      options || (options = {});
      for (var i = 0, l = this.models.length; i < l; i++) {
        this._removereference(this.models[i]);
      }
      this._reset();
      this.add(models, {silent: true, parse: options.parse});
      if (!options.silent) this.trigger('reset', this, options);
      return this;
    },

    // fetch the default set of models for this collection, resetting the
    // collection when they arrive. if `add: true` is passed, appends the
    // models to the collection instead of resetting.
    fetch: function(options) {
      options = options ? _.clone(options) : {};
      if (options.parse === undefined) options.parse = true;
      var collection = this;
      var success = options.success;
      options.success = function(resp, status, xhr) {
        collection[options.add ? 'add' : 'reset'](collection.parse(resp, xhr), options);
        if (success) success(collection, resp);
      };
      options.error = backbone.wraperror(options.error, collection, options);
      return (this.sync || backbone.sync).call(this, 'read', this, options);
    },

    // create a new instance of a model in this collection. add the model to the
    // collection immediately, unless `wait: true` is passed, in which case we
    // wait for the server to agree.
    create: function(model, options) {
      var coll = this;
      options = options ? _.clone(options) : {};
      model = this._preparemodel(model, options);
      if (!model) return false;
      if (!options.wait) coll.add(model, options);
      var success = options.success;
      options.success = function(nextmodel, resp, xhr) {
        if (options.wait) coll.add(nextmodel, options);
        if (success) {
          success(nextmodel, resp);
        } else {
          nextmodel.trigger('sync', model, resp, options);
        }
      };
      model.save(null, options);
      return model;
    },

    // **parse** converts a response into a list of models to be added to the
    // collection. the default implementation is just to pass it through.
    parse: function(resp, xhr) {
      return resp;
    },

    // proxy to _'s chain. can't be proxied the same way the rest of the
    // underscore methods are proxied because it relies on the underscore
    // constructor.
    chain: function () {
      return _(this.models).chain();
    },

    // reset all internal state. called when the collection is reset.
    _reset: function(options) {
      this.length = 0;
      this.models = [];
      this._byid  = {};
      this._bycid = {};
    },

    // prepare a model or hash of attributes to be added to this collection.
    _preparemodel: function(model, options) {
      if (!(model instanceof backbone.model)) {
        var attrs = model;
        options.collection = this;
        model = new this.model(attrs, options);
        if (!model._validate(model.attributes, options)) model = false;
      } else if (!model.collection) {
        model.collection = this;
      }
      return model;
    },

    // internal method to remove a model's ties to a collection.
    _removereference: function(model) {
      if (this == model.collection) {
        delete model.collection;
      }
      model.off('all', this._onmodelevent, this);
    },

    // internal method called every time a model in the set fires an event.
    // sets need to update their indexes when models change ids. all other
    // events simply proxy through. "add" and "remove" events that originate
    // in other collections are ignored.
    _onmodelevent: function(ev, model, collection, options) {
      if ((ev == 'add' || ev == 'remove') && collection != this) return;
      if (ev == 'destroy') {
        this.remove(model, options);
      }
      if (model && ev === 'change:' + model.idattribute) {
        delete this._byid[model.previous(model.idattribute)];
        this._byid[model.id] = model;
      }
      this.trigger.apply(this, arguments);
    }

  });

  // underscore methods that we want to implement on the collection.
  var methods = ['foreach', 'each', 'map', 'reduce', 'reduceright', 'find',
    'detect', 'filter', 'select', 'reject', 'every', 'all', 'some', 'any',
    'include', 'contains', 'invoke', 'max', 'min', 'sortby', 'sortedindex',
    'toarray', 'size', 'first', 'initial', 'rest', 'last', 'without', 'indexof',
    'shuffle', 'lastindexof', 'isempty', 'groupby'];

  // mix in each underscore method as a proxy to `collection#models`.
  _.each(methods, function(method) {
    backbone.collection.prototype[method] = function() {
      return _[method].apply(_, [this.models].concat(_.toarray(arguments)));
    };
  });

  // backbone.router
  // -------------------

  // routers map faux-urls to actions, and fire events when routes are
  // matched. creating a new one sets its `routes` hash, if not set statically.
  backbone.router = function(options) {
    options || (options = {});
    if (options.routes) this.routes = options.routes;
    this._bindroutes();
    this.initialize.apply(this, arguments);
  };

  // cached regular expressions for matching named param parts and splatted
  // parts of route strings.
  var namedparam    = /:\w+/g;
  var splatparam    = /\*\w+/g;
  var escaperegexp  = /[-[\]{}()+?.,\\^$|#\s]/g;

  // set up all inheritable **backbone.router** properties and methods.
  _.extend(backbone.router.prototype, backbone.events, {

    // initialize is an empty function by default. override it with your own
    // initialization logic.
    initialize: function(){},

    // manually bind a single named route to a callback. for example:
    //
    //     this.route('search/:query/p:num', 'search', function(query, num) {
    //       ...
    //     });
    //
    route: function(route, name, callback) {
      backbone.history || (backbone.history = new backbone.history);
      if (!_.isregexp(route)) route = this._routetoregexp(route);
      if (!callback) callback = this[name];
      backbone.history.route(route, _.bind(function(fragment) {
        var args = this._extractparameters(route, fragment);
        callback && callback.apply(this, args);
        this.trigger.apply(this, ['route:' + name].concat(args));
        backbone.history.trigger('route', this, name, args);
      }, this));
      return this;
    },

    // simple proxy to `backbone.history` to save a fragment into the history.
    navigate: function(fragment, options) {
      backbone.history.navigate(fragment, options);
    },

    // bind all defined routes to `backbone.history`. we have to reverse the
    // order of the routes here to support behavior where the most general
    // routes can be defined at the bottom of the route map.
    _bindroutes: function() {
      if (!this.routes) return;
      var routes = [];
      for (var route in this.routes) {
        routes.unshift([route, this.routes[route]]);
      }
      for (var i = 0, l = routes.length; i < l; i++) {
        this.route(routes[i][0], routes[i][1], this[routes[i][1]]);
      }
    },

    // convert a route string into a regular expression, suitable for matching
    // against the current location hash.
    _routetoregexp: function(route) {
      route = route.replace(escaperegexp, '\\$&')
                   .replace(namedparam, '([^\/]+)')
                   .replace(splatparam, '(.*?)');
      return new regexp('^' + route + '$');
    },

    // given a route, and a url fragment that it matches, return the array of
    // extracted parameters.
    _extractparameters: function(route, fragment) {
      return route.exec(fragment).slice(1);
    }

  });

  // backbone.history
  // ----------------

  // handles cross-browser history management, based on url fragments. if the
  // browser does not support `onhashchange`, falls back to polling.
  backbone.history = function() {
    this.handlers = [];
    _.bindall(this, 'checkurl');
  };

  // cached regex for cleaning leading hashes and slashes .
  var routestripper = /^[#\/]/;

  // cached regex for detecting msie.
  var isexplorer = /msie [\w.]+/;

  // has the history handling already been started?
  var historystarted = false;

  // set up all inheritable **backbone.history** properties and methods.
  _.extend(backbone.history.prototype, backbone.events, {

    // the default interval to poll for hash changes, if necessary, is
    // twenty times a second.
    interval: 50,

    // get the cross-browser normalized url fragment, either from the url,
    // the hash, or the override.
    getfragment: function(fragment, forcepushstate) {
      if (fragment == null) {
        if (this._haspushstate || forcepushstate) {
          fragment = window.location.pathname;
          var search = window.location.search;
          if (search) fragment += search;
        } else {
          fragment = window.location.hash;
        }
      }
      fragment = decodeuricomponent(fragment);
      if (!fragment.indexof(this.options.root)) fragment = fragment.substr(this.options.root.length);
      return fragment.replace(routestripper, '');
    },

    // start the hash change handling, returning `true` if the current url matches
    // an existing route, and `false` otherwise.
    start: function(options) {

      // figure out the initial configuration. do we need an iframe?
      // is pushstate desired ... is it available?
      if (historystarted) throw new error("backbone.history has already been started");
      this.options          = _.extend({}, {root: '/'}, this.options, options);
      this._wantshashchange = this.options.hashchange !== false;
      this._wantspushstate  = !!this.options.pushstate;
      this._haspushstate    = !!(this.options.pushstate && window.history && window.history.pushstate);
      var fragment          = this.getfragment();
      var docmode           = document.documentmode;
      var oldie             = (isexplorer.exec(navigator.useragent.tolowercase()) && (!docmode || docmode <= 7));
      if (oldie) {
        this.iframe = $('<iframe src="javascript:0" tabindex="-1" />').hide().appendto('body')[0].contentwindow;
        this.navigate(fragment);
      }

      // depending on whether we're using pushstate or hashes, and whether
      // 'onhashchange' is supported, determine how we check the url state.
      if (this._haspushstate) {
        $(window).bind('popstate', this.checkurl);
      } else if (this._wantshashchange && ('onhashchange' in window) && !oldie) {
        $(window).bind('hashchange', this.checkurl);
      } else if (this._wantshashchange) {
        this._checkurlinterval = setinterval(this.checkurl, this.interval);
      }

      // determine if we need to change the base url, for a pushstate link
      // opened by a non-pushstate browser.
      this.fragment = fragment;
      historystarted = true;
      var loc = window.location;
      var atroot  = loc.pathname == this.options.root;

      // if we've started off with a route from a `pushstate`-enabled browser,
      // but we're currently in a browser that doesn't support it...
      if (this._wantshashchange && this._wantspushstate && !this._haspushstate && !atroot) {
        this.fragment = this.getfragment(null, true);
        window.location.replace(this.options.root + '#' + this.fragment);
        // return immediately as browser will do redirect to new url
        return true;

      // or if we've started out with a hash-based route, but we're currently
      // in a browser where it could be `pushstate`-based instead...
      } else if (this._wantspushstate && this._haspushstate && atroot && loc.hash) {
        this.fragment = loc.hash.replace(routestripper, '');
        window.history.replacestate({}, document.title, loc.protocol + '//' + loc.host + this.options.root + this.fragment);
      }

      if (!this.options.silent) {
        return this.loadurl();
      }
    },

    // disable backbone.history, perhaps temporarily. not useful in a real app,
    // but possibly useful for unit testing routers.
    stop: function() {
      $(window).unbind('popstate', this.checkurl).unbind('hashchange', this.checkurl);
      clearinterval(this._checkurlinterval);
      historystarted = false;
    },

    // add a route to be tested when the fragment changes. routes added later
    // may override previous routes.
    route: function(route, callback) {
      this.handlers.unshift({route: route, callback: callback});
    },

    // checks the current url to see if it has changed, and if it has,
    // calls `loadurl`, normalizing across the hidden iframe.
    checkurl: function(e) {
      var current = this.getfragment();
      if (current == this.fragment && this.iframe) current = this.getfragment(this.iframe.location.hash);
      if (current == this.fragment || current == decodeuricomponent(this.fragment)) return false;
      if (this.iframe) this.navigate(current);
      this.loadurl() || this.loadurl(window.location.hash);
    },

    // attempt to load the current url fragment. if a route succeeds with a
    // match, returns `true`. if no defined routes matches the fragment,
    // returns `false`.
    loadurl: function(fragmentoverride) {
      var fragment = this.fragment = this.getfragment(fragmentoverride);
      var matched = _.any(this.handlers, function(handler) {
        if (handler.route.test(fragment)) {
          handler.callback(fragment);
          return true;
        }
      });
      return matched;
    },

    // save a fragment into the hash history, or replace the url state if the
    // 'replace' option is passed. you are responsible for properly url-encoding
    // the fragment in advance.
    //
    // the options object can contain `trigger: true` if you wish to have the
    // route callback be fired (not usually desirable), or `replace: true`, if
    // you which to modify the current url without adding an entry to the history.
    navigate: function(fragment, options) {
      if (!historystarted) return false;
      if (!options || options === true) options = {trigger: options};
      var frag = (fragment || '').replace(routestripper, '');
      if (this.fragment == frag || this.fragment == decodeuricomponent(frag)) return;

      // if pushstate is available, we use it to set the fragment as a real url.
      if (this._haspushstate) {
        if (frag.indexof(this.options.root) != 0) frag = this.options.root + frag;
        this.fragment = frag;
        window.history[options.replace ? 'replacestate' : 'pushstate']({}, document.title, frag);

      // if hash changes haven't been explicitly disabled, update the hash
      // fragment to store history.
      } else if (this._wantshashchange) {
        this.fragment = frag;
        this._updatehash(window.location, frag, options.replace);
        if (this.iframe && (frag != this.getfragment(this.iframe.location.hash))) {
          // opening and closing the iframe tricks ie7 and earlier to push a history entry on hash-tag change.
          // when replace is true, we don't want this.
          if(!options.replace) this.iframe.document.open().close();
          this._updatehash(this.iframe.location, frag, options.replace);
        }

      // if you've told us that you explicitly don't want fallback hashchange-
      // based history, then `navigate` becomes a page refresh.
      } else {
        window.location.assign(this.options.root + fragment);
      }
      if (options.trigger) this.loadurl(fragment);
    },

    // update the hash location, either replacing the current entry, or adding
    // a new one to the browser history.
    _updatehash: function(location, fragment, replace) {
      if (replace) {
        location.replace(location.tostring().replace(/(javascript:|#).*$/, '') + '#' + fragment);
      } else {
        location.hash = fragment;
      }
    }
  });

  // backbone.view
  // -------------

  // creating a backbone.view creates its initial element outside of the dom,
  // if an existing element is not provided...
  backbone.view = function(options) {
    this.cid = _.uniqueid('view');
    this._configure(options || {});
    this._ensureelement();
    this.initialize.apply(this, arguments);
    this.delegateevents();
  };

  // cached regex to split keys for `delegate`.
  var eventsplitter = /^(\s+)\s*(.*)$/;

  // list of view options to be merged as properties.
  var viewoptions = ['model', 'collection', 'el', 'id', 'attributes', 'classname', 'tagname'];

  // set up all inheritable **backbone.view** properties and methods.
  _.extend(backbone.view.prototype, backbone.events, {

    // the default `tagname` of a view's element is `"div"`.
    tagname: 'div',

    // jquery delegate for element lookup, scoped to dom elements within the
    // current view. this should be prefered to global lookups where possible.
    $: function(selector) {
      return this.$el.find(selector);
    },

    // initialize is an empty function by default. override it with your own
    // initialization logic.
    initialize: function(){},

    // **render** is the core function that your view should override, in order
    // to populate its element (`this.el`), with the appropriate html. the
    // convention is for **render** to always return `this`.
    render: function() {
      return this;
    },

    // remove this view from the dom. note that the view isn't present in the
    // dom by default, so calling this method may be a no-op.
    remove: function() {
      this.$el.remove();
      return this;
    },

    // for small amounts of dom elements, where a full-blown template isn't
    // needed, use **make** to manufacture elements, one at a time.
    //
    //     var el = this.make('li', {'class': 'row'}, this.model.escape('title'));
    //
    make: function(tagname, attributes, content) {
      var el = document.createelement(tagname);
      if (attributes) $(el).attr(attributes);
      if (content) $(el).html(content);
      return el;
    },

    // change the view's element (`this.el` property), including event
    // re-delegation.
    setelement: function(element, delegate) {
      this.$el = $(element);
      this.el = this.$el[0];
      if (delegate !== false) this.delegateevents();
      return this;
    },

    // set callbacks, where `this.events` is a hash of
    //
    // *{"event selector": "callback"}*
    //
    //     {
    //       'mousedown .title':  'edit',
    //       'click .button':     'save'
    //       'click .open':       function(e) { ... }
    //     }
    //
    // pairs. callbacks will be bound to the view, with `this` set properly.
    // uses event delegation for efficiency.
    // omitting the selector binds the event to `this.el`.
    // this only works for delegate-able events: not `focus`, `blur`, and
    // not `change`, `submit`, and `reset` in internet explorer.
    delegateevents: function(events) {
      if (!(events || (events = getvalue(this, 'events')))) return;
      this.undelegateevents();
      for (var key in events) {
        var method = events[key];
        if (!_.isfunction(method)) method = this[events[key]];
        if (!method) throw new error('event "' + events[key] + '" does not exist');
        var match = key.match(eventsplitter);
        var eventname = match[1], selector = match[2];
        method = _.bind(method, this);
        eventname += '.delegateevents' + this.cid;
        if (selector === '') {
          this.$el.bind(eventname, method);
        } else {
          this.$el.delegate(selector, eventname, method);
        }
      }
    },

    // clears all callbacks previously bound to the view with `delegateevents`.
    // you usually don't need to use this, but may wish to if you have multiple
    // backbone views attached to the same dom element.
    undelegateevents: function() {
      this.$el.unbind('.delegateevents' + this.cid);
    },

    // performs the initial configuration of a view with a set of options.
    // keys with special meaning *(model, collection, id, classname)*, are
    // attached directly to the view.
    _configure: function(options) {
      if (this.options) options = _.extend({}, this.options, options);
      for (var i = 0, l = viewoptions.length; i < l; i++) {
        var attr = viewoptions[i];
        if (options[attr]) this[attr] = options[attr];
      }
      this.options = options;
    },

    // ensure that the view has a dom element to render into.
    // if `this.el` is a string, pass it through `$()`, take the first
    // matching element, and re-assign it to `el`. otherwise, create
    // an element from the `id`, `classname` and `tagname` properties.
    _ensureelement: function() {
      if (!this.el) {
        var attrs = getvalue(this, 'attributes') || {};
        if (this.id) attrs.id = this.id;
        if (this.classname) attrs['class'] = this.classname;
        this.setelement(this.make(this.tagname, attrs), false);
      } else {
        this.setelement(this.el, false);
      }
    }

  });

  // the self-propagating extend function that backbone classes use.
  var extend = function (protoprops, classprops) {
    var child = inherits(this, protoprops, classprops);
    child.extend = this.extend;
    return child;
  };

  // set up inheritance for the model, collection, and view.
  backbone.model.extend = backbone.collection.extend =
    backbone.router.extend = backbone.view.extend = extend;

  // backbone.sync
  // -------------

  // map from crud to http for our default `backbone.sync` implementation.
  var methodmap = {
    'create': 'post',
    'update': 'put',
    'delete': 'delete',
    'read':   'get'
  };

  // override this function to change the manner in which backbone persists
  // models to the server. you will be passed the type of request, and the
  // model in question. by default, makes a restful ajax request
  // to the model's `url()`. some possible customizations could be:
  //
  // * use `settimeout` to batch rapid-fire updates into a single request.
  // * send up the models as xml instead of json.
  // * persist models via websockets instead of ajax.
  //
  // turn on `backbone.emulatehttp` in order to send `put` and `delete` requests
  // as `post`, with a `_method` parameter containing the true http method,
  // as well as all requests with the body as `application/x-www-form-urlencoded`
  // instead of `application/json` with the model in a param named `model`.
  // useful when interfacing with server-side languages like **php** that make
  // it difficult to read the body of `put` requests.
  backbone.sync = function(method, model, options) {
    var type = methodmap[method];

    // default json-request options.
    var params = {type: type, datatype: 'json'};

    // ensure that we have a url.
    if (!options.url) {
      params.url = getvalue(model, 'url') || urlerror();
    }

    // ensure that we have the appropriate request data.
    if (!options.data && model && (method == 'create' || method == 'update')) {
      params.contenttype = 'application/json';
      params.data = json.stringify(model.tojson());
    }

    // for older servers, emulate json by encoding the request into an html-form.
    if (backbone.emulatejson) {
      params.contenttype = 'application/x-www-form-urlencoded';
      params.data = params.data ? {model: params.data} : {};
    }

    // for older servers, emulate http by mimicking the http method with `_method`
    // and an `x-http-method-override` header.
    if (backbone.emulatehttp) {
      if (type === 'put' || type === 'delete') {
        if (backbone.emulatejson) params.data._method = type;
        params.type = 'post';
        params.beforesend = function(xhr) {
          xhr.setrequestheader('x-http-method-override', type);
        };
      }
    }

    // don't process data on a non-get request.
    if (params.type !== 'get' && !backbone.emulatejson) {
      params.processdata = false;
    }

    // make the request, allowing the user to override any ajax options.
    return $.ajax(_.extend(params, options));
  };

  // wrap an optional error callback with a fallback error event.
  backbone.wraperror = function(onerror, originalmodel, options) {
    return function(model, resp) {
      resp = model === originalmodel ? resp : model;
      if (onerror) {
        onerror(originalmodel, resp, options);
      } else {
        originalmodel.trigger('error', originalmodel, resp, options);
      }
    };
  };

  // helpers
  // -------

  // shared empty constructor function to aid in prototype-chain creation.
  var ctor = function(){};

  // helper function to correctly set up the prototype chain, for subclasses.
  // similar to `goog.inherits`, but uses a hash of prototype properties and
  // class properties to be extended.
  var inherits = function(parent, protoprops, staticprops) {
    var child;

    // the constructor function for the new subclass is either defined by you
    // (the "constructor" property in your `extend` definition), or defaulted
    // by us to simply call the parent's constructor.
    if (protoprops && protoprops.hasownproperty('constructor')) {
      child = protoprops.constructor;
    } else {
      child = function(){ parent.apply(this, arguments); };
    }

    // inherit class (static) properties from parent.
    _.extend(child, parent);

    // set the prototype chain to inherit from `parent`, without calling
    // `parent`'s constructor function.
    ctor.prototype = parent.prototype;
    child.prototype = new ctor();

    // add prototype properties (instance properties) to the subclass,
    // if supplied.
    if (protoprops) _.extend(child.prototype, protoprops);

    // add static properties to the constructor function, if supplied.
    if (staticprops) _.extend(child, staticprops);

    // correctly set child's `prototype.constructor`.
    child.prototype.constructor = child;

    // set a convenience property in case the parent's prototype is needed later.
    child.__super__ = parent.prototype;

    return child;
  };

  // helper function to get a value from a backbone object as a property
  // or as a function.
  var getvalue = function(object, prop) {
    if (!(object && object[prop])) return null;
    return _.isfunction(object[prop]) ? object[prop]() : object[prop];
  };

  // throw an error when a url is needed, and none is supplied.
  var urlerror = function() {
    throw new error('a "url" property or function must be specified');
  };

}).call(this);
