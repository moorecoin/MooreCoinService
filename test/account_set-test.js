var async     = require('async');
var assert    = require('assert');
var remote    = require('ripple-lib').remote;
var testutils = require('./testutils');
var config    = testutils.init_config();

suite('account set', function() {
  var $ = { };

  setup(function(done) {
    testutils.build_setup().call($, function() {
      $.remote.local_signing = true;
      done();
    });
  });

  teardown(function(done) {
    testutils.build_teardown().call($, done);
  });

  test('null accountset', function(done) {
    var self = this;

    var steps = [
      function (callback) {
        self.what = 'send null accountset';

        var transaction = $.remote.transaction().accountset('root');
        transaction.setflags(0);

        transaction.once('submitted', function(m) {
          assert.strictequal(m.engine_result, 'tessuccess');
          callback();
        });

        transaction.submit();
      },

      function (callback) {
        self.what = 'check account flags';

        $.remote.requestaccountflags('root', 'current', function(err, m) {
          assert.iferror(err);
          assert.strictequal(m, 0);
          done();
        });
      }
    ]

    async.series(steps, function(err) {
      assert(!err, self.what + ': ' + err);
      done();
    });
  });

  test('set requiredesttag', function(done) {
    var self = this;

    var steps = [
      function (callback) {
        self.what = 'set requiredesttag.';

        $.remote.transaction()
        .account_set('root')
        .set_flags('requiredesttag')
        .on('submitted', function (m) {
          //console.log('proposed: %s', json.stringify(m));

          if (m.engine_result === 'tessuccess') {
            callback(null);
          } else {
            //console.log(m);
            callback(new error(m.engine_result));
          }
        })
        .submit();
      },

      function (callback) {
        self.what = 'check requiredesttag';

        $.remote.request_account_flags('root', 'current')
        .on('success', function (m) {
          var wrong = !(m.node.flags & remote.flags.account_root.requiredesttag);

          if (wrong) {
            //console.log('set requiredesttag: failed: %s', json.stringify(m));
          }

          callback(wrong ? new error(wrong) : null);
        })
        .request();
      },

      function (callback) {
        self.what = 'clear requiredesttag.';

        $.remote.transaction()
        .account_set('root')
        .set_flags('optionaldesttag')
        .on('submitted', function (m) {
          //console.log('proposed: %s', json.stringify(m));
          callback(m.engine_result === 'tessuccess' ? null : m.engine_result);
        })
        .submit();
      },

      function (callback) {
        self.what = 'check no requiredesttag';

        $.remote.request_account_flags('root', 'current')
        .on('success', function (m) {
          var wrong = !!(m.node.flags & remote.flags.account_root.requiredesttag);

          if (wrong) {
            console.log('clear requiredesttag: failed: %s', json.stringify(m));
          }

          callback(wrong ? new error(m) : null);
        })
        .request();
      }
    ]

    async.waterfall(steps,function (error) {
      assert(!error, self.what + ': ' + error);
      done();
    });
  });

  test('set requireauth',  function (done) {
    var self = this;

    var steps = [
      function (callback) {
        self.what = 'set requireauth.';

        $.remote.transaction()
        .account_set('root')
        .set_flags('requireauth')
        .on('submitted', function (m) {
          //console.log('proposed: %s', json.stringify(m));
          callback(m.engine_result === 'tessuccess' ? null : new error(m));
        })
        .submit();
      },

      function (callback) {
        self.what = 'check requireauth';

        $.remote.request_account_flags('root', 'current')
        .on('error', callback)
        .on('success', function (m) {
          var wrong = !(m.node.flags & remote.flags.account_root.requireauth);

          if (wrong) {
            console.log('set requireauth: failed: %s', json.stringify(m));
          }

          callback(wrong ? new error(m) : null);
        })
        .request();
      },

      function (callback) {
        self.what = 'clear requireauth.';

        $.remote.transaction()
        .account_set('root')
        .set_flags('optionalauth')
        .on('submitted', function (m) {
          //console.log('proposed: %s', json.stringify(m));

          callback(m.engine_result !== 'tessuccess');
        })
        .submit();
      },

      function (callback) {
        self.what = 'check no requireauth';

        $.remote.request_account_flags('root', 'current')
        .on('error', callback)
        .on('success', function (m) {
          var wrong = !!(m.node.flags & remote.flags.account_root.requireauth);

          if (wrong) {
            console.log('clear requireauth: failed: %s', json.stringify(m));
          }

          callback(wrong ? new error(m) : null);
        })
        .request();
      }
      // xxx also check fails if something is owned.
    ]

    async.waterfall(steps, function(error) {
      assert(!error, self.what + ': ' + error);
      done();
    });
  });

  test('set disallowxrp', function(done) {
    var self = this;

    var steps = [
      function (callback) {
        self.what = 'set disallowxrp.';

        $.remote.transaction()
        .account_set('root')
        .set_flags('disallowxrp')
        .on('submitted', function (m) {
          //console.log('proposed: %s', json.stringify(m));
          callback(m.engine_result === 'tessuccess' ? null : new error(m));
        })
        .submit();
      },

      function (callback) {
        self.what = 'check disallowxrp';

        $.remote.request_account_flags('root', 'current')
        .on('error', callback)
        .on('success', function (m) {
          var wrong = !(m.node.flags & remote.flags.account_root.disallowxrp);

          if (wrong) {
            console.log('set requiredesttag: failed: %s', json.stringify(m));
          }

          callback(wrong ? new error(m) : null);
        })
        .request();
      },

      function (callback) {
        self.what = 'clear disallowxrp.';

        $.remote.transaction()
        .account_set('root')
        .set_flags('allowxrp')
        .on('submitted', function (m) {
          //console.log('proposed: %s', json.stringify(m));

          callback(m.engine_result === 'tessuccess' ? null : new error(m));
        })
        .submit();
      },

      function (callback) {
        self.what = 'check allowxrp';

        $.remote.request_account_flags('root', 'current')
        .on('error', callback)
        .on('success', function (m) {
          var wrong = !!(m.node.flags & remote.flags.account_root.disallowxrp);

          if (wrong) {
            console.log('clear disallowxrp: failed: %s', json.stringify(m));
          }

          callback(wrong ? new error(m) : null);
        })
        .request();
      }
    ]

    async.waterfall(steps, function(err) {
      assert(!err);
      done();
    });
  });
});
