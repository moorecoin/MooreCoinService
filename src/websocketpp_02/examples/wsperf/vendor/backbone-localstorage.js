// a simple module to replace `backbone.sync` with *localstorage*-based
// persistence. models are given guids, and saved into a json object. simple
// as that.

// generate four random hex digits.
function s4() {
   return (((1+math.random())*0x10000)|0).tostring(16).substring(1);
};

// generate a pseudo-guid by concatenating random hexadecimal.
function guid() {
   return (s4()+s4()+"-"+s4()+"-"+s4()+"-"+s4()+"-"+s4()+s4()+s4());
};

// our store is represented by a single js object in *localstorage*. create it
// with a meaningful name, like the name you'd give a table.
var store = function(name) {
  this.name = name;
  var store = localstorage.getitem(this.name);
  this.data = (store && json.parse(store)) || {};
};

_.extend(store.prototype, {

  // save the current state of the **store** to *localstorage*.
  save: function() {
    localstorage.setitem(this.name, json.stringify(this.data));
  },

  // add a model, giving it a (hopefully)-unique guid, if it doesn't already
  // have an id of it's own.
  create: function(model) {
    if (!model.id) model.id = model.attributes.id = guid();
    this.data[model.id] = model;
    this.save();
    return model;
  },

  // update a model by replacing its copy in `this.data`.
  update: function(model) {
    this.data[model.id] = model;
    this.save();
    return model;
  },

  // retrieve a model from `this.data` by id.
  find: function(model) {
    return this.data[model.id];
  },

  // return the array of all models currently in storage.
  findall: function() {
    return _.values(this.data);
  },

  // delete a model from `this.data`, returning it.
  destroy: function(model) {
    delete this.data[model.id];
    this.save();
    return model;
  }

});

// override `backbone.sync` to use delegate to the model or collection's
// *localstorage* property, which should be an instance of `store`.
backbone.sync = function(method, model, options) {

  var resp;
  var store = model.localstorage || model.collection.localstorage;

  switch (method) {
    case "read":    resp = model.id ? store.find(model) : store.findall(); break;
    case "create":  resp = store.create(model);                            break;
    case "update":  resp = store.update(model);                            break;
    case "delete":  resp = store.destroy(model);                           break;
  }

  if (resp) {
    options.success(resp);
  } else {
    options.error("record not found");
  }
};