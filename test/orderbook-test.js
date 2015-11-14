var async       = require('async');
var assert      = require('assert');
var account     = require('ripple-lib').uint160;
var remote      = require('ripple-lib').remote;
var transaction = require('ripple-lib').transaction;
var testutils   = require('./testutils');
var config      = testutils.init_config();
  
suite('order book', function() {
  var $ = { };
 
  setup(function(done) {
    testutils.build_setup().call($, done);
  });
 
  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });
 
  test('track offers', function (done) {
    var self = this;
 
    var steps = [
      function(callback) {
          self.what = 'create accounts';

          testutils.create_accounts(
            $.remote,
            'root',
            '20000.0',
            [ 'mtgox', 'alice', 'bob' ],
            callback
          );
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

        testutils.verify_balance(
          $.remote,
          [ 'mtgox', 'alice', 'bob' ],
          '20000000000',
          callback
        );
      },
 
      function (callback) {
        self.what = 'set transfer rate';
 
        var tx = $.remote.transaction('accountset', {
          account: 'mtgox'
        });
 
        tx.transferrate(1.1 * 1e9);
 
        tx.submit(function(err, m) {
          assert.iferror(err);
          assert.strictequal(m.engine_result, 'tessuccess');
          callback();
        });
 
        testutils.ledger_wait($.remote, tx);
      },
 
      function (callback) {
        self.what = 'set limits';
 
        testutils.credit_limits($.remote, {
          'alice' : '1000/usd/mtgox',
          'bob' : '1000/usd/mtgox'
        },
        callback);
      },
 
      function (callback) {
        self.what = 'distribute funds';
 
        testutils.payments($.remote, {
          'mtgox' : [ '100/usd/alice', '50/usd/bob' ]
        },
        callback);
      },
 
      function (callback) {
        self.what = 'create offer';
 
        // get 4000/xrp pay 10/usd : offer pays 10 usd for 4000 xrp
        var tx = $.remote.transaction('offercreate', {
          account: 'alice',
          taker_pays: '4000',
          taker_gets: '10/usd/mtgox'
        });
 
        tx.submit(function(err, m) {
          assert.iferror(err);
          assert.strictequal(m.engine_result, 'tessuccess');
          callback();
        });
 
        testutils.ledger_wait($.remote, tx);
      },
 
      function (callback) {
        self.what = 'create order book';

        var ob = $.remote.createorderbook({
          currency_pays: 'xrp',
          issuer_gets: account.json_rewrite('mtgox'),
          currency_gets: 'usd'
        });
 
        ob.on('model', function(){});

        ob.getoffers(function(err, offers) {
          assert.iferror(err);
          //console.log('offers', offers);
          
          var expected = [
              { account: 'rg1qqv2nh2gr7rcz1p8yycbukccn633jcn',
                bookdirectory: 'ae0a97f385ffe42e3096ba3f98a0173090fe66a3c2482fe0570e35fa931a0000',
                booknode: '0000000000000000',
                flags: 0,
                ledgerentrytype: 'offer',
                ownernode: '0000000000000000',
                previoustxnid: offers[0].previoustxnid,
                previoustxnlgrseq: offers[0].previoustxnlgrseq,
                sequence: 2,
                takergets: { currency: 'usd',
                  issuer: 'rgihwhaqu8g7ahwavtq6ix5rvsfcbgzw6v',
                  value: '10'
                },
                takerpays: '4000',
                index: 'cd6ae78ee0a5438978501a0404d9093597f57b705d566b5070d58bd48f98468c',
                owner_funds: '100',
                quality: '400',
                is_fully_funded: true,
                taker_gets_funded: '10',
                taker_pays_funded: '4000' }
          ]
          assert.deepequal(offers, expected);
 
          callback(null, ob);
        });
      },

      function (ob, callback) {
        self.what = 'create offer';
 
        // get 5/usd pay 2000/xrp: offer pays 2000 xrp for 5 usd 
        var tx = $.remote.transaction('offercreate', {
          account: 'bob',
          taker_pays: '5/usd/mtgox',
          taker_gets: '2000',
        });
 
        tx.submit(function(err, m) {
          assert.iferror(err);
          assert.strictequal(m.engine_result, 'tessuccess');
          callback(null, ob);
        });
 
        testutils.ledger_wait($.remote, tx);
      },      
      
      function (ob, callback) {
        self.what = 'check order book tracking';   

        ob.getoffers(function(err, offers) {
          assert.iferror(err);
          //console.log('offers', offers);

          var expected = [
              { account: 'rg1qqv2nh2gr7rcz1p8yycbukccn633jcn',
                bookdirectory: 'ae0a97f385ffe42e3096ba3f98a0173090fe66a3c2482fe0570e35fa931a0000',
                booknode: '0000000000000000',
                flags: 0,
                ledgerentrytype: 'offer',
                ownernode: '0000000000000000',
                previoustxnid: offers[0].previoustxnid,                
                previoustxnlgrseq: offers[0].previoustxnlgrseq,
                sequence: 2,
                takergets: 
                 { currency: 'usd',
                   issuer: 'rgihwhaqu8g7ahwavtq6ix5rvsfcbgzw6v',
                   value: '5' },
                takerpays: '2000',
                index: 'cd6ae78ee0a5438978501a0404d9093597f57b705d566b5070d58bd48f98468c',
                owner_funds: '94.5',
                quality: '400',
                is_fully_funded: true,
                taker_gets_funded: '5',
                taker_pays_funded: '2000' }
          ]
          assert.deepequal(offers, expected);

          callback();
        });
      },
    ];
 
    async.waterfall(steps, function (error) {
      assert(!error, self.what + ': ' + error);
      done();
    });
  });
});
