var async       = require("async");
var assert      = require('assert');
var amount      = require("ripple-lib").amount;
var remote      = require("ripple-lib").remote;
var transaction = require("ripple-lib").transaction;
var server      = require("./server").server;
var testutils   = require("./testutils");
var config      = testutils.init_config();

suite("offer tests", function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test("offer create then cancel in one ledger", function (done) {
      var self = this;
      var final_create;

      async.waterfall([
          function (callback) {
            $.remote.transaction()
              .offer_create("root", "500", "100/usd/root")
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess', m);
                })
              .on('final', function (m) {
                  // console.log("final: offer_create: %s", json.stringify(m));

                  assert.strictequal('tessuccess', m.metadata.transactionresult);

                  assert(final_create);
                })
              .submit();
          },
          function (m, callback) {
            $.remote.transaction()
              .offer_cancel("root", m.tx_json.sequence)
              .on('submitted', function (m) {
                  // console.log("proposed: offer_cancel: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess', m);
                })
              .on('final', function (m) {
                  // console.log("final: offer_cancel: %s", json.stringify(m, undefined, 2));

                  assert.strictequal('tessuccess', m.metadata.transactionresult);
                  assert(final_create);
                  done();
                })
              .submit();
          },
          function (m, callback) {
            $.remote
              .once('ledger_closed', function (message) {
                  // console.log("ledger_closed: %d: %s", ledger_index, ledger_hash);
                  final_create  = message;
                })
              .ledger_accept();
          }
        ], function (error) {
          // console.log("result: error=%s", error);
          assert(!error, self.what || "unspecifide error");

          done();
        });
  });

  test("offer create then offer create with cancel in one ledger", function (done) {
      var self = this;
      var final_create;
      var sequence_first;
      var sequence_second;
      var dones = 0;

      async.waterfall([
          function (callback) {
            $.remote.transaction()
              .offer_create("root", "500", "100/usd/root")
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess', m);
                })
              .on('final', function (m) {
                  // console.log("final: offer_create: %s", json.stringify(m));

                  assert.strictequal('tessuccess', m.metadata.transactionresult);
                  assert(final_create);

                  if (3 === ++dones)
                    done();
                })
              .submit();
          },
          function (m, callback) {
            sequence_first  = m.tx_json.sequence;

            // test canceling existant offer.
            $.remote.transaction()
              .offer_create("root", "300", "100/usd/root", undefined, sequence_first)
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess', m);
                })
              .on('final', function (m) {
                  // console.log("final: offer_create: %s", json.stringify(m));

                  assert.strictequal('tessuccess', m.metadata.transactionresult);
                  assert(final_create);

                  if (3 === ++dones)
                    done();
                })
              .submit();
          },
          function (m, callback) {
            sequence_second  = m.tx_json.sequence;
            self.what = "verify offer canceled.";

            testutils.verify_offer_not_found($.remote, "root", sequence_first, callback);
          },
          function (callback) {
            self.what = "verify offer replaced.";

            testutils.verify_offer($.remote, "root", sequence_second, "300", "100/usd/root", callback);
          },
          function (callback) {
            // test canceling non-existant offer.
            $.remote.transaction()
              .offer_create("root", "400", "200/usd/root", undefined, sequence_first)
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess', m);
                })
              .on('final', function (m) {
                  // console.log("final: offer_create: %s", json.stringify(m));

                  assert.strictequal('tessuccess', m.metadata.transactionresult);
                  assert(final_create);

                  if (3 === ++dones)
                    done();
                })
              .submit();
          },
          function (callback) {
            $.remote
              .once('ledger_closed', function (message) {
                  // console.log("ledger_closed: %d: %s", ledger_index, ledger_hash);
                  final_create  = message;
                })
              .ledger_accept();
          }
        ], function (error) {
          // console.log("result: error=%s", error);
          assert(!error, self.what);

          done();
        });
    });

  test("offer create then self crossing offer, no trust lines with self", function (done) {
      var self = this;

      async.waterfall([
        function (callback) {
          self.what = "create first offer.";

          $.remote.transaction()
            .offer_create("root", "500/btc/root", "100/usd/root")
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));

                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
        },
        function (callback) {
          self.what = "create crossing offer.";

          $.remote.transaction()
            .offer_create("root", "100/usd/root", "500/btc/root")
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));

                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
        }
      ], function (error) {
        // console.log("result: error=%s", error);
        assert(!error, self.what);
        done();
      });
    });

  test("offer create then crossing offer with xrp. negative balance.", function (done) {
      var self = this;

      var alices_initial_balance = 499946999680;
      var bobs_initial_balance = 10199999920;

      async.waterfall([
        function (callback) {
          self.what = "create mtgox account.";
          testutils.payment($.remote, "root", "mtgox", 1149999730, callback);
        },
        function (callback) {
          self.what = "create alice account.";

          testutils.payment($.remote, "root", "alice", alices_initial_balance, callback);
        },
        function (callback) {
          self.what = "create bob account.";

          testutils.payment($.remote, "root", "bob", bobs_initial_balance, callback);
        },
        function (callback) {
          self.what = "set transfer rate.";

          $.remote.transaction()
            .account_set("mtgox")
            .transfer_rate(1005000000)
            .once('proposed', function (m) {
                // console.log("proposed: %s", json.stringify(m));
                callback(m.engine_result !== 'tessuccess');
              })
            .submit();
        },
        function (callback) {
          self.what = "set limits.";

          testutils.credit_limits($.remote,
            {
              "alice" : "500/usd/mtgox",
              "bob" : "50/usd/mtgox",
              "mtgox" : "100/usd/alice",
            },
            callback);
        },
        function (callback) {
          self.what = "distribute funds.";

          testutils.payments($.remote,
            {
              "mtgox" : [ "50/usd/alice", "2710505431213761e-33/usd/bob" ]
            },
            callback);
        },
        function (callback) {
          self.what = "create first offer.";

          $.remote.transaction()
            .offer_create("alice", "50/usd/mtgox", "150000.0")    // get 50/usd pay 150000/xrp
              .once('proposed', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));

                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
        },
        function (callback) {
          self.what = "unfund offer.";

          testutils.payments($.remote,
            {
              "alice" : "100/usd/mtgox"
            },
            callback);
        },
        function (callback) {
          self.what = "set limits 2.";

          testutils.credit_limits($.remote,
            {
              "mtgox" : "0/usd/alice",
            },
            callback);
        },
        function (callback) {
          self.what = "verify balances. 1";

          testutils.verify_balances($.remote,
            {
              "alice"   : [ "-50/usd/mtgox" ],
              "bob"     : [ "2710505431213761e-33/usd/mtgox" ],
            },
            callback);
        },
        function (callback) {
          self.what = "create crossing offer.";

          $.remote.transaction()
            .offer_create("bob", "2000.0", "1/usd/mtgox")  // get 2,000/xrp pay 1/usd (has insufficient usd)
              .once('proposed', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));

                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
        },
        function (callback) {
          self.what = "verify balances. 2";

          var alices_fees, alices_num_transactions, alices_tx_fee_units_total,
              alices_tx_fee_units_total, alices_final_balance,

              bobs_fees, bobs_num_transactions, bobs_tx_fee_units_total,
                            bobs_tx_fee_units_total, bobs_final_balance;

          alices_num_transactions = 3;
          alices_tx_fee_units_total = alices_num_transactions * transaction.fee_units["default"]
          alices_tx_fees_total = $.remote.fee_tx(alices_tx_fee_units_total);
          alices_final_balance = amount.from_json(alices_initial_balance)
                                       .subtract(alices_tx_fees_total);

          bobs_num_transactions = 2;
          bobs_tx_fee_units_total = bobs_num_transactions * transaction.fee_units["default"]
          bobs_tx_fees_total = $.remote.fee_tx(bobs_tx_fee_units_total);
          bobs_final_balance = amount.from_json(bobs_initial_balance)
                                       .subtract(bobs_tx_fees_total);

          testutils.verify_balances($.remote,
            {
              "alice"   : [ "-50/usd/mtgox", alices_final_balance.to_json()],
              "bob"     : [   "2710505431213761e-33/usd/mtgox",
              bobs_final_balance.to_json()

                  // bobs_final_balance.to_json()
                  // string(10199999920-($.remote.fee_tx(2*(transaction.fee_units['default'])))).to_number()
                  ],
            },
            callback);
        },
//        function (callback) {
//          self.what = "display ledger";
//
//          $.remote.request_ledger('current', true)
//            .on('success', function (m) {
//                console.log("ledger: %s", json.stringify(m, undefined, 2));
//
//                callback();
//              })
//            .request();
//        },
      ], function (error) {
        // console.log("result: error=%s", error);
        assert(!error, self.what);
        done();
      });
    });

  test("offer create then crossing offer with xrp. reverse order." , function (done) {
      var self = this;

      async.waterfall([
        function (callback) {
          self.what = "create accounts.";

          testutils.create_accounts($.remote, "root", "100000.0", ["alice", "bob", "mtgox"], callback);
        },
        function (callback) {
          self.what = "set limits.";

          testutils.credit_limits($.remote,
            {
              "alice" : "1000/usd/mtgox",
              "bob" : "1000/usd/mtgox"
            },
            callback);
        },
        function (callback) {
          self.what = "distribute funds.";

          testutils.payments($.remote,
            {
              "mtgox" : "500/usd/alice"
            },
            callback);
        },
        function (callback) {
          self.what = "create first offer.";

          $.remote.transaction()
            .offer_create("bob", "1/usd/mtgox", "4000.0")         // get 1/usd pay 4000/xrp : offer pays 4000 xrp for 1 usd
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));

                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
        },
        function (callback) {
          self.what = "create crossing offer.";

          // existing offer pays better than this wants.
          // fully consume existing offer.
          // pay 1 usd, get 4000 xrp.

          $.remote.transaction()
            .offer_create("alice", "150000.0", "50/usd/mtgox")  // get 150,000/xrp pay 50/usd : offer pays 1 usd for 3000 xrp
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));

                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
        },
        function (callback) {
          self.what = "verify balances.";

          testutils.verify_balances($.remote,
            {
              // "bob"     : [   "1/usd/mtgox", string(100000000000-4000000000-(number($.remote.fee_tx(transaction.fee_units['default'] * 2).to_json())))  ],
              "bob"     : [   "1/usd/mtgox", string(100000000000-4000000000-($.remote.fee_tx(transaction.fee_units['default'] * 2).to_number())) ],
              "alice"   : [ "499/usd/mtgox", string(100000000000+4000000000-($.remote.fee_tx(transaction.fee_units['default'] * 2).to_number())) ],
            },
            callback);
        },
//        function (callback) {
//          self.what = "display ledger";
//
//          $.remote.request_ledger('current', true)
//            .on('success', function (m) {
//                console.log("ledger: %s", json.stringify(m, undefined, 2));
//
//                callback();
//              })
//            .request();
//        },
      ], function (error) {
        // console.log("result: error=%s", error);
        assert(!error, self.what);
        done();
      });
    });

  test("offer create then crossing offer with xrp.", function (done) {
      var self = this;

      async.waterfall([
        function (callback) {
          self.what = "create accounts.";

          testutils.create_accounts($.remote, "root", "100000.0", ["alice", "bob", "mtgox"], callback);
        },
        function (callback) {
          self.what = "set limits.";

          testutils.credit_limits($.remote,
            {
              "alice" : "1000/usd/mtgox",
              "bob" : "1000/usd/mtgox"
            },
            callback);
        },
        function (callback) {
          self.what = "distribute funds.";

          testutils.payments($.remote,
            {
              "mtgox" : "500/usd/alice"
            },
            callback);
        },
        function (callback) {
          self.what = "create first offer.";

          $.remote.transaction()
            .offer_create("alice", "150000.0", "50/usd/mtgox")  // pays 1 usd for 3000 xrp
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));

                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
        },
        function (callback) {
          self.what = "create crossing offer.";

          $.remote.transaction()
            .offer_create("bob", "1/usd/mtgox", "4000.0") // pays 4000 xrp for 1 usd
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));

                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
        },
        function (callback) {
          self.what = "verify balances.";

          // new offer pays better than old wants.
          // fully consume new offer.
          // pay 1 usd, get 3000 xrp.

          testutils.verify_balances($.remote,
            {
              "alice"   : [ "499/usd/mtgox", string(100000000000+3000000000-($.remote.fee_tx(2*(transaction.fee_units['default'])).to_number())) ],
              "bob"     : [   "1/usd/mtgox", string(100000000000-3000000000-($.remote.fee_tx(2*(transaction.fee_units['default'])).to_number())) ],
            },
            callback);
        },
//        function (callback) {
//          self.what = "display ledger";
//
//          $.remote.request_ledger('current', true)
//            .on('success', function (m) {
//                console.log("ledger: %s", json.stringify(m, undefined, 2));
//
//                callback();
//              })
//            .request();
//        },
      ], function (error) {
        // console.log("result: error=%s", error);
        assert(!error, self.what);
        done();
      });
    });

  test("offer create then crossing offer with xrp with limit override.", function (done) {
      var self = this;

      async.waterfall([
        function (callback) {
          self.what = "create accounts.";

          testutils.create_accounts($.remote, "root", "100000.0", ["alice", "bob", "mtgox"], callback);
        },
        function (callback) {
          self.what = "set limits.";

          testutils.credit_limits($.remote,
            {
              "alice" : "1000/usd/mtgox",
//              "bob" : "1000/usd/mtgox"
            },
            callback);
        },
        function (callback) {
          self.what = "distribute funds.";

          testutils.payments($.remote,
            {
              "mtgox" : "500/usd/alice"
            },
            callback);
        },
        function (callback) {
          self.what = "create first offer.";

          $.remote.transaction()
            .offer_create("alice", "150000.0", "50/usd/mtgox")  // 300 xrp = 1 usd
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));

                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
        },
//        function (callback) {
//          self.what = "display ledger";
//
//          $.remote.request_ledger('current', true)
//            .on('success', function (m) {
//                console.log("ledger: %s", json.stringify(m, undefined, 2));
//
//                callback();
//              })
//            .request();
//        },
        function (callback) {
          self.what = "create crossing offer.";

          $.remote.transaction()
            .offer_create("bob", "1/usd/mtgox", "3000.0") //
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));

                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
        },
        function (callback) {
          self.what = "verify balances.";

          testutils.verify_balances($.remote,
            {
              "alice"   : [ "499/usd/mtgox", string(100000000000+3000000000-($.remote.fee_tx(2*(transaction.fee_units['default'])).to_number())) ],
              "bob"     : [   "1/usd/mtgox", string(100000000000-3000000000-($.remote.fee_tx(1*(transaction.fee_units['default'])).to_number())) ],
            },
            callback);
        },
//        function (callback) {
//          self.what = "display ledger";
//
//          $.remote.request_ledger('current', true)
//            .on('success', function (m) {
//                console.log("ledger: %s", json.stringify(m, undefined, 2));
//
//                callback();
//              })
//            .request();
//        },
      ], function (error) {
        // console.log("result: error=%s", error);
        assert(!error, self.what);
        done();
      });
    });

  test("offer_create then ledger_accept then offer_cancel then ledger_accept.", function (done) {
      var self = this;
      var final_create;
      var offer_seq;

      async.waterfall([
          function (callback) {
            $.remote.transaction()
              .offer_create("root", "500", "100/usd/root")
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));

                  offer_seq = m.tx_json.sequence;

                  callback(m.engine_result !== 'tessuccess');
                })
              .on('final', function (m) {
                  // console.log("final: offer_create: %s", json.stringify(m));

                  assert.strictequal('tessuccess', m.metadata.transactionresult);

                  final_create  = m;

                  callback();
                })
              .submit();
          },
          function (callback) {
            if (!final_create) {
              $.remote
                .once('ledger_closed', function (mesage) {
                    // console.log("ledger_closed: %d: %s", ledger_index, ledger_hash);

                  })
                .ledger_accept();
            }
            else {
              callback();
            }
          },
          function (callback) {
            // console.log("cancel: offer_cancel: %d", offer_seq);

            $.remote.transaction()
              .offer_cancel("root", offer_seq)
              .on('submitted', function (m) {
                  // console.log("proposed: offer_cancel: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');
                })
              .on('final', function (m) {
                  // console.log("final: offer_cancel: %s", json.stringify(m));

                  assert.strictequal('tessuccess', m.metadata.transactionresult);
                  assert(final_create);

                  done();
                })
              .submit();
          },
          // see if ledger_accept will crash.
          function (callback) {
            $.remote
              .once('ledger_closed', function (mesage) {
                  // console.log("ledger_closed: a: %d: %s", ledger_index, ledger_hash);
                  callback();
                })
              .ledger_accept();
          },
          function (callback) {
            $.remote
              .once('ledger_closed', function (mesage) {
                  // console.log("ledger_closed: b: %d: %s", ledger_index, ledger_hash);
                  callback();
                })
              .ledger_accept();
          },
        ], function (error) {
          // console.log("result: error=%s", error);
          assert(!error, self.what);

          if (error) done();
        });
    });

  test.skip("new user offer_create then ledger_accept then offer_cancel then ledger_accept.", function (done) {

      var self = this;
      var final_create;
      var offer_seq;

      async.waterfall([
          function (callback) {
            $.remote.transaction()
              .payment('root', 'alice', "1000")
              .on('submitted', function (m) {
                // console.log("proposed: %s", json.stringify(m));
                assert.strictequal(m.engine_result, 'tessuccess');
                callback();
              })
              .submit()
          },
          function (callback) {
            $.remote.transaction()
              .offer_create("alice", "500", "100/usd/alice")
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));

                  offer_seq = m.tx_json.sequence;

                  callback(m.engine_result !== 'tessuccess');
                })
              .on('final', function (m) {
                  // console.log("final: offer_create: %s", json.stringify(m));

                  assert.strictequal('tessuccess', m.metadata.transactionresult);

                  final_create  = m;

                  callback();
                })
              .submit();
          },
          function (callback) {
            if (!final_create) {
              $.remote
                .once('ledger_closed', function (mesage) {
                    // console.log("ledger_closed: %d: %s", ledger_index, ledger_hash);

                  })
                .ledger_accept();
            }
            else {
              callback();
            }
          },
          function (callback) {
            // console.log("cancel: offer_cancel: %d", offer_seq);

            $.remote.transaction()
              .offer_cancel("alice", offer_seq)
              .on('submitted', function (m) {
                  // console.log("proposed: offer_cancel: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');
                })
              .on('final', function (m) {
                  // console.log("final: offer_cancel: %s", json.stringify(m));

                  assert.strictequal('tessuccess', m.metadata.transactionresult);
                  assert(final_create);

                  done();
                })
              .submit();
          },
          // see if ledger_accept will crash.
          function (callback) {
            $.remote
              .once('ledger_closed', function (mesage) {
                  // console.log("ledger_closed: a: %d: %s", ledger_index, ledger_hash);
                  callback();
                })
              .ledger_accept();
          },
          function (callback) {
            $.remote
              .once('ledger_closed', function (mesage) {
                  // console.log("ledger_closed: b: %d: %s", ledger_index, ledger_hash);
                  callback();
                })
              .ledger_accept();
          },
        ], function (error) {
          // console.log("result: error=%s", error);
          assert(!error, self.what);
          if (error) done();
        });
    });

  test("offer cancel past and future sequence", function (done) {
      var self = this;
      var final_create;

      async.waterfall([
          function (callback) {
            $.remote.transaction()
              .payment('root', 'alice', amount.from_json("10000.0"))
              .once('submitted', function (m) {
                  //console.log("proposed: createaccount: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess', m);
                })
              .once('error', function(m) {
                  //console.log("error: %s", m);
                  assert(false);
                  callback(m);
                })
              .submit();
          },
          // past sequence but wrong
          function (m, callback) {
            $.remote.transaction()
              .offer_cancel("root", m.tx_json.sequence)
              .once('submitted', function (m) {
                  //console.log("proposed: offer_cancel past: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess', m);
                })
              .submit();
          },
          // same sequence
          function (m, callback) {
            $.remote.transaction()
              .offer_cancel("root", m.tx_json.sequence+1)
              .once('submitted', function (m) {
                  //console.log("proposed: offer_cancel same: %s", json.stringify(m));
                  callback(m.engine_result !== 'tembad_sequence', m);
                })
              .submit();
          },
          // future sequence
          function (m, callback) {
            $.remote.transaction()
              .offer_cancel("root", m.tx_json.sequence+2)
              .once('submitted', function (m) {
                  //console.log("error: offer_cancel future: %s", json.stringify(m));
                  callback(m.engine_result !== 'tembad_sequence');
                })
              .submit();
          },
          // see if ledger_accept will crash.
          function (callback) {
            $.remote
              .once('ledger_closed', function (message) {
                  //console.log("ledger_closed: a: %d: %s", message.ledger_index, message.ledger_hash);
                  callback();
                })
              .ledger_accept();
          },
          function (callback) {
            $.remote
              .once('ledger_closed', function (mesage) {
                  //console.log("ledger_closed: b: %d: %s", message.ledger_index, message.ledger_hash);
                  callback();
                })
              .ledger_accept();
          },
          function (callback) {
            callback();
          }
        ], function (error) {
          //console.log("result: error=%s", error);
          assert(!error, self.what);
          done();
        });
    });

  test("ripple currency conversion : entire offer", function (done) {
    // mtgox in, xrp out
      var self = this;
      var seq;

      async.waterfall([
          function (callback) {
            self.what = "create accounts.";

            testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox"], callback);
          },
          function (callback) {
            self.what = "owner count 0.";

            testutils.verify_owner_count($.remote, "bob", 0, callback);
          },
          function (callback) {
            self.what = "set limits.";

            testutils.credit_limits($.remote,
              {
                "alice" : "100/usd/mtgox",
                "bob" : "1000/usd/mtgox"
              },
              callback);
          },
          function (callback) {
            self.what = "owner counts after trust.";

            testutils.verify_owner_counts($.remote,
              {
                "alice" : 1,
                "bob" : 1,
              },
              callback);
          },
          function (callback) {
            self.what = "distribute funds.";

            testutils.payments($.remote,
              {
                "mtgox" : "100/usd/alice"
              },
              callback);
          },
          function (callback) {
            self.what = "create offer.";

            $.remote.transaction()
              .offer_create("bob", "100/usd/mtgox", "500")
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');

                  seq = m.tx_json.sequence;
                })
              .submit();
          },
          function (callback) {
            self.what = "owner counts after offer create.";

            testutils.verify_owner_counts($.remote,
              {
                "alice" : 1,
                "bob" : 2,
              },
              callback);
          },
          function (callback) {
            self.what = "verify offer balance.";

            testutils.verify_offer($.remote, "bob", seq, "100/usd/mtgox", "500", callback);
          },
          function (callback) {
            self.what = "alice converts usd to xrp.";

            $.remote.transaction()
              .payment("alice", "alice", "500")
              .send_max("100/usd/mtgox")
              .on('submitted', function (m) {
                  // console.log("proposed: %s", json.stringify(m));

                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
          },
          function (callback) {
            self.what = "verify balances.";

            testutils.verify_balances($.remote,
              {
                "alice"   : [ "0/usd/mtgox", string(10000000000+500-($.remote.fee_tx(2*(transaction.fee_units['default'])).to_number())) ],
                "bob"     : "100/usd/mtgox",
              },
              callback);
          },
          function (callback) {
            self.what = "verify offer consumed.";

            testutils.verify_offer_not_found($.remote, "bob", seq, callback);
          },
          function (callback) {
            self.what = "owner counts after consumed.";

            testutils.verify_owner_counts($.remote,
              {
                "alice" : 1,
                "bob" : 1,
              },
              callback);
          },
        ], function (error) {
          assert(!error, self.what);
          done();
        });
    });

  test("ripple currency conversion : offerer into debt", function (done) {
    // alice in, carol out
      var self = this;
      var seq;

      async.waterfall([
          function (callback) {
            self.what = "create accounts.";

            testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol"], callback);
          },
          function (callback) {
            self.what = "set limits.";

            testutils.credit_limits($.remote,
              {
                "alice" : "2000/eur/carol",
                "bob" : "100/usd/alice",
                "carol" : "1000/eur/bob"
              },
              callback);
          },
          function (callback) {
            self.what = "create offer to exchange.";

            $.remote.transaction()
              .offer_create("bob", "50/usd/alice", "200/eur/carol")
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));
                  callback(m.engine_result !== 'tecunfunded_offer');

                  seq = m.tx_json.sequence;
                })
              .submit();
          },
//          function (callback) {
//            self.what = "alice converts usd to eur via offer.";
//
//            $.remote.transaction()
//              .offer_create("alice", "200/eur/carol", "50/usd/alice")
//              .on('submitted', function (m) {
//                  // console.log("proposed: offer_create: %s", json.stringify(m));
//                  callback(m.engine_result !== 'tessuccess');
//
//                  seq = m.tx_json.sequence;
//                })
//              .submit();
//          },
//          function (callback) {
//            self.what = "verify balances.";
//
//            testutils.verify_balances($.remote,
//              {
//                "alice"   : [ "-50/usd/bob", "200/eur/carol" ],
//                "bob"     : [ "50/usd/alice", "-200/eur/carol" ],
//                "carol"   : [ "-200/eur/alice", "200/eur/bob" ],
//              },
//              callback);
//          },
//          function (callback) {
//            self.what = "verify offer consumed.";
//
//            testutils.verify_offer_not_found($.remote, "bob", seq, callback);
//          },
        ], function (error) {
          assert(!error, self.what);
          done();
        });
    });

  test("ripple currency conversion : in parts", function (done) {
      var self = this;
      var seq;

      async.waterfall([
          function (callback) {
            self.what = "create accounts.";

            testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox"], callback);
          },
          function (callback) {
            self.what = "set limits.";

            testutils.credit_limits($.remote,
              {
                "alice" : "200/usd/mtgox",
                "bob" : "1000/usd/mtgox"
              },
              callback);
          },
          function (callback) {
            self.what = "distribute funds.";

            testutils.payments($.remote,
              {
                "mtgox" : "200/usd/alice"
              },
              callback);
          },
          function (callback) {
            self.what = "create offer.";

            $.remote.transaction()
              .offer_create("bob", "100/usd/mtgox", "500")
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');

                  seq = m.tx_json.sequence;
                })
              .submit();
          },
          function (callback) {
            self.what = "alice converts usd to xrp.";

            $.remote.transaction()
              .payment("alice", "alice", "200")
              .send_max("100/usd/mtgox")
              .on('submitted', function (m) {
                  // console.log("proposed: %s", json.stringify(m));

                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
          },
          function (callback) {
            self.what = "verify offer balance.";

            testutils.verify_offer($.remote, "bob", seq, "60/usd/mtgox", "300", callback);
          },
          function (callback) {
            self.what = "verify balances.";

            testutils.verify_balances($.remote,
              {
                "alice"   : [ "160/usd/mtgox", string(10000000000+200-($.remote.fee_tx(2*(transaction.fee_units['default'])).to_number())) ],
                "bob"     : "40/usd/mtgox",
              },
              callback);
          },
          function (callback) {
            self.what = "alice converts usd to xrp should fail due to partialpayment.";

            $.remote.transaction()
              .payment("alice", "alice", "600")
              .send_max("100/usd/mtgox")
              .on('submitted', function (m) {
                  // console.log("proposed: %s", json.stringify(m));

                  callback(m.engine_result !== 'tecpath_partial');
                })
              .submit();
          },
          function (callback) {
            self.what = "alice converts usd to xrp.";

            $.remote.transaction()
              .payment("alice", "alice", "600")
              .send_max("100/usd/mtgox")
              .set_flags('partialpayment')
              .on('submitted', function (m) {
                  // console.log("proposed: %s", json.stringify(m));

                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
          },
          function (callback) {
            self.what = "verify offer consumed.";

            testutils.verify_offer_not_found($.remote, "bob", seq, callback);
          },
          function (callback) {
            self.what = "verify balances.";

            testutils.verify_balances($.remote,
              {
                "alice"   : [ "100/usd/mtgox", string(10000000000+200+300-($.remote.fee_tx(4*(transaction.fee_units['default'])).to_number())) ],
                "bob"     : "100/usd/mtgox",
              },
              callback);
          },
        ], function (error) {
          assert(!error, self.what);
          done();
        });
    });
});

suite("offer cross currency", function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test("ripple cross currency payment - start with xrp", function (done) {
    // alice --> [xrp --> carol --> usd/mtgox] --> bob
      var self = this;
      var seq;

      // $.remote.set_trace();

      async.waterfall([
          function (callback) {
            self.what = "create accounts.";

            testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol", "mtgox"], callback);
          },
          function (callback) {
            self.what = "set limits.";

            testutils.credit_limits($.remote,
              {
                "carol" : "1000/usd/mtgox",
                "bob" : "2000/usd/mtgox"
              },
              callback);
          },
          function (callback) {
            self.what = "distribute funds.";

            testutils.payments($.remote,
              {
                "mtgox" : "500/usd/carol"
              },
              callback);
          },
          function (callback) {
            self.what = "create offer.";

            $.remote.transaction()
              .offer_create("carol", "500.0", "50/usd/mtgox")
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');

                  seq = m.tx_json.sequence;
                })
              .submit();
          },
          function (callback) {
            self.what = "alice send usd/mtgox converting from xrp.";

            $.remote.transaction()
              .payment("alice", "bob", "25/usd/mtgox")
              .send_max("333.0")
              .on('submitted', function (m) {
                  // console.log("proposed: %s", json.stringify(m));

                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
          },
          function (callback) {
            self.what = "verify balances.";

            testutils.verify_balances($.remote,
              {
//              "alice"   : [ "500" ],
                "bob"     : "25/usd/mtgox",
                "carol"   : "475/usd/mtgox",
              },
              callback);
          },
          function (callback) {
            self.what = "verify offer consumed.";

            testutils.verify_offer_not_found($.remote, "bob", seq, callback);
          },
        ], function (error) {
          assert(!error, self.what);
          done();
        });
    });

  test("ripple cross currency payment - end with xrp", function (done) {
    // alice --> [usd/mtgox --> carol --> xrp] --> bob
      var self = this;
      var seq;

      // $.remote.set_trace();

      async.waterfall([
          function (callback) {
            self.what = "create accounts.";

            testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol", "mtgox"], callback);
          },
          function (callback) {
            self.what = "set limits.";

            testutils.credit_limits($.remote,
              {
                "alice" : "1000/usd/mtgox",
                "carol" : "2000/usd/mtgox"
              },
              callback);
          },
          function (callback) {
            self.what = "distribute funds.";

            testutils.payments($.remote,
              {
                "mtgox" : "500/usd/alice"
              },
              callback);
          },
          function (callback) {
            self.what = "create offer.";

            $.remote.transaction()
              .offer_create("carol", "50/usd/mtgox", "500")
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');

                  seq = m.tx_json.sequence;
                })
              .submit();
          },
          function (callback) {
            self.what = "alice send xrp to bob converting from usd/mtgox.";

            $.remote.transaction()
              .payment("alice", "bob", "250")
              .send_max("333/usd/mtgox")
              .on('submitted', function (m) {
                  // console.log("proposed: %s", json.stringify(m));

                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
          },
          function (callback) {
            self.what = "verify balances.";

            testutils.verify_balances($.remote,
              {
                "alice"   : "475/usd/mtgox",
                "bob"     : "10000000250",
                "carol"   : "25/usd/mtgox",
              },
              callback);
          },
          function (callback) {
            self.what = "verify offer partially consumed.";

            testutils.verify_offer($.remote, "carol", seq, "25/usd/mtgox", "250", callback);
          },
        ], function (error) {
          assert(!error, self.what);
          done();
        });
    });

  test("ripple cross currency bridged payment", function (done) {
    // alice --> [usd/mtgox --> carol --> xrp] --> [xrp --> dan --> eur/bitstamp] --> bob
      var self = this;
      var seq_carol;
      var seq_dan;

      //$.remote.set_trace();

      async.waterfall([
          function (callback) {
            self.what = "create accounts.";

            testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol", "dan", "bitstamp", "mtgox"], callback);
          },
          function (callback) {
            self.what = "set limits.";

            testutils.credit_limits($.remote,
              {
                "alice" : "1000/usd/mtgox",
                "bob" : "1000/eur/bitstamp",
                "carol" : "1000/usd/mtgox",
                "dan" : "1000/eur/bitstamp"
              },
              callback);
          },
          function (callback) {
            self.what = "distribute funds.";

            testutils.payments($.remote,
              {
                "bitstamp" : "400/eur/dan",
                "mtgox" : "500/usd/alice",
              },
              callback);
          },
          function (callback) {
            self.what = "create offer carol.";

            $.remote.transaction()
              .offer_create("carol", "50/usd/mtgox", "500")
              .once('proposed', function (m) {
                  //console.log("proposed: offer_create: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');

                  seq_carol = m.tx_json.sequence;
                })
              .submit();
          },
          function (callback) {
            self.what = "create offer dan.";

            $.remote.transaction()
              .offer_create("dan", "500", "50/eur/bitstamp")
              .once('proposed', function (m) {
                  //console.log("proposed: offer_create: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');

                  seq_dan = m.tx_json.sequence;
                })
              .submit();
          },
          function (callback) {
            self.what = "alice send eur/bitstamp to bob converting from usd/mtgox.";

            $.remote.transaction()
              .payment("alice", "bob", "30/eur/bitstamp")
              .send_max("333/usd/mtgox")
              .pathadd( [ { currency: "xrp" } ])
              .once('submitted', function (m) {
                  //console.log("proposed: %s", json.stringify(m));

                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
          },
          function (callback) {
            self.what = "verify balances.";

            testutils.verify_balances($.remote,
              {
                "alice"   : "470/usd/mtgox",
                "bob"     : "30/eur/bitstamp",
                "carol"   : "30/usd/mtgox",
                "dan"     : "370/eur/bitstamp",
              },
              callback);
          },
          function (callback) {
            self.what = "verify carol offer partially consumed.";

            testutils.verify_offer($.remote, "carol", seq_carol, "20/usd/mtgox", "200", callback);
          },
          function (callback) {
            self.what = "verify dan offer partially consumed.";

            testutils.verify_offer($.remote, "dan", seq_dan, "200", "20/eur/mtgox", callback);
          },
        ], function (error) {
          assert(!error, self.what);
          done();
        });
    });
});

suite("offer tests 3", function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test("offer fee consumes funds", function (done) {
      var self = this;
      var final_create;

      async.waterfall([
          function (callback) {
            // provide micro amounts to compensate for fees to make results round nice.
            self.what = "create accounts.";

            // alice has 3 entries in the ledger, via trust lines
            var max_owner_count = 3; //
            // we start off with a
            var reserve_amount = $.remote.reserve(max_owner_count);
            // console.log("\n");
            // console.log("reserve_amount reserve(max_owner_count=%s): %s", max_owner_count,  reserve_amount.to_human());

            // this.tx_json.fee = this.remote.fee_tx(this.fee_units()).to_json();

             //  1 for each trust limit == 3 (alice < mtgox/amazon/bitstamp)
             //  1 for payment          == 4
            var max_txs_per_user = 4;

            // we don't have access to the tx object[s] created below so we
            // just dig into fee_units straight away
            var fee_units_for_all_txs = ( transaction.fee_units["default"] *
                                          max_txs_per_user );

            starting_xrp = reserve_amount.add($.remote.fee_tx(fee_units_for_all_txs))
            // console.log("starting_xrp after %s fee units: ",  fee_units_for_all_txs, starting_xrp.to_human());

            starting_xrp = starting_xrp.add(amount.from_json('100.0'));
            // console.log("starting_xrp adding 100 xrp to sell", starting_xrp.to_human());

            testutils.create_accounts($.remote,
                "root",
                starting_xrp.to_json(),
                ["alice", "bob", "mtgox", "amazon", "bitstamp"],
                callback);
          },
          function (callback) {
            self.what = "set limits.";

            testutils.credit_limits($.remote,
              {
                "alice" : ["1000/usd/mtgox", "1000/usd/amazon","1000/usd/bitstamp"],
                "bob" :   ["1000/usd/mtgox", "1000/usd/amazon"],
              },
              callback);
          },
          function (callback) {
            self.what = "distribute funds.";

            testutils.payments($.remote,
              {
                "mtgox" : [ "500/usd/bob" ],
              },
              callback);
          },
          function (callback) {
            self.what = "create offer bob.";

            $.remote.transaction()
              .offer_create("bob", "200.0", "200/usd/mtgox")
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');
                  seq_carol = m.tx_json.sequence;
                })
              .submit();
          },
          function (callback) {
            // alice has 350 fees - a reserve of 50 = 250 reserve = 100 available.
            // ask for more than available to prove reserve works.
            self.what = "create offer alice.";

            $.remote.transaction()
              .offer_create("alice", "200/usd/mtgox", "200.0")
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');

                  seq_carol = m.tx_json.sequence;
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

            testutils.verify_balances($.remote,
              {
                "alice"   : [ "100/usd/mtgox", "350.0"],
                "bob"     : ["400/usd/mtgox", ],
              },
              callback);
          },
        ], function (error) {
          // console.log("result: error=%s", error);
          assert(!error, self.what);

          done();
        });
    });

  test("offer create then cross offer", function (done) {
      var self = this;
      var final_create;

      async.waterfall([
          function (callback) {
            self.what = "create accounts.";

            testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox"], callback);
          },
          function (callback) {
            self.what = "set transfer rate.";

            $.remote.transaction()
              .account_set("mtgox")
              .transfer_rate(1005000000)
              .once('proposed', function (m) {
                  // console.log("proposed: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
          },
          function (callback) {
            self.what = "set limits.";

            testutils.credit_limits($.remote,
              {
                "alice" : "1000/usd/mtgox",
                "bob" : "1000/usd/mtgox",
                "mtgox" : "50/usd/alice",
              },
              callback);
          },
          function (callback) {
            self.what = "distribute funds.";

            testutils.payments($.remote,
              {
                "mtgox" : [ "1/usd/bob" ],
                "alice" : [ "50/usd/mtgox" ]
              },
              callback);
          },
          function (callback) {
            self.what = "set limits 2.";

            testutils.credit_limits($.remote,
              {
                "mtgox" : "0/usd/alice",
              },
              callback);
          },
          function (callback) {
            self.what = "create offer alice.";

            $.remote.transaction()
              .offer_create("alice", "50/usd/mtgox", "150000.0")
              .once('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');

                  seq_carol = m.tx_json.sequence;
                })
              .submit();
          },
          function (callback) {
            self.what = "create offer bob.";

            $.remote.transaction()
              .offer_create("bob", "100.0", ".1/usd/mtgox")
              .once('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');

                  seq_carol = m.tx_json.sequence;
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

            testutils.verify_balances($.remote,
              {
                "alice"   : "-49.96666666666667/usd/mtgox",
                "bob"     : "0.9665/usd/mtgox",
              },
              callback);
          },
        ], function (error) {
          // console.log("result: error=%s", error);
          assert(!error, self.what);
          done();
        });
    });
});

suite("offer tfsell", function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test("basic sell", function (done) {
      var self = this;
      var final_create, seq_carol;

      async.waterfall([
          function (callback) {
            // provide micro amounts to compensate for fees to make results round nice.
            self.what = "create accounts.";

            var req_amount = $.remote.reserve(1).add($.remote.fee_tx(20)).add(100000000);
            testutils.create_accounts($.remote, "root", req_amount.to_json(),
                                      ["alice", "bob", "mtgox"], callback);
          },
          function (callback) {
            self.what = "set limits.";

            testutils.credit_limits($.remote,
              {
                "alice" : "1000/usd/mtgox",
                "bob" : "1000/usd/mtgox",
              },
              callback);
          },
          function (callback) {
            self.what = "distribute funds.";

            testutils.payments($.remote,
              {
                "mtgox" : [ "500/usd/bob" ],
              },
              callback);
          },
          function (callback) {
            self.what = "create offer bob.";

            $.remote.transaction()
              .offer_create("bob", "200.0", "200/usd/mtgox")
              .set_flags('sell')            // should not matter at all.
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));
                  if (m.engine_result !== 'tessuccess') {
                    throw new error("bob's offercreate tx did not succeed: "+m.engine_result);
                  } else callback(null);

                  seq_carol = m.tx_json.sequence;
                })
              .submit();
          },
          function (callback) {
            // alice has 350 + fees - a reserve of 50 = 250 reserve = 100 available.
            // ask for more than available to prove reserve works.
            self.what = "create offer alice.";

            $.remote.transaction()
              .offer_create("alice", "200/usd/mtgox", "200.0")
              .set_flags('sell')            // should not matter at all.
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');

                  seq_carol = m.tx_json.sequence;
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

            testutils.verify_balances($.remote,
              {
                "alice"   : [ "100/usd/mtgox", "250.0" ],
                "bob"     : "400/usd/mtgox",
              },
              callback);
          },
        ], function (error) {
          // console.log("result: error=%s", error);
          assert(!error, self.what);

          done();
        });
    });

  test("2x sell exceed limit", function (done) {
      var self = this;
      var final_create, seq_carol;

      async.waterfall([
          function (callback) {
            // provide micro amounts to compensate for fees to make results round nice.
            self.what = "create accounts.";
            var starting_xrp = $.amount_for({
              ledger_entries: 1,
              default_transactions: 2,
              extra: '100.0'
            });
            testutils.create_accounts($.remote, "root", starting_xrp, ["alice", "bob", "mtgox"], callback);
          },
          function (callback) {
            self.what = "set limits.";

            testutils.credit_limits($.remote,
              {
                "alice" : "150/usd/mtgox",
                "bob" : "1000/usd/mtgox",
              },
              callback);
          },
          function (callback) {
            self.what = "distribute funds.";

            testutils.payments($.remote,
              {
                "mtgox" : [ "500/usd/bob" ],
              },
              callback);
          },
          function (callback) {
            self.what = "create offer bob.";

            // taker pays 200 xrp for 100 usd.
            // selling usd.
            $.remote.transaction()
              .offer_create("bob", "100.0", "200/usd/mtgox")
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');
                  seq_carol = m.tx_json.sequence;
                })
              .submit();
          },
          function (callback) {
            // alice has 350 fees - a reserve of 50 = 250 reserve = 100 available.
            // ask for more than available to prove reserve works.
            self.what = "create offer alice.";

            // taker pays 100 usd for 100 xrp.
            // selling xrp.
            // will sell all 100 xrp and get more usd than asked for.
            $.remote.transaction()
              .offer_create("alice", "100/usd/mtgox", "100.0")
              .set_flags('sell')
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));
                  if (m.engine_result !== 'tessuccess') {
                    callback(new error("alice's offercreate didn't succeed: "+m.engine_result));
                  } else callback(null);

                  seq_carol = m.tx_json.sequence;
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

            testutils.verify_balances($.remote,
              {
                "alice"   : [ "200/usd/mtgox", "250.0" ],
                "bob"     : "300/usd/mtgox",
              },
              callback);
          },
        ], function (error) {
          // console.log("result: error=%s", error);
          assert(!error, self.what);

          done();
        });
    });
});

suite("client issue #535", function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test("gateway cross currency", function (done) {
      var self = this;
      var final_create;

      async.waterfall([
          function (callback) {
            // provide micro amounts to compensate for fees to make results round nice.
            self.what = "create accounts.";

            var starting_xrp = $.amount_for({
              ledger_entries: 1,
              default_transactions: 2,
              extra: '100.0'
            });

            testutils.create_accounts($.remote, "root", starting_xrp, ["alice", "bob", "mtgox"], callback);
          },
          function (callback) {
            self.what = "set limits.";

            testutils.credit_limits($.remote,
              {
                "alice" : [ "1000/xts/mtgox", "1000/xxx/mtgox" ],
                "bob" : [ "1000/xts/mtgox", "1000/xxx/mtgox" ],
              },
              callback);
          },
          function (callback) {
            self.what = "distribute funds.";

            testutils.payments($.remote,
              {
                "mtgox" : [ "100/xts/alice", "100/xxx/alice", "100/xts/bob", "100/xxx/bob", ],
              },
              callback);
          },
          function (callback) {
            self.what = "create offer alice.";

            $.remote.transaction()
              .offer_create("alice", "100/xts/mtgox", "100/xxx/mtgox")
              .on('submitted', function (m) {
                  // console.log("proposed: offer_create: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');

                  seq_carol = m.tx_json.sequence;
                })
              .submit();
          },
          function (callback) {
            self.what = "bob converts xts to xxx.";

            $.remote.transaction()
              .payment("bob", "bob", "1/xxx/bob")
              .send_max("1.5/xts/bob")
              .build_path(true)
              .on('submitted', function (m) {
                  if (m.engine_result !== 'tessuccess')
                    console.log("proposed: %s", json.stringify(m, undefined, 2));

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

            testutils.verify_balances($.remote,
              {
                "alice"   : [ "101/xts/mtgox", "99/xxx/mtgox", ],
                "bob"   : [ "99/xts/mtgox", "101/xxx/mtgox", ],
              },
              callback);
          },
        ], function (error) {
          if (error)
            //console.log("result: %s: error=%s", self.what, error);
          assert(!error, self.what);

          done();
        });
  });
});
// vim:sw=2:sts=2:ts=8:et
