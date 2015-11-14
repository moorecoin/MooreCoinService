/* -------------------------------- requires -------------------------------- */

var child = require("child_process");
var assert = require("assert");

/* --------------------------------- config --------------------------------- */

if (process.argv[2] == null) {
  [
   'usage: ',
   '',
   '  `node bin/stop-test.js i,j [rippled_path] [rippled_conf]`',
   '',
   '  launch rippled and stop it after n seconds for all n in [i, j}',
   '  for all even values of n launch rippled with `--fg`',
   '  for values of n where n % 3 == 0 launch rippled with `--fg`\n',
   'examples: ',
   '',
   '  $ node bin/stop-test.js 5,10',
   ('  $ node bin/stop-test.js 1,4 ' +
      'build/clang.debug/rippled $home/.confs/rippled.cfg')
   ]
      .foreach(function(l){console.log(l)});

  process.exit();
} else {
  var testrange = process.argv[2].split(',').map(number);
  var rippledpath = process.argv[3] || 'build/rippled'
  var rippledconf = process.argv[4] || 'rippled.cfg'
}

var options = {
  env: process.env,
  stdio: 'ignore' // we could dump the child io when it fails abnormally
};

// default args
var conf_args = ['--conf='+rippledconf];
var start_args  = conf_args.concat([/*'--net'*/])
var stop_args = conf_args.concat(['stop']);

/* --------------------------------- helpers -------------------------------- */

function start(args) {
    return child.spawn(rippledpath, args, options);
}
function stop(rippled) { child.execfile(rippledpath, stop_args, options)}
function secs_l8r(ms, f) {settimeout(f, ms * 1000); }

function show_results_and_exit(results) {
  console.log(json.stringify(results, undefined, 2));
  process.exit();
}

var timetakes = function (range) {
  function sumrange(n) {return (n+1) * n /2}
  var ret = sumrange(range[1]);
  if (range[0] > 1) {
    ret = ret - sumrange(range[0] - 1)
  }
  var stopping = (range[1] - range[0]) * 0.5;
  return ret + stopping;
}

/* ---------------------------------- test ---------------------------------- */

console.log("test will take ~%s seconds", timetakes(testrange));

(function onetest(n /* seconds */, results) {
  if (n >= testrange[1]) {
    // show_results_and_exit(results);
    console.log(json.stringify(results, undefined, 2));
    onetest(testrange[0], []);
    return;
  }

  var args = start_args;
  if (n % 2 == 0) {args = args.concat(['--fg'])}
  if (n % 3 == 0) {args = args.concat(['--net'])}

  var result = {args: args, alive_for: n};
  results.push(result);

  console.log("\nlaunching `%s` with `%s` for %d seconds",
                rippledpath, json.stringify(args), n);

  rippled = start(args);
  console.log("rippled pid: %d", rippled.pid);

  // defaults
  var b4stopsent = false;
  var stopsent = false;
  var stop_took = null;

  rippled.once('exit', function(){
    if (!stopsent && !b4stopsent) {
      console.warn('\nrippled exited itself b4 stop issued');
      process.exit();
    };

    // the io handles close after exit, may have implications for
    // `stdio:'inherit'` option to `child.spawn`.
    rippled.once('close', function() {
      result.stop_took = (+new date() - stop_took) / 1000; // seconds
      console.log("stopping after %d seconds took %s seconds",
                   n, result.stop_took);
      onetest(n+1, results);
    });
  });

  secs_l8r(n, function(){
    console.log("stopping rippled after %d seconds", n);

    // possible race here ?
    // seems highly unlikely, but i was having issues at one point
    b4stopsent=true;
    stop_took = (+new date());
    // when does `exit` actually get sent?
    stop();
    stopsent=true;

    // sometimes we want to attach with a debugger.
    if (process.env.abort_tests_on_stall != null) {
      // we wait 30 seconds, and if it hasn't stopped, we abort the process
      secs_l8r(30, function() {
        if (result.stop_took == null) {
          console.log("rippled has stalled");
          process.exit();
        };
      });
    }
  })
}(testrange[0], []));