/**
 * bin/update_bintypes.js
 *
 * this unholy abomination of a script generates the javascript file
 * src/js/bintypes.js from various parts of the c++ source code.
 *
 * this should *not* be part of any automatic build process unless the c++
 * source data are brought into a more easily parseable format. until then,
 * simply run this script manually and fix as needed.
 */

// xxx: process ledgerformats.(h|cpp) as well.

var filenameproto = __dirname + '/../src/cpp/ripple/serializeproto.h',
    filenametxformatsh = __dirname + '/../src/cpp/ripple/transactionformats.h',
    filenametxformats = __dirname + '/../src/cpp/ripple/transactionformats.cpp';

var fs = require('fs');

var output = [];

// stage 1: get the field types and codes from serializeproto.h
var types = {},
    fields = {};
string(fs.readfilesync(filenameproto)).split('\n').foreach(function (line) {
  line = line.replace(/^\s+|\s+$/g, '').replace(/\s+/g, '');
  if (!line.length || line.slice(0, 2) === '//' || line.slice(-1) !== ')') return;

  var tmp = line.slice(0, -1).split('('),
      type = tmp[0],
      opts = tmp[1].split(',');

  if (type === 'type') types[opts[1]] = [opts[0], +opts[2]];
  else if (type === 'field') fields[opts[0]] = [types[opts[1]][0], +opts[2]];
});

output.push('var st = require("./serializedtypes");');
output.push('');
output.push('var required = exports.required = 0,');
output.push('    optional = exports.optional = 1,');
output.push('    default  = exports.default  = 2;');
output.push('');

function pad(s, n) { while (s.length < n) s += ' '; return s; }
function padl(s, n) { while (s.length < n) s = ' '+s; return s; }

object.keys(types).foreach(function (type) {
  output.push(pad('st.'+types[type][0]+'.id', 25) + ' = '+types[type][1]+';');
});
output.push('');

// stage 2: get the transaction type ids from transactionformats.h
var ttconsts = {};
string(fs.readfilesync(filenametxformatsh)).split('\n').foreach(function (line) {
  var regex = /tt([a-z_]+)\s+=\s+([0-9-]+)/;
  var match = line.match(regex);
  if (match) ttconsts[match[1]] = +match[2];
});

// stage 3: get the transaction formats from transactionformats.cpp
var base = [],
    sections = [],
    current = base;
string(fs.readfilesync(filenametxformats)).split('\n').foreach(function (line) {
  line = line.replace(/^\s+|\s+$/g, '').replace(/\s+/g, '');

  var d_regex = /declare_tf\(([a-za-z]+),tt([a-z_]+)/;
  var d_match = line.match(d_regex);

  var s_regex = /soelement\(sf([a-z]+),soe_(required|optional|default)/i;
  var s_match = line.match(s_regex);

  if (d_match) sections.push(current = [d_match[1], ttconsts[d_match[2]]]);
  else if (s_match) current.push([s_match[1], s_match[2]]);
});

function removefinalcomma(arr) {
  arr[arr.length-1] = arr[arr.length-1].slice(0, -1);
}

output.push('var base = [');
base.foreach(function (field) {
  var spec = fields[field[0]];
  output.push('  [ '+
              pad("'"+field[0]+"'", 21)+', '+
              pad(field[1], 8)+', '+
              padl(""+spec[1], 2)+', '+
              'st.'+pad(spec[0], 3)+
              ' ],');
});
removefinalcomma(output);
output.push('];');
output.push('');


output.push('exports.tx = {');
sections.foreach(function (section) {
  var name = section.shift(),
      ttid = section.shift();

  output.push('  '+name+': ['+ttid+'].concat(base, [');
  section.foreach(function (field) {
    var spec = fields[field[0]];
    output.push('    [ '+
                pad("'"+field[0]+"'", 21)+', '+
                pad(field[1], 8)+', '+
                padl(""+spec[1], 2)+', '+
                'st.'+pad(spec[0], 3)+
                ' ],');
  });
  removefinalcomma(output);
  output.push('  ]),');
});
removefinalcomma(output);
output.push('};');
output.push('');

console.log(output.join('\n'));

