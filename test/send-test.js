var async     = require("async");
var assert    = require('assert');
var amount    = require("ripple-lib").amount;
var remote    = require("ripple-lib").remote;
var server    = require("./server").server;
var testutils = require("./testutils");
var config    = testutils.init_config();

suite('sending', function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test("send xrp to non-existent account with insufficent fee", function (done) {
    var self    = this;
    var ledgers = 20;
    var got_proposed;

    $.remote.transaction()
    .payment('root', 'alice', "1")
    .once('submitted', function (m) {
      // transaction got an error.
      // console.log("proposed: %s", json.stringify(m));
      assert.strictequal(m.engine_result, 'tecno_dst_insuf_xrp');
      got_proposed  = true;
      $.remote.ledger_accept();    // move it along.
    })
    .once('final', function (m) {
      // console.log("final: %s", json.stringify(m, undefined, 2));
      assert.strictequal(m.engine_result, 'tecno_dst_insuf_xrp');
      done();
    })
    .submit();
  });

  // also test transaction becomes lost after tecno_dst.
  test("credit_limit to non-existent account = tecno_dst", function (done) {
    $.remote.transaction()
    .ripple_line_set("root", "100/usd/alice")
    .once('submitted', function (m) {
      //console.log("proposed: %s", json.stringify(m));
      assert.strictequal(m.engine_result, 'tecno_dst');
      done();
    })
    .submit();
  });

  test("credit_limit", function (done) {
    var self = this;

    var steps = [
      function (callback) {
        self.what = "create accounts.";
        testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox"], callback);
      },

      function (callback) {
        self.what = "check a non-existent credit limit.";

        $.remote.request_ripple_balance("alice", "mtgox", "usd", 'current')
        .on('ripple_state', function (m) {
          callback(new error(m));
        })
        .on('error', function(m) {
          // console.log("error: %s", json.stringify(m));

          assert.strictequal('remoteerror', m.error);
          assert.strictequal('entrynotfound', m.remote.error);
          callback();
        })
        .request();
      },

      function (callback) {
        self.what = "create a credit limit.";
        testutils.credit_limit($.remote, "alice", "800/usd/mtgox", callback);
      },

      function (callback) {
        $.remote.request_ripple_balance("alice", "mtgox", "usd", 'current')
        .on('ripple_state', function (m) {
          //                console.log("balance: %s", json.stringify(m));
          //                console.log("account_balance: %s", m.account_balance.to_text_full());
          //                console.log("account_limit: %s", m.account_limit.to_text_full());
          //                console.log("peer_balance: %s", m.peer_balance.to_text_full());
          //                console.log("peer_limit: %s", m.peer_limit.to_text_full());
          assert(m.account_balance.equals("0/usd/alice"));
          assert(m.account_limit.equals("800/usd/mtgox"));
          assert(m.peer_balance.equals("0/usd/mtgox"));
          assert(m.peer_limit.equals("0/usd/alice"));

          callback();
        })
        .request();
      },

      function (callback) {
        self.what = "modify a credit limit.";
        testutils.credit_limit($.remote, "alice", "700/usd/mtgox", callback);
      },

      function (callback) {
        $.remote.request_ripple_balance("alice", "mtgox", "usd", 'current')
        .on('ripple_state', function (m) {
          assert(m.account_balance.equals("0/usd/alice"));
          assert(m.account_limit.equals("700/usd/mtgox"));
          assert(m.peer_balance.equals("0/usd/mtgox"));
          assert(m.peer_limit.equals("0/usd/alice"));

          callback();
        })
        .request();
      },
      // set negative limit.
      function (callback) {
        $.remote.transaction()
        .ripple_line_set("alice", "-1/usd/mtgox")
        .once('submitted', function (m) {
          assert.strictequal('tembad_limit', m.engine_result);
          callback();
        })
        .submit();
      },

      //          function (callback) {
      //            self.what = "display ledger";
      //
      //            $.remote.request_ledger('current', true)
      //              .on('success', function (m) {
      //                  console.log("ledger: %s", json.stringify(m, undefined, 2));
      //
      //                  callback();
      //                })
      //              .request();
      //          },

      function (callback) {
        self.what = "zero a credit limit.";
        testutils.credit_limit($.remote, "alice", "0/usd/mtgox", callback);
      },

      function (callback) {
        self.what = "make sure line is deleted.";

        $.remote.request_ripple_balance("alice", "mtgox", "usd", 'current')
        .on('ripple_state', function (m) {
          // used to keep lines.
          // assert(m.account_balance.equals("0/usd/alice"));
          // assert(m.account_limit.equals("0/usd/alice"));
          // assert(m.peer_balance.equals("0/usd/mtgox"));
          // assert(m.peer_limit.equals("0/usd/mtgox"));
          callback(new error(m));
        })
        .on('error', function (m) {
          // console.log("error: %s", json.stringify(m));
          assert.strictequal('remoteerror', m.error);
          assert.strictequal('entrynotfound', m.remote.error);

          callback();
        })
        .request();
      },
      // todo check in both owner books.
      function (callback) {
        self.what = "set another limit.";
        testutils.credit_limit($.remote, "alice", "600/usd/bob", callback);
      },

      function (callback) {
        self.what = "set limit on other side.";
        testutils.credit_limit($.remote, "bob", "500/usd/alice", callback);
      },

      function (callback) {
        self.what = "check ripple_line's state from alice's pov.";

        $.remote.request_ripple_balance("alice", "bob", "usd", 'current')
        .on('ripple_state', function (m) {
          // console.log("proposed: %s", json.stringify(m));

          assert(m.account_balance.equals("0/usd/alice"));
          assert(m.account_limit.equals("600/usd/bob"));
          assert(m.peer_balance.equals("0/usd/bob"));
          assert(m.peer_limit.equals("500/usd/alice"));

          callback();
        })
        .request();
      },

      function (callback) {
        self.what = "check ripple_line's state from bob's pov.";

        $.remote.request_ripple_balance("bob", "alice", "usd", 'current')
        .on('ripple_state', function (m) {
          assert(m.account_balance.equals("0/usd/bob"));
          assert(m.account_limit.equals("500/usd/alice"));
          assert(m.peer_balance.equals("0/usd/alice"));
          assert(m.peer_limit.equals("600/usd/bob"));

          callback();
        })
        .request();
      }
    ]

    async.waterfall(steps, function(error) {
      assert(!error, self.what);
      done();
    });
  });
});

suite('sending future', function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test('direct ripple', function(done) {
    var self = this;

    // $.remote.set_trace();

    var steps = [
      function (callback) {
        self.what = "create accounts.";
        testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob"], callback);
      },

      function (callback) {
        self.what = "set alice's limit.";
        testutils.credit_limit($.remote, "alice", "600/usd/bob", callback);
      },

      function (callback) {
        self.what = "set bob's limit.";
        testutils.credit_limit($.remote, "bob", "700/usd/alice", callback);
      },

      function (callback) {
        self.what = "set alice send bob partial with alice as issuer.";

        $.remote.transaction()
        .payment('alice', 'bob', "24/usd/alice")
        .once('submitted', function (m) {
          // console.log("proposed: %s", json.stringify(m));
          callback(m.engine_result !== 'tessuccess');
        })
        .once('final', function (m) {
          assert(m.engine_result !== 'tessuccess');
        })
        .submit();
      },

      function (callback) {
        self.what = "verify balance.";

        $.remote.request_ripple_balance("alice", "bob", "usd", 'current')
        .once('ripple_state', function (m) {
          assert(m.account_balance.equals("-24/usd/alice"));
          assert(m.peer_balance.equals("24/usd/bob"));

          callback();
        })
        .request();
      },

      function (callback) {
        self.what = "set alice send bob more with bob as issuer.";

        $.remote.transaction()
        .payment('alice', 'bob', "33/usd/bob")
        .once('submitted', function (m) {
          // console.log("proposed: %s", json.stringify(m));
          callback(m.engine_result !== 'tessuccess');
        })
        .once('final', function (m) {
          assert(m.engine_result !== 'tessuccess');
        })
        .submit();
      },

      function (callback) {
        self.what = "verify balance from bob's pov.";

        $.remote.request_ripple_balance("bob", "alice", "usd", 'current')
        .once('ripple_state', function (m) {
          assert(m.account_balance.equals("57/usd/bob"));
          assert(m.peer_balance.equals("-57/usd/alice"));

          callback();
        })
        .request();
      },

      function (callback) {
        self.what = "bob send back more than sent.";

        $.remote.transaction()
        .payment('bob', 'alice', "90/usd/bob")
        .once('submitted', function (m) {
          // console.log("proposed: %s", json.stringify(m));
          callback(m.engine_result !== 'tessuccess');
        })
        .once('final', function (m) {
          assert(m.engine_result !== 'tessuccess');
        })
        .submit();
      },

      function (callback) {
        self.what = "verify balance from alice's pov: 1";

        $.remote.request_ripple_balance("alice", "bob", "usd", 'current')
        .once('ripple_state', function (m) {
          assert(m.account_balance.equals("33/usd/alice"));

          callback();
        })
        .request();
      },

      function (callback) {
        self.what = "alice send to limit.";

        $.remote.transaction()
        .payment('alice', 'bob', "733/usd/bob")
        .once('submitted', function (m) {
          // console.log("submitted: %s", json.stringify(m));
          callback(m.engine_result !== 'tessuccess');
        })
        .once('final', function (m) {
          assert(m.engine_result !== 'tessuccess');
        })
        .submit();
      },

      function (callback) {
        self.what = "verify balance from alice's pov: 2";

        $.remote.request_ripple_balance("alice", "bob", "usd", 'current')
        .once('ripple_state', function (m) {
          assert(m.account_balance.equals("-700/usd/alice"));

          callback();
        })
        .request();
      },

      function (callback) {
        self.what = "bob send to limit.";

        $.remote.transaction()
        .payment('bob', 'alice', "1300/usd/bob")
        .once('submitted', function (m) {
          // console.log("submitted: %s", json.stringify(m));
          callback(m.engine_result !== 'tessuccess');
        })
        .once('final', function (m) {
          assert(m.engine_result !== 'tessuccess');
        })
        .submit();
      },

      function (callback) {
        self.what = "verify balance from alice's pov: 3";

        $.remote.request_ripple_balance("alice", "bob", "usd", 'current')
        .once('ripple_state', function (m) {
          assert(m.account_balance.equals("600/usd/alice"));

          callback();
        })
        .request();
      },

      function (callback) {
        // if this gets applied out of order, it could stop the big payment.
        self.what = "bob send past limit.";

        $.remote.transaction()
        .payment('bob', 'alice', "1/usd/bob")
        .once('submitted', function (m) {
          // console.log("submitted: %s", json.stringify(m));
          callback(m.engine_result !== 'tecpath_dry');
        })
        .submit();
      },

      function (callback) {
        self.what = "verify balance from alice's pov: 4";

        $.remote.request_ripple_balance("alice", "bob", "usd", 'current')
        .once('ripple_state', function (m) {
          assert(m.account_balance.equals("600/usd/alice"));

          callback();
        })
        .request();
      },

      //        function (callback) {
      //          // make sure all is good after canonical ordering.
      //          self.what = "close the ledger and check balance.";
      //
      //          $.remote
      //            .once('ledger_closed', function (message) {
      //                // console.log("ledger_closed: a: %d: %s", ledger_closed_index, ledger_closed);
      //                callback();
      //              })
      //            .ledger_accept();
      //        },
      //        function (callback) {
      //          self.what = "verify balance from alice's pov: 5";
      //
      //          $.remote.request_ripple_balance("alice", "bob", "usd", 'current')
      //            .once('ripple_state', function (m) {
      //                console.log("account_balance: %s", m.account_balance.to_text_full());
      //                console.log("account_limit: %s", m.account_limit.to_text_full());
      //                console.log("peer_balance: %s", m.peer_balance.to_text_full());
      //                console.log("peer_limit: %s", m.peer_limit.to_text_full());
      //
      //                assert(m.account_balance.equals("600/usd/alice"));
      //
      //                callback();
      //              })
      //            .request();
      //        },
    ]

    async.waterfall(steps, function(error) {
      assert(!error, self.what);
      done();
    });
  });
});

suite('gateway', function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test("customer to customer with and without transfer fee", function (done) {
    var self = this;

    // $.remote.set_trace();

    var steps = [
      function (callback) {
        self.what = "create accounts.";
        testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox"], callback);
      },

      function (callback) {
        self.what = "set credit limits.";

        testutils.credit_limits($.remote, {
          "alice" : "100/aud/mtgox",
          "bob"   : "100/aud/mtgox",
        },
        callback);
      },

      function (callback) {
        self.what = "distribute funds.";

        testutils.payments($.remote, {
          "mtgox" : [ "1/aud/alice" ],
        },
        callback);
      },

      function (callback) {
        self.what = "verify balances.";

        testutils.verify_balances($.remote, {
          "alice"   : "1/aud/mtgox",
          "mtgox"   : "-1/aud/alice",
        },
        callback);
      },

      function (callback) {
        self.what = "alice sends bob 1 aud";

        $.remote.transaction()
        .payment("alice", "bob", "1/aud/mtgox")
        .once('proposed', function (m) {
          // console.log("proposed: %s", json.stringify(m));

          callback(m.engine_result !== 'tessuccess');
        })
        .submit();
      },

      function (callback) {
        self.what = "verify balances 2.";

        testutils.verify_balances($.remote, {
          "alice"   : "0/aud/mtgox",
          "bob"     : "1/aud/mtgox",
          "mtgox"   : "-1/aud/bob",
        },
        callback);
      },

      function (callback) {
        self.what = "set transfer rate.";

        $.remote.transaction()
        .account_set("mtgox")
        .transfer_rate(1e9*1.1)
        .once('proposed', function (m) {
          // console.log("proposed: %s", json.stringify(m));
          callback(m.engine_result !== 'tessuccess');
        })
        .submit();
      },

      function (callback) {
        self.what = "bob sends alice 0.5 aud";

        $.remote.transaction()
        .payment("bob", "alice", "0.5/aud/mtgox")
        .send_max("0.55/aud/mtgox") // !!! very important.
        .once('proposed', function (m) {
          // console.log("proposed: %s", json.stringify(m));

          callback(m.engine_result !== 'tessuccess');
        })
        .submit();
      },

      function (callback) {
        self.what = "verify balances 3.";

        testutils.verify_balances($.remote, {
          "alice"   : "0.5/aud/mtgox",
          "bob"     : "0.45/aud/mtgox",
          "mtgox"   : [ "-0.5/aud/alice","-0.45/aud/bob" ],
        },
        callback);
      },
    ]

    async.waterfall(steps, function(error) {
      assert(!error, self.what);
      done();
    });
  });

  test("customer to customer, transfer fee, default path with and without specific issuer for amount and sendmax", function (done) {

    var self = this;

    // $.remote.set_trace();

    var steps = [
      function (callback) {
        self.what = "create accounts.";
        testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox"], callback);
      },

      function (callback) {
        self.what = "set transfer rate.";

        $.remote.transaction()
        .account_set("mtgox")
        .transfer_rate(1e9*1.1)
        .once('submitted', function (m) {
          // console.log("submitted: %s", json.stringify(m));
          callback(m.engine_result !== 'tessuccess');
        })
        .submit();
      },

      function (callback) {
        self.what = "set credit limits.";

        testutils.credit_limits($.remote, {
          "alice" : "100/aud/mtgox",
          "bob"   : "100/aud/mtgox",
        },
        callback);
      },

      function (callback) {
        self.what = "distribute funds.";

        testutils.payments($.remote, {
          "mtgox" : [ "4.4/aud/alice" ],
        },
        callback);
      },

      function (callback) {
        self.what = "verify balances.";

        testutils.verify_balances($.remote, {
          "alice"   : "4.4/aud/mtgox",
        },
        callback);
      },

      function (callback) {
        self.what = "alice sends 1.1/aud/mtgox bob 1/aud/mtgox";

        $.remote.transaction()
        .payment("alice", "bob", "1/aud/mtgox")
        .send_max("1.1/aud/mtgox")
        .once('submitted', function (m) {
          // console.log("submitted: %s", json.stringify(m));

          callback(m.engine_result !== 'tessuccess');
        })
        .submit();
      },

      function (callback) {
        self.what = "verify balances 2.";

        testutils.verify_balances($.remote, {
          "alice"   : "3.3/aud/mtgox",
          "bob"     : "1/aud/mtgox",
        },
        callback);
      },

      function (callback) {
        self.what = "alice sends 1.1/aud/mtgox bob 1/aud/bob";

        $.remote.transaction()
        .payment("alice", "bob", "1/aud/bob")
        .send_max("1.1/aud/mtgox")
        .once('submitted', function (m) {
          // console.log("submitted: %s", json.stringify(m));

          callback(m.engine_result !== 'tessuccess');
        })
        .submit();
      },

      function (callback) {
        self.what = "verify balances 3.";

        testutils.verify_balances($.remote, {
          "alice"   : "2.2/aud/mtgox",
          "bob"     : "2/aud/mtgox",
        },
        callback);
      },

      function (callback) {
        self.what = "alice sends 1.1/aud/alice bob 1/aud/mtgox";

        $.remote.transaction()
        .payment("alice", "bob", "1/aud/mtgox")
        .send_max("1.1/aud/alice")
        .once('submitted', function (m) {
          // console.log("submitted: %s", json.stringify(m));

          callback(m.engine_result !== 'tessuccess');
        })
        .submit();
      },

      function (callback) {
        self.what = "verify balances 4.";

        testutils.verify_balances($.remote, {
          "alice"   : "1.1/aud/mtgox",
          "bob"     : "3/aud/mtgox",
        },
        callback);
      },

      function (callback) {
        // must fail, doesn't know to use the mtgox
        self.what = "alice sends 1.1/aud/alice bob 1/aud/bob";

        $.remote.transaction()
        .payment("alice", "bob", "1/aud/bob")
        .send_max("1.1/aud/alice")
        .once('submitted', function (m) {
          // console.log("submitted: %s", json.stringify(m));

          callback(m.engine_result !== 'tecpath_dry');
        })
        .submit();
      },

      function (callback) {
        self.what = "verify balances 5.";

        testutils.verify_balances($.remote, {
          "alice"   : "1.1/aud/mtgox",
          "bob"     : "3/aud/mtgox",
        },
        callback);
      }
    ]

    async.waterfall(steps, function(error) {
      assert(!error, self.what);
      done();
    });
  });

  test("subscribe test customer to customer with and without transfer fee", function (done) {
    var self = this;

    // $.remote.set_trace();

    var steps = [
      function (callback) {
        self.what = "create accounts.";
        testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox"], callback);
      },

      function (callback) { testutils.ledger_close($.remote, callback); },

      function (callback) {
        self.what = "set credit limits.";

        testutils.credit_limits($.remote, {
          "alice" : "100/aud/mtgox",
          "bob"   : "100/aud/mtgox",
        },
        callback);
      },

      function (callback) { testutils.ledger_close($.remote, callback); },

      function (callback) {
        self.what = "distribute funds.";

        testutils.payments($.remote, {
          "mtgox" : [ "1/aud/alice" ],
        },
        callback);
      },

      function (callback) { testutils.ledger_close($.remote, callback); },

      function (callback) {
        self.what = "verify balances.";

        testutils.verify_balances($.remote, {
          "alice"   : "1/aud/mtgox",
          "mtgox"   : "-1/aud/alice",
        },
        callback);
      },

      function (callback) {
        self.what = "alice sends bob 1 aud";

        $.remote.transaction()
        .payment("alice", "bob", "1/aud/mtgox")
        .on('proposed', function (m) {
          // console.log("proposed: %s", json.stringify(m));

          callback(m.engine_result !== 'tessuccess');
        })
        .submit();
      },

      function (callback) { testutils.ledger_close($.remote, callback); },

      function (callback) {
        self.what = "verify balances 2.";

        testutils.verify_balances($.remote, {
          "alice"   : "0/aud/mtgox",
          "bob"     : "1/aud/mtgox",
          "mtgox"   : "-1/aud/bob",
        },
        callback);
      },

      function (callback) {
        self.what = "set transfer rate.";

        $.remote.transaction()
        .account_set("mtgox")
        .transfer_rate(1e9*1.1)
        .once('proposed', function (m) {
          // console.log("proposed: %s", json.stringify(m));
          callback(m.engine_result !== 'tessuccess');
        })
        .submit();
      },

      function (callback) { testutils.ledger_close($.remote, callback); },

      function (callback) {
        self.what = "bob sends alice 0.5 aud";

        $.remote.transaction()
        .payment("bob", "alice", "0.5/aud/mtgox")
        .send_max("0.55/aud/mtgox") // !!! very important.
        .on('proposed', function (m) {
          // console.log("proposed: %s", json.stringify(m));

          callback(m.engine_result !== 'tessuccess');
        })
        .submit();
      },

      function (callback) {
        self.what = "verify balances 3.";

        testutils.verify_balances($.remote, {
          "alice"   : "0.5/aud/mtgox",
          "bob"     : "0.45/aud/mtgox",
          "mtgox"   : [ "-0.5/aud/alice","-0.45/aud/bob" ],
        },
        callback);
      },

      function (callback) {
        self.what  = "subscribe and accept.";
        self.count = 0;
        self.found = 0;

        $.remote
        .on('transaction', function (m) {
          // console.log("account: %s", json.stringify(m));
          self.found = 1;
        })
        .on('ledger_closed', function (m) {
          // console.log("ledger_close: %d: %s", self.count, json.stringify(m));
          if (self.count) {
            callback(!self.found);
          } else {
            self.count  = 1;
            $.remote.ledger_accept();
          }
        })
        .request_subscribe().accounts("mtgox")
        .request();

        $.remote.ledger_accept();
      }
    ]

    async.waterfall(steps, function(error) {
      assert(!error, self.what);
      done();
    });
  });

  test("subscribe test: customer to customer with and without transfer fee: transaction retry logic", function (done) {

    var self = this;

    // $.remote.set_trace();

    var steps = [
      function (callback) {
        self.what = "create accounts.";

        testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox"], callback);
      },

      function (callback) {
        self.what = "set credit limits.";

        testutils.credit_limits($.remote,
                                {
                                  "alice" : "100/aud/mtgox",
                                  "bob"   : "100/aud/mtgox",
                                },
                                callback);
      },

      function (callback) {
        self.what = "distribute funds.";

        testutils.payments($.remote, {
          "mtgox" : [ "1/aud/alice" ],
        },
        callback);
      },

      function (callback) {
        self.what = "verify balances.";

        testutils.verify_balances($.remote, {
          "alice"   : "1/aud/mtgox",
          "mtgox"   : "-1/aud/alice",
        },
        callback);
      },

      function (callback) {
        self.what = "alice sends bob 1 aud";

        $.remote.transaction()
        .payment("alice", "bob", "1/aud/mtgox")
        .on('proposed', function (m) {
          // console.log("proposed: %s", json.stringify(m));

          callback(m.engine_result !== 'tessuccess');
        })
        .submit();
      },

      function (callback) {
        self.what = "verify balances 2.";

        testutils.verify_balances($.remote, {
          "alice"   : "0/aud/mtgox",
          "bob"     : "1/aud/mtgox",
          "mtgox"   : "-1/aud/bob",
        },
        callback);
      },

      //          function (callback) {
      //            self.what = "set transfer rate.";
      //
      //            $.remote.transaction()
      //              .account_set("mtgox")
      //              .transfer_rate(1e9*1.1)
      //              .once('proposed', function (m) {
      //                  // console.log("proposed: %s", json.stringify(m));
      //                  callback(m.engine_result !== 'tessuccess');
      //                })
      //              .submit();
      //          },

      // we now need to ensure that all prior transactions have executed before
      // the next transaction is submitted, as txn application logic has
      // changed.
      function(next){$.remote.ledger_accept(function(){next();});},

      function (callback) {
        self.what = "bob sends alice 0.5 aud";

        $.remote.transaction()
        .payment("bob", "alice", "0.5/aud/mtgox")
        .on('proposed', function (m) {
          // console.log("proposed: %s", json.stringify(m));

          callback(m.engine_result !== 'tessuccess');
        })
        .submit();
      },

      function (callback) {
        self.what = "verify balances 3.";

        testutils.verify_balances($.remote, {
          "alice"   : "0.5/aud/mtgox",
          "bob"     : "0.5/aud/mtgox",
          "mtgox"   : [ "-0.5/aud/alice","-0.5/aud/bob" ],
        },
        callback);
      },

      function (callback) {
        self.what  = "subscribe and accept.";
        self.count = 0;
        self.found = 0;

        $.remote
        .on('transaction', function (m) {
          // console.log("account: %s", json.stringify(m));
          self.found  = 1;
        })
        .on('ledger_closed', function (m) {
          // console.log("ledger_close: %d: %s", self.count, json.stringify(m));

          if (self.count) {
            callback(!self.found);
          } else {
            self.count  = 1;
            $.remote.ledger_accept();
          }
        })
        .request_subscribe().accounts("mtgox")
        .request();

        $.remote.ledger_accept();
      },
      function (callback) {
        self.what = "verify balances 4.";

        testutils.verify_balances($.remote, {
          "alice"   : "0.5/aud/mtgox",
          "bob"     : "0.5/aud/mtgox",
          "mtgox"   : [ "-0.5/aud/alice","-0.5/aud/bob" ],
        },
        callback);
      },
    ]

    async.waterfall(steps, function (error) {
      assert(!error, self.what);
      done();
    });
  });
});


suite('indirect ripple', function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test("indirect ripple", function (done) {
    var self = this;

    // $.remote.set_trace();

    var steps = [
      function (callback) {
        self.what = "create accounts.";

        testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox"], callback);
      },
      function (callback) {
        self.what = "set credit limits.";

        testutils.credit_limits($.remote, {
          "alice" : "600/usd/mtgox",
          "bob"   : "700/usd/mtgox",
        },
        callback);
      },
      function (callback) {
        self.what = "distribute funds.";

        testutils.payments($.remote, {
          "mtgox" : [ "70/usd/alice", "50/usd/bob" ],
        },
        callback);
      },
      function (callback) {
        self.what = "verify alice balance with mtgox.";

        testutils.verify_balance($.remote, "alice", "70/usd/mtgox", callback);
      },
      function (callback) {
        self.what = "verify bob balance with mtgox.";

        testutils.verify_balance($.remote, "bob", "50/usd/mtgox", callback);
      },
      function (callback) {
        self.what = "alice sends more than has to issuer: 100 out of 70";

        $.remote.transaction()
        .payment("alice", "mtgox", "100/usd/mtgox")
        .once('submitted', function (m) {
          // console.log("proposed: %s", json.stringify(m));
          callback(m.engine_result !== 'tecpath_partial');
        })
        .submit();
      },
      function (callback) {
        self.what = "alice sends more than has to bob: 100 out of 70";

        $.remote.transaction()
        .payment("alice", "bob", "100/usd/mtgox")
        .once('submitted', function (m) {
          //console.log("proposed: %s", json.stringify(m));
          callback(m.engine_result !== 'tecpath_partial');
        })
        .submit();
      }
    ]

    async.waterfall(steps, function(error) {
      assert(!error, self.what);
      done();
    });
  });

  test("indirect ripple with path", function (done) {
    var self = this;

    var steps = [
      function (callback) {
        self.what = "create accounts.";

        testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox"], callback);
      },
      function (callback) {
        self.what = "set credit limits.";

        testutils.credit_limits($.remote, {
          "alice" : "600/usd/mtgox",
          "bob"   : "700/usd/mtgox",
        },
        callback);
      },
      function (callback) {
        self.what = "distribute funds.";

        testutils.payments($.remote, {
          "mtgox" : [ "70/usd/alice", "50/usd/bob" ],
        },
        callback);
      },
      function (callback) {
        self.what = "alice sends via a path";

        $.remote.transaction()
        .payment("alice", "bob", "5/usd/mtgox")
        .pathadd( [ { account: "mtgox" } ])
        .on('proposed', function (m) {
          // console.log("proposed: %s", json.stringify(m));

          callback(m.engine_result !== 'tessuccess');
        })
        .submit();
      },
      function (callback) {
        self.what = "verify alice balance with mtgox.";

        testutils.verify_balance($.remote, "alice", "65/usd/mtgox", callback);
      },
      function (callback) {
        self.what = "verify bob balance with mtgox.";

        testutils.verify_balance($.remote, "bob", "55/usd/mtgox", callback);
      }
    ]

    async.waterfall(steps, function(error) {
      assert(!error, self.what);
      done();
    });
  });

  test("indirect ripple with multi path", function (done) {
    var self = this;

    var steps = [
      function (callback) {
        self.what = "create accounts.";

        testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol", "amazon", "mtgox"], callback);
      },
      function (callback) {
        self.what = "set credit limits.";

        testutils.credit_limits($.remote, {
          "amazon"  : "2000/usd/mtgox",
          "bob"   : [ "600/usd/alice", "1000/usd/mtgox" ],
          "carol" : [ "700/usd/alice", "1000/usd/mtgox" ],
        },
        callback);
      },
      function (callback) {
        self.what = "distribute funds.";

        testutils.payments($.remote, {
          "mtgox" : [ "100/usd/bob", "100/usd/carol" ],
        },
        callback);
      },
      function (callback) {
        self.what = "alice pays amazon via multiple paths";

        $.remote.transaction()
        .payment("alice", "amazon", "150/usd/mtgox")
        .pathadd( [ { account: "bob" } ])
        .pathadd( [ { account: "carol" } ])
        .on('proposed', function (m) {
          // console.log("proposed: %s", json.stringify(m));

          callback(m.engine_result !== 'tessuccess');
        })
        .submit();
      },
      function (callback) {
        self.what = "verify balances.";

        testutils.verify_balances($.remote, {
          "alice"   : [ "-100/usd/bob", "-50/usd/carol" ],
          "amazon"  : "150/usd/mtgox",
          "bob"     : "0/usd/mtgox",
          "carol"   : "50/usd/mtgox",
        },
        callback);
      },
    ]

    async.waterfall(steps, function(error) {
      assert(!error, self.what);
      done();
    });
  });

  test("indirect ripple with path and transfer fee", function (done) {
    var self = this;

    var steps = [
      function (callback) {
        self.what = "create accounts.";

        testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol", "amazon", "mtgox"], callback);
      },
      function (callback) {
        self.what = "set mtgox transfer rate.";

        testutils.transfer_rate($.remote, "mtgox", 1.1e9, callback);
      },
      function (callback) {
        self.what = "set credit limits.";

        testutils.credit_limits($.remote, {
          "amazon"  : "2000/usd/mtgox",
          "bob"   : [ "600/usd/alice", "1000/usd/mtgox" ],
          "carol" : [ "700/usd/alice", "1000/usd/mtgox" ],
        },
        callback);
      },
      function (callback) {
        self.what = "distribute funds.";

        testutils.payments($.remote, {
          "mtgox" : [ "100/usd/bob", "100/usd/carol" ],
        },
        callback);
      },
      function (callback) {
        self.what = "alice pays amazon via multiple paths";

        $.remote.transaction()
        .payment("alice", "amazon", "150/usd/mtgox")
        .send_max("200/usd/alice")
        .pathadd( [ { account: "bob" } ])
        .pathadd( [ { account: "carol" } ])
        .on('proposed', function (m) {
          // console.log("proposed: %s", json.stringify(m));

          callback(m.engine_result !== 'tessuccess');
        })
        .submit();
      },
      //          function (callback) {
      //            self.what = "display ledger";
      //
      //            $.remote.request_ledger('current', true)
      //              .on('success', function (m) {
      //                  console.log("ledger: %s", json.stringify(m, undefined, 2));
      //
      //                  callback();
      //                })
      //              .request();
      //          },
      function (callback) {
        self.what = "verify balances.";

        // 65.00000000000001 is correct.
        // this is result of limited precision.
        testutils.verify_balances($.remote, {
          "alice"   : [ "-100/usd/bob", "-65.00000000000001/usd/carol" ],
          "amazon"  : "150/usd/mtgox",
          "bob"     : "0/usd/mtgox",
          "carol"   : "35/usd/mtgox",
        },
        callback);
      }
    ]

    async.waterfall(steps, function(error) {
      assert(!error, self.what);
      done();
    });
  })
});

suite('invoice id', function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test('set invoiceid on payment', function(done) {
    var self = this;

    var steps = [
      function (callback) {
        self.what = 'create accounts';
        testutils.create_accounts($.remote, 'root', '10000.0', [ 'alice' ], callback);
      },

      function (callback) {
        self.what = 'send a payment with invoiceid';

        var tx = $.remote.transaction();
        tx.payment('root', 'alice', '10000');
        tx.invoiceid('deadbeef');

        tx.once('submitted', function(m) {
          assert.strictequal(m.engine_result, 'tessuccess');
          assert.strictequal(m.tx_json.invoiceid, 'deadbeef00000000000000000000000000000000000000000000000000000000');
          callback();
        });

        tx.submit();
      }
    ]

    async.series(steps, function(err) {
      assert(!err, self.what + ': ' + err);
      done();
    });
  });
});

// vim:sw=2:sts=2:ts=8:et
