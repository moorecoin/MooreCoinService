var async       = require("async");
var assert      = require('assert');
var amount      = require("ripple-lib").amount;
var remote      = require("ripple-lib").remote;
var transaction = require("ripple-lib").transaction;
var server      = require("./server").server;
var testutils   = require("./testutils");
var config      = testutils.init_config();

suite('basic path finding', function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test("no direct path, no intermediary -> no alternatives", function (done) {
      var self = this;

      var steps = [
        function (callback) {
          self.what = "create accounts.";
          testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob"], callback);
        },

        function (callback) {
          self.what = "find path from alice to bob";
          var request = $.remote.request_ripple_path_find("alice", "bob", "5/usd/bob", [ { 'currency' : "usd" } ]);
          request.callback(function(err, m) {
            if (m.alternatives.length) {
              callback(new error(m));
            } else {
              callback(null);
            }
          });
        }
      ]

      async.waterfall(steps, function (error) {
        assert(!error, self.what);
        done();
      });
  });

  test("direct path, no intermediary", function (done) {
      var self = this;

      var steps = [
        function (callback) {
          self.what = "create accounts.";
          testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob"], callback);
        },
        function (callback) {
          self.what = "set credit limits.";

          testutils.credit_limits($.remote, {
            "bob"   : "700/usd/alice",
          },
          callback);
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
//          function (callback) {
//            self.what = "display available lines from alice";
//
//            $.remote.request_account_lines("alice", undefined, 'current')
//              .on('success', function (m) {
//                  console.log("lines: %s", json.stringify(m, undefined, 2));
//
//                  callback();
//                })
//              .request();
//          },
        function (callback) {
          self.what = "find path from alice to bob";
          $.remote.request_ripple_path_find("alice", "bob", "5/usd/bob",
            [ { 'currency' : "usd" } ])
            .on('success', function (m) {
                // console.log("proposed: %s", json.stringify(m));

                // 1 alternative.
                assert.strictequal(1, m.alternatives.length)
                // path is empty.
                assert.strictequal(0, m.alternatives[0].paths_canonical.length)

                callback();
              })
            .request();
        },
      ]

      async.waterfall(steps, function(error) {
        assert(!error);
        done();
      });
    });

  test("payment auto path find (using build_path)", function (done) {
      var self = this;

      var steps = [
        function (callback) {
          self.what = "create accounts.";

          testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox"], callback);
        },
        function (callback) {
          self.what = "set credit limits.";

          testutils.credit_limits($.remote,
            {
              "alice" : "600/usd/mtgox",
              "bob"   : "700/usd/mtgox",
            },
            callback);
        },
        function (callback) {
          self.what = "distribute funds.";

          testutils.payments($.remote,
            {
              "mtgox" : [ "70/usd/alice" ],
            },
            callback);
        },
        function (callback) {
          self.what = "payment with auto path";

          $.remote.transaction()
            .payment('alice', 'bob', "24/usd/bob")
            .build_path(true)
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
              "alice"   : "46/usd/mtgox",
              "mtgox"   : [ "-46/usd/alice", "-24/usd/bob" ],
              "bob"     : "24/usd/mtgox",
            },
            callback);
        },
      ]

      async.waterfall(steps, function(error) {
        assert(!error, self.what);
        done();
      });
    });

    test("path find", function (done) {
      var self = this;

      var steps = [
        function (callback) {
          self.what = "create accounts.";

          testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox"], callback);
        },
        function (callback) {
          self.what = "set credit limits.";

          testutils.credit_limits($.remote,
            {
              "alice" : "600/usd/mtgox",
              "bob"   : "700/usd/mtgox",
            },
            callback);
        },
        function (callback) {
          self.what = "distribute funds.";

          testutils.payments($.remote,
            {
              "mtgox" : [ "70/usd/alice", "50/usd/bob" ],
            },
            callback);
        },
        function (callback) {
          self.what = "find path from alice to mtgox";

          $.remote.request_ripple_path_find("alice", "bob", "5/usd/mtgox",
            [ { 'currency' : "usd" } ])
            .on('success', function (m) {
                // console.log("proposed: %s", json.stringify(m));

                // 1 alternative.
                assert.strictequal(1, m.alternatives.length)
                // path is empty.
                assert.strictequal(0, m.alternatives[0].paths_canonical.length)

                callback();
              })
            .request();
        }
      ]

      async.waterfall(steps, function (error) {
        assert(!error, self.what);
        done();
      });
    });

  test("alternative paths - consume both", function (done) {
      var self = this;

      async.waterfall([
          function (callback) {
            self.what = "create accounts.";

            testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox", "bitstamp"], callback);
          },
          function (callback) {
            self.what = "set credit limits.";

            testutils.credit_limits($.remote,
              {
                "alice" : [ "600/usd/mtgox", "800/usd/bitstamp" ],
                "bob"   : [ "700/usd/mtgox", "900/usd/bitstamp" ]
              },
              callback);
          },
          function (callback) {
            self.what = "distribute funds.";

            testutils.payments($.remote,
              {
                "bitstamp" : "70/usd/alice",
                "mtgox" : "70/usd/alice",
              },
              callback);
          },
          function (callback) {
            self.what = "payment with auto path";

            $.remote.transaction()
              .payment('alice', 'bob', "140/usd/bob")
              .build_path(true)
              .once('submitted', function (m) {
                  // console.log("proposed: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
          },
          function (callback) {
            self.what = "verify balances.";

            testutils.verify_balances($.remote,
              {
                "alice"     : [ "0/usd/mtgox", "0/usd/bitstamp" ],
                "bob"       : [ "70/usd/mtgox", "70/usd/bitstamp" ],
                "bitstamp"  : [ "0/usd/alice", "-70/usd/bob" ],
                "mtgox"     : [ "0/usd/alice", "-70/usd/bob" ],
              },
              callback);
          },
        ], function (error) {
          assert(!error, self.what);
          done();
        });
    });

  test("alternative paths - consume best transfer", function (done) {
      var self = this;

      async.waterfall([
          function (callback) {
            self.what = "create accounts.";

            testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox", "bitstamp"], callback);
          },
          function (callback) {
            self.what = "set transfer rate.";

            $.remote.transaction()
              .account_set("bitstamp")
              .transfer_rate(1e9*1.1)
              .once('submitted', function (m) {
                  // console.log("proposed: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
          },
          function (callback) {
            self.what = "set credit limits.";

            testutils.credit_limits($.remote,
              {
                "alice" : [ "600/usd/mtgox", "800/usd/bitstamp" ],
                "bob"   : [ "700/usd/mtgox", "900/usd/bitstamp" ]
              },
              callback);
          },
          function (callback) {
            self.what = "distribute funds.";

            testutils.payments($.remote,
              {
                "bitstamp" : "70/usd/alice",
                "mtgox" : "70/usd/alice",
              },
              callback);
          },
          function (callback) {
            self.what = "payment with auto path";

            $.remote.transaction()
              .payment('alice', 'bob', "70/usd/bob")
              .build_path(true)
              .once('submitted', function (m) {
                  // console.log("proposed: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
          },
          function (callback) {
            self.what = "verify balances.";

            testutils.verify_balances($.remote,
              {
                "alice"     : [ "0/usd/mtgox", "70/usd/bitstamp" ],
                "bob"       : [ "70/usd/mtgox", "0/usd/bitstamp" ],
                "bitstamp"  : [ "-70/usd/alice", "0/usd/bob" ],
                "mtgox"     : [ "0/usd/alice", "-70/usd/bob" ],
              },
              callback);
          },
        ], function (error) {
          assert(!error, self.what);
          done();
        });
    });

  test("alternative paths - consume best transfer first", function (done) {
      var self = this;

      async.waterfall([
          function (callback) {
            self.what = "create accounts.";

            testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox", "bitstamp"], callback);
          },
          function (callback) {
            self.what = "set transfer rate.";

            $.remote.transaction()
              .account_set("bitstamp")
              .transfer_rate(1e9*1.1)
              .once('submitted', function (m) {
                  // console.log("proposed: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
          },
          function (callback) {
            self.what = "set credit limits.";

            testutils.credit_limits($.remote,
              {
                "alice" : [ "600/usd/mtgox", "800/usd/bitstamp" ],
                "bob"   : [ "700/usd/mtgox", "900/usd/bitstamp" ]
              },
              callback);
          },
          function (callback) {
            self.what = "distribute funds.";

            testutils.payments($.remote,
              {
                "bitstamp" : "70/usd/alice",
                "mtgox" : "70/usd/alice",
              },
              callback);
          },
          function (callback) {
            self.what = "payment with auto path";

            $.remote.transaction()
              .payment('alice', 'bob', "77/usd/bob")
              .build_path(true)
              .send_max("100/usd/alice")
              .once('submitted', function (m) {
                  // console.log("proposed: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
          },
          function (callback) {
            self.what = "verify balances.";

            testutils.verify_balances($.remote,
              {
                "alice"     : [ "0/usd/mtgox", "62.3/usd/bitstamp" ],
                "bob"       : [ "70/usd/mtgox", "7/usd/bitstamp" ],
                "bitstamp"  : [ "-62.3/usd/alice", "-7/usd/bob" ],
                "mtgox"     : [ "0/usd/alice", "-70/usd/bob" ],
              },
              callback);
          },
        ], function (error) {
          assert(!error, self.what);
          done();
        });
    });
});

suite('more path finding', function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test("alternative paths - limit returned paths to best quality", function (done) {
    var self = this;

    async.waterfall([
                    function (callback) {
      self.what = "create accounts.";

      testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol", "dan", "mtgox", "bitstamp"], callback);
    },
    function (callback) {
      self.what = "set transfer rate.";

      $.remote.transaction()
      .account_set("carol")
      .transfer_rate(1e9*1.1)
      .once('submitted', function (m) {
        //console.log("proposed: %s", json.stringify(m));
        callback(m.engine_result !== 'tessuccess');
      })
      .submit();
    },
    function (callback) {
      self.what = "set credit limits.";

      testutils.credit_limits($.remote,
        {
          "alice" : [ "800/usd/bitstamp", "800/usd/carol", "800/usd/dan", "800/usd/mtgox", ],
          "bob"   : [ "800/usd/bitstamp", "800/usd/carol", "800/usd/dan", "800/usd/mtgox", ],
          "dan"   : [ "800/usd/alice", "800/usd/bob" ],
        },
        callback);
    },
    function (callback) {
      self.what = "distribute funds.";

      testutils.payments($.remote,
         {
           "bitstamp" : "100/usd/alice",
           "carol" : "100/usd/alice",
           "mtgox" : "100/usd/alice",
         },
         callback);
    },
    // xxx what should this check?
    function (callback) {
      self.what = "find path from alice to bob";

      $.remote.request_ripple_path_find("alice", "bob", "5/usd/bob",
         [ { 'currency' : "usd" } ])
         .on('success', function (m) {
           // console.log("proposed: %s", json.stringify(m));

           // 1 alternative.
           //                  buster.assert.equals(1, m.alternatives.length)
           //                  // path is empty.
           //                  buster.assert.equals(0, m.alternatives[0].paths_canonical.length)

           callback();
         })
         .request();
    }
    ], function (error) {
      assert(!error, self.what);
      done();
    });
  });

  test("alternative paths - consume best transfer", function (done) {
      var self = this;

      async.waterfall([
          function (callback) {
            self.what = "create accounts.";

            testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox", "bitstamp"], callback);
          },
          function (callback) {
            self.what = "set transfer rate.";

            $.remote.transaction()
              .account_set("bitstamp")
              .transfer_rate(1e9*1.1)
              .once('submitted', function (m) {
                  // console.log("proposed: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
          },
          function (callback) {
            self.what = "set credit limits.";

            testutils.credit_limits($.remote,
              {
                "alice" : [ "600/usd/mtgox", "800/usd/bitstamp" ],
                "bob"   : [ "700/usd/mtgox", "900/usd/bitstamp" ]
              },
              callback);
          },
          function (callback) {
            self.what = "distribute funds.";

            testutils.payments($.remote,
              {
                "bitstamp" : "70/usd/alice",
                "mtgox" : "70/usd/alice",
              },
              callback);
          },
          function (callback) {
            self.what = "payment with auto path";

            $.remote.transaction()
              .payment('alice', 'bob', "70/usd/bob")
              .build_path(true)
              .once('submitted', function (m) {
                  // console.log("proposed: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
          },
          function (callback) {
            self.what = "verify balances.";

            testutils.verify_balances($.remote,
              {
                "alice"     : [ "0/usd/mtgox", "70/usd/bitstamp" ],
                "bob"       : [ "70/usd/mtgox", "0/usd/bitstamp" ],
                "bitstamp"  : [ "-70/usd/alice", "0/usd/bob" ],
                "mtgox"     : [ "0/usd/alice", "-70/usd/bob" ],
              },
              callback);
          },
        ], function (error) {
          assert(!error, self.what);
          done();
        });
    });

  test("alternative paths - consume best transfer first", function (done) {
      var self = this;

      async.waterfall([
          function (callback) {
            self.what = "create accounts.";

            testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "mtgox", "bitstamp"], callback);
          },
          function (callback) {
            self.what = "set transfer rate.";

            $.remote.transaction()
              .account_set("bitstamp")
              .transfer_rate(1e9*1.1)
              .once('submitted', function (m) {
                  // console.log("proposed: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
          },
          function (callback) {
            self.what = "set credit limits.";

            testutils.credit_limits($.remote,
              {
                "alice" : [ "600/usd/mtgox", "800/usd/bitstamp" ],
                "bob"   : [ "700/usd/mtgox", "900/usd/bitstamp" ]
              },
              callback);
          },
          function (callback) {
            self.what = "distribute funds.";

            testutils.payments($.remote,
              {
                "bitstamp" : "70/usd/alice",
                "mtgox" : "70/usd/alice",
              },
              callback);
          },
          function (callback) {
            self.what = "payment with auto path";

            $.remote.transaction()
              .payment('alice', 'bob', "77/usd/bob")
              .build_path(true)
              .send_max("100/usd/alice")
              .once('submitted', function (m) {
                  // console.log("proposed: %s", json.stringify(m));
                  callback(m.engine_result !== 'tessuccess');
                })
              .submit();
          },
          function (callback) {
            self.what = "verify balances.";

            testutils.verify_balances($.remote,
              {
                "alice"     : [ "0/usd/mtgox", "62.3/usd/bitstamp" ],
                "bob"       : [ "70/usd/mtgox", "7/usd/bitstamp" ],
                "bitstamp"  : [ "-62.3/usd/alice", "-7/usd/bob" ],
                "mtgox"     : [ "0/usd/alice", "-70/usd/bob" ],
              },
              callback);
          },
        ], function (error) {
          assert(!error, self.what);
          done();
        });
    });
  });

  suite('issues', function() {
      var $ = { };

      setup(function(done) {
        testutils.build_setup().call($, done);
      });

      teardown(function(done) {
        testutils.build_teardown().call($, done);
      });

      test("path negative: issue #5", function (done) {
        var self = this;

        async.waterfall([
            function (callback) {
              self.what = "create accounts.";

              testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol", "dan"], callback);
            },
            function (callback) {
              self.what = "set credit limits.";

              testutils.credit_limits($.remote,
                {
                  //  2. acct 4 trusted all the other accts for 100 usd
                  "dan"   : [ "100/usd/alice", "100/usd/bob", "100/usd/carol" ],
                  //  3. acct 2 acted as a nexus for acct 1 and 3, was trusted by 1 and 3 for 100 usd
                  "alice" : [ "100/usd/bob" ],
                  "carol" : [ "100/usd/bob" ],
                },
                callback);
            },
            function (callback) {
              // 4. acct 2 sent acct 3 a 75 iou
              self.what = "bob sends carol 75.";

              $.remote.transaction()
                .payment("bob", "carol", "75/usd/bob")
                .once('submitted', function (m) {
                    // console.log("proposed: %s", json.stringify(m));

                    callback(m.engine_result !== 'tessuccess');
                  })
                .submit();
            },
            function (callback) {
              self.what = "verify balances.";

              testutils.verify_balances($.remote,
                {
                  "bob"   : [ "-75/usd/carol" ],
                  "carol"   : "75/usd/bob",
                },
                callback);
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
              self.what = "find path from alice to bob";

              // 5. acct 1 sent a 25 usd iou to acct 2
              $.remote.request_ripple_path_find("alice", "bob", "25/usd/bob",
                [ { 'currency' : "usd" } ])
                .on('success', function (m) {
                    // console.log("proposed: %s", json.stringify(m));

                    // 0 alternatives.
                    //assert.strictequal(0, m.alternatives.length)

                    callback(m.alternatives.length !== 0);
                  })
                .request();
            },
            function (callback) {
              self.what = "alice fails to send to bob.";

              $.remote.transaction()
                .payment('alice', 'bob', "25/usd/alice")
                .once('submitted', function (m) {
                    // console.log("proposed: %s", json.stringify(m));
                    callback(m.engine_result !== 'tecpath_dry');
                  })
                .submit();
            },
            function (callback) {
              self.what = "verify balances final.";

              testutils.verify_balances($.remote,
                {
                  "alice" : [ "0/usd/bob", "0/usd/dan"],
                  "bob"   : [ "0/usd/alice", "-75/usd/carol", "0/usd/dan" ],
                  "carol" : [ "75/usd/bob", "0/usd/dan" ],
                  "dan" : [ "0/usd/alice", "0/usd/bob", "0/usd/carol" ],
                },
                callback);
            },
          ], function (error) {
            assert(!error, self.what);
            done();
          });
      });

    //
    // alice -- limit 40 --> bob
    // alice --> carol --> dan --> bob
    // balance of 100 usd bob - balance of 37 usd -> rod
    //
    test("path negative: ripple-client issue #23: smaller", function (done) {
        var self = this;

        async.waterfall([
            function (callback) {
              self.what = "create accounts.";

              testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol", "dan"], callback);
            },
            function (callback) {
              self.what = "set credit limits.";

              testutils.credit_limits($.remote,
                {
                  "bob"   : [ "40/usd/alice", "20/usd/dan" ],
                  "carol" : [ "20/usd/alice" ],
                  "dan"   : [ "20/usd/carol" ],
                },
                callback);
            },
            function (callback) {
              self.what = "payment.";

              $.remote.transaction()
                .payment('alice', 'bob', "55/usd/bob")
                .build_path(true)
                .once('submitted', function (m) {
                    // console.log("proposed: %s", json.stringify(m));
                    callback(m.engine_result !== 'tessuccess');
                  })
                .submit();
            },
            function (callback) {
              self.what = "verify balances.";

              testutils.verify_balances($.remote,
                {
                  "bob"   : [ "40/usd/alice", "15/usd/dan" ],
                },
                callback);
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
          ], function (error) {
            assert(!error, self.what);
            done();
          });
      });

    //
    // alice -120 usd-> amazon -25 usd-> bob
    // alice -25 usd-> carol -75 usd -> dan -100 usd-> bob
    //
    test("path negative: ripple-client issue #23: larger", function (done) {
        var self = this;

        async.waterfall([
            function (callback) {
              self.what = "create accounts.";

              testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol", "dan", "amazon"], callback);
            },
            function (callback) {
              self.what = "set credit limits.";

              testutils.credit_limits($.remote,
                {
                  "amazon"  : [ "120/usd/alice" ],
                  "bob"     : [ "25/usd/amazon", "100/usd/dan" ],
                  "carol"   : [ "25/usd/alice" ],
                  "dan"     : [ "75/usd/carol" ],
                },
                callback);
            },
            function (callback) {
              self.what = "payment.";

              $.remote.transaction()
                .payment('alice', 'bob', "50/usd/bob")
                .build_path(true)
                .once('submitted', function (m) {
                    // console.log("proposed: %s", json.stringify(m));
                    callback(m.engine_result !== 'tessuccess');
                  })
                .submit();
            },
            function (callback) {
              self.what = "verify balances.";

              testutils.verify_balances($.remote,
                {
                  "alice"   : [ "-25/usd/amazon", "-25/usd/carol" ],
                  "bob"     : [ "25/usd/amazon", "25/usd/dan" ],
                  "carol"   : [ "25/usd/alice", "-25/usd/dan" ],
                  "dan"     : [ "25/usd/carol", "-25/usd/bob" ],
                },
                callback);
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
          ], function (error) {
            assert(!error, self.what);
            done();
          });
    });
  });

  suite('via offers', function() {
    var $ = { };

    setup(function(done) {
      testutils.build_setup().call($, done);
    });

    teardown(function(done) {
      testutils.build_teardown().call($, done);
    });

    //carol holds mtgoxaud, sells mtgoxaud for xrp
      //bob will hold mtgoxaud
    //alice pays bob mtgoxaud using xrp
    test("via gateway", function (done) {
      var self = this;

      async.waterfall([
                      function (callback) {
        self.what = "create accounts.";

        testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol", "mtgox"], callback);
      },
      function (callback) {
        self.what = "set transfer rate.";

        $.remote.transaction()
        .account_set("mtgox")
        .transfer_rate(1005000000)
        .once('submitted', function (m) {
          //console.log("proposed: %s", json.stringify(m));
          callback(m.engine_result !== 'tessuccess');
        })
        .submit();
      },
      function (callback) {
        self.what = "set credit limits.";

        testutils.credit_limits($.remote,
                                {
                                  "bob" : [ "100/aud/mtgox" ],
                                  "carol" : [ "100/aud/mtgox" ],
                                },
                                callback);
      },
      function (callback) {
        self.what = "distribute funds.";

        testutils.payments($.remote,
                           {
                             "mtgox" : "50/aud/carol",
                           },
                           callback);
      },
      function (callback) {
        self.what = "carol create offer.";

        $.remote.transaction()
        .offer_create("carol", "50.0", "50/aud/mtgox")
        .once('submitted', function (m) {
          //console.log("proposed: offer_create: %s", json.stringify(m));
          callback(m.engine_result !== 'tessuccess');

          seq_carol = m.tx_json.sequence;
        })
        .submit();
      },
      function (callback) {
        self.what = "alice sends bob 10/aud/mtgox using xrp.";

        //xxx also try sending 10/aux/bob
        $.remote.transaction()
        .payment("alice", "bob", "10/aud/mtgox")
        .build_path(true)
        .send_max("100.0")
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
                                    "bob"   : "10/aud/mtgox",
                                    "carol"   : "39.95/aud/mtgox",
                                  },
                                  callback);
      },
      function (callback) {
        self.what = "display ledger";

        $.remote.request_ledger('current', true)
        .on('success', function (m) {
          //console.log("ledger: %s", json.stringify(m, undefined, 2));

          callback();
        })
        .request();
      },
      function (callback) {
        self.what = "find path from alice to bob";

        // 5. acct 1 sent a 25 usd iou to acct 2
        $.remote.request_ripple_path_find("alice", "bob", "25/usd/bob",
          [ { 'currency' : "usd" } ])
          .on('success', function (m) {
            // console.log("proposed: %s", json.stringify(m));
            // 0 alternatives.
            assert.strictequal(0, m.alternatives.length)
            callback();
          })
          .request();
      }
      ], function (error) {
        assert(!error, self.what);
        done();
      });
    });

    //carol holds mtgoxaud, sells mtgoxaud for xrp
    //bob will hold mtgoxaud
    //alice pays bob mtgoxaud using xrp
    test.skip("via gateway : fix me fails due to xrp rounding and not properly handling dry.", function (done) {
      var self = this;

      async.waterfall([
                      function (callback) {
        self.what = "create accounts.";

        testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol", "mtgox"], callback);
      },
      function (callback) {
        self.what = "set transfer rate.";

        $.remote.transaction()
        .account_set("mtgox")
        .transfer_rate(1005000000)
        .once('submitted', function (m) {
          //console.log("proposed: %s", json.stringify(m));
          callback(m.engine_result !== 'tessuccess');
        })
        .submit();
      },
      function (callback) {
        self.what = "set credit limits.";

        testutils.credit_limits($.remote,
                                {
                                  "bob" : [ "100/aud/mtgox" ],
                                  "carol" : [ "100/aud/mtgox" ],
                                },
                                callback);
      },
      function (callback) {
        self.what = "distribute funds.";

        testutils.payments($.remote,
                           {
                             "mtgox" : "50/aud/carol",
                           },
                           callback);
      },
      function (callback) {
        self.what = "carol create offer.";

        $.remote.transaction()
        .offer_create("carol", "50", "50/aud/mtgox")
        .once('submitted', function (m) {
          // console.log("proposed: offer_create: %s", json.stringify(m));
          callback(m.engine_result !== 'tessuccess');

          seq_carol = m.tx_json.sequence;
        })
        .submit();
      },
      function (callback) {
        self.what = "alice sends bob 10/aud/mtgox using xrp.";

        // xxx also try sending 10/aux/bob
        $.remote.transaction()
        .payment("alice", "bob", "10/aud/mtgox")
        .build_path(true)
        .send_max("100")
        .once('submitted', function (m) {
          // console.log("proposed: %s", json.stringify(m));

          callback(m.engine_result !== 'tessuccess');
        })
        .submit();
      },
      function (callback) {
        self.what = "verify balances.";

        testutils.verify_balances($.remote,
                                  {
                                    "bob"   : "10/aud/mtgox",
                                    "carol"   : "39.95/aud/mtgox",
                                  },
                                  callback);
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
      //          function (callback) {
      //            self.what = "find path from alice to bob";
      //
      //            // 5. acct 1 sent a 25 usd iou to acct 2
      //            $.remote.request_ripple_path_find("alice", "bob", "25/usd/bob",
      //              [ { 'currency' : "usd" } ])
      //              .on('success', function (m) {
      //                  // console.log("proposed: %s", json.stringify(m));
      //
      //                  // 0 alternatives.
      //                  assert.strictequal(0, m.alternatives.length)
      //
      //                  callback();
      //                })
      //              .request();
      //          },
      ], function (error) {
        assert(!error, self.what);
        done();
      });
  });
});

suite('indirect paths', function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test("path find", function (done) {
      var self = this;

      async.waterfall([
          function (callback) {
            self.what = "create accounts.";

            testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob", "carol"], callback);
          },
          function (callback) {
            self.what = "set credit limits.";

            testutils.credit_limits($.remote,
              {
                "bob"   : "1000/usd/alice",
                "carol" : "2000/usd/bob",
              },
              callback);
          },
          function (callback) {
            self.what = "find path from alice to carol";

            $.remote.request_ripple_path_find("alice", "carol", "5/usd/carol",
              [ { 'currency' : "usd" } ])
              .on('success', function (m) {
                  // console.log("proposed: %s", json.stringify(m));

                  // 1 alternative.
                  assert.strictequal(1, m.alternatives.length)
                  // path is empty.
                  assert.strictequal(0, m.alternatives[0].paths_canonical.length)

                  callback();
                })
              .request();
          } ], function (error) {
          assert(!error, self.what);
          done();
        });
    });
});

suite('quality paths', function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test("quality set and test", function (done) {
    var self = this;

    async.waterfall([
        function (callback) {
          self.what = "create accounts.";

          testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob"], callback);
        },
        function (callback) {
          self.what = "set credit limits extended.";

          testutils.credit_limits($.remote,
            {
              "bob"   : "1000/usd/alice:2000,1400000000",
            },
            callback);
        },
        function (callback) {
          self.what = "verify credit limits extended.";

          testutils.verify_limit($.remote, "bob", "1000/usd/alice:2000,1400000000", callback);
        },
      ], function (error) {
        assert(!error, self.what);
        done();
      });
  });

  test.skip("quality payment (broken due to rounding)", function (done) {
    var self = this;

    async.waterfall([
        function (callback) {
          self.what = "create accounts.";

          testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob"], callback);
        },
        function (callback) {
          self.what = "set credit limits extended.";

          testutils.credit_limits($.remote,
            {
              "bob"   : "1000/usd/alice:" + .9*1e9 + "," + 1e9,
            },
            callback);
        },
        function (callback) {
          self.what = "payment with auto path";

          $.remote.trace = true;

          $.remote.transaction()
            .payment('alice', 'bob', "100/usd/bob")
            .send_max("120/usd/alice")
//              .set_flags('partialpayment')
//              .build_path(true)
            .once('submitted', function (m) {
                //console.log("proposed: %s", json.stringify(m));
                callback(m.engine_result !== 'tessuccess');
              })
            .submit();
        },
        function (callback) {
          self.what = "display ledger";

          $.remote.request_ledger('current', { accounts: true, expand: true })
            .on('success', function (m) {
                //console.log("ledger: %s", json.stringify(m, undefined, 2));

                callback();
              })
            .request();
        },
      ], function (error) {
        assert(!error, self.what);
        done();
      });
  });
});

suite("trust auto clear", function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test("trust normal clear", function (done) {
      var self = this;

      async.waterfall([
          function (callback) {
            self.what = "create accounts.";

            testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob"], callback);
          },
          function (callback) {
            self.what = "set credit limits.";

            // mutual trust.
            testutils.credit_limits($.remote,
              {
                "alice"   : "1000/usd/bob",
                "bob"   : "1000/usd/alice",
              },
              callback);
          },
          function (callback) {
            self.what = "verify credit limits.";

            testutils.verify_limit($.remote, "bob", "1000/usd/alice", callback);
          },
          function (callback) {
            self.what = "clear credit limits.";

            // mutual trust.
            testutils.credit_limits($.remote,
              {
                "alice"   : "0/usd/bob",
                "bob"   : "0/usd/alice",
              },
              callback);
          },
          function (callback) {
            self.what = "verify credit limits.";

            testutils.verify_limit($.remote, "bob", "0/usd/alice", function (m) {
                var success = m && 'remoteerror' === m.error && 'entrynotfound' === m.remote.error;

                callback(!success);
              });
          },
          // yyy could verify owner counts are zero.
        ], function (error) {
          assert(!error, self.what);
          done();
        });
  });

  test("trust auto clear", function (done) {
      var self = this;

      async.waterfall([
          function (callback) {
            self.what = "create accounts.";

            testutils.create_accounts($.remote, "root", "10000.0", ["alice", "bob"], callback);
          },
          function (callback) {
            self.what = "set credit limits.";

            // mutual trust.
            testutils.credit_limits($.remote,
              {
                "alice" : "1000/usd/bob",
              },
              callback);
          },
          function (callback) {
            self.what = "distribute funds.";

            testutils.payments($.remote,
              {
                "bob" : [ "50/usd/alice" ],
              },
              callback);
          },
          function (callback) {
            self.what = "clear credit limits.";

            // mutual trust.
            testutils.credit_limits($.remote,
              {
                "alice"   : "0/usd/bob",
              },
              callback);
          },
          function (callback) {
            self.what = "verify credit limits.";

            testutils.verify_limit($.remote, "alice", "0/usd/bob", callback);
          },
          function (callback) {
            self.what = "return funds.";

            testutils.payments($.remote,
              {
                "alice" : [ "50/usd/bob" ],
              },
              callback);
          },
          function (callback) {
            self.what = "verify credit limit gone.";

            testutils.verify_limit($.remote, "bob", "0/usd/alice", function (m) {
                var success = m && 'remoteerror' === m.error && 'entrynotfound' === m.remote.error;

                callback(!success);
              });
          },
        ], function (error) {
          assert(!error, self.what);
          done();
        });
    });
});
// vim:sw=2:sts=2:ts=8:et
