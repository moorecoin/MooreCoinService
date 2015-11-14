# integration tests

## basic usage.

documentation for installation of dependencies and running these
tests can be found with the
[_rippled build instructions_][unit_testing].
(also for [_windows_][windows_unit_testing],
[_os x_][osx_unit_testing],
or [_ubuntu_][ubuntu_unit_testing].)

## advanced usage.

these instructions assume familiarity with the instructions linked above.

### debugging rippled

by default, each test will start and stop an independent instance of `rippled`.
this ensures that each test is run against the known
[_genesis ledger_][genesis_ledger].

to use a running `rippled`, particularly one running in a debugger, follow
these steps:

1. make a copy of the example configuration file: `cp -i test/config-example.js test/config.js`

2. edit `test/config.js` to select the "debug" server configuration.
  * change the existing default server to: `exports.server_default = "debug";`
  (near the top of the file).

3. create a `rippled.cfg` file for the tests.
  1. run `npm test`. the tests will fail. **this failure is expected.**
  2. copy and/or rename the `tmp/server/debug/rippled.cfg` file to somewhere
  convenient.

4. start `rippled` (in a debugger) with command line options
`-av --conf <rippled-created-above.cfg>`.

5. set any desired breakpoints in the `rippled` source.

6. running one test per [_genesis ledger_][genesis_ledger] is highly recommended.
if the relevant `.js` file contains more than one test, change `test(` to
`test.only(` for the single desired test.
  * to run multiple tests, change `test(` to `test.skip(` for any undesired tests
  in the .js file.

7. start test(s) in the [_node-inspector_][node_inspector] debugger.
(note that the tests can be run without the debugger, but there will probably
be problems with timeouts or reused ledgers).
  1. `node_modules/node-inspector/bin/inspector.js &`
  2. `node node_modules/.bin/mocha --debug --debug-brk test/<testfile.js>`
  3. browse to http://127.0.0.1:8080/debug?port=5858 in a browser supported
  by [_node-inspector_][node_inspector] (i.e. chrome or safari).

8. to run multiple tests, put a breakpoint in the following function:
  * file `testutils.js` -> function `build_teardown()` -> nested function
  `teardown()` -> nested series function `stop_server()`.
    * when this breakpoint is hit, stop and restart `rippled`.

9. use the [_node-inspector ui_][node_inspector_ui] to step through and run
the test(s) until control is handed off to `rippled`. when the request is
finished control will be handed back to node-inspector, which may or may not
stop depending on which breakpoints are set.

### after debugging

1. to return to the default behavior, edit `test/config.js` and change the
default server back to its original value: `exports.server_default = "alpha";`.
  * alternately, delete `test/config.js`.

[unit_testing]: https://wiki.ripple.com/rippled_build_instructions#unit_testing
[windows_unit_testing]: https://wiki.ripple.com/visual_studio_2013_build_instructions#unit_tests_.28recommended.29
[osx_unit_testing]: https://wiki.ripple.com/osx_build_instructions#system_tests_.28recommended.29
[ubuntu_unit_testing]: https://wiki.ripple.com/ubuntu_build_instructions#system_tests_.28recommended.29
[genesis_ledger]: https://wiki.ripple.com/genesis_ledger
[node_inspector]: https://wiki.ripple.com/rippled_build_instructions#node-inspector
[node_inspector_ui]: https://github.com/node-inspector/node-inspector/blob/master/readme.md
