////var assert      = require('assert');
////var async       = require("async");
////var extend      = require('extend');
////var amount      = require("ripple-lib").amount;
////var remote      = require("ripple-lib").remote;
////var transaction = require("ripple-lib").transaction;
////var rippleerror = require("ripple-lib").rippleerror;
////var server      = require("./server").server;
////var testutils   = require("./testutils");
////var config      = testutils.init_config();
////
////// hard-coded limits we'll be testing:
////var binary_limit    = 500;
////var nonbinary_limit = 200;
////
////var account         = "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth";
////var offset          = 180;
////var limit           = 170;
////
////var first_batch     = 199; // within both limits
////var second_batch    = 10; // between nonbinary_limit and binary_limit
////var third_batch     = 295; // exceeds both limits
////var verbose         = false;
////
////
////suite('account_tx tests', function() {
////  var $ = { };
////
////  setup(function(done) {
////    testutils.build_setup().call($, done);
////  });
////
////  teardown(function(done) {
////    testutils.build_teardown().call($, done);
////  });
////
////  test('make many transactions and query using account_tx', function(done) {
////    var root_id = $.remote.account('root')._account_id;
////    $.remote.request_subscribe().accounts(root_id).request();
////
////    var self = this;
////    var transactioncounter = 0;
////    var final_create;
////
////    function createoffer(callback) {
////      var tx = $.remote.transaction();
////      tx.offer_create("root", "500", "100/usd/root");
////
////      tx.once('proposed', function(m) {
////        $.remote.ledger_accept();
////      });
////
////      tx.once('error', callback);
////
////      tx.once('final', function(m) {
////        callback();
////      });
////
////      tx.submit();
////    };
////
////    function lotsoftransactions(count, callback) {
////      ;(function nextoffer(i) {
////        createoffer(function(err) {
////          console.log(i, count);
////          if (err) callback(err);
////          else if (++i === count) callback(null);
////          else nextoffer(i);
////        });
////      })(0);
////    };
////
////    function firstbatch() {
////      lotsoftransactions(first_batch, function(a, b) {
////        runtests(self, first_batch, 0, void(0), function() {
////          console.log('2');
////          runtests(self, first_batch, offset, void(0), function() {
////            console.log('3');
////            runtests(self, first_batch, -1, limit, secondbatch);
////          })
////        })});
////    }
////
////    function secondbatch() {
////      lotsoftransactions(second_batch, function() {
////        runtests(self, first_batch+second_batch, 0, void(0), function() {
////          runtests(self, first_batch+second_batch, offset, void(0), thirdbatch);
////        })});
////    }
////
////    function thirdbatch() {
////      lotsoftransactions(third_batch, function() {
////        runtests(self, first_batch+second_batch+third_batch, 0, void(0), function() {
////          runtests(self, first_batch+second_batch+third_batch, offset, void(0), done);
////        })});
////    }
////
////    firstbatch();
////
////    function errorhandler(callback) {
////      return function(r) {
////        if (verbose) console.log("error!");
////        if (verbose) console.log(r);
////        callback(r);
////      }
////    }
////
////    function txoptions(ext) {
////      var defaults = {
////        account:           account,
////        ledger_index_min:  -1,
////        ledger_index_max:  -1
////      }
////      return extend(defaults, ext);
////    };
////
////    function sortbyledger(a, b) {
////      assert(a.tx, 'transaction missing');
////      assert(b.tx, 'transaction missing');
////      if (a.tx.inledger > b.tx.inledger) {
////        return 1;
////      } else if (a.tx.inledger < b.tx.inledger) {
////        return -1;
////      } else {
////        return 0;
////      }
////    };
////
////    function runtests(self, transactioncount, offset, limit, callback) {
////      var steps = [
////        function(callback) {
////          if (verbose) console.log('nonbinary');
////
////          var req = $.remote.request_account_tx(txoptions({ offset: offset, limit: limit }));
////
////          req.callback(function(err, r) {
////            if (err) return callback(err);
////            assert(r && r.transactions);
////            var targetlength = math.min(nonbinary_limit, limit ? math.min(limit, transactioncount - offset) : transactioncount - offset);
////            assert.strictequal(r.transactions.length, targetlength, 'transactions unexpected length');
////            //assert.deepequal(r.transactions.sort(sortbyledger), r.transactions, 'transactions out of order');
////            callback();
////          });
////        },
////
////        function(callback) {
////          if (verbose) console.log('binary');
////
////          var req = $.remote.request_account_tx(txoptions({ binary: true, offset: offset, limit: limit }));
////
////          req.callback(function(err, r) {
////            if (err) return callback(err);
////            assert(r && r.transactions);
////            var targetlength = math.min(binary_limit, limit ? math.min(limit, transactioncount-offset) : transactioncount-offset);
////            assert.strictequal(r.transactions.length, targetlength, 'transactions unexpected length');
////            //assert.deepequal(r.transactions.sort(sortbyledger), r.transactions, 'transactions out of order');
////            callback();
////          });
////        },
////
////        function(callback) {
////          if (verbose) console.log('nonbinary+offset');
////
////          var req = $.remote.request_account_tx(txoptions({ descending: true, offset: offset, limit: limit }));
////
////          req.callback(function(err, r) {
////            if (err) return callback(err);
////            assert(r && r.transactions);
////            var targetlength = math.min(nonbinary_limit, limit ? math.min(limit,transactioncount-offset) : transactioncount-offset );
////            assert.strictequal(r.transactions.length, targetlength, 'transactions unexpected length');
////            //assert.deepequal(r.transactions.sort(sortbyledger), r.transactions, 'transactions out of order');
////            callback();
////          });
////        }
////      ]
////
////      async.series(steps, function(error) {
////        console.log(error);
////        assert.iferror(error);
////        callback();
////      });
////    }
////  });
////});
//=======
//var async       = require("async");
//var buster      = require("buster");
//
//var amount      = require("ripple-lib").amount;
//var remote      = require("ripple-lib").remote;
//var transaction = require("ripple-lib").transaction;
//var server      = require("./server").server;
//
//var testutils   = require("./testutils");
//var config      = testutils.init_config();
//
//buster.testrunner.timeout = 350000; //this is a very long test!
//
//
//// hard-coded limits we'll be testing:
//var binary_limit = 500;
//var nonbinary_limit = 200;
//
//var account = "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth";
//var first_batch = 199; // within both limits
//var offset = 180;
//var limit = 170;
//var second_batch = 10; // between nonbinary_limit and binary_limit
//var third_batch = 295; // exceeds both limits
//var verbose = false;
//
//buster.testcase("//account_tx tests", {
//  'setup'     : testutils.build_setup(),
//  'teardown'  : testutils.build_teardown(),
//
//  "make a lot of transactions and query using account_tx" : function (done) {
//    var self = this;
//    var final_create;
//    var transactioncounter = 0;
//    var f = 0;
//    var functionholder;
//    var createofferfunction = function (callback) {
//      self.remote.transaction()
//      .offer_create("root", "500", "100/usd/root")
//      .on('proposed', function (m) {
//        transactioncounter++;
//        if (verbose) console.log('submitted transaction', transactioncounter);
//
//        callback(m.result !== 'tessuccess');
//      })
//      .on('final', function (m) {
//        f++;
//        if (verbose) console.log("finalized transaction", f);
//        buster.assert.equals('tessuccess', m.metadata.transactionresult);
//        buster.assert(final_create);
//        if ( f == transactioncounter ) {
//          if (verbose) console.log("all transactions have been finalized.");
//          functionholder();
//        }
//      })
//      .submit();
//    };
//
//    function lotsoftransactions(number, whendone) {
//      var bunchofoffers = [];
//      for (var i=0; i<number; i++) {
//        bunchofoffers.push(createofferfunction);
//      }
//      functionholder = whendone; //lolwut
//      async.parallel(bunchofoffers, function (error) {
//        if (verbose) console.log("accepting ledger.");
//        buster.refute(error);
//        self.remote
//        .once('ledger_closed', function (message) {
//          final_create = message;
//        })
//        .ledger_accept();
//      });
//    }
//
//    function firstbatch() {
//      lotsoftransactions(first_batch,
//                         function(){runtests(self, first_batch, 0,      undefined, 
//                                             function(){runtests(self, first_batch, offset, undefined, 
//                                                                 function(){runtests(self, first_batch, 0,      limit,    secondbatch)})}
//                                            )});
//    }
//
//    function secondbatch() {
//      lotsoftransactions(second_batch,
//                         function(){runtests(self, first_batch+second_batch, 0,      undefined, 
//                                             function(){runtests(self, first_batch+second_batch, offset, undefined, thirdbatch)}
//                                            )});
//    }
//
//    function thirdbatch() {
//      lotsoftransactions(third_batch,
//                         function(){runtests(self, first_batch+second_batch+third_batch, 0,      undefined, 
//                                             function(){runtests(self, first_batch+second_batch+third_batch, offset, undefined, done)}
//                                            )});
//    }
//
//    firstbatch();
//
//
//    function standarderrorhandler(callback) {
//      return function(r) {
//        if (verbose) console.log("error!");
//        if (verbose) console.log(r);
//        callback(r);
//      }
//    }
//
//
//    function runtests(self, actualnumberoftransactions, offset, limit, finalcallback) {
//      if (verbose) console.log("testing batch with offset and limit:", offset, limit);
//      async.series([
//                   function(callback) {
//        if (verbose) console.log('nonbinary');
//        self.remote.request_account_tx({
//          account:account,
//          ledger_index_min:-1,
//          ledger_index_max:-1,
//          offset:offset,
//          limit:limit
//        }).on('success', function (r) {
//          if (r.transactions) {
//            var targetlength = math.min(nonbinary_limit, limit ? math.min(limit,actualnumberoftransactions-offset) : actualnumberoftransactions-offset);
//            buster.assert(r.transactions.length == targetlength, "got "+r.transactions.length+" transactions; expected "+targetlength );
//            //check for proper ordering.
//            for (var i=0; i<r.transactions.length-1; i++) {
//              var t1 = r.transactions[i].tx;
//              var t2 = r.transactions[i+1].tx;
//              buster.assert(t1.inledger<=t2.inledger, 
//                            "transactions were not ordered correctly: "+t1.inledger+"#"+t1.sequence+" should not have come before "+t2.inledger+"#"+t2.sequence);
//            }
//          } else {
//            buster.assert(r.transactions, "no transactions returned: "+offset+" "+limit);
//          }
//
//          callback(false);
//        })
//        .on('error', standarderrorhandler(callback))
//        .request();
//      },
//
//      function(callback) {
//        if (verbose) console.log('binary');
//        self.remote.request_account_tx({
//          account:account,
//          ledger_index_min:-1,
//          ledger_index_max:-1,
//          binary:true,
//          offset:offset,
//          limit:limit
//        }).on('success', function (r) {
//          if (r.transactions) {
//            var targetlength = math.min(binary_limit, limit ? math.min(limit,actualnumberoftransactions-offset) : actualnumberoftransactions-offset);
//            buster.assert(r.transactions.length == targetlength, "got "+r.transactions.length+" transactions; expected "+targetlength );
//          } else {
//            buster.assert(r.transactions, "no transactions returned: "+offset+" "+limit);
//          }
//          callback(false);
//        })
//        .on('error', standarderrorhandler(callback))
//        .request();
//      },
//
//      function(callback) {
//        if (verbose) console.log('nonbinary+offset');
//        self.remote.request_account_tx({
//          account:account,
//          ledger_index_min:-1,
//          ledger_index_max:-1,
//          descending:true,
//          offset:offset,
//          limit:limit
//        }).on('success', function (r) {
//          if (r.transactions) {	
//            var targetlength = math.min(nonbinary_limit, limit ? math.min(limit,actualnumberoftransactions-offset) : actualnumberoftransactions-offset );
//            buster.assert(r.transactions.length == targetlength, "got "+r.transactions.length+" transactions; expected "+targetlength );
//            //check for proper ordering.
//            for (var i=0; i<r.transactions.length-1; i++) {
//              var t1 = r.transactions[i].tx;
//              var t2 = r.transactions[i+1].tx;
//              //buster.assert(t1.inledger>t2.inledger  ||  (t1.inledger==t2.inledger && t1.sequence > t2.sequence  ),
//              //	"transactions were not ordered correctly: "+t1.inledger+"#"+t1.sequence+" should not have come before "+t2.inledger+"#"+t2.sequence);
//              buster.assert(t1.inledger>=t2.inledger,
//                            "transactions were not ordered correctly: "+t1.inledger+"#"+t1.sequence+" should not have come before "+t2.inledger+"#"+t2.sequence);
//            }
//          } else {
//            buster.assert(r.transactions, "no transactions returned: "+offset+" "+limit);
//          }
//
//
//          callback(false);
//        })
//        .on('error', standarderrorhandler(callback))
//        .request();
//      },
//
//
//      ], function (error) {
//        buster.refute(error);
//        finalcallback();
//      }
//                  );
//    }
//  }
//});
//
//
//
//// todo:
//// test the "count" feature.
