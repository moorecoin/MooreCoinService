/**
   this is a sample ripple npm integration test intended to be copied as a basis
   for new npm tests.
*/

// these three lines are required to initialize any test suite.
var async       = require('async');
var testutils   = require('./testutils');
var config      = testutils.init_config();

// delete any of these next variables that aren't used in the test.
var account     = require('ripple-lib').uint160;
var amount      = require('ripple-lib').amount;
var currency    = require('ripple-lib').uint160;
var remote      = require('ripple-lib').remote;
var server      = require('./server').server;
var transaction = require('ripple-lib').transaction;
var assert      = require('assert');
var extend      = require('extend');
var fs          = require('fs');
var http        = require('http');
var path        = require('path');

suite('sample test suite', function() {
  var $ = {};
  var opts = {};

  setup(function(done) {
    testutils.build_setup(opts).call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test('sample test', function (done) {
    var self = this;

    var steps = [
      function stepone(callback) {
          self.what = 'step one of the sample test';
          assert(true);
          callback();
      },

      function steptwo(callback) {
          self.what = 'step two of the sample test';
          assert(true);
          callback();
      },
    ];

    async.waterfall(steps, function (error) {
      assert(!error, self.what + ': ' + error);
      done();
    });
  });
});
