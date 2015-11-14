http parser
===========

[![build status](https://travis-ci.org/joyent/http-parser.png?branch=master)](https://travis-ci.org/joyent/http-parser)

this is a parser for http messages written in c. it parses both requests and
responses. the parser is designed to be used in performance http
applications. it does not make any syscalls nor allocations, it does not
buffer data, it can be interrupted at anytime. depending on your
architecture, it only requires about 40 bytes of data per message
stream (in a web server that is per connection).

features:

  * no dependencies
  * handles persistent streams (keep-alive).
  * decodes chunked encoding.
  * upgrade support
  * defends against buffer overflow attacks.

the parser extracts the following information from http messages:

  * header fields and values
  * content-length
  * request method
  * response status code
  * transfer-encoding
  * http version
  * request url
  * message body


usage
-----

one `http_parser` object is used per tcp connection. initialize the struct
using `http_parser_init()` and set the callbacks. that might look something
like this for a request parser:
```c
http_parser_settings settings;
settings.on_url = my_url_callback;
settings.on_header_field = my_header_field_callback;
/* ... */

http_parser *parser = malloc(sizeof(http_parser));
http_parser_init(parser, http_request);
parser->data = my_socket;
```

when data is received on the socket execute the parser and check for errors.

```c
size_t len = 80*1024, nparsed;
char buf[len];
ssize_t recved;

recved = recv(fd, buf, len, 0);

if (recved < 0) {
  /* handle error. */
}

/* start up / continue the parser.
 * note we pass recved==0 to signal that eof has been recieved.
 */
nparsed = http_parser_execute(parser, &settings, buf, recved);

if (parser->upgrade) {
  /* handle new protocol */
} else if (nparsed != recved) {
  /* handle error. usually just close the connection. */
}
```

http needs to know where the end of the stream is. for example, sometimes
servers send responses without content-length and expect the client to
consume input (for the body) until eof. to tell http_parser about eof, give
`0` as the forth parameter to `http_parser_execute()`. callbacks and errors
can still be encountered during an eof, so one must still be prepared
to receive them.

scalar valued message information such as `status_code`, `method`, and the
http version are stored in the parser structure. this data is only
temporally stored in `http_parser` and gets reset on each new message. if
this information is needed later, copy it out of the structure during the
`headers_complete` callback.

the parser decodes the transfer-encoding for both requests and responses
transparently. that is, a chunked encoding is decoded before being sent to
the on_body callback.


the special problem of upgrade
------------------------------

http supports upgrading the connection to a different protocol. an
increasingly common example of this is the web socket protocol which sends
a request like

        get /demo http/1.1
        upgrade: websocket
        connection: upgrade
        host: example.com
        origin: http://example.com
        websocket-protocol: sample

followed by non-http data.

(see http://tools.ietf.org/html/draft-hixie-thewebsocketprotocol-75 for more
information the web socket protocol.)

to support this, the parser will treat this as a normal http message without a
body. issuing both on_headers_complete and on_message_complete callbacks. however
http_parser_execute() will stop parsing at the end of the headers and return.

the user is expected to check if `parser->upgrade` has been set to 1 after
`http_parser_execute()` returns. non-http data begins at the buffer supplied
offset by the return value of `http_parser_execute()`.


callbacks
---------

during the `http_parser_execute()` call, the callbacks set in
`http_parser_settings` will be executed. the parser maintains state and
never looks behind, so buffering the data is not necessary. if you need to
save certain data for later usage, you can do that from the callbacks.

there are two types of callbacks:

* notification `typedef int (*http_cb) (http_parser*);`
    callbacks: on_message_begin, on_headers_complete, on_message_complete.
* data `typedef int (*http_data_cb) (http_parser*, const char *at, size_t length);`
    callbacks: (requests only) on_uri,
               (common) on_header_field, on_header_value, on_body;

callbacks must return 0 on success. returning a non-zero value indicates
error to the parser, making it exit immediately.

in case you parse http message in chunks (i.e. `read()` request line
from socket, parse, read half headers, parse, etc) your data callbacks
may be called more than once. http-parser guarantees that data pointer is only
valid for the lifetime of callback. you can also `read()` into a heap allocated
buffer to avoid copying memory around if this fits your application.

reading headers may be a tricky task if you read/parse headers partially.
basically, you need to remember whether last header callback was field or value
and apply following logic:

    (on_header_field and on_header_value shortened to on_h_*)
     ------------------------ ------------ --------------------------------------------
    | state (prev. callback) | callback   | description/action                         |
     ------------------------ ------------ --------------------------------------------
    | nothing (first call)   | on_h_field | allocate new buffer and copy callback data |
    |                        |            | into it                                    |
     ------------------------ ------------ --------------------------------------------
    | value                  | on_h_field | new header started.                        |
    |                        |            | copy current name,value buffers to headers |
    |                        |            | list and allocate new buffer for new name  |
     ------------------------ ------------ --------------------------------------------
    | field                  | on_h_field | previous name continues. reallocate name   |
    |                        |            | buffer and append callback data to it      |
     ------------------------ ------------ --------------------------------------------
    | field                  | on_h_value | value for current header started. allocate |
    |                        |            | new buffer and copy callback data to it    |
     ------------------------ ------------ --------------------------------------------
    | value                  | on_h_value | value continues. reallocate value buffer   |
    |                        |            | and append callback data to it             |
     ------------------------ ------------ --------------------------------------------


parsing urls
------------

a simplistic zero-copy url parser is provided as `http_parser_parse_url()`.
users of this library may wish to use it to parse urls constructed from
consecutive `on_url` callbacks.

see examples of reading in headers:

* [partial example](http://gist.github.com/155877) in c
* [from http-parser tests](http://github.com/joyent/http-parser/blob/37a0ff8/test.c#l403) in c
* [from node library](http://github.com/joyent/node/blob/842eaf4/src/http.js#l284) in javascript
