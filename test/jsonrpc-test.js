var async        = require("async");
var assert       = require('assert');
var http         = require("http");
var jsonrpc      = require("simple-jsonrpc");
var eventemitter = require('events').eventemitter;
var remote       = require("ripple-lib").remote;
var testutils    = require("./testutils");
var config       = testutils.init_config();

function build_setup(options) {
  var setup = testutils.build_setup(options);

  return function (done) {
    var self  = this;

    var http_config = config.http_servers["zed"];

    self.server_events = new eventemitter;

    self.server = http.createserver(function (req, res) {
      // console.log("request");
      var input = "";

      req.setencoding('utf8');

      req.on('data', function (buffer) {
        // console.log("data: %s", buffer);
        input = input + buffer;
      });

      req.on('end', function () {
        var request = json.parse(input);
        // console.log("req: %s", json.stringify(request, undefined, 2));
        self.server_events.emit('request', request, res);
      });

      req.on('close', function () { });
    });

    self.server.listen(http_config.port, http_config.ip, void(0), function () {
      // console.log("server up: %s %d", http_config.ip, http_config.port);
      setup.call(self, done);
    });
  };
};

function build_teardown() {
  var teardown = testutils.build_teardown();

  return function (done) {
    var self  = this;

    self.server.close(function () {
      // console.log("server closed");

      teardown.call(self, done);
    });
  };
};

suite('json-rpc', function() {
  var $ = { };

  setup(function(done) {
    build_setup().call($, done);
  });

  teardown(function(done) {
    build_teardown().call($, done);
  });

  test('server info', function(done) {
    var rippled_config = testutils.get_server_config(config);
    var client  = jsonrpc.client("http://" + rippled_config.rpc_ip + ":" + rippled_config.rpc_port);

    client.call('server_info', [ ], function (result) {
      // console.log(json.stringify(result, undefined, 2));
      assert(typeof result === 'object');
      assert('info' in result);
      done();
    });
  });

  test('subscribe server', function(done) {
    var rippled_config = testutils.get_server_config(config);
    var client         = jsonrpc.client("http://" + rippled_config.rpc_ip + ":" + rippled_config.rpc_port);
    var http_config    = config.http_servers["zed"];

    client.call('subscribe', [{
      'url' :  "http://" + http_config.ip + ":" + http_config.port,
      'streams' : [ 'server' ],
    }], function (result) {
      // console.log(json.stringify(result, undefined, 2));
      assert(typeof result === 'object');
      assert('random' in result);
      done();
    });
  });

  test('subscribe ledger', function(done) {
    var self = this;

    var rippled_config = testutils.get_server_config(config);
    var client         = jsonrpc.client("http://" + rippled_config.rpc_ip + ":" + rippled_config.rpc_port);
    var http_config    = config.http_servers["zed"];

    var steps = [
      function (callback) {
        self.what = "subscribe.";

        client.call('subscribe', [{
          'url' :  "http://" + http_config.ip + ":" + http_config.port,
          'streams' : [ 'ledger' ],
        }], function (result) {
          //console.log(json.stringify(result, undefined, 2));
          assert(typeof result === 'object');
          assert('ledger_index' in result);
          callback();
        });
      },

      function (callback) {
        self.what = "accept a ledger.";

        $.server_events.once('request', function (request, response) {
          // console.log("got: %s", json.stringify(request, undefined, 2));

          assert.strictequal(1, request.params.seq);
          assert.strictequal(3, request.params.ledger_index);

          response.statuscode = 200;
          response.end(json.stringify({
            jsonrpc:  "2.0",
            result:   {},
            id:       request.id
          }));

          callback();
        });

        $.remote.ledger_accept();
      },

      function (callback) {
        self.what = "accept another ledger.";

        $.server_events.once('request', function (request, response) {
          // console.log("got: %s", json.stringify(request, undefined, 2));

          assert.strictequal(2, request.params.seq);
          assert.strictequal(4, request.params.ledger_index);

          response.statuscode = 200;
          response.end(json.stringify({
            jsonrpc:  "2.0",
            result:   {},
            id:       request.id
          }));

          callback();
        });

        $.remote.ledger_accept();
      }
    ]

    async.waterfall(steps, function(error) {
      assert(!error, self.what);
      done();
    });
  });
});
