#!/usr/bin/node
//
// ledger?l=l
// transaction?h=h
// ledger_entry?l=l&h=h
// account?l=l&a=a
// directory?l=l&dir_root=h&i=i
// directory?l=l&o=a&i=i     // owner directory
// offer?l=l&offer=h
// offer?l=l&account=a&i=i
// ripple_state=l=l&a=a&b=a&c=c
// account_lines?l=l&a=a
//
// a=address
// c=currency 3 letter code
// h=hash
// i=index
// l=current | closed | validated | index | hash
//

var async     = require("async");
var extend    = require("extend");
var http      = require("http");
var url       = require("url");

var remote    = require("ripple-lib").remote;

var program   = process.argv[1];

var httpd_response = function (res, opts) {
  var self=this;

  res.statuscode = opts.statuscode;
  res.end(
    "<html>"
      + "<head><title>title</title></head>"
      + "<body background=\"#ffffff\">"
      + "state:" + self.state
      + "<ul>"
      + "<li><a href=\"/\">home</a>"
      + "<li>" + html_link('r4em4gbqfr1qgqlxspf4r7h84qe9mb6icc')
//      + "<li><a href=\""+test+"\">rhb9cjawyb4rj91vrwn96dkukg4bwdtyth</a>"
      + "<li><a href=\"/ledger\">ledger</a>"
      + "</ul>"
      + (opts.body || '')
      + '<hr><pre>'
      + (opts.url || '')
      + '</pre>'
      + "</body>"
      + "</html>"
    );
};

var html_link = function (generic) {
  return '<a href="' + build_uri({ type: 'account', account: generic}) + '">' + generic + '</a>';
};

// build a link to a type.
var build_uri = function (params, opts) {
  var c;

  if (params.type === 'account') {
    c = {
        pathname: 'account',
        query: {
          l: params.ledger,
          a: params.account,
        },
      };

  } else if (params.type === 'ledger') {
    c = {
        pathname: 'ledger',
        query: {
          l: params.ledger,
        },
      };

  } else if (params.type === 'transaction') {
    c = {
        pathname: 'transaction',
        query: {
          h: params.hash,
        },
      };
  } else {
    c = {};
  }

  opts  = opts || {};

  c.protocol  = "http";
  c.hostname  = opts.hostname || self.base.hostname;
  c.port      = opts.port || self.base.port;

  return url.format(c);
};

var build_link = function (item, link) {
console.log(link);
  return "<a href=" + link + ">" + item + "</a>";
};

var rewrite_field = function (type, obj, field, opts) {
  if (field in obj) {
    obj[field]  = rewrite_type(type, obj[field], opts);
  }
};

var rewrite_type = function (type, obj, opts) {
  if ('amount' === type) {
    if ('string' === typeof obj) {
      // xrp.
      return '<b>' + obj + '</b>';

    } else {
      rewrite_field('address', obj, 'issuer', opts);

      return obj; 
    }
    return build_link(
      obj,
      build_uri({
          type: 'account',
          account: obj
        }, opts)
    );
  }
  if ('address' === type) {
    return build_link(
      obj,
      build_uri({
          type: 'account',
          account: obj
        }, opts)
    );
  }
  else if ('ledger' === type) {
    return build_link(
      obj,
      build_uri({
          type: 'ledger',
          ledger: obj,
        }, opts)
      );
  }
  else if ('node' === type) {
    // a node
    if ('previoustxnid' in obj)
      obj.previoustxnid      = rewrite_type('transaction', obj.previoustxnid, opts);

    if ('offer' === obj.ledgerentrytype) {
      if ('newfields' in obj) {
        if ('takergets' in obj.newfields)
          obj.newfields.takergets = rewrite_type('amount', obj.newfields.takergets, opts);

        if ('takerpays' in obj.newfields)
          obj.newfields.takerpays = rewrite_type('amount', obj.newfields.takerpays, opts);
      }
    }

    obj.ledgerentrytype  = '<b>' + obj.ledgerentrytype + '</b>';

    return obj;
  }
  else if ('transaction' === type) {
    // reference to a transaction.
    return build_link(
      obj,
      build_uri({
          type: 'transaction',
          hash: obj
        }, opts)
      );
  }

  return 'error: ' + type;
};

var rewrite_object = function (obj, opts) {
  var out = extend({}, obj);

  rewrite_field('address', out, 'account', opts);

  rewrite_field('ledger', out, 'parent_hash', opts);
  rewrite_field('ledger', out, 'ledger_index', opts);
  rewrite_field('ledger', out, 'ledger_current_index', opts);
  rewrite_field('ledger', out, 'ledger_hash', opts);

  if ('ledger' in obj) {
    // it's a ledger header.
    out.ledger  = rewrite_object(out.ledger, opts);

    if ('ledger_hash' in out.ledger)
      out.ledger.ledger_hash = '<b>' + out.ledger.ledger_hash + '</b>';

    delete out.ledger.hash;
    delete out.ledger.totalcoins;
  }

  if ('transactiontype' in obj) {
    // it's a transaction.
    out.transactiontype = '<b>' + obj.transactiontype + '</b>';

    rewrite_field('amount', out, 'takergets', opts);
    rewrite_field('amount', out, 'takerpays', opts);
    rewrite_field('ledger', out, 'inledger', opts);

    out.meta.affectednodes = out.meta.affectednodes.map(function (node) {
        var kind  = 'creatednode' in node
          ? 'creatednode'
          : 'modifiednode' in node
            ? 'modifiednode'
            : 'deletednode' in node
              ? 'deletednode'
              : undefined;
        
        if (kind) {
          node[kind]  = rewrite_type('node', node[kind], opts);
        }
        return node;
      });
  }
  else if ('node' in obj && 'ledgerentrytype' in obj.node) {
    // its a ledger entry.

    if (obj.node.ledgerentrytype === 'accountroot') {
      rewrite_field('address', out.node, 'account', opts);
      rewrite_field('transaction', out.node, 'previoustxnid', opts);
      rewrite_field('ledger', out.node, 'previoustxnlgrseq', opts);
    }

    out.node.ledgerentrytype = '<b>' + out.node.ledgerentrytype + '</b>';
  }

  return out;
};

var augment_object = function (obj, opts, done) {
  if (obj.node.ledgerentrytype == 'accountroot') {
    var   tx_hash   = obj.node.previoustxnid;
    var   tx_ledger = obj.node.previoustxnlgrseq;

    obj.history                 = [];

    async.whilst(
      function () { return tx_hash; },
      function (callback) {
// console.log("augment_object: request: %s %s", tx_hash, tx_ledger);
        opts.remote.request_tx(tx_hash)
          .on('success', function (m) {
              tx_hash   = undefined;
              tx_ledger = undefined;

//console.log("augment_object: ", json.stringify(m));
              m.meta.affectednodes.filter(function(n) {
// console.log("augment_object: ", json.stringify(n));
// if (n.modifiednode)
// console.log("augment_object: %s %s %s %s %s %s/%s", 'modifiednode' in n, n.modifiednode && (n.modifiednode.ledgerentrytype === 'accountroot'), n.modifiednode && n.modifiednode.finalfields && (n.modifiednode.finalfields.account === obj.node.account), object.keys(n)[0], n.modifiednode && (n.modifiednode.ledgerentrytype), obj.node.account, n.modifiednode && n.modifiednode.finalfields && n.modifiednode.finalfields.account);
// if ('modifiednode' in n && n.modifiednode.ledgerentrytype === 'accountroot')
// {
//   console.log("***: ", json.stringify(m));
//   console.log("***: ", json.stringify(n));
// }
                  return 'modifiednode' in n
                    && n.modifiednode.ledgerentrytype === 'accountroot'
                    && n.modifiednode.finalfields
                    && n.modifiednode.finalfields.account === obj.node.account;
                })
              .foreach(function (n) {
                  tx_hash   = n.modifiednode.previoustxnid;
                  tx_ledger = n.modifiednode.previoustxnlgrseq;

                  obj.history.push({
                      tx_hash:    tx_hash,
                      tx_ledger:  tx_ledger
                    });
console.log("augment_object: next: %s %s", tx_hash, tx_ledger);
                });

              callback();
            })
          .on('error', function (m) {
              callback(m);
            })
          .request();
      },
      function (err) {
        if (err) {
          done();
        }
        else {
          async.foreach(obj.history, function (o, callback) {
              opts.remote.request_account_info(obj.node.account)
                .ledger_index(o.tx_ledger)
                .on('success', function (m) {
//console.log("augment_object: ", json.stringify(m));
                    o.balance       = m.account_data.balance;
//                    o.account_data  = m.account_data;
                    callback();
                  })
                .on('error', function (m) {
                    o.error = m;
                    callback();
                  })
                .request();
            },
            function (err) {
              done(err);
            });
        }
      });
  }
  else {
    done();
  }
};

if (process.argv.length < 4 || process.argv.length > 7) {
  console.log("usage: %s ws_ip ws_port [<ip> [<port> [<start>]]]", program);
}
else {
  var ws_ip   = process.argv[2];
  var ws_port = process.argv[3];
  var ip      = process.argv.length > 4 ? process.argv[4] : "127.0.0.1";
  var port    = process.argv.length > 5 ? process.argv[5] : "8080";

// console.log("start");
  var self  = this;
  
  var remote  = (new remote({
                    websocket_ip: ws_ip,
                    websocket_port: ws_port,
                    trace: false
                  }))
                  .on('state', function (m) {
                      console.log("state: %s", m);

                      self.state   = m;
                    })
//                  .once('ledger_closed', callback)
                  .connect()
                  ;

  self.base = {
      hostname: ip,
      port:     port,
      remote:   remote,
    };

// console.log("serve");
  var server  = http.createserver(function (req, res) {
      var input = "";

      req.setencoding();

      req.on('data', function (buffer) {
          // console.log("data: %s", buffer);
          input = input + buffer;
        });

      req.on('end', function () {
          // console.log("url: %s", req.url);
          // console.log("headers: %s", json.stringify(req.headers, undefined, 2));

          var _parsed = url.parse(req.url, true);
          var _url    = json.stringify(_parsed, undefined, 2);

          // console.log("headers: %s", json.stringify(_parsed, undefined, 2));
          if (_parsed.pathname === "/account") {
              var request = remote
                .request_ledger_entry('account_root')
                .ledger_index(-1)
                .account_root(_parsed.query.a)
                .on('success', function (m) {
                    // console.log("account_root: %s", json.stringify(m, undefined, 2));

                    augment_object(m, self.base, function() {
                      httpd_response(res,
                          {
                            statuscode: 200,
                            url: _url,
                            body: "<pre>"
                              + json.stringify(rewrite_object(m, self.base), undefined, 2)
                              + "</pre>"
                          });
                    });
                  })
                .request();

          } else if (_parsed.pathname === "/ledger") {
            var request = remote
              .request_ledger(undefined, { expand: true, transactions: true })
              .on('success', function (m) {
                  // console.log("ledger: %s", json.stringify(m, undefined, 2));

                  httpd_response(res,
                      {
                        statuscode: 200,
                        url: _url,
                        body: "<pre>"
                          + json.stringify(rewrite_object(m, self.base), undefined, 2)
                          +"</pre>"
                      });
                })

            if (_parsed.query.l && _parsed.query.l.length === 64) {
              request.ledger_hash(_parsed.query.l);
            }
            else if (_parsed.query.l) {
              request.ledger_index(number(_parsed.query.l));
            }
            else {
              request.ledger_index(-1);
            }

            request.request();

          } else if (_parsed.pathname === "/transaction") {
              var request = remote
                .request_tx(_parsed.query.h)
//                .request_transaction_entry(_parsed.query.h)
//              .ledger_select(_parsed.query.l)
                .on('success', function (m) {
                    // console.log("transaction: %s", json.stringify(m, undefined, 2));

                    httpd_response(res,
                        {
                          statuscode: 200,
                          url: _url,
                          body: "<pre>"
                            + json.stringify(rewrite_object(m, self.base), undefined, 2)
                            +"</pre>"
                        });
                  })
                .on('error', function (m) {
                    httpd_response(res,
                        {
                          statuscode: 200,
                          url: _url,
                          body: "<pre>"
                            + 'error: ' + json.stringify(m, undefined, 2)
                            +"</pre>"
                        });
                  })
                .request();

          } else {
            var test  = build_uri({
                type: 'account',
                ledger: 'closed',
                account: 'rhb9cjawyb4rj91vrwn96dkukg4bwdtyth',
              }, self.base);

            httpd_response(res,
                {
                  statuscode: req.url === "/" ? 200 : 404,
                  url: _url,
                });
          }
        });
    });

  server.listen(port, ip, undefined,
    function () {
      console.log("listening at: http://%s:%s", ip, port);
    });
}

// vim:sw=2:sts=2:ts=8:et
