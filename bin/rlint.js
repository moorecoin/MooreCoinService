#!/usr/bin/node

var async       = require('async');
var remote      = require('ripple-lib').remote;
var transaction = require('ripple-lib').transaction;
var uint160     = require('ripple-lib').uint160;
var amount      = require('ripple-lib').amount;

var book_key = function (book) {
  return book.taker_pays.currency
    + ":" + book.taker_pays.issuer
    + ":" + book.taker_gets.currency
    + ":" + book.taker_gets.issuer;
};

var book_key_cross = function (book) {
  return book.taker_gets.currency
    + ":" + book.taker_gets.issuer
    + ":" + book.taker_pays.currency
    + ":" + book.taker_pays.issuer;
};

var ledger_verify = function (ledger) {
  var dir_nodes = ledger.accountstate.filter(function (entry) {
      return entry.ledgerentrytype === 'directorynode'    // only directories
        && entry.index === entry.rootindex                // only root nodes
        && 'takergetscurrency' in entry;                  // only offer directories
    });

  var books = {};

  dir_nodes.foreach(function (node) {
      var book = {
        taker_gets: {
            currency: uint160.from_generic(node.takergetscurrency).to_json(),
            issuer: uint160.from_generic(node.takergetsissuer).to_json()
          },
        taker_pays: {
          currency: uint160.from_generic(node.takerpayscurrency).to_json(),
          issuer: uint160.from_generic(node.takerpaysissuer).to_json()
        },
        quality: amount.from_quality(node.rootindex),
        index: node.rootindex
      };

      books[book_key(book)] = book;

//      console.log(json.stringify(node, undefined, 2));
    });

//  console.log(json.stringify(dir_entry, undefined, 2));
  console.log("#%s books: %s", ledger.ledger_index, object.keys(books).length);

  object.keys(books).foreach(function (key) {
      var book        = books[key];
      var key_cross   = book_key_cross(book);
      var book_cross  = books[key_cross];

      if (book && book_cross && !book_cross.done)
      {
        var book_cross_quality_inverted = amount.from_json("1.0/1/1").divide(book_cross.quality);

        if (book_cross_quality_inverted.compareto(book.quality) >= 0)
        {
          // crossing books
          console.log("crossing: #%s :: %s :: %s :: %s :: %s :: %s :: %s", ledger.ledger_index, key, book.quality.to_text(), book_cross.quality.to_text(), book_cross_quality_inverted.to_text(),
            book.index, book_cross.index);
        }

        book_cross.done = true;
      }
    });

  var ripple_selfs  = {};

  var accounts  = {};
  var counts    = {};

  ledger.accountstate.foreach(function (entry) {
      if (entry.ledgerentrytype === 'offer')
      {
        counts[entry.account] = (counts[entry.account] || 0) + 1;
      }
      else if (entry.ledgerentrytype === 'ripplestate')
      {
        if (entry.flags & (0x10000 | 0x40000))
        {
          counts[entry.lowlimit.issuer]   = (counts[entry.lowlimit.issuer] || 0) + 1;
        }

        if (entry.flags & (0x20000 | 0x80000))
        {
          counts[entry.highlimit.issuer]  = (counts[entry.highlimit.issuer] || 0) + 1;
        }

        if (entry.highlimit.issuer === entry.lowlimit.issuer)
          ripple_selfs[entry.account] = entry;
      }
      else if (entry.ledgerentrytype == 'accountroot')
      {
        accounts[entry.account] = entry;
      }
    });

  var low               = 0;  // accounts with too low a count.
  var high              = 0;
  var missing_accounts  = 0;  // objects with no referencing account.
  var missing_objects   = 0;  // accounts specifying an object but having none.

  object.keys(counts).foreach(function (account) {
      if (account in accounts)
      {
        if (counts[account] !== accounts[account].ownercount)
        {
          if (counts[account] < accounts[account].ownercount)
          {
            high  += 1;
            console.log("%s: high count %s/%s", account, counts[account], accounts[account].ownercount);
          }
          else
          {
            low   += 1;
            console.log("%s: low count %s/%s", account, counts[account], accounts[account].ownercount);
          }
        }
      }
      else
      {
        missing_accounts  += 1;

        console.log("%s: missing : count %s", account, counts[account]);
      }
    });

  object.keys(accounts).foreach(function (account) {
      if (!('ownercount' in accounts[account]))
      {
          console.log("%s: bad entry : %s", account, json.stringify(accounts[account], undefined, 2));
      }
      else if (!(account in counts) && accounts[account].ownercount)
      {
          missing_objects += 1;

          console.log("%s: no objects : %s/%s", account, 0, accounts[account].ownercount);
      }
    });

  if (low)
    console.log("counts too low = %s", low);

  if (high)
    console.log("counts too high = %s", high);

  if (missing_objects)
    console.log("missing_objects = %s", missing_objects);

  if (missing_accounts)
    console.log("missing_accounts = %s", missing_accounts);

  if (object.keys(ripple_selfs).length)
    console.log("ripplestate selfs = %s", object.keys(ripple_selfs).length);

};

var ledger_request = function (remote, ledger_index, done) {
 remote.request_ledger(undefined, {
      accounts: true,
      expand: true,
    })
  .ledger_index(ledger_index)
  .on('success', function (m) {
      // console.log("ledger: ", ledger_index);
      // console.log("ledger: ", json.stringify(m, undefined, 2));
      done(m.ledger);
    })
  .on('error', function (m) {
      console.log("error");
      done();
    })
  .request();
};

var usage = function () {
  console.log("rlint.js _websocket_ip_ _websocket_port_ ");
};

var finish = function (remote) {
  remote.disconnect();

  // xxx because remote.disconnect() doesn't work:
  process.exit();
};

console.log("args: ", process.argv.length);
console.log("args: ", process.argv);

if (process.argv.length < 4) {
  usage();
}
else {
  var remote  = remote.from_config({
        websocket_ip:   process.argv[2],
        websocket_port: process.argv[3],
      })
    .once('ledger_closed', function (m) {
        console.log("ledger_closed: ", json.stringify(m, undefined, 2));

        if (process.argv.length === 5) {
          var ledger_index  = process.argv[4];

          ledger_request(remote, ledger_index, function (l) {
              if (l) {
                ledger_verify(l);
              }

              finish(remote);
            });

        } else if (process.argv.length === 6) {
          var ledger_start  = number(process.argv[4]);
          var ledger_end    = number(process.argv[5]);
          var ledger_cursor = ledger_end;

          async.whilst(
            function () {
              return ledger_start <= ledger_cursor && ledger_cursor <=ledger_end;
            },
            function (callback) {
              // console.log(ledger_cursor);

              ledger_request(remote, ledger_cursor, function (l) {
                  if (l) {
                    ledger_verify(l);
                  }

                  --ledger_cursor;

                  callback();
                });
            },
            function (error) {
              finish(remote);
            });

        } else {
          finish(remote);
        }
      })
    .connect();
}

// vim:sw=2:sts=2:ts=8:et
