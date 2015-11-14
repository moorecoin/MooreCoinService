/* -------------------------------- requires -------------------------------- */

var async       = require("async");
var assert      = require('assert');
var uint160     = require("ripple-lib").uint160;
var testutils   = require("./testutils");
var config      = testutils.init_config();

/* --------------------------------- consts --------------------------------- */

// some parts of the test take a long time
var skip_ticket_expiry_phase = process.env.travis == null &&
                               process.env.run_ticket_expiry == null;

var root_account = uint160.json_rewrite('root');
var alice_account = uint160.json_rewrite('alice');

/* --------------------------------- helpers -------------------------------- */

var prettyj = function(obj) {
  return json.stringify(obj, undefined, 2);
}

// the create a transaction, submit it, pass a ledger pattern is pervasive
var submit_transaction_factory = function(remote) {
  return function(kwargs, cb) {
    tx = remote.transaction();
    tx.tx_json = kwargs;
    tx.submit(cb);
    testutils.ledger_wait(remote, tx);
  };
};

/* ---------------------------------- tests --------------------------------- */

suite.skip("ticket tests", function() {
  var $ = { };
  var submit_tx;

  setup(function(done) {
    testutils.build_setup().call($, function(){
      submit_tx = submit_transaction_factory($.remote);
      done();
    });
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test("delete a non existent ticket", function(done) {
    submit_tx(
      {
        transactiontype: 'ticketcancel',
        account:  root_account,
        ticketid: '0000000000000000000000000000000000000000000000000000000000000001'
      },
      function(err, message){
        assert.equal(err.engine_result, 'tecno_entry');
        done();
      }
    );
  });

  test("create a ticket with non existent target", function(done) {
    submit_tx(
      {
        transactiontype: 'ticketcreate',
        account: root_account,
        target: alice_account
      },
      function(err, message){
        assert.equal(err.engine_result, 'tecno_target');
        done();
      }
    );
  });

  test("create a ticket where target == account. target unsaved", function(done) {
    submit_tx(
      {
        account: root_account,
        target : root_account,
        transactiontype:'ticketcreate'
      },
      function(err, message){
        assert.iferror(err);
        assert.deepequal(
          message.metadata.affectednodes[1],
          {"creatednode":
            {"ledgerentrytype": "ticket",
             "ledgerindex": "7f58a0ae17775ba3404d55d406dd1c2e91eadd7af3f03a26877bce764ccb75e3",
             /*note there's no `target` saved in the ticket */
             "newfields": {"account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
                           "sequence": 1}}});

        done();
      }
    );
  });

  test("create a ticket, then delete by creator", function (done) {
    var steps = [
      function(callback) {
        submit_tx(
          {
            transactiontype: 'ticketcreate',
            account: root_account,
          },
          function(err, message){
            var affected = message.metadata.affectednodes;
            var account = affected[0].modifiednode;
            var ticket = affected[1].creatednode;

            assert.equal(ticket.ledgerentrytype, 'ticket');
            assert.equal(account.ledgerentrytype, 'accountroot');
            assert.equal(account.previousfields.ownercount, 0);
            assert.equal(account.finalfields.ownercount, 1);
            assert.equal(ticket.newfields.sequence, account.previousfields.sequence);
            assert.equal(ticket.ledgerindex, '7f58a0ae17775ba3404d55d406dd1c2e91eadd7af3f03a26877bce764ccb75e3');

            callback();
          }
        );
      },
      function delete_ticket(callback) {
        submit_tx(
          {
            transactiontype: 'ticketcancel',
            account: root_account,
            ticketid: '7f58a0ae17775ba3404d55d406dd1c2e91eadd7af3f03a26877bce764ccb75e3'
          },
          function(err, message){
            assert.iferror(err);
            assert.equal(message.engine_result, 'tessuccess');

            var affected = message.metadata.affectednodes;
            var account = affected[0].modifiednode;
            var ticket = affected[1].deletednode;
            var directory = affected[2].deletednode;

            assert.equal(ticket.ledgerentrytype, 'ticket');
            assert.equal(account.ledgerentrytype, 'accountroot');
            assert.equal(directory.ledgerentrytype, 'directorynode');

            callback();
          }
        );
      }
    ]
    async.waterfall(steps, done.bind(this));
  });

  test("expiration - future", function (done) {
    // 1000 seconds is an arbitrarily chosen amount of time to expire the
    // ticket. the main thing is that it's not in the past (no ticket ledger
    // entry would be created) or 0 (malformed)
    seconds_from_now = 1000;

    var expiration = $.remote._ledger_time + seconds_from_now;

    submit_tx(
      {
        account: root_account,
        transactiontype: 'ticketcreate',
        expiration: expiration,
      },
      function(err, message){
        assert.iferror(err);

        var affected = message.metadata.affectednodes;
        var account = affected[0].modifiednode;
        var ticket = affected[1].creatednode;

        assert.equal(ticket.ledgerentrytype, 'ticket');
        assert.equal(account.ledgerentrytype, 'accountroot');
        assert.equal(account.previousfields.ownercount, 0);
        assert.equal(account.finalfields.ownercount, 1);
        assert.equal(ticket.newfields.sequence, account.previousfields.sequence);
        assert.equal(ticket.newfields.expiration, expiration);

        done();
      }
    );
  });

  test("expiration - past", function (done) {
    var expiration = $.remote._ledger_time - 1000;

    submit_tx(
      {
        account: root_account,
        transactiontype: 'ticketcreate',
        expiration: expiration,
      },
      function(err, message){
        assert.iferror(err);
        assert.equal(message.engine_result, 'tessuccess');
        var affected = message.metadata.affectednodes;
        assert.equal(affected.length, 1); // just the account
        var account = affected[0].modifiednode;
        assert.equal(account.finalfields.account, root_account);

        done();
      }
    );
  });

  test("expiration - zero", function (done) {
    var expiration = 0;

    submit_tx(
      {
        account: root_account,
        transactiontype: 'ticketcreate',
        expiration: expiration,
      },
      function(err, message) {
        assert.equal(err.engine_result, 'tembad_expiration');
        done();
      }
    );
  });

  test("create a ticket, delete by target", function (done) {
    var steps = [
      function create_alice(callback) {
        testutils.create_accounts($.remote, "root", "10000.0", ["alice"], callback);
      },
      function create_ticket(callback) {
        var account = root_account;
        var target = alice_account;
        submit_tx(
          {
            transactiontype: 'ticketcreate',
            account: account,
            target: target,
          },
          function(err, message) {
            assert.iferror(err);
            assert.deepequal(message.metadata.affectednodes[1],
              {
              creatednode: {
                ledgerentrytype: "ticket",
                ledgerindex: "c231ba31a0e13a4d524a75f990ce0d6890b800ff1ae75e51a2d33559547ac1a2",
                newfields: {
                  account: account,
                  target: target,
                  sequence: 2,
                }
              }
            });
            callback();
          }
        );
      },
      function alice_cancels_ticket(callback) {
        submit_tx(
          {
            account: alice_account,
            transactiontype: 'ticketcancel',
            ticketid: 'c231ba31a0e13a4d524a75f990ce0d6890b800ff1ae75e51a2d33559547ac1a2',
          },
          function(err, message) {
            assert.iferror(err);
            assert.equal(message.engine_result, 'tessuccess');
            assert.deepequal(
              message.metadata.affectednodes[2],
              {"deletednode":
                {"finalfields": {
                  "account": root_account,
                  "flags": 0,
                  "ownernode": "0000000000000000",
                  "sequence": 2,
                  "target": alice_account},
                "ledgerentrytype": "ticket",
                "ledgerindex":
                  "c231ba31a0e13a4d524a75f990ce0d6890b800ff1ae75e51a2d33559547ac1a2"}}
            );
            callback();
          }
        );
      }
    ]
    async.waterfall(steps, done.bind(this));
  });

  test("create a ticket, let it expire, delete by a random", function (done) {
    this.timeout(55000);
    var remote = $.remote;
    var expiration = $.remote._ledger_time + 1;

    steps = [
      function create_ticket(callback) {
        submit_tx(
          {
            account: root_account,
            transactiontype: 'ticketcreate',
            expiration: expiration,

          },
          function(err, message) {
            callback(null, message);
          }
        );
      },
      function verify_creation(message, callback){
        var affected = message.metadata.affectednodes;
        var account = affected[0].modifiednode;
        var ticket = affected[1].creatednode;

        assert.equal(ticket.ledgerentrytype, 'ticket');
        assert.equal(account.ledgerentrytype, 'accountroot');
        assert.equal(account.previousfields.ownercount, 0);
        assert.equal(account.finalfields.ownercount, 1);
        assert.equal(ticket.newfields.sequence, account.previousfields.sequence);
        assert.equal(ticket.ledgerindex, '7f58a0ae17775ba3404d55d406dd1c2e91eadd7af3f03a26877bce764ccb75e3');

        callback();
      },
      function create_account_for_issuing_expiration(callback){
        testutils.create_accounts($.remote,
                                  "root", "1000.0", ["alice"], callback);
      },
      function delete_ticket(callback) {
        submit_tx(
          {
            transactiontype: 'ticketcancel',
            account: alice_account,
            ticketid: '7f58a0ae17775ba3404d55d406dd1c2e91eadd7af3f03a26877bce764ccb75e3',

          },
          function(err, message) {
            // at this point we are unauthorized
            // but it should be expired soon!
            assert.equal(err.engine_result, 'tecno_permission');
            callback();
          }
        );
      },
      function close_ledger(callback) {
        if (skip_ticket_expiry_phase) {
          return done();
        };

        settimeout(callback, 10001);
      },
      function close_ledger(callback) {
        remote.ledger_accept(function(){callback();});
      },
      function close_ledger(callback) {
        settimeout(callback, 10001);
      },
      function close_ledger(callback) {
        remote.ledger_accept(function(){callback();});
      },
      function close_ledger(callback) {
        settimeout(callback, 10001);
      },
      function close_ledger(callback) {
        remote.ledger_accept(function(){callback();});
      },
      function delete_ticket(callback) {
        submit_tx(
          {
            transactiontype: 'ticketcancel',
            account: alice_account,
            ticketid: '7f58a0ae17775ba3404d55d406dd1c2e91eadd7af3f03a26877bce764ccb75e3',
          },
          function(err, message) {
            callback(null, message);
          }
        );
      },

      function verify_deletion (message, callback){
        assert.equal(message.engine_result, 'tessuccess');

        var affected = message.metadata.affectednodes;
        var account = affected[0].modifiednode;
        var ticket = affected[1].deletednode;
        var account2 = affected[2].modifiednode;
        var directory = affected[3].deletednode;

        assert.equal(ticket.ledgerentrytype, 'ticket');
        assert.equal(account.ledgerentrytype, 'accountroot');
        assert.equal(directory.ledgerentrytype, 'directorynode');
        assert.equal(account2.ledgerentrytype, 'accountroot');

        callback();
      }
    ]
    async.waterfall(steps, done.bind(this));
  });
});
// vim:sw=2:sts=2:ts=8:etq
