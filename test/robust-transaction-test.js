var async       = require('async');
var assert      = require('assert');
var ripple      = require('ripple-lib');
var amount      = require('ripple-lib').amount;
var remote      = require('ripple-lib').remote;
var transaction = require('ripple-lib').transaction;
var server      = require('./server').server;
var testutils   = require('./testutils');
var config      = testutils.init_config();

var make_suite = process.env.travis != null ? suite.skip : suite;
make_suite('robust transaction submission', function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, function() {
      $.remote.local_signing = true;

      $.remote.request_subscribe()
      .accounts($.remote.account('root')._account_id)
      .callback(done);
    });
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  // payment is submitted (without a destination tag)
  // to a destination which requires one.
  //
  // the sequence is now in the future.
  //
  // immediately subsequent transactions should err
  // with terpre_seq.
  //
  // gaps in the sequence should be filled with an
  // empty transaction.
  //
  // transaction should ultimately succeed.
  //
  // subsequent transactions should be submitted with
  // an up-to-date transction sequence. i.e. the
  // internal sequence should always catch up.

  test('sequence realignment', function(done) {
    var self = this;

    var steps = [

      function createaccounts(callback) {
        self.what = 'create accounts';
        testutils.create_accounts($.remote, 'root', '20000.0', [ 'alice', 'bob' ], callback);
      },

      function setrequiredesttag(callback) {
        self.what = 'set requiredesttag';

        var tx = $.remote.transaction().account_set('alice');
        tx.set_flags('requiredesttag');

        tx.once('submitted', function(m) {

          assert.strictequal('tessuccess', m.engine_result);
        });

        tx.once('final', function() {
          callback();
        });

        tx.submit();

        testutils.ledger_wait($.remote, tx);
      },

      function sendinvalidtransaction(callback) {
        self.what = 'send transaction without a destination tag';

        var tx = $.remote.transaction().payment({
          from: 'root',
          to: 'alice',
          amount: amount.from_human('1xrp')
        });

        tx.once('submitted', function(m) {
          assert.strictequal('tefdst_tag_needed', m.engine_result);
        });

        tx.submit();

        //invoke callback immediately
        callback();
      },

      function sendvalidtransaction(callback) {
        self.what = 'send normal transaction which should succeed';

        var tx = $.remote.transaction().payment({
          from:    'root',
          to:      'bob',
          amount:  amount.from_human('1xrp')
        });

        tx.on('submitted', function(m) {
          //console.log('submitted', m);
        });

        tx.once('resubmitted', function() {
          self.resubmitted = true;
        });

        //first attempt at submission should result in
        //terpre_seq as the sequence is still in the future
        tx.once('submitted', function(m) {
          assert.strictequal('terpre_seq', m.engine_result);
        });

        tx.once('final', function() {
          callback();
        });

        tx.submit();

        testutils.ledger_wait($.remote, tx);
      },

      function checkpending(callback) {
        self.what = 'check pending';
        var pending = $.remote.getaccount('root')._transactionmanager._pending;
        assert.strictequal(pending._queue.length, 0, 'pending transactions persisting');
        callback();
      },

      function verifybalance(callback) {
        self.what = 'verify balance';
        testutils.verify_balance($.remote, 'bob', '20001000000', callback);
      }

    ]

    async.series(steps, function(err) {
      assert(!err, self.what + ': ' + err);
      assert(self.resubmitted, 'transaction failed to resubmit');
      done();
    });
  });

  // submit a normal payment which should succeed.
  //
  // remote disconnects immediately after submission
  // and before the validated transaction result is
  // received.
  //
  // remote reconnects in the future. during this
  // time it is presumed that the transaction should
  // have succeeded, but an immediate response was
  // not possible, as the server was disconnected.
  //
  // upon reconnection, recent account transaction
  // history is loaded.
  //
  // the submitted transaction should be detected,
  // and the transaction should ultimately succeed.

  test('temporary server disconnection', function(done) {
    var self = this;

    var steps = [

      function createaccounts(callback) {
        self.what = 'create accounts';
        testutils.create_accounts($.remote, 'root', '20000.0', [ 'alice' ], callback);
      },

      function submittransaction(callback) {
        self.what = 'submit a transaction';

        var tx = $.remote.transaction().payment({
          from: 'root',
          to: 'alice',
          amount: amount.from_human('1xrp')
        });

        tx.submit();

        setimmediate(function() {
          $.remote.once('disconnect', function remotedisconnected() {
            assert(!$.remote._connected);

            tx.once('final', function(m) {
              assert.strictequal(m.engine_result, 'tessuccess');
              callback();
            });

            $.remote.connect(function() {
              testutils.ledger_wait($.remote, tx);
            });
          });

          $.remote.disconnect();
        });
      },

      function waitledger(callback) {
        self.what = 'wait ledger';
        $.remote.once('ledger_closed', function() {
          callback();
        });
        $.remote.ledger_accept();
      },

      function checkpending(callback) {
        self.what = 'check pending';
        var pending = $.remote.getaccount('root')._transactionmanager._pending;
        assert.strictequal(pending._queue.length, 0, 'pending transactions persisting');
        callback();
      },

      function verifybalance(callback) {
        self.what = 'verify balance';
        testutils.verify_balance($.remote, 'alice', '20001000000', callback);
      }

    ]

    async.series(steps, function(err) {
      assert(!err, self.what + ': ' + err);
      done();
    });
  });

  test('temporary server disconnection -- reconnect after max ledger wait', function(done) {
    var self = this;

    var steps = [

      function createaccounts(callback) {
        self.what = 'create accounts';
        testutils.create_accounts($.remote, 'root', '20000.0', [ 'alice' ], callback);
      },

      function waitledgers(callback) {
        self.what = 'wait ledger';
        $.remote.once('ledger_closed', function() {
          callback();
        });
        $.remote.ledger_accept();
      },

      function verifybalance(callback) {
        self.what = 'verify balance';
        testutils.verify_balance($.remote, 'alice', '20000000000', callback);
      },

      function submittransaction(callback) {
        self.what = 'submit a transaction';

        var tx = $.remote.transaction().payment({
          from: 'root',
          to: 'alice',
          amount: amount.from_human('1xrp')
        });

        tx.once('submitted', function(m) {
          assert.strictequal(m.engine_result, 'tessuccess');

          var handlemessage = $.remote._handlemessage;
          $.remote._handlemessage = function(){};

          var ledgers = 0;

          ;(function nextledger() {
            if (++ledgers > 8) {
              tx.once('final', function() { callback(); });
              $.remote._handlemessage = handlemessage;
              $.remote.disconnect(function() {
                assert(!$.remote._connected);
                var pending = $.remote.getaccount('root')._transactionmanager._pending;
                assert.strictequal(pending._queue.length, 1, 'pending transactions persisting');
                $.remote.connect();
              });
            } else {
              $.remote._getserver().once('ledger_closed', function() {
                settimeout(nextledger, 20);
              });
              $.remote.ledger_accept();
            }
          })();
        });

        tx.submit();
      },

      function waitledgers(callback) {
        self.what = 'wait ledgers';

        var ledgers = 0;

        ;(function nextledger() {
          $.remote.once('ledger_closed', function() {
            if (++ledgers === 3) {
              callback();
            } else {
              settimeout(nextledger, process.env.travis ? 400 : 100 );
            }
          });
          $.remote.ledger_accept();
        })();
      },

      function checkpending(callback) {
        self.what = 'check pending';
        var pending = $.remote.getaccount('root')._transactionmanager._pending;
        assert.strictequal(pending._queue.length, 0, 'pending transactions persisting');
        callback();
      },

      function verifybalance(callback) {
        self.what = 'verify balance';
        testutils.verify_balance($.remote, 'alice', '20001000000', callback);
      }

    ]

    async.series(steps, function(err) {
      assert(!err, self.what + ': ' + err);
      done();
    });
  });

  // submit request times out
  //
  // since the transaction id is generated locally, the
  // transaction should still validate from the account
  // transaction stream, even without a response to the
  // original submit request

  test('submission timeout', function(done) {
    var self = this;

    var steps = [

      function createaccounts(callback) {
        self.what = 'create accounts';
        testutils.create_accounts($.remote, 'root', '20000.0', [ 'alice' ], callback);
      },

      function submittransaction(callback) {
        self.what = 'submit a transaction whose submit request times out';

        var tx = $.remote.transaction().payment({
          from: 'root',
          to: 'alice',
          amount: amount.from_human('1xrp')
        });

        var timed_out = false;

        $.remote.getaccount('root')._transactionmanager._submissiontimeout = 0;

        // a response from transaction submission should never
        // actually be received
        tx.once('timeout', function() { timed_out = true; });

        tx.once('final', function(m) {
          assert(timed_out, 'transaction submission failed to time out');
          assert.strictequal(m.engine_result, 'tessuccess');
          callback();
        });

        tx.submit();

        testutils.ledger_wait($.remote, tx);
      },

      function checkpending(callback) {
        self.what = 'check pending';
        assert.strictequal($.remote.getaccount('root')._transactionmanager._pending.length(), 0, 'pending transactions persisting');
        callback();
      },

      function verifybalance(callback) {
        self.what = 'verify balance';
        testutils.verify_balance($.remote, 'alice', '20001000000', callback);
      }

    ]

    async.series(steps, function(err) {
      assert(!err, self.what + ': ' + err);
      done();
    });
  });

  // subscribing to accounts_proposed will result in ripple-lib
  // being streamed non-validated (proposed) transactions
  //
  // this test ensures that only validated transactions will
  // trigger a transaction success event

  test('subscribe to accounts_proposed', function(done) {
    var self = this;

    var series = [

      function subscribetoaccountsproposed(callback) {
        self.what = 'subscribe to accounts_proposed';

        $.remote.requestsubscribe()
        .addaccountproposed('root')
        .callback(callback);
      },

      function submittransaction(callback) {
        self.what = 'submit a transaction';

        var tx = $.remote.transaction().accountset('root');

        var receivedproposedtransaction = false;

        $.remote.on('transaction', function(tx) {
          if (tx.status === 'proposed') {
            receivedproposedtransaction = true;
          }
        });

        tx.submit(function(err, m) {
          assert(!err, err);
          assert(receivedproposedtransaction, 'did not received proposed transaction from stream');
          assert(m.engine_result, 'tessuccess');
          assert(m.validated, 'transaction is finalized with invalidated transaction stream response');
          done();
        });

        testutils.ledger_wait($.remote, tx);
      }

    ]

    async.series(series, function(err, m) {
      assert(!err, self.what + ': ' + err);
      done();
    });
  });

  // validate that lastledgersequence works
  test('set lastledgersequence', function(done) {
    var self = this;

    var series = [

      function createaccounts(callback) {
        self.what = 'create accounts';
        testutils.create_accounts($.remote, 'root', '20000.0', [ 'alice' ], callback);
      },

      function submittransaction(callback) {
        var tx = $.remote.transaction().payment('root', 'alice', '1');
        tx.lastledger(0);

        tx.once('submitted', function(m) {
          assert.strictequal(m.engine_result, 'tefmax_ledger');
          callback();
        });

        tx.submit();
      }

    ]

    async.series(series, function(err) {
      assert(!err, self.what);
      done();
    });
  });
});
