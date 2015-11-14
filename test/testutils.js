var async       = require('async');
var assert      = require('assert');
var extend      = require('extend');
var amount      = require('ripple-lib').amount;
var remote      = require('ripple-lib').remote;
var transaction = require('ripple-lib').transaction;
var server      = require('./server').server;
var server      = { };

function get_config() {
  var cfg = require(__dirname + '/config-example');

  // see if the person testing wants to override the configuration by creating a
  // file called test/config.js.
  try {
    cfg = extend({}, cfg, require(__dirname + '/config'));
  } catch (e) { }

  return cfg;
};

function init_config() {
  return require('ripple-lib').config.load(get_config());
};

exports.get_server_config =
get_server_config =
function(config, host) {
  config = config || init_config();
  host = host || config.server_default;
  return extend({}, config.default_server_config, config.servers[host]);
}

function prepare_tests(tests, fn) {
  var tests = typeof tests === 'string' ? [ tests ] : tests;
  var result = [ ];
  for (var i in tests) {
    result.push(fn(tests[i], i));
  }
  return result;
};

/**
 * helper called by test cases to generate a setup routine.
 *
 * by default you would call this without options, but it is useful to
 * be able to plug options in during development for quick and easy
 * debugging.
 *
 * @example
 *   buster.testcase('foobar', {
 *     setup: testutils.build_setup({verbose: true}),
 *     // ...
 *   });
 *
 * @param opts {object} these options allow quick-and-dirty test-specific
 *   customizations of your test environment.
 * @param opts.verbose {bool} enable all debug output (then cover your ears
 *   and run)
 * @param opts.verbose_ws {bool} enable tracing in the remote class. prints
 *   websocket traffic.
 * @param opts.verbose_server {bool} set the -v option when running rippled.
 * @param opts.no_server {bool} don't auto-run rippled.
 * @param host {string} identifier for the host configuration to be used.
 */
function build_setup(opts, host) {
  var config = get_config();
  var host = host || config.server_default;
  var opts = opts || {};

  // normalize options
  if (opts.verbose) {
    opts.verbose_ws     = true;
    opts.verbose_server = true;
  };

  function setup(done) {
    var self = this;

    self.compute_fees_amount_for_txs = function(txs) {
      var fee_units = transaction.fee_units['default'] * txs;
      return self.remote.fee_tx(fee_units);
    };

    self.amount_for = function(options) {
      var reserve = self.remote.reserve(options.ledger_entries || 0);
      var fees = self.compute_fees_amount_for_txs(options.default_transactions || 0)
      return reserve.add(fees).add(options.extra || 0);
    };

    this.store = this.store || {};
    this.store[host] = this.store[host] || { };
    var data = this.store[host];

    data.opts = opts;

    var steps = [
      function run_server(callback) {
        var server_config = get_server_config(config, host);

        if (opts.no_server || server_config.no_server)  {
          return callback();
        }

        data.server = server.from_config(host, server_config, !!opts.verbose_server);

        // setting undefined is a noop here
        if (data.opts.ledger_file != null) {
          data.server.set_ledger_file(data.opts.ledger_file);
        };

        data.server.once('started', function() {
          callback();
        });

        data.server.once('exited', function () {
          // if know the remote, tell it server is gone.
          if (self.remote) {
            try {
              self.remote.setserverfatal();
            } catch (e) {
              self.remote.serverfatal();
            }
          }
        });

        server[host] = data.server;
        data.server.start();
      },

      function connect_websocket(callback) {
        self.remote = data.remote = remote.from_config(host, !!opts.verbose_ws);

        // todo:
        self.remote.once('connected', function() {
        // self.remote.once('ledger_closed', function() {
          callback();
        });
        self.remote.connect();
      }
    ];

    async.series(steps, done);
  };

  return setup;
};

/**
 * generate teardown routine.
 *
 * @param host {string} identifier for the host configuration to be used.
 */

function build_teardown(host) {
  var config = get_config();
  var host = host || config.server_default;

  function teardown(done) {
    var data = this.store[host];
    var opts = data.opts;
    var server_config = get_server_config(config, host);

    var series = [
      function disconnect_websocket(callback) {
        data.remote.once('disconnected', callback)
        data.remote.once('error', function (m) {
          //console.log('server error: ', m);
        })
        data.remote.connect(false);
      },

      function stop_server(callback) {
        if (opts.no_server || server_config.no_server) {
          callback();
        } else {
          data.server.once('stopped', callback)
          data.server.stop();
          delete server[host];
        }
      }
    ];

    if (!(opts.no_server || server_config.no_server) && data.server.stopped) {
      done()
    } else {
      async.series(series, done);
    }
  };

  return teardown;
};

function account_dump(remote, account, callback) {
  var self = this;

  this.what = 'get latest account_root';

  var request = remote.request_ledger_entry('account_root');
  request.ledger_hash(remote.ledger_hash());
  request.account_root('root');
  request.callback(function(err, r) {
    assert(!err, self.what);
    if (err) {
      //console.log('error: %s', m);
      callback(err);
    } else {
      //console.log('account_root: %s', json.stringify(r, undefined, 2));
      callback();
    }
  });

  // get closed ledger hash
  // get account root
  // construct a json result
};

exports.fund_account =
fund_account =
function(remote, src, account, amount, callback) {
    // cache the seq as 1.
    // otherwise, when other operations attempt to opperate async against the account they may get confused.
    remote.set_account_seq(account, 1);

    var tx = remote.transaction();

    tx.payment(src, account, amount);

    tx.once('proposed', function (result) {
      //console.log('proposed: %s', json.stringify(result));
      callback(result.engine_result === 'tessuccess' ? null : new error());
    });

    tx.once('error', function (result) {
      //console.log('error: %s', json.stringify(result));
      callback(result);
    });

    tx.submit();
}

exports.create_account =
create_account =
function(remote, src, account, amount, callback) {
  // before creating the account, check if it exists in the ledger.
  // if it does, regardless of the balance, fail the test, because
  // the ledger is not in the expected state.
  var info = remote.requestaccountinfo(account);

  info.once('success', function(result) {
    // the account exists. fail by returning an error to callback.
    callback(new error("account " + account + " already exists"));
  });

  info.once('error', function(result) {
    if (result.error === "remoteerror" && result.remote.error === "actnotfound") {
      // rippled indicated the account does not exist. create it by funding it.
      fund_account(remote, src, account, amount, callback);
    } else {
      // some other error occurred. pass it up to the callback.
      callback(result);
    }
  });

  info.request();
}

function create_accounts(remote, src, amount, accounts, callback) {
  assert.strictequal(arguments.length, 5);

  remote.set_account_seq(src, 1);

  async.foreach(accounts, function (account, callback) {
    create_account(remote, src, account, amount, callback);
  }, callback);
};

function credit_limit(remote, src, amount, callback) {
  assert.strictequal(arguments.length, 4);

  var _m = amount.match(/^(\d+\/...\/[^\:]+)(?::(\d+)(?:,(\d+))?)?$/);

  if (!_m) {
    //console.log('credit_limit: parse error: %s', amount);
    return callback(new error('parse_error'));
  }

  // console.log('credit_limit: parsed: %s', json.stringify(_m, undefined, 2));
  var account_limit = _m[1];
  var quality_in    = _m[2];
  var quality_out   = _m[3];

  var tx = remote.transaction()

  tx.ripple_line_set(src, account_limit, quality_in, quality_out)

  tx.once('proposed', function (m) {
    // console.log('proposed: %s', json.stringify(m));
    callback(m.engine_result === 'tessuccess' ? null : new error());
  });

  tx.once('error', function (m) {
    // console.log('error: %s', json.stringify(m));
    callback(m);
  });

  tx.submit();
};

function verify_limit(remote, src, amount, callback) {
  assert.strictequal(arguments.length, 4);

  var _m = amount.match(/^(\d+\/...\/[^\:]+)(?::(\d+)(?:,(\d+))?)?$/);

  if (!_m) {
    // console.log('credit_limit: parse error: %s', amount);
    return callback(new error('parse_error'));
  }

  // console.log('_m', _m.length, _m);
  // console.log('verify_limit: parsed: %s', json.stringify(_m, undefined, 2));
  var account_limit = _m[1];
  var quality_in    = number(_m[2]);
  var quality_out   = number(_m[3]);
  var limit         = amount.from_json(account_limit);

  var options = {
    account:   src,
    issuer:    limit.issuer().to_json(),
    currency:  limit.currency().to_json(),
    ledger:    'current'
  };

  remote.request_ripple_balance(options, function(err, m) {
    if (err) {
      callback(err);
    } else {
      assert(m.account_limit.equals(limit));
      assert(isnan(quality_in) || m.account_quality_in === quality_in);
      assert(isnan(quality_out) || m.account_quality_out === quality_out);
      callback(null);
    }
  });
};

function credit_limits(remote, balances, callback) {
  assert.strictequal(arguments.length, 3);

  var limits = [ ];

  for (var src in balances) {
    prepare_tests(balances[src], function(amount) {
      limits.push({
        source: src,
        amount: amount
      });
    });
  }

  function iterator(limit, callback) {
    credit_limit(remote, limit.source, limit.amount, callback);
  }

  async.eachseries(limits, iterator, callback);
};

function ledger_close(remote, callback) {
  remote.once('ledger_closed', function (m) {
    callback();
  });
  remote.ledger_accept();
};

function payment(remote, src, dst, amount, callback) {
  assert.strictequal(arguments.length, 5);

  var tx = remote.transaction();

  tx.payment(src, dst, amount);

  tx.once('proposed', function (m) {
    // console.log('proposed: %s', json.stringify(m));
    callback(m.engine_result === 'tessuccess' ? null : new error());
  });

  tx.once('error', function (m) {
    // console.log('error: %s', json.stringify(m));
    callback(m);
  });
  tx.submit();
};

function payments(remote, balances, callback) {
  assert.strictequal(arguments.length, 3);

  var sends = [ ];

  for (var src in balances) {
    prepare_tests(balances[src], function(amount_json) {
      sends.push({
        source:        src,
        destination :  amount.from_json(amount_json).issuer().to_json(),
        amount :       amount_json
      });
    });
  }

  function iterator(send, callback) {
    payment(remote, send.source, send.destination, send.amount, callback);
  }

  async.eachseries(sends, iterator, callback);
};

function transfer_rate(remote, src, billionths, callback) {
  assert.strictequal(arguments.length, 4);

  var tx = remote.transaction();
  tx.account_set(src);
  tx.transfer_rate(billionths);

  tx.once('proposed', function (m) {
    // console.log('proposed: %s', json.stringify(m));
    callback(m.engine_result === 'tessuccess' ? null : new error());
  });

  tx.once('error', function (m) {
    // console.log('error: %s', json.stringify(m));
    callback(m);
  });

  tx.submit();
};

function verify_balance(remote, src, amount_json, callback) {
  assert.strictequal(arguments.length, 4);

  var amount_req  = amount.from_json(amount_json);

  if (amount_req.is_native()) {
    remote.request_account_balance(src, 'current', function(err, amount_act) {
      if (err) {
        return callback(err);
      }
      var valid_balance = amount_act.equals(amount_req, true);
      if (!valid_balance) {
        //console.log('verify_balance: failed: %s / %s',
        //amount_act.to_text_full(),
        //amount_req.to_text_full());
      }
      callback(valid_balance ? null : new error('balance invalid: ' + amount_json + ' / ' + amount_act.to_json()));
    });
  } else {
    var issuer = amount_req.issuer().to_json();
    var currency = amount_req.currency().to_json();
    remote.request_ripple_balance(src, issuer, currency, 'current', function(err, m) {
      if (err) {
        return callback(err);
      }
      // console.log('balance: %s', json.stringify(m));
      // console.log('account_balance: %s', m.account_balance.to_text_full());
      // console.log('account_limit: %s', m.account_limit.to_text_full());
      // console.log('issuer_balance: %s', m.issuer_balance.to_text_full());
      // console.log('issuer_limit: %s', m.issuer_limit.to_text_full());
      var account_balance = amount.from_json(m.account_balance);
      var valid_balance = account_balance.equals(amount_req, true);

      if (!valid_balance) {
        //console.log('verify_balance: failed: %s vs %s / %s: %s',
        //src,
        //account_balance.to_text_full(),
        //amount_req.to_text_full(),
        //account_balance.not_equals_why(amount_req, true));
      }

      callback(valid_balance ? null : new error('balance invalid: ' + amount_json + ' / ' + account_balance.to_json()));
    })
  }
};

function verify_balances(remote, balances, callback) {
  var tests = [ ];

  for (var src in balances) {
    prepare_tests(balances[src], function(amount) {
      tests.push({ source: src, amount: amount });
    });
  }

  function iterator(test, callback) {
    verify_balance(remote, test.source, test.amount, callback)
  }

  async.every(tests, iterator, callback);
};

// --> owner: account
// --> seq: sequence number of creating transaction.
// --> taker_gets: json amount
// --> taker_pays: json amount
function verify_offer(remote, owner, seq, taker_pays, taker_gets, callback) {
  assert.strictequal(arguments.length, 6);

  var request = remote.request_ledger_entry('offer')
  request.offer_id(owner, seq)
  request.callback(function(err, m) {
    var wrong = err
    || !amount.from_json(m.node.takergets).equals(amount.from_json(taker_gets), true)
    || !amount.from_json(m.node.takerpays).equals(amount.from_json(taker_pays), true);

    if (wrong) {
      //console.log('verify_offer: failed: %s', json.stringify(m));
    }

    callback(wrong ? (err || new error()) : null);
  });
};

function verify_offer_not_found(remote, owner, seq, callback) {
  assert.strictequal(arguments.length, 4);

  var request = remote.request_ledger_entry('offer');

  request.offer_id(owner, seq);

  request.once('success', function (m) {
    //console.log('verify_offer_not_found: found offer: %s', json.stringify(m));
    callback(new error('entryfound'));
  });

  request.once('error', function (m) {
    // console.log('verify_offer_not_found: success: %s', json.stringify(m));
    var is_not_found = m.error === 'remoteerror' && m.remote.error === 'entrynotfound';
    if (is_not_found) {
      callback(null);
    } else {
      callback(new error());
    }
  });

  request.request();
};

function verify_owner_count(remote, account, count, callback) {
  assert(arguments.length === 4);
  var options = { account: account, ledger: 'current' };
  remote.request_owner_count(options, function(err, owner_count) {
    //console.log('owner_count: %s/%d', owner_count, value);
    callback(owner_count === count ? null : new error());
  });
};

function verify_owner_counts(remote, counts, callback) {
  var tests = prepare_tests(counts, function(account) {
    return { account: account, count: counts[account] };
  });

  function iterator(test, callback) {
    verify_owner_count(remote, test.account, test.count, callback)
  }

  async.every(tests, iterator, callback);
};

var istravis = boolean(process.env.travis);

function ledger_wait(remote, tx) {
  ;(function nextledger() {
    remote.once('ledger_closed', function() {
      if (!tx.finalized) {
        settimeout(nextledger, istravis ? 400 : 100);
      }
    });
    remote.ledger_accept();
  })();
};

exports.account_dump           = account_dump;
exports.build_setup            = build_setup;
exports.build_teardown         = build_teardown;
exports.create_accounts        = create_accounts;
exports.credit_limit           = credit_limit;
exports.credit_limits          = credit_limits;
exports.get_config             = get_config;
exports.init_config            = init_config;
exports.ledger_close           = ledger_close;
exports.payment                = payment;
exports.payments               = payments;
exports.transfer_rate          = transfer_rate;
exports.verify_balance         = verify_balance;
exports.verify_balances        = verify_balances;
exports.verify_limit           = verify_limit;
exports.verify_offer           = verify_offer;
exports.verify_offer_not_found = verify_offer_not_found;
exports.verify_owner_count     = verify_owner_count;
exports.verify_owner_counts    = verify_owner_counts;
exports.ledger_wait            = ledger_wait;

process.on('uncaughtexception', function() {
  object.keys(server).foreach(function(host) {
    server[host].stop();
  });
});

// vim:sw=2:sts=2:ts=8:et
