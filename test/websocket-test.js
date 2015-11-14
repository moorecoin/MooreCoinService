var assert    = require('assert');
var extend    = require("extend");
var server    = require("./server").server;
var remote    = require("ripple-lib").remote;
var testutils = require('./testutils');
var config    = testutils.init_config();

suite('websocket connection', function() {
  var server;

  setup(function(done) {
    this.timeout(2000);

    var host = config.server_default;
    var cfg = testutils.get_server_config(config, host);
    if (cfg.no_server) {
      done();
    } else {
      server = server.from_config(host, cfg);
      server.once('started', done)
      server.start();
    }
  });

  teardown(function(done) {
    this.timeout(2000);
    
    var cfg = testutils.get_server_config(config);
    if (cfg.no_server) {
      done();
    } else {
      server.on('stopped', done);
      server.stop();
    }
  });

  test('websocket connect and disconnect', function(done) {
    // this timeout probably doesn't need to be this long, because
    // the test itself completes in less than half a second. 
    // however, some of the overhead, especially on windows can
    // push the measured time out this far.
    this.timeout(3000);

    var host = config.server_default;
    var alpha = remote.from_config(host);

    alpha.on('connected', function () {
      alpha.on('disconnected', function () {
        // closed
        done();
      });
      alpha.connect(false);
    })

    alpha.connect();
  });
});

// vim:sw=2:sts=2:ts=8:et
