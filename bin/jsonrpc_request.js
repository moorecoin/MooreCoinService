#!/usr/bin/node
//
// this is a tool to issue json-rpc requests from the command line.
//
// this can be used to test a json-rpc server.
//
// requires: npm simple-jsonrpc
//

var jsonrpc   = require('simple-jsonrpc');

var program   = process.argv[1];

if (5 !== process.argv.length) {
  console.log("usage: %s <url> <method> <json>", program);
}
else {
  var url       = process.argv[2];
  var method    = process.argv[3];
  var json_raw  = process.argv[4];
  var json;

  try {
    json      = json.parse(json_raw);
  }
  catch (e) {
      console.log("json parse error: %s", e.message);
      throw e;
  }

  var client  = jsonrpc.client(url);

  client.call(method, json,
    function (result) {
      console.log(json.stringify(result, undefined, 2));
    },
    function (error) {
      console.log(json.stringify(error, undefined, 2));
    });
}

// vim:sw=2:sts=2:ts=8:et
