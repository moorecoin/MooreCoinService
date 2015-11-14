var async       = require('async');
var assert      = require('assert');
var ripple      = require('ripple-lib');
var amount      = require('ripple-lib').amount;
var remote      = require('ripple-lib').remote;
var transaction = require('ripple-lib').transaction;
var server      = require('./server').server;
var testutils   = require('./testutils');
var config      = testutils.init_config();

suite('noripple', function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test('set and clear noripple', function(done) {
    var self = this;

    var steps = [

      function (callback) {
        self.what = 'create accounts.';
        testutils.create_accounts($.remote, 'root', '10000.0', [ 'alice' ], callback);
      },

      function (callback) {
        self.what = 'check a non-existent credit limit';

        $.remote.request_ripple_balance('alice', 'root', 'usd', 'current', function(err) {
          assert.strictequal('remoteerror', err.error);
          assert.strictequal('entrynotfound', err.remote.error);
          callback();
        });
      },

      function (callback) {
        self.what = 'create a credit limit with noripple flag';

        var tx = $.remote.transaction();
        tx.trustset('root', '100/usd/alice');
        tx.setflags('noripple');

        tx.once('submitted', function() {
          $.remote.ledger_accept();
        });

        tx.once('error', callback);
        tx.once('proposed', function(res) {
          callback();
        });

        tx.submit();
      },

      function (callback) {
        self.what = 'check no-ripple sender';

        $.remote.requestaccountlines({ account: 'root', ledger: 'validated' }, function(err, m) {
          if (err) return callback(err);
          assert(typeof m === 'object');
          assert(array.isarray(m.lines));
          assert(m.lines[0].no_ripple);
          callback();
        });
      },

      function (callback) {
        self.what = 'check no-ripple destination';

        $.remote.requestaccountlines({ account: 'alice', ledger: 'validated' }, function(err, m) {
          if (err) return callback(err);
          assert(typeof m === 'object');
          assert(array.isarray(m.lines));
          assert(m.lines[0].no_ripple_peer);
          callback();
        });
      },

      function (callback) {
        self.what = 'create a credit limit with clearnoripple flag';

        var tx = $.remote.transaction();
        tx.trustset('root', '100/usd/alice');
        tx.setflags('clearnoripple');

        tx.once('submitted', function() {
          $.remote.ledger_accept();
        });

        tx.once('error', callback);
        tx.once('proposed', function(res) {
          callback();
        });

        tx.submit();
      },

      function (callback) {
        self.what = 'check no-ripple cleared sender';

        $.remote.requestaccountlines({ account: 'root', ledger: 'validated' }, function(err, m) {
          if (err) return callback(err);
          assert(typeof m === 'object');
          assert(array.isarray(m.lines));
          assert(!m.lines[0].no_ripple);
          callback();
        });
      },

      function (callback) {
        self.what = 'check no-ripple cleared destination';

        $.remote.requestaccountlines({ account: 'alice', ledger: 'validated' }, function(err, m) {
          if (err) return callback(err);
          assert(typeof m === 'object');
          assert(array.isarray(m.lines));
          assert(!m.lines[0].no_ripple_peer);
          callback();
        });
      }

    ]

    async.series(steps, function(err) {
      assert(!err, self.what + ': ' + err);
      done();
    });
  });

  test('set noripple on line with negative balance', function(done) {
    // setting noripple on a line with negative balance should fail
    var self = this;

    var steps = [

      function (callback) {
        self.what = 'create accounts';

        testutils.create_accounts(
          $.remote,
          'root',
          '10000.0',
          [ 'alice', 'bob', 'carol' ],
          callback);
      },

      function (callback) {
        self.what = 'set credit limits';

        testutils.credit_limits($.remote, {
          bob: '100/usd/alice',
          carol: '100/usd/bob'
        }, callback);
      },

      function (callback) {
        self.what = 'payment';

        var tx = $.remote.transaction();
        tx.buildpath(true);
        tx.payment('alice', 'carol', '50/usd/carol');

        tx.once('submitted', function(m) {
          assert.strictequal(m.engine_result, 'tessuccess');
          $.remote.ledger_accept();
        });

        tx.submit(callback);
      },

      function (callback) {
        self.what = 'set noripple alice';

        var tx = $.remote.transaction();
        tx.trustset('alice', '100/usd/bob');
        tx.setflags('noripple');

        tx.once('submitted', function(m) {
          assert.strictequal(m.engine_result, 'tessuccess');
          $.remote.ledger_accept();
        });

        tx.submit(callback);
      },

      function (callback) {
        self.what = 'set noripple carol';

        var tx = $.remote.transaction();
        tx.trustset('bob', '100/usd/carol');
        tx.setflags('noripple');

        tx.once('submitted', function(m) {
          assert.strictequal(m.engine_result, 'tessuccess');
          $.remote.ledger_accept();
        });

        tx.submit(callback);
      },

      function (callback) {
        self.what = 'find path alice > carol';

        var request = $.remote.requestripplepathfind('alice', 'carol', '1/usd/carol', [ { currency: 'usd' } ]);
        request.callback(function(err, paths) {
          assert.iferror(err);
          assert(array.isarray(paths.alternatives));
          assert.strictequal(paths.alternatives.length, 1);
          callback();
        });
      },

      function (callback) {
        $.remote.requestaccountlines({ account: 'alice' }, function(err, res) {
          assert.iferror(err);
          assert.strictequal(typeof res, 'object');
          assert(array.isarray(res.lines));
          assert.strictequal(res.lines.length, 1);
          assert(!(res.lines[0].no_ripple));
          callback();
        });
      }

    ]

    async.series(steps, function(error) {
      assert(!error, self.what + ': ' + error);
      done();
    });
  });

  test('pairwise noripple', function(done) {
    var self = this;

    var steps = [

      function (callback) {
        self.what = 'create accounts';

        testutils.create_accounts(
          $.remote,
          'root',
          '10000.0',
          [ 'alice', 'bob', 'carol' ],
          callback);
      },

      function (callback) {
        self.what = 'set credit limits';

        testutils.credit_limits($.remote, {
          bob: '100/usd/alice',
          carol: '100/usd/bob'
        }, callback);
      },

      function (callback) {
        self.what = 'set noripple alice';

        var tx = $.remote.transaction();
        tx.trustset('bob', '100/usd/alice');
        tx.setflags('noripple');

        tx.once('submitted', function(m) {
          assert.strictequal(m.engine_result, 'tessuccess');
          $.remote.ledger_accept();
        });

        tx.submit(callback);
      },

      function (callback) {
        self.what = 'set noripple carol';

        var tx = $.remote.transaction();
        tx.trustset('bob', '100/usd/carol');
        tx.setflags('noripple');

        tx.once('submitted', function(m) {
          assert.strictequal(m.engine_result, 'tessuccess');
          $.remote.ledger_accept();
        });

        tx.submit(callback);
      },

      function (callback) {
        self.what = 'find path alice > carol';

        var request = $.remote.requestripplepathfind('alice', 'carol', '1/usd/carol', [ { currency: 'usd' } ]);
        request.callback(function(err, paths) {
          assert.iferror(err);
          assert(array.isarray(paths.alternatives));
          assert.strictequal(paths.alternatives.length, 0);
          callback();
        });
      },

      function (callback) {
        self.what = 'payment';

        var tx = $.remote.transaction();
        tx.buildpath(true);
        tx.payment('alice', 'carol', '1/usd/carol');

        tx.once('submitted', function(m) {
          assert.strictequal(m.engine_result, 'tecpath_dry');
          $.remote.ledger_accept();
          callback();
        });

        tx.submit();
      }

    ]

    async.series(steps, function(error) {
      assert(!error, self.what + ': ' + error);
      done();
    });
  });
});
