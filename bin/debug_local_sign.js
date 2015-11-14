var ripple = require('ripple-lib');

var v = {
  seed: "snopbrxtmemymhuvtgbuqafg1sutb",
  addr: "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth"
};

var remote = ripple.remote.from_config({
  "trusted" : true,
  "websocket_ip" : "127.0.0.1",
  "websocket_port" : 5006,
  "websocket_ssl" : false,
  "local_signing" : true
});

var tx_json = {
	"account" : v.addr,
	"amount" : "10000000",
	"destination" : "reu2ulpieqm1bal8pyzmxnnx1afx9scks",
	"fee" : "10",
	"flags" : 0,
	"sequence" : 3,
	"transactiontype" : "payment"

  //"signingpubkey": '0396941b22791a448e5877a44ce98434db217d6fb97d63f0dad23be49ed45173c9'
};

remote.on('connected', function () {
  var req = remote.request_sign(v.seed, tx_json);
  req.message.debug_signing = true;
  req.on('success', function (result) {
    console.log("server result");
    console.log(result);

    var sim = {};
    var tx = remote.transaction();
    tx.tx_json = tx_json;
    tx._secret = v.seed;
    tx.complete();
    var unsigned = tx.serialize().to_hex();
    tx.sign();

    sim.tx_blob = tx.serialize().to_hex();
    sim.tx_json = tx.tx_json;
    sim.tx_signing_hash = tx.signing_hash().to_hex();
    sim.tx_unsigned = unsigned;

    console.log("\nlocal result");
    console.log(sim);

    remote.connect(false);
  });
  req.on('error', function (err) {
    if (err.error === "remoteerror" && err.remote.error === "srcactnotfound") {
      console.log("please fund account "+v.addr+" to run this test.");
    } else {
      console.log('error', err);
    }
    remote.connect(false);
  });
  req.request();

});
remote.connect();
