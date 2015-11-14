// create and stop test servers.
//
// usage:
// s = new server(name, config)
// s.verbose()  : optional
//  .start()
//      'started'
//
// s.stop()     : stops server is started.
//   'stopped'
//

// provide servers
//
// servers are created in tmp/server/$server
//

var child        = require("child_process");
var fs           = require("fs");
var path         = require("path");
var util         = require("util");
var assert       = require('assert');
var eventemitter = require('events').eventemitter;
var nodeutils     = require("./nodeutils");

// create a server object
function server(name, config, verbose) {
  this.name        = name;
  this.config      = config;
  this.started     = false;
  this.quiet       = !verbose;
  this.stopping    = false;
  this.ledger_file = null;

  var nodejs_version = process.version.match(/^v(\d+)+\.(\d+)\.(\d+)$/).slice(1,4);
  var wanted_version = [ 0, 8, 18 ];

  while (wanted_version.length && nodejs_version.length && nodejs_version[0] == wanted_version[0])
  {
    nodejs_version.shift();
    wanted_version.shift();
  }

  var sgn = !nodejs_version.length && !wanted_version.length
            ? 0
            : nodejs_version.length
              ? nodejs_version[0] - wanted_version[0]
              : -1;

  if (sgn < 0) {
    console.log("\n*** node.js version is too low.");
    throw "nodejs version is too low.";
  }
};

util.inherits(server, eventemitter);

server.from_config = function (name, config, verbose) {
  return new server(name, config, verbose);
};

server.prototype.serverpath = function() {
  return path.resolve("tmp/server", this.name);
};

server.prototype.configpath = function() {
  return path.join(this.serverpath(), "rippled.cfg");
};

// write a server's rippled.cfg.
server.prototype._writeconfig = function(done) {
  var self  = this;

  fs.writefile(
    this.configpath(),
    object.keys(this.config).map(function(o) {
        return util.format("[%s]\n%s\n", o, self.config[o]);
      }).join(""),
    'utf8', done);
};

server.prototype.set_ledger_file = function(fn) {
  this.ledger_file = __dirname + '/fixtures/' + fn;
}

// spawn the server.
server.prototype._serverspawnsync = function() {
  var self  = this;

  var rippledpath = this.config.rippled_path;
  // override the config with command line if provided
  if (process.env["npm_config_rippled"]) {
    rippledpath = path.resolve(process.cwd(),
        process.env["npm_config_rippled"]);
  } else {
    for (var i = process.argv.length-1; i >= 0; --i) {
      arg = process.argv[i].split("=", 2);
      if (arg.length === 2 && arg[0] === "--rippled") {
        rippledpath = path.resolve(process.cwd(), arg[1]);
        break;
      }
    };
  }
  assert(rippledpath, "rippled_path not provided");

  var args  = [
    "-a",
    "-v",
    "--conf=rippled.cfg"
  ];
  if (this.config.rippled_args) {
    args = this.config.rippled_args.concat(args);
  };

  if (this.ledger_file != null) {
    args.push('--ledgerfile=' + this.ledger_file)
  };

  var options = {
    cwd: this.serverpath(),
    env: process.env,
    stdio: this.quiet ? 'pipe': 'inherit'
  };

  // spawn in standalone mode for now.
  this.child = child.spawn(rippledpath, args, options);

  if (!this.quiet)
    console.log("server: start %s: %s --conf=%s",
                this.child.pid,
                rippledpath,
                args.join(" "),
                this.configpath());


  var stderr = [];
  self.child.stderr.on('data', function(buf) { stderr.push(buf); });

  // by default, just log exits.
  this.child.on('exit', function(code, signal) {
    if (!self.quiet) console.log("server: spawn: server exited code=%s: signal=%s", code, signal);

    self.emit('exited');

    // dump server logs on an abnormal exit
    if (self.quiet && (!self.stopping)) {
      process.stderr.write("rippled stderr:\n");
      for (var i = 0; i < stderr.length; i++) {
        process.stderr.write(stderr[i]);
      };
    };

    // if could not exec: code=127, signal=null
    // if regular exit: code=0, signal=null
    // fail the test if the server has not called "stop".
    assert(self.stopping, 'server died with signal '+signal);
  });
};

// prepare server's working directory.
server.prototype._makebase = function (done) {
  var serverpath  = this.serverpath();
  var self  = this;

  // reset the server directory, build it if needed.
  nodeutils.resetpath(serverpath, '0777', function (e) {
    if (e) throw e;
    self._writeconfig(done);
  });
};

server.prototype.verbose = function () {
  this.quiet  = false;

  return this;
};

// create a standalone server.
// prepare the working directory and spawn the server.
server.prototype.start = function () {
  var self      = this;

  if (!this.quiet) console.log("server: start: %s: %s", this.name, json.stringify(this.config));

  this._makebase(function (e) {
    if (e) throw e;
    self._serverspawnsync();
    self.emit('started');
  });

  return this;
};

// stop a standalone server.
server.prototype.stop = function () {
  var self  = this;

  self.stopping = true;

  if (!this.child) {
    console.log("server: stop: can't stop");
    return;
  }

  // update the on exit to invoke done.
  this.child.on('exit', function (code, signal) {
    if (!self.quiet) console.log("server: stop: server exited");
    self.stopped = true;
    self.emit('stopped');
    delete self.child;
  });

  this.child.kill();

  return this;
};

exports.server = server;

// vim:sw=2:sts=2:ts=8:et
