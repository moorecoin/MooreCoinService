var async       = require("async");
var assert      = require('assert');
var amount      = require("ripple-lib").amount;
var remote      = require("ripple-lib").remote;
var transaction = require("ripple-lib").transaction;
var server      = require("./server").server;
var testutils   = require("./testutils");
var config      = testutils.init_config();

function makeoffer($, account, takerpays, takergets, callback) {
  $.remote.transaction()
    .offer_create(account, takerpays, takergets)
    .on('submitted', function (m) {
        // console.log("proposed: offer_create: %s", json.stringify(m));
        // console.log("proposed: offer_create: result %s. takergets %s. takerpays %s.", m.engine_result, json.stringify(m.tx_json.takergets), json.stringify(m.tx_json.takerpays));
        assert.strictequal('tessuccess', m.engine_result);
        $.remote
          .once('ledger_closed', function (message) {
              // console.log("ledger_closed: %d: %s", message.ledger_index, message.ledger_hash);
              assert(message);
            })
          .ledger_accept();
      })
    .on('final', function (m) {
        // console.log("final: offer_create: %s", json.stringify(m));
        // console.log("final: offer_create: result %s. validated %s. takergets %s. takerpays %s.", m.engine_result, m.validated, json.stringify(m.tx_json.takergets), json.stringify(m.tx_json.takerpays));

        callback(m.metadata.transactionresult !== 'tessuccess');
    })
    .submit();
}

function makeofferwithevent($, account, takerpays, takergets, callback) {
  $.remote.once('transaction', function(m) {
    // console.log("transaction: %s", json.stringify(m));
    // console.log("transaction: meta: %s, takerpays: %s, takergets: %s", m.meta.affectednodes.length, amount.from_json(m.transaction.takerpays).to_human_full(), amount.from_json(m.transaction.takergets).to_human_full());

    assert.strictequal(amount.from_json(takergets).to_human_full(),
      amount.from_json(m.transaction.takergets).to_human_full());
    assert.strictequal(amount.from_json(takerpays).to_human_full(),
      amount.from_json(m.transaction.takerpays).to_human_full());
    assert.strictequal("offercreate", m.transaction.transactiontype);
    // we don't get quality from the event
    var foundoffer = false;
    var foundroot = false;
    for(var n = m.meta.affectednodes.length-1; n >= 0; --n) {
      var node = m.meta.affectednodes[n];
      // console.log(node);
      if(node.creatednode && 
        node.creatednode.ledgerentrytype === "offer") {
          foundoffer = true;
          var fields = node.creatednode.newfields;
          assert.strictequal(amount.from_json(takergets).to_human_full(),
            amount.from_json(fields.takergets).to_human_full());
          assert.strictequal(amount.from_json(takerpays).to_human_full(),
            amount.from_json(fields.takerpays).to_human_full());
      } else if (node.modifiednode) {
        assert(node.modifiednode.ledgerentrytype != "offer");
        if(node.modifiednode.ledgerentrytype === "accountroot") {
          foundroot = true;
          var mod = node.modifiednode;
          assert(mod.finalfields.ownercount == 
              mod.previousfields.ownercount + 1);
        }
      }
    }
    assert(foundoffer);
    assert(foundroot);

    callback(m.engine_result !== 'tessuccess');
  });
  makeoffer($, account, takerpays, takergets, function () {});
}

function matchoffers(expected, actual) {
  assert.strictequal(expected.length, actual.length);
  var found = [];
  for(var i = 0; i < actual.length; ++i) {
    var offer = actual[i];
    // console.log("got: ", offer);
    for(var j = 0; j < expected.length; ++j) {
      var expectedoffer = expected[j];
      // console.log("checking: ", expectedoffer);
      if(amount.from_json(expectedoffer.takergets).to_human_full() ===
        amount.from_json(offer.takergets).to_human_full()
        && amount.from_json(expectedoffer.takerpays).to_human_full() ===
        amount.from_json(offer.takerpays).to_human_full()
        && expectedoffer.quality === offer.quality) {
        // console.log("found");
        found.push(expectedoffer);
        expected.splice(j, 1);
        break;
      }
    }
  }
  if(expected.length != 0 || actual.length != found.length) {
    console.log("received: ", actual.length);
    for(i = 0; i < actual.length; ++i) {
      var offer = actual[i];
      console.log("  takergets: %s, takerpays %s",
          amount.from_json(offer.takergets).to_human_full(),
          amount.from_json(offer.takerpays).to_human_full());
    }
    console.log("found: ", found.length);
    for(i = 0; i < found.length; ++i) {
      var offer = found[i];
      console.log("  takergets: %s, takerpays %s", offer.takergets,
          offer.takerpays);
    }
    console.log("not found: ", expected.length);
    for(i = 0; i < expected.length; ++i) {
      var offer = expected[i];
      console.log("  takergets: %s, takerpays %s", offer.takergets,
          offer.takerpays);
    }
  }
  assert.strictequal(0, expected.length);
  assert.strictequal(actual.length, found.length);
}

function buildofferfunctions($, offers) {
  functions = [];
  for(var i = 0; i < offers.length; ++i) {
    var offer = offers[i];
    functions.push(function(offer) {
      // console.log("offer pre: ", offer);
      return function(callback) {
        // console.log("offer in: ", offer);
        makeoffer($, "root", offer.takerpays, offer.takergets, callback);
      }
    }(offer));
  }
  return functions;
}

suite("subscribe book tests", function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test("one side: empty book", function (done) {
      var self = this;

      async.waterfall([
          function (callback) {
            var request = $.remote.requestsubscribe(null);
            request.addbook({
              "taker_gets" : {
                  "currency" : "xrp"
              },
              "taker_pays" : {
                  "currency" : "usd", "issuer" : "root"
              }
            }, true);
            request.once('success', function(res) {
              // console.log("subscribe: %s", json.stringify(res));

              assert.strictequal(0, res.offers.length);
              assert(!res.asks);
              assert(!res.bids);

              callback(null);
            });
            request.once('error', function(err) {
              // console.log(err);
              callback(err);
            });
            request.request();
          },
          function (callback) {
            // make another ask. make sure we get notified
            makeofferwithevent($, "root", "700", "100/usd/root", callback);
          },
          function (callback) {
            // make another bid. make sure we get notified
            makeofferwithevent($, "root", "100/usd/root", "75", callback);
          }
        ], function (error) {
          // console.log("result: error=%s", error);
          assert(!error, self.what || "unspecified error");

          done();
        });
  });

  test("one side: offers in book", function (done) {
      var self = this;
      var asktakerpays = "500";
      var asktakergets = "100/usd/root";
      var askquality = "5";
      var bidtakerpays = asktakergets;
      var bidtakergets = "200";
      var bidquality = "0.5";

      async.waterfall([
          function(callback) {
            // create an ask: takerpays 500, takergets 100/usd
            makeoffer($, "root", asktakerpays, asktakergets, callback);
          },
          function(callback) {
            // create an bid: takerpays 100/usd, takergets 1200
            makeoffer($, "root", bidtakerpays, bidtakergets, callback);
          },
          function (callback) {
            var request = $.remote.requestsubscribe(null);
            request.addbook({
              "taker_gets" : {
                  "currency" : "xrp"
              },
              "taker_pays" : {
                  "currency" : "usd", "issuer" : "root"
              }
            }, true);
            request.once('success', function(res) {
              // console.log("subscribe: %s", json.stringify(res));

              assert.strictequal(1, res.offers.length);
              var bid = res.offers[0];
              assert.strictequal(amount.from_json(bidtakergets).to_human_full(),
                amount.from_json(bid.takergets).to_human_full());
              assert.strictequal(amount.from_json(bidtakerpays).to_human_full(),
                amount.from_json(bid.takerpays).to_human_full());
              assert.strictequal(bidquality, bid.quality);

              assert(!res.asks);
              assert(!res.bids);

              callback(null);
            });
            request.once('error', function(err) {
              // console.log(err);
              callback(err);
            });
            request.request();
          },
          function (callback) {
            // make another ask. make sure we get notified
            makeofferwithevent($, "root", "700", "100/usd/root", callback);
          },
          function (callback) {
            // make another bid. make sure we get notified
            makeofferwithevent($, "root", "100/usd/root", "75", callback);
          }
        ], function (error) {
          // console.log("result: error=%s", error);
          assert(!error, self.what || "unspecified error");

          done();
        });
  });

  test("both sides: empty book", function (done) {
      var self = this;

      async.waterfall([
          function (callback) {
            var request = $.remote.requestsubscribe(null);
            request.addbook({
              "both" : true,
              "taker_gets" : {
                  "currency" : "xrp"
              },
              "taker_pays" : {
                  "currency" : "usd", "issuer" : "root"
              }
            }, true);
            request.once('success', function(res) {
              // console.log("subscribe: %s", json.stringify(res));

              assert.strictequal(0, res.asks.length);
              assert.strictequal(0, res.bids.length);
              assert(!res.offers);

              callback(null);
            });
            request.once('error', function(err) {
              // console.log(err);
              callback(err);
            });
            request.request();
          },
          function (callback) {
            // make another ask. make sure we get notified
            makeofferwithevent($, "root", "700", "100/usd/root", callback);
          },
          function (callback) {
            // make another bid. make sure we get notified
            makeofferwithevent($, "root", "100/usd/root", "75", callback);
          }
        ], function (error) {
          // console.log("result: error=%s", error);
          assert(!error, self.what || "unspecified error");

          done();
        });
  });

  test("both sides: offers in book", function (done) {
      var self = this;
      var asktakerpays = "500";
      var asktakergets = "100/usd/root";
      var askquality = "5";
      var bidtakerpays = asktakergets;
      var bidtakergets = "200";
      var bidquality = "0.5";

      async.waterfall([
          function(callback) {
            // create an ask: takerpays 500, takergets 100/usd
            makeoffer($, "root", asktakerpays, asktakergets, callback);
          },
          function(callback) {
            // create an bid: takerpays 100/usd, takergets 1200
            makeoffer($, "root", bidtakerpays, bidtakergets, callback);
          },
          function (callback) {
            var request = $.remote.requestsubscribe(null);
            request.addbook({
              "both" : true,
              "taker_gets" : {
                  "currency" : "xrp"
              },
              "taker_pays" : {
                  "currency" : "usd", "issuer" : "root"
              }
            }, true);
            request.once('success', function(res) {
              // console.log("subscribe: %s", json.stringify(res));

              assert.strictequal(1, res.asks.length);
              var ask = res.asks[0];
              assert.strictequal(amount.from_json(asktakergets).to_human_full(),
                amount.from_json(ask.takergets).to_human_full());
              assert.strictequal(amount.from_json(asktakerpays).to_human_full(),
                amount.from_json(ask.takerpays).to_human_full());
              assert.strictequal(askquality, ask.quality);

              assert.strictequal(1, res.bids.length);
              var bid = res.bids[0];
              assert.strictequal(amount.from_json(bidtakergets).to_human_full(),
                amount.from_json(bid.takergets).to_human_full());
              assert.strictequal(amount.from_json(bidtakerpays).to_human_full(),
                amount.from_json(bid.takerpays).to_human_full());
              assert.strictequal(bidquality, bid.quality);

              assert(!res.offers);

              callback(null);
            });
            request.once('error', function(err) {
              // console.log(err);
              callback(err);
            });
            request.request();
          },
          function (callback) {
            // make another ask. make sure we get notified
            makeofferwithevent($, "root", "700", "100/usd/root", callback);
          },
          function (callback) {
            // make another bid. make sure we get notified
            makeofferwithevent($, "root", "100/usd/root", "75", callback);
          }
        ], function (error) {
          // console.log("result: error=%s", error);
          assert(!error, self.what || "unspecified error");

          done();
        });
  });

  test("multiple books: one side: empty book", function (done) {
      var self = this;

      async.waterfall([
          function (callback) {
            var request = $.remote.requestsubscribe(null);
            request.addbook({
              "taker_gets" : {
                  "currency" : "xrp"
              },
              "taker_pays" : {
                  "currency" : "usd", "issuer" : "root"
              }
            }, true);
            request.addbook({
              "taker_gets" : {
                  "currency" : "cny", "issuer" : "root"
              },
              "taker_pays" : {
                  "currency" : "jpy", "issuer" : "root"
              }
            }, true);
            request.once('success', function(res) {
              // console.log("subscribe: %s", json.stringify(res));

              assert.strictequal(0, res.offers.length);
              assert(!res.asks);
              assert(!res.bids);

              callback(null);
            });
            request.once('error', function(err) {
              // console.log(err);
              callback(err);
            });
            request.request();
          },
          function (callback) {
            // make another ask. make sure we get notified
            makeofferwithevent($, "root", "700", "100/usd/root", callback);
          },
          function (callback) {
            // make another bid. make sure we get notified
            makeofferwithevent($, "root", "100/usd/root", "75", callback);
          },
          function (callback) {
            // make another ask. make sure we get notified
            makeofferwithevent($, "root", "700/cny/root", "100/jpy/root", callback);
          },
          function (callback) {
            // make another bid. make sure we get notified
            makeofferwithevent($, "root", "100/jpy/root", "75/cny/root", callback);
          }
        ], function (error) {
          // console.log("result: error=%s", error);
          assert(!error, self.what || "unspecified error");

          done();
        });
  });

  test("multiple books: one side: offers in book", function (done) {
      var self = this;
      var asks = [
        { takerpays: "500", takergets: "100/usd/root", quality: "5" },
        { takerpays: "500/cny/root", takergets: "100/jpy/root", quality: "5" },
      ];
      var bids = [
        { takerpays: "100/usd/root", takergets: "200", quality: "0.5" },
        { takerpays: "100/jpy/root", takergets: "200/cny/root", quality: "0.5" },
      ];

      var functions = buildofferfunctions($, asks)
        .concat(buildofferfunctions($, bids));

      async.waterfall(
        functions.concat([
          function (callback) {
            var request = $.remote.requestsubscribe(null);
            request.addbook({
              "taker_gets" : {
                  "currency" : "xrp"
              },
              "taker_pays" : {
                  "currency" : "usd", "issuer" : "root"
              }
            }, true);
            request.addbook({
              "taker_gets" : {
                  "currency" : "cny", "issuer" : "root"
              },
              "taker_pays" : {
                  "currency" : "jpy", "issuer" : "root"
              }
            }, true);
            request.once('success', function(res) {
              // console.log("subscribe: %s", json.stringify(res));

              matchoffers(bids, res.offers);

              assert(!res.asks);
              assert(!res.bids);

              callback(null);
            });
            request.once('error', function(err) {
              // console.log(err);
              callback(err);
            });
            request.request();
          },
          function (callback) {
            // make another ask. make sure we get notified
            makeofferwithevent($, "root", "700", "100/usd/root", callback);
          },
          function (callback) {
            // make another bid. make sure we get notified
            makeofferwithevent($, "root", "100/usd/root", "75", callback);
          },
          function (callback) {
            // make another ask. make sure we get notified
            makeofferwithevent($, "root", "700/cny/root", "100/jpy/root", callback);
          },
          function (callback) {
            // make another bid. make sure we get notified
            makeofferwithevent($, "root", "100/jpy/root", "75/cny/root", callback);
          }
        ]), function (error) {
          // console.log("result: error=%s", error);
          assert(!error, self.what || "unspecified error");

          done();
        });
  });

  test("multiple books: both sides: empty book", function (done) {
      var self = this;

      async.waterfall([
          function (callback) {
            var request = $.remote.requestsubscribe(null);
            request.addbook({
              "both" : true,
              "taker_gets" : {
                  "currency" : "xrp"
              },
              "taker_pays" : {
                  "currency" : "usd", "issuer" : "root"
              }
            }, true);
            request.addbook({
              "both" : true,
              "taker_gets" : {
                  "currency" : "cny", "issuer" : "root"
              },
              "taker_pays" : {
                  "currency" : "jpy", "issuer" : "root"
              }
            }, true);
            request.once('success', function(res) {
              // console.log("subscribe: %s", json.stringify(res));

              assert.strictequal(0, res.asks.length);
              assert.strictequal(0, res.bids.length);
              assert(!res.offers);

              callback(null);
            });
            request.once('error', function(err) {
              // console.log(err);
              callback(err);
            });
            request.request();
          },
          function (callback) {
            // make another ask. make sure we get notified
            makeofferwithevent($, "root", "700", "100/usd/root", callback);
          },
          function (callback) {
            // make another bid. make sure we get notified
            makeofferwithevent($, "root", "100/usd/root", "75", callback);
          },
          function (callback) {
            // make another ask. make sure we get notified
            makeofferwithevent($, "root", "700/cny/root", "100/jpy/root", callback);
          },
          function (callback) {
            // make another bid. make sure we get notified
            makeofferwithevent($, "root", "100/jpy/root", "75/cny/root", callback);
          }
        ], function (error) {
          // console.log("result: error=%s", error);
          assert(!error, self.what || "unspecified error");

          done();
        });
  });

  test("multiple books: both sides: offers in book", function (done) {
      var self = this;
      var asks = [
        { takerpays: "500", takergets: "100/usd/root", quality: "5" },
        { takerpays: "500/cny/root", takergets: "100/jpy/root", quality: "5" },
      ];
      var bids = [
        { takerpays: "100/usd/root", takergets: "200", quality: "0.5" },
        { takerpays: "100/jpy/root", takergets: "200/cny/root", quality: "0.5" },
      ];

      var functions = buildofferfunctions($, asks)
        .concat(buildofferfunctions($, bids));

      async.waterfall(
        functions.concat([
          function (callback) {
            var request = $.remote.requestsubscribe(null);
            request.addbook({
              "both" : true,
              "taker_gets" : {
                  "currency" : "xrp"
              },
              "taker_pays" : {
                  "currency" : "usd", "issuer" : "root"
              }
            }, true);
            request.addbook({
              "both" : true,
              "taker_gets" : {
                  "currency" : "cny", "issuer" : "root"
              },
              "taker_pays" : {
                  "currency" : "jpy", "issuer" : "root"
              }
            }, true);
            request.once('success', function(res) {
              // console.log("subscribe: %s", json.stringify(res));

              matchoffers(asks, res.asks);
              matchoffers(bids, res.bids);

              assert(!res.offers);

              callback(null);
            });
            request.once('error', function(err) {
              // console.log(err);
              callback(err);
            });
            request.request();
          },
          function (callback) {
            // make another ask. make sure we get notified
            makeofferwithevent($, "root", "700", "100/usd/root", callback);
          },
          function (callback) {
            // make another bid. make sure we get notified
            makeofferwithevent($, "root", "100/usd/root", "75", callback);
          },
          function (callback) {
            // make another ask. make sure we get notified
            makeofferwithevent($, "root", "700/cny/root", "100/jpy/root", callback);
          },
          function (callback) {
            // make another bid. make sure we get notified
            makeofferwithevent($, "root", "100/jpy/root", "75/cny/root", callback);
          }
        ]), function (error) {
          // console.log("result: error=%s", error);
          assert(!error, self.what || "unspecified error");

          done();
        });
  });

});

