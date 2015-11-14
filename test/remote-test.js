var assert    = require('assert');
var remote    = require('ripple-lib').remote;
var testutils = require('./testutils.js');
var config    = testutils.init_config();

suite('remote functions', function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, done);
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test('request_ledger with ledger index', function(done) {
    var request = $.remote.request_ledger();
    request.ledger_index(3);
    request.callback(function(err, m) {
      assert(!err);
      assert.strictequal(m.ledger.ledger_index, '3');
      done();
    });
  });

  test('request_ledger with ledger index string', function(done) {
    var request = $.remote.request_ledger();
    request.ledger_index('3');
    request.callback(function(err, m) {
      assert(err);
      assert.strictequal(err.error, 'remoteerror');
      assert.strictequal(err.remote.error, 'invalidparams');
      done();
    });
  });

  test('request_ledger with ledger identifier', function(done) {
    var request = $.remote.request_ledger();
    request.ledger_index('current');
    request.callback(function(err, m) {
      assert(!err);
      assert.strictequal(m.ledger.ledger_index, '3');
      done();
    });
  });

  test('request_ledger_current', function(done) {
    $.remote.request_ledger_current(function(err, m) {
      assert(!err);
      assert.strictequal(m.ledger_current_index, 3);
      done();
    });
  });

  test('request_ledger_hash', function(done) {
    $.remote.request_ledger_hash(function(err, m) {
      assert(!err);
      assert.strictequal(m.ledger_index, 2);
      done();
    })
  });

  test('manual account_root success', function(done) {
    var self = this;

    $.remote.request_ledger_hash(function(err, r) {
      //console.log('result: %s', json.stringify(r));
      assert(!err);
      assert('ledger_hash' in r, 'result missing property "ledger_hash"');

      var request = $.remote.request_ledger_entry('account_root')
      .ledger_hash(r.ledger_hash)
      .account_root('rhb9cjawyb4rj91vrwn96dkukg4bwdtyth');

      request.callback(function(err, r) {
        // console.log('account_root: %s', json.stringify(r));
        assert(!err);
        assert('node' in r, 'result missing property "node"');
        done();
      });
    });
  });

  test('account_root remote malformedaddress', function(done) {
    var self = this;

    $.remote.request_ledger_hash(function(err, r) {
      // console.log('result: %s', json.stringify(r));
      assert(!err);

      var request = $.remote.request_ledger_entry('account_root')
      .ledger_hash(r.ledger_hash)
      .account_root('zhb9cjawyb4rj91vrwn96dkukg4bwdtyth');

      request.callback(function(err, r) {
        // console.log('account_root: %s', json.stringify(r));
        assert(err);
        assert.strictequal(err.error, 'remoteerror');
        assert.strictequal(err.remote.error, 'malformedaddress');
        done();
      });
    })
  });

  test('account_root entrynotfound', function(done) {
    var self = this;

    $.remote.request_ledger_hash(function(err, r) {
      // console.log('result: %s', json.stringify(r));
      assert(!err);

      var request = $.remote.request_ledger_entry('account_root')
      .ledger_hash(r.ledger_hash)
      .account_root('alice');

      request.callback(function(err, r) {
        // console.log('error: %s', m);
        assert(err);
        assert.strictequal(err.error, 'remoteerror');
        assert.strictequal(err.remote.error, 'entrynotfound');
        done();
      });
    })
  });

  test('ledger_entry index', function(done) {
    var self = this;

    $.remote.request_ledger_hash(function(err, r) {
      assert(!err);

      var request = $.remote.request_ledger_entry('index')
      .ledger_hash(r.ledger_hash)
      .account_root('alice')
      .index('2b6ac232aa4c4be41bf49d2459fa4a0347e1b543a4c92fcee0821c0201e2e9a8');

      request.callback(function(err, r) {
        assert(!err);
        assert('node_binary' in r, 'result missing property "node_binary"');
        done();
      });
    })
  });

  test('create account', function(done) {
    var self = this;

    var root_id = $.remote.account('root')._account_id;

    $.remote.request_subscribe().accounts(root_id).request();

    $.remote.transaction()
    .payment('root', 'alice', '10000.0')
    .once('error', done)
    .once('proposed', function(res) {
      //console.log('submitted', res);
      $.remote.ledger_accept();
    })
    .once('success', function (r) {
      //console.log('account_root: %s', json.stringify(r));
      // need to verify account and balance.
      done();
    })
    .submit();
  });

  test('create account final', function(done) {
    var self = this;
    var got_proposed;
    var got_success;

    var root_id = $.remote.account('root')._account_id;

    $.remote.request_subscribe().accounts(root_id).request();

    var transaction = $.remote.transaction()
    .payment('root', 'alice', '10000.0')

    transaction.once('submitted', function (m) {
      // console.log('proposed: %s', json.stringify(m));
      // buster.assert.equals(m.result, 'terno_dst_insuf_xrp');
      assert.strictequal(m.engine_result, 'tessuccess');
    });

    transaction.submit(done);

    testutils.ledger_wait($.remote, transaction);
  });
});

//vim:sw=2:sts=2:ts=8:et
