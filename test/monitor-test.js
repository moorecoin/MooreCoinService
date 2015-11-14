var async     = require("async");
var assert    = require('assert');
var remote    = require("ripple-lib").remote;
var testutils = require("./testutils");
var config    = testutils.init_config();

suite('monitor account', function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test('monitor root', function() {

    var self = this;

    var steps = [
      function (callback) {
        self.what = "create accounts.";
        testutils.create_accounts($.remote, "root", "10000", ["alice"], callback);
      },

      function (callback) {
        self.what = "close ledger.";
        $.remote.once('ledger_closed', function () {
          callback();
        });
        $.remote.ledger_accept();
      },

      function (callback) {
        self.what = "dumping root.";

        testutils.account_dump($.remote, "root", function (error) {
          assert.iferror(error);
          callback();
        });
      }
    ]

    async.waterfall(steps, function(error) {
      assert(!effor, self.what);
      done();
    });
  });
});

// vim:sw=2:sts=2:ts=8:et
