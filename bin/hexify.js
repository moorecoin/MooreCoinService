#!/usr/bin/node
//
// returns hex of lowercasing a string.
//

var stringtohex = function (s) {
  return array.prototype.map.call(s, function (c) {
      var b = c.charcodeat(0);

      return b < 16 ? "0" + b.tostring(16) : b.tostring(16);
    }).join("");
};

if (3 != process.argv.length) {
  process.stderr.write("usage: " + process.argv[1] + " string\n\nreturns hex of lowercasing string.\n");
  process.exit(1);

} else {

  process.stdout.write(stringtohex(process.argv[2].tolowercase()) + "\n");
}

// vim:sw=2:sts=2:ts=8:et
