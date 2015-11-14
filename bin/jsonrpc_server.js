#!/usr/bin/node
//
// this is a tool to listen for json-rpc requests at an ip and port.
//
// this will report the request to console and echo back the request as the response.
//

var http      = require("http");

var program   = process.argv[1];

if (4 !== process.argv.length) {
  console.log("usage: %s <ip> <port>", program);
}
else {
  var ip      = process.argv[2];
  var port    = process.argv[3];

  var server  = http.createserver(function (req, res) {
      console.log("connect");
      var input = "";

      req.setencoding();

      req.on('data', function (buffer) {
          // console.log("data: %s", buffer);
          input = input + buffer;
        });

      req.on('end', function () {
          // console.log("end");

          var json_req;

          console.log("url: %s", req.url);
          console.log("headers: %s", json.stringify(req.headers, undefined, 2));

          try {
            json_req = json.parse(input);

            console.log("req: %s", json.stringify(json_req, undefined, 2));
          }
          catch (e) {
            console.log("bad json: %s", e.message);

            json_req = { error : e.message }
          }

          res.statuscode = 200;
          res.end(json.stringify({
              jsonrpc: "2.0",
              result: { request : json_req },
              id: req.id
            }));
        });

      req.on('close', function () {
          console.log("close");
        });
    });

  server.listen(port, ip, undefined,
    function () {
      console.log("listening at: %s:%s", ip, port);
    });
}

// vim:sw=2:sts=2:ts=8:et
