0.4.0 - 2014-11-04
- breaking api change: all websocket++ methods now throw an exception of type
  `websocketpp::exception` which derives from `std::exception`. this normalizes
  all exception types under the standard exception hierarchy and allows
  websocket++ exceptions to be caught in the same statement as others. the error
  code that was previously thrown is wrapped in the exception object and can be
  accessed via the `websocketpp::exception::code()` method.
- breaking api change: custom logging policies have some new required
  constructors that take generic config settings rather than pointers to
  std::ostreams. this allows writing logging policies that do not involve the
  use of std::ostream. this does not affect anyone using the built in logging
  policies.
- breaking utility change: `websocketpp::lib::net::htonll` and
  `websocketpp::lib::net::ntohll` have been prefixed with an underscore to avoid
  conflicts with similarly named macros in some operating systems. if you are
  using the websocket++ provided 64 bit host/network byte order functions you
  will need to switch to the prefixed versions.
- breaking utility change: the signature of `base64_encode` has changed from
  `websocketpp::base64_encode(unsigned char const *, unsigned int)` to
  `websocketpp::base64_encode(unsigned char const *, size_t)`.
- breaking utility change: the signature of `sha1::calc` has changed from
  `websocketpp::sha1::calc(void const *, int, unsigned char *)` to
  `websocketpp::sha1::calc(void const *, size_t, unsigned char *)`
- feature: adds incomplete `minimal_server` and `minimal_client` configs that
  can be used to build custom configs without pulling in the dependencies of
  `core` or `core_client`. these configs will offer a stable base config to
  future-proof custom configs.
- improvement: core library no longer has std::iostream as a dependency.
  std::iostream is still required for the optional iostream logging policy and
  iostream transport.
- bug: c++11 chrono support was being incorrectly detected by the `boost_config`
  header. thank you max dmitrichenko for reporting and a patch.
- bug: use of `std::put_time` is now guarded by a unique flag rather than a
  chrono library flag. thank you max dmitrichenko for reporting.
- bug: fixes non-thread safe use of std::localtime. #347 #383
- compatibility: adjust usage of std::min to be more compatible with systems
  that define a min(...) macro.
- compatibility: removes unused parameters from all library, test, and example
  code. this assists with those developing with -werror and -wunused-parameter
  #376
- compatibility: renames ntohll and htonll methods to avoid conflicts with
  platform specific macros. #358 #381, #382 thank you logotype, unphased,
  svendjo
- cleanup: removes unused functions, fixes variable shadow warnings, normalizes
  all whitespace in library, examples, and tests to 4 spaces. #376

0.3.0 - 2014-08-10
- feature: adds `start_perpetual` and `stop_perpetual` methods to asio transport
  these may be used to replace manually managed `asio::io_service::work` objects
- feature: allow setting pong and handshake timeouts at runtime.
- feature: allows changing the listen backlog queue length.
- feature: split tcp init into pre and post init.
- feature: adds uri method to extract query string from uri. thank you banaan
  for code. #298
- feature: adds a compile time switch to asio transport config to disable
  certain multithreading features (some locks, asio strands)
- feature: adds the ability to pause reading on a connection. paused connections
  will not read more data from their socket, allowing tcp flow control to work
  without blocking the main thread.
- feature: adds the ability to specify whether or not to use the `so_reuseaddr`
  tcp socket option. the default for this value has been changed from `true` to
  `false`.
- feature: adds the ability to specify a maximum message size.
- feature: adds `close::status::get_string(...)` method to look up a human
  readable string given a close code value.
- feature: adds `connection::read_all(...)` method to iostream transport as a
  convenience method for reading all data into the connection buffer without the
  end user needing to manually loop on `read_some`.
- improvement: open, close, and pong timeouts can be disabled entirely by
  setting their duration to 0.
- improvement: numerous performance improvements. including: tuned default
  buffer sizes based on profiling, caching of handler binding for async
  reads/writes, non-malloc allocators for read/write handlers, disabling of a
  number of questionably useful range sanity checks in tight inner loops.
- improvement: cleaned up the handling of tls related errors. tls errors will
  now be reported with more detail on the info channel rather than all being
  `tls_short_read` or `pass_through`. in addition, many cases where a tls short
  read was in fact expected are no longer classified as errors. expected tls
  short reads and quasi-expected socket shutdown related errors will no longer
  be reported as unclean websocket shutdowns to the application. information
  about them will remain in the info error channel for debugging purposes.
- improvement: `start_accept` and `listen` errors are now reported to the caller
  either via an exception or an ec parameter.
- improvement: outgoing writes are now batched for improved message throughput
  and reduced system call and tcp frame overhead.
- bug: fix some cases of calls to empty lib::function objects.
- bug: fix memory leak of connection objects due to cached handlers holding on to
  reference counted pointers. #310 thank you otaras for reporting.
- bug: fix issue with const endpoint accessors (such as `get_user_agent`) not
  compiling due to non-const mutex use. #292 thank you logofive for reporting.
- bug: fix handler allocation crash with multithreaded `io_service`.
- bug: fixes incorrect whitespace handling in header parsing. #301 thank you
  wolfram schroers for reporting
- bug: fix a crash when parsing empty http headers. thank you thingol for
  reporting.
- bug: fix a crash following use of the `stop_listening` function. thank you
  thingol for reporting.
- bug: fix use of variable names that shadow function parameters. the library
  should compile cleanly with -wshadow now. thank you giszo for reporting. #318
- bug: fix an issue where `set_open_handshake_timeout` was ignored by server
  code. thank you robin rowe for reporting.
- bug: fix an issue where custom timeout values weren't being propagated from
  endpoints to new connections.
- bug: fix a number of memory leaks related to server connection failures. #323
  #333 #334 #335 thank you droppy and aydany for reporting and patches.
  reporting.
- compatibility: fix compile time conflict with visual studio's min/max macros.
  thank you robin rowe for reporting.
- documentation: examples and test suite build system now defaults to clang on
  os x

0.3.0-alpha4 - 2013-10-11
- http requests ending normally are no longer logged as errors. thank you banaan
  for reporting. #294
- eliminates spurious expired timers in certain error conditions. thank you
  banaan for reporting. #295
- consolidates all bundled library licenses into the copying file. #294
- updates bundled sha1 library to one with a cleaner interface and more
  straight-forward license. thank you lotodore for reporting and evgeni golov
  for reviewing. #294
- re-introduces strands to asio transport, allowing `io_service` thread pools to
  be used (with some limitations).
- removes endpoint code that kept track of a connection list that was never used
  anywhere. removes a lock and reduces connection creation/deletion complexity
  from o(log n) to o(1) in the number of connections.
- a number of internal changes to transport apis
- deprecates iostream transport `readsome` in favor of `read_some` which is more
  consistent with the naming of the rest of the library.
- adds preliminary signaling to iostream transport of eof and fatal transport
  errors
- updates transport code to use shared pointers rather than raw pointers to
  prevent asio from retaining pointers to connection methods after the
  connection goes out of scope. #293 thank you otaras for reporting.
- fixes an issue where custom headers couldn't be set for client connections
  thank you jerry win and wolfram schroers for reporting.
- fixes a compile error on visual studio when using interrupts. thank you javier
  rey neira for reporting this.
- adds new 1012 and 1013 close codes per iana registry
- add `set_remote_endpoint` method to iostream transport.
- add `set_secure` method to iostream transport.
- fix typo in .gitattributes file. thank you jstarasov for reporting this. #280
- add missing locale include. thank you toninoso for reporting this. #281
- refactors `asio_transport` endpoint and adds full documentation and exception
  free varients of all methods.
- removes `asio_transport` endpoint method cancel(). use `stop_listen()` instead
- wrap internal `io_service` `run_one()` method
- suppress error when trying to shut down a connection that was already closed

0.3.0-alpha3 - 2013-07-16
- minor refactor to bundled sha1 library
- http header comparisons are now case insensitive. #220, #275
- refactors uri to be exception free and not use regular expressions. this
  eliminates the dependency on boost or c++11 regex libraries allowing native
  c++11 usage on gcc 4.4 and higher and significantly reduces staticly built
  binary sizes.
- updates handling of server and user-agent headers to better handle custom
  settings and allow suppression of these headers for security purposes.
- fix issue where pong timeout handler always fired. thank you steven klassen
  for reporting this bug.
- add ping and pong endpoint wrapper methods
- add `get_request()` pass through method to connection to allow calling methods
  specific to the http policy in use.
- fix issue compile error with `websocketpp_strict_masking` enabled and another
  issue where `websocketpp_strict_masking` was not applied to incoming messages.
  thank you petter norby for reporting and testing these bugs. #264
- add additional macro guards for use with boost_config. thank you breyed
  for testing and code. #261

0.3.0-alpha2 - 2013-06-09
- fix a regression that caused servers being sent two close frames in a row
  to end a connection uncleanly. #259
- fix a regression that caused spurious frames following a legitimate close
  frames to erroneously trigger handlers. #258
- change default http response error code when no http_handler is defined from
  500/internal server error to 426/upgrade required
- remove timezone from logger timestamp to work around issues with the windows
  implimentation of strftime. thank you breyed for testing and code. #257
- switch integer literals to char literals to improve vcpp compatibility.
  thank you breyed for testing and code. #257
- add msvcpp warning suppression for the bundled sha1 library. thank you breyed
  for testing and code. #257

0.3.0-alpha1 - 2013-06-09
- initial release
