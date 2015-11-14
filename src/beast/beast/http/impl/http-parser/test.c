/* copyright joyent, inc. and other node contributors. all rights reserved.
 *
 * permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "software"), to
 * deal in the software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the software, and to permit persons to whom the software is
 * furnished to do so, subject to the following conditions:
 *
 * the above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the software.
 *
 * the software is provided "as is", without warranty of any kind, express or
 * implied, including but not limited to the warranties of merchantability,
 * fitness for a particular purpose and noninfringement. in no event shall the
 * authors or copyright holders be liable for any claim, damages or other
 * liability, whether in an action of contract, tort or otherwise, arising
 * from, out of or in connection with the software or the use or other dealings
 * in the software.
 */
#include "http_parser.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h> /* rand */
#include <string.h>
#include <stdarg.h>

#if defined(__apple__)
# undef strlcat
# undef strlncpy
# undef strlcpy
#endif  /* defined(__apple__) */

#undef true
#define true 1
#undef false
#define false 0

#define max_headers 13
#define max_element_size 2048

#define min(a,b) ((a) < (b) ? (a) : (b))

static http_parser *parser;

struct message {
  const char *name; // for debugging purposes
  const char *raw;
  enum http_parser_type type;
  enum http_method method;
  int status_code;
  char response_status[max_element_size];
  char request_path[max_element_size];
  char request_url[max_element_size];
  char fragment[max_element_size];
  char query_string[max_element_size];
  char body[max_element_size];
  size_t body_size;
  const char *host;
  const char *userinfo;
  uint16_t port;
  int num_headers;
  enum { none=0, field, value } last_header_element;
  char headers [max_headers][2][max_element_size];
  int should_keep_alive;

  const char *upgrade; // upgraded body

  unsigned short http_major;
  unsigned short http_minor;

  int message_begin_cb_called;
  int headers_complete_cb_called;
  int message_complete_cb_called;
  int message_complete_on_eof;
  int body_is_final;
};

static int currently_parsing_eof;

static struct message messages[5];
static int num_messages;
static http_parser_settings *current_pause_parser;

/* * r e q u e s t s * */
const struct message requests[] =
#define curl_get 0
{ {.name= "curl get"
  ,.type= http_request
  ,.raw= "get /test http/1.1\r\n"
         "user-agent: curl/7.18.0 (i486-pc-linux-gnu) libcurl/7.18.0 openssl/0.9.8g zlib/1.2.3.3 libidn/1.1\r\n"
         "host: 0.0.0.0=5000\r\n"
         "accept: */*\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_get
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/test"
  ,.request_url= "/test"
  ,.num_headers= 3
  ,.headers=
    { { "user-agent", "curl/7.18.0 (i486-pc-linux-gnu) libcurl/7.18.0 openssl/0.9.8g zlib/1.2.3.3 libidn/1.1" }
    , { "host", "0.0.0.0=5000" }
    , { "accept", "*/*" }
    }
  ,.body= ""
  }

#define firefox_get 1
, {.name= "firefox get"
  ,.type= http_request
  ,.raw= "get /favicon.ico http/1.1\r\n"
         "host: 0.0.0.0=5000\r\n"
         "user-agent: mozilla/5.0 (x11; u; linux i686; en-us; rv:1.9) gecko/2008061015 firefox/3.0\r\n"
         "accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
         "accept-language: en-us,en;q=0.5\r\n"
         "accept-encoding: gzip,deflate\r\n"
         "accept-charset: iso-8859-1,utf-8;q=0.7,*;q=0.7\r\n"
         "keep-alive: 300\r\n"
         "connection: keep-alive\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_get
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/favicon.ico"
  ,.request_url= "/favicon.ico"
  ,.num_headers= 8
  ,.headers=
    { { "host", "0.0.0.0=5000" }
    , { "user-agent", "mozilla/5.0 (x11; u; linux i686; en-us; rv:1.9) gecko/2008061015 firefox/3.0" }
    , { "accept", "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8" }
    , { "accept-language", "en-us,en;q=0.5" }
    , { "accept-encoding", "gzip,deflate" }
    , { "accept-charset", "iso-8859-1,utf-8;q=0.7,*;q=0.7" }
    , { "keep-alive", "300" }
    , { "connection", "keep-alive" }
    }
  ,.body= ""
  }

#define dumbfuck 2
, {.name= "dumbfuck"
  ,.type= http_request
  ,.raw= "get /dumbfuck http/1.1\r\n"
         "aaaaaaaaaaaaa:++++++++++\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_get
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/dumbfuck"
  ,.request_url= "/dumbfuck"
  ,.num_headers= 1
  ,.headers=
    { { "aaaaaaaaaaaaa",  "++++++++++" }
    }
  ,.body= ""
  }

#define fragment_in_uri 3
, {.name= "fragment in url"
  ,.type= http_request
  ,.raw= "get /forums/1/topics/2375?page=1#posts-17408 http/1.1\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_get
  ,.query_string= "page=1"
  ,.fragment= "posts-17408"
  ,.request_path= "/forums/1/topics/2375"
  /* xxx request url does include fragment? */
  ,.request_url= "/forums/1/topics/2375?page=1#posts-17408"
  ,.num_headers= 0
  ,.body= ""
  }

#define get_no_headers_no_body 4
, {.name= "get no headers no body"
  ,.type= http_request
  ,.raw= "get /get_no_headers_no_body/world http/1.1\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false /* would need connection: close */
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_get
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/get_no_headers_no_body/world"
  ,.request_url= "/get_no_headers_no_body/world"
  ,.num_headers= 0
  ,.body= ""
  }

#define get_one_header_no_body 5
, {.name= "get one header no body"
  ,.type= http_request
  ,.raw= "get /get_one_header_no_body http/1.1\r\n"
         "accept: */*\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false /* would need connection: close */
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_get
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/get_one_header_no_body"
  ,.request_url= "/get_one_header_no_body"
  ,.num_headers= 1
  ,.headers=
    { { "accept" , "*/*" }
    }
  ,.body= ""
  }

#define get_funky_content_length 6
, {.name= "get funky content length body hello"
  ,.type= http_request
  ,.raw= "get /get_funky_content_length_body_hello http/1.0\r\n"
         "content-length: 5\r\n"
         "\r\n"
         "hello"
  ,.should_keep_alive= false
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 0
  ,.method= http_get
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/get_funky_content_length_body_hello"
  ,.request_url= "/get_funky_content_length_body_hello"
  ,.num_headers= 1
  ,.headers=
    { { "content-length" , "5" }
    }
  ,.body= "hello"
  }

#define post_identity_body_world 7
, {.name= "post identity body world"
  ,.type= http_request
  ,.raw= "post /post_identity_body_world?q=search#hey http/1.1\r\n"
         "accept: */*\r\n"
         "transfer-encoding: identity\r\n"
         "content-length: 5\r\n"
         "\r\n"
         "world"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_post
  ,.query_string= "q=search"
  ,.fragment= "hey"
  ,.request_path= "/post_identity_body_world"
  ,.request_url= "/post_identity_body_world?q=search#hey"
  ,.num_headers= 3
  ,.headers=
    { { "accept", "*/*" }
    , { "transfer-encoding", "identity" }
    , { "content-length", "5" }
    }
  ,.body= "world"
  }

#define post_chunked_all_your_base 8
, {.name= "post - chunked body: all your base are belong to us"
  ,.type= http_request
  ,.raw= "post /post_chunked_all_your_base http/1.1\r\n"
         "transfer-encoding: chunked\r\n"
         "\r\n"
         "1e\r\nall your base are belong to us\r\n"
         "0\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_post
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/post_chunked_all_your_base"
  ,.request_url= "/post_chunked_all_your_base"
  ,.num_headers= 1
  ,.headers=
    { { "transfer-encoding" , "chunked" }
    }
  ,.body= "all your base are belong to us"
  }

#define two_chunks_mult_zero_end 9
, {.name= "two chunks ; triple zero ending"
  ,.type= http_request
  ,.raw= "post /two_chunks_mult_zero_end http/1.1\r\n"
         "transfer-encoding: chunked\r\n"
         "\r\n"
         "5\r\nhello\r\n"
         "6\r\n world\r\n"
         "000\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_post
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/two_chunks_mult_zero_end"
  ,.request_url= "/two_chunks_mult_zero_end"
  ,.num_headers= 1
  ,.headers=
    { { "transfer-encoding", "chunked" }
    }
  ,.body= "hello world"
  }

#define chunked_w_trailing_headers 10
, {.name= "chunked with trailing headers. blech."
  ,.type= http_request
  ,.raw= "post /chunked_w_trailing_headers http/1.1\r\n"
         "transfer-encoding: chunked\r\n"
         "\r\n"
         "5\r\nhello\r\n"
         "6\r\n world\r\n"
         "0\r\n"
         "vary: *\r\n"
         "content-type: text/plain\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_post
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/chunked_w_trailing_headers"
  ,.request_url= "/chunked_w_trailing_headers"
  ,.num_headers= 3
  ,.headers=
    { { "transfer-encoding",  "chunked" }
    , { "vary", "*" }
    , { "content-type", "text/plain" }
    }
  ,.body= "hello world"
  }

#define chunked_w_bullshit_after_length 11
, {.name= "with bullshit after the length"
  ,.type= http_request
  ,.raw= "post /chunked_w_bullshit_after_length http/1.1\r\n"
         "transfer-encoding: chunked\r\n"
         "\r\n"
         "5; ihatew3;whatthefuck=aretheseparametersfor\r\nhello\r\n"
         "6; blahblah; blah\r\n world\r\n"
         "0\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_post
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/chunked_w_bullshit_after_length"
  ,.request_url= "/chunked_w_bullshit_after_length"
  ,.num_headers= 1
  ,.headers=
    { { "transfer-encoding", "chunked" }
    }
  ,.body= "hello world"
  }

#define with_quotes 12
, {.name= "with quotes"
  ,.type= http_request
  ,.raw= "get /with_\"stupid\"_quotes?foo=\"bar\" http/1.1\r\n\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_get
  ,.query_string= "foo=\"bar\""
  ,.fragment= ""
  ,.request_path= "/with_\"stupid\"_quotes"
  ,.request_url= "/with_\"stupid\"_quotes?foo=\"bar\""
  ,.num_headers= 0
  ,.headers= { }
  ,.body= ""
  }

#define apachebench_get 13
/* the server receiving this request should not wait for eof
 * to know that content-length == 0.
 * how to represent this in a unit test? message_complete_on_eof
 * compare with no_content_length_response.
 */
, {.name = "apachebench get"
  ,.type= http_request
  ,.raw= "get /test http/1.0\r\n"
         "host: 0.0.0.0:5000\r\n"
         "user-agent: apachebench/2.3\r\n"
         "accept: */*\r\n\r\n"
  ,.should_keep_alive= false
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 0
  ,.method= http_get
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/test"
  ,.request_url= "/test"
  ,.num_headers= 3
  ,.headers= { { "host", "0.0.0.0:5000" }
             , { "user-agent", "apachebench/2.3" }
             , { "accept", "*/*" }
             }
  ,.body= ""
  }

#define query_url_with_question_mark_get 14
/* some clients include '?' characters in query strings.
 */
, {.name = "query url with question mark"
  ,.type= http_request
  ,.raw= "get /test.cgi?foo=bar?baz http/1.1\r\n\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_get
  ,.query_string= "foo=bar?baz"
  ,.fragment= ""
  ,.request_path= "/test.cgi"
  ,.request_url= "/test.cgi?foo=bar?baz"
  ,.num_headers= 0
  ,.headers= {}
  ,.body= ""
  }

#define prefix_newline_get 15
/* some clients, especially after a post in a keep-alive connection,
 * will send an extra crlf before the next request
 */
, {.name = "newline prefix get"
  ,.type= http_request
  ,.raw= "\r\nget /test http/1.1\r\n\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_get
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/test"
  ,.request_url= "/test"
  ,.num_headers= 0
  ,.headers= { }
  ,.body= ""
  }

#define upgrade_request 16
, {.name = "upgrade request"
  ,.type= http_request
  ,.raw= "get /demo http/1.1\r\n"
         "host: example.com\r\n"
         "connection: upgrade\r\n"
         "sec-websocket-key2: 12998 5 y3 1  .p00\r\n"
         "sec-websocket-protocol: sample\r\n"
         "upgrade: websocket\r\n"
         "sec-websocket-key1: 4 @1  46546xw%0l 1 5\r\n"
         "origin: http://example.com\r\n"
         "\r\n"
         "hot diggity dogg"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_get
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/demo"
  ,.request_url= "/demo"
  ,.num_headers= 7
  ,.upgrade="hot diggity dogg"
  ,.headers= { { "host", "example.com" }
             , { "connection", "upgrade" }
             , { "sec-websocket-key2", "12998 5 y3 1  .p00" }
             , { "sec-websocket-protocol", "sample" }
             , { "upgrade", "websocket" }
             , { "sec-websocket-key1", "4 @1  46546xw%0l 1 5" }
             , { "origin", "http://example.com" }
             }
  ,.body= ""
  }

#define connect_request 17
, {.name = "connect request"
  ,.type= http_request
  ,.raw= "connect 0-home0.netscape.com:443 http/1.0\r\n"
         "user-agent: mozilla/1.1n\r\n"
         "proxy-authorization: basic agvsbg86d29ybgq=\r\n"
         "\r\n"
         "some data\r\n"
         "and yet even more data"
  ,.should_keep_alive= false
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 0
  ,.method= http_connect
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= ""
  ,.request_url= "0-home0.netscape.com:443"
  ,.num_headers= 2
  ,.upgrade="some data\r\nand yet even more data"
  ,.headers= { { "user-agent", "mozilla/1.1n" }
             , { "proxy-authorization", "basic agvsbg86d29ybgq=" }
             }
  ,.body= ""
  }

#define report_req 18
, {.name= "report request"
  ,.type= http_request
  ,.raw= "report /test http/1.1\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_report
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/test"
  ,.request_url= "/test"
  ,.num_headers= 0
  ,.headers= {}
  ,.body= ""
  }

#define no_http_version 19
, {.name= "request with no http version"
  ,.type= http_request
  ,.raw= "get /\r\n"
         "\r\n"
  ,.should_keep_alive= false
  ,.message_complete_on_eof= false
  ,.http_major= 0
  ,.http_minor= 9
  ,.method= http_get
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/"
  ,.request_url= "/"
  ,.num_headers= 0
  ,.headers= {}
  ,.body= ""
  }

#define msearch_req 20
, {.name= "m-search request"
  ,.type= http_request
  ,.raw= "m-search * http/1.1\r\n"
         "host: 239.255.255.250:1900\r\n"
         "man: \"ssdp:discover\"\r\n"
         "st: \"ssdp:all\"\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_msearch
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "*"
  ,.request_url= "*"
  ,.num_headers= 3
  ,.headers= { { "host", "239.255.255.250:1900" }
             , { "man", "\"ssdp:discover\"" }
             , { "st", "\"ssdp:all\"" }
             }
  ,.body= ""
  }

#define line_folding_in_header 21
, {.name= "line folding in header value"
  ,.type= http_request
  ,.raw= "get / http/1.1\r\n"
         "line1:   abc\r\n"
         "\tdef\r\n"
         " ghi\r\n"
         "\t\tjkl\r\n"
         "  mno \r\n"
         "\t \tqrs\r\n"
         "line2: \t line2\t\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_get
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/"
  ,.request_url= "/"
  ,.num_headers= 2
  ,.headers= { { "line1", "abcdefghijklmno qrs" }
             , { "line2", "line2\t" }
             }
  ,.body= ""
  }


#define query_terminated_host 22
, {.name= "host terminated by a query string"
  ,.type= http_request
  ,.raw= "get http://hypnotoad.org?hail=all http/1.1\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_get
  ,.query_string= "hail=all"
  ,.fragment= ""
  ,.request_path= ""
  ,.request_url= "http://hypnotoad.org?hail=all"
  ,.host= "hypnotoad.org"
  ,.num_headers= 0
  ,.headers= { }
  ,.body= ""
  }

#define query_terminated_hostport 23
, {.name= "host:port terminated by a query string"
  ,.type= http_request
  ,.raw= "get http://hypnotoad.org:1234?hail=all http/1.1\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_get
  ,.query_string= "hail=all"
  ,.fragment= ""
  ,.request_path= ""
  ,.request_url= "http://hypnotoad.org:1234?hail=all"
  ,.host= "hypnotoad.org"
  ,.port= 1234
  ,.num_headers= 0
  ,.headers= { }
  ,.body= ""
  }

#define space_terminated_hostport 24
, {.name= "host:port terminated by a space"
  ,.type= http_request
  ,.raw= "get http://hypnotoad.org:1234 http/1.1\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_get
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= ""
  ,.request_url= "http://hypnotoad.org:1234"
  ,.host= "hypnotoad.org"
  ,.port= 1234
  ,.num_headers= 0
  ,.headers= { }
  ,.body= ""
  }

#define patch_req 25
, {.name = "patch request"
  ,.type= http_request
  ,.raw= "patch /file.txt http/1.1\r\n"
         "host: www.example.com\r\n"
         "content-type: application/example\r\n"
         "if-match: \"e0023aa4e\"\r\n"
         "content-length: 10\r\n"
         "\r\n"
         "cccccccccc"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_patch
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/file.txt"
  ,.request_url= "/file.txt"
  ,.num_headers= 4
  ,.headers= { { "host", "www.example.com" }
             , { "content-type", "application/example" }
             , { "if-match", "\"e0023aa4e\"" }
             , { "content-length", "10" }
             }
  ,.body= "cccccccccc"
  }

#define connect_caps_request 26
, {.name = "connect caps request"
  ,.type= http_request
  ,.raw= "connect home0.netscape.com:443 http/1.0\r\n"
         "user-agent: mozilla/1.1n\r\n"
         "proxy-authorization: basic agvsbg86d29ybgq=\r\n"
         "\r\n"
  ,.should_keep_alive= false
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 0
  ,.method= http_connect
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= ""
  ,.request_url= "home0.netscape.com:443"
  ,.num_headers= 2
  ,.upgrade=""
  ,.headers= { { "user-agent", "mozilla/1.1n" }
             , { "proxy-authorization", "basic agvsbg86d29ybgq=" }
             }
  ,.body= ""
  }

#if !http_parser_strict
#define utf8_path_req 27
, {.name= "utf-8 path request"
  ,.type= http_request
  ,.raw= "get /未露/未t/pope?q=1#narf http/1.1\r\n"
         "host: github.com\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_get
  ,.query_string= "q=1"
  ,.fragment= "narf"
  ,.request_path= "/未露/未t/pope"
  ,.request_url= "/未露/未t/pope?q=1#narf"
  ,.num_headers= 1
  ,.headers= { {"host", "github.com" }
             }
  ,.body= ""
  }

#define hostname_underscore 28
, {.name = "hostname underscore"
  ,.type= http_request
  ,.raw= "connect home_0.netscape.com:443 http/1.0\r\n"
         "user-agent: mozilla/1.1n\r\n"
         "proxy-authorization: basic agvsbg86d29ybgq=\r\n"
         "\r\n"
  ,.should_keep_alive= false
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 0
  ,.method= http_connect
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= ""
  ,.request_url= "home_0.netscape.com:443"
  ,.num_headers= 2
  ,.upgrade=""
  ,.headers= { { "user-agent", "mozilla/1.1n" }
             , { "proxy-authorization", "basic agvsbg86d29ybgq=" }
             }
  ,.body= ""
  }
#endif  /* !http_parser_strict */

/* see https://github.com/ry/http-parser/issues/47 */
#define eat_trailing_crlf_no_connection_close 29
, {.name = "eat crlf between requests, no \"connection: close\" header"
  ,.raw= "post / http/1.1\r\n"
         "host: www.example.com\r\n"
         "content-type: application/x-www-form-urlencoded\r\n"
         "content-length: 4\r\n"
         "\r\n"
         "q=42\r\n" /* note the trailing crlf */
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_post
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/"
  ,.request_url= "/"
  ,.num_headers= 3
  ,.upgrade= 0
  ,.headers= { { "host", "www.example.com" }
             , { "content-type", "application/x-www-form-urlencoded" }
             , { "content-length", "4" }
             }
  ,.body= "q=42"
  }

/* see https://github.com/ry/http-parser/issues/47 */
#define eat_trailing_crlf_with_connection_close 30
, {.name = "eat crlf between requests even if \"connection: close\" is set"
  ,.raw= "post / http/1.1\r\n"
         "host: www.example.com\r\n"
         "content-type: application/x-www-form-urlencoded\r\n"
         "content-length: 4\r\n"
         "connection: close\r\n"
         "\r\n"
         "q=42\r\n" /* note the trailing crlf */
  ,.should_keep_alive= false
  ,.message_complete_on_eof= false /* input buffer isn't empty when on_message_complete is called */
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_post
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/"
  ,.request_url= "/"
  ,.num_headers= 4
  ,.upgrade= 0
  ,.headers= { { "host", "www.example.com" }
             , { "content-type", "application/x-www-form-urlencoded" }
             , { "content-length", "4" }
             , { "connection", "close" }
             }
  ,.body= "q=42"
  }

#define purge_req 31
, {.name = "purge request"
  ,.type= http_request
  ,.raw= "purge /file.txt http/1.1\r\n"
         "host: www.example.com\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_purge
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/file.txt"
  ,.request_url= "/file.txt"
  ,.num_headers= 1
  ,.headers= { { "host", "www.example.com" } }
  ,.body= ""
  }

#define search_req 32
, {.name = "search request"
  ,.type= http_request
  ,.raw= "search / http/1.1\r\n"
         "host: www.example.com\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_search
  ,.query_string= ""
  ,.fragment= ""
  ,.request_path= "/"
  ,.request_url= "/"
  ,.num_headers= 1
  ,.headers= { { "host", "www.example.com" } }
  ,.body= ""
  }

#define proxy_with_basic_auth 33
, {.name= "host:port and basic_auth"
  ,.type= http_request
  ,.raw= "get http://a%12:b!&*$@hypnotoad.org:1234/toto http/1.1\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.method= http_get
  ,.fragment= ""
  ,.request_path= "/toto"
  ,.request_url= "http://a%12:b!&*$@hypnotoad.org:1234/toto"
  ,.host= "hypnotoad.org"
  ,.userinfo= "a%12:b!&*$"
  ,.port= 1234
  ,.num_headers= 0
  ,.headers= { }
  ,.body= ""
  }


, {.name= null } /* sentinel */
};

/* * r e s p o n s e s * */
const struct message responses[] =
#define google_301 0
{ {.name= "google 301"
  ,.type= http_response
  ,.raw= "http/1.1 301 moved permanently\r\n"
         "location: http://www.google.com/\r\n"
         "content-type: text/html; charset=utf-8\r\n"
         "date: sun, 26 apr 2009 11:11:49 gmt\r\n"
         "expires: tue, 26 may 2009 11:11:49 gmt\r\n"
         "x-$prototypebi-version: 1.6.0.3\r\n" /* $ char in header field */
         "cache-control: public, max-age=2592000\r\n"
         "server: gws\r\n"
         "content-length:  219  \r\n"
         "\r\n"
         "<html><head><meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\">\n"
         "<title>301 moved</title></head><body>\n"
         "<h1>301 moved</h1>\n"
         "the document has moved\n"
         "<a href=\"http://www.google.com/\">here</a>.\r\n"
         "</body></html>\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 301
  ,.response_status= "moved permanently"
  ,.num_headers= 8
  ,.headers=
    { { "location", "http://www.google.com/" }
    , { "content-type", "text/html; charset=utf-8" }
    , { "date", "sun, 26 apr 2009 11:11:49 gmt" }
    , { "expires", "tue, 26 may 2009 11:11:49 gmt" }
    , { "x-$prototypebi-version", "1.6.0.3" }
    , { "cache-control", "public, max-age=2592000" }
    , { "server", "gws" }
    , { "content-length", "219  " }
    }
  ,.body= "<html><head><meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\">\n"
          "<title>301 moved</title></head><body>\n"
          "<h1>301 moved</h1>\n"
          "the document has moved\n"
          "<a href=\"http://www.google.com/\">here</a>.\r\n"
          "</body></html>\r\n"
  }

#define no_content_length_response 1
/* the client should wait for the server's eof. that is, when content-length
 * is not specified, and "connection: close", the end of body is specified
 * by the eof.
 * compare with apachebench_get
 */
, {.name= "no content-length response"
  ,.type= http_response
  ,.raw= "http/1.1 200 ok\r\n"
         "date: tue, 04 aug 2009 07:59:32 gmt\r\n"
         "server: apache\r\n"
         "x-powered-by: servlet/2.5 jsp/2.1\r\n"
         "content-type: text/xml; charset=utf-8\r\n"
         "connection: close\r\n"
         "\r\n"
         "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
         "<soap-env:envelope xmlns:soap-env=\"http://schemas.xmlsoap.org/soap/envelope/\">\n"
         "  <soap-env:body>\n"
         "    <soap-env:fault>\n"
         "       <faultcode>soap-env:client</faultcode>\n"
         "       <faultstring>client error</faultstring>\n"
         "    </soap-env:fault>\n"
         "  </soap-env:body>\n"
         "</soap-env:envelope>"
  ,.should_keep_alive= false
  ,.message_complete_on_eof= true
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 200
  ,.response_status= "ok"
  ,.num_headers= 5
  ,.headers=
    { { "date", "tue, 04 aug 2009 07:59:32 gmt" }
    , { "server", "apache" }
    , { "x-powered-by", "servlet/2.5 jsp/2.1" }
    , { "content-type", "text/xml; charset=utf-8" }
    , { "connection", "close" }
    }
  ,.body= "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
          "<soap-env:envelope xmlns:soap-env=\"http://schemas.xmlsoap.org/soap/envelope/\">\n"
          "  <soap-env:body>\n"
          "    <soap-env:fault>\n"
          "       <faultcode>soap-env:client</faultcode>\n"
          "       <faultstring>client error</faultstring>\n"
          "    </soap-env:fault>\n"
          "  </soap-env:body>\n"
          "</soap-env:envelope>"
  }

#define no_headers_no_body_404 2
, {.name= "404 no headers no body"
  ,.type= http_response
  ,.raw= "http/1.1 404 not found\r\n\r\n"
  ,.should_keep_alive= false
  ,.message_complete_on_eof= true
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 404
  ,.response_status= "not found"
  ,.num_headers= 0
  ,.headers= {}
  ,.body_size= 0
  ,.body= ""
  }

#define no_reason_phrase 3
, {.name= "301 no response phrase"
  ,.type= http_response
  ,.raw= "http/1.1 301\r\n\r\n"
  ,.should_keep_alive = false
  ,.message_complete_on_eof= true
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 301
  ,.response_status= ""
  ,.num_headers= 0
  ,.headers= {}
  ,.body= ""
  }

#define trailing_space_on_chunked_body 4
, {.name="200 trailing space on chunked body"
  ,.type= http_response
  ,.raw= "http/1.1 200 ok\r\n"
         "content-type: text/plain\r\n"
         "transfer-encoding: chunked\r\n"
         "\r\n"
         "25  \r\n"
         "this is the data in the first chunk\r\n"
         "\r\n"
         "1c\r\n"
         "and this is the second one\r\n"
         "\r\n"
         "0  \r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 200
  ,.response_status= "ok"
  ,.num_headers= 2
  ,.headers=
    { {"content-type", "text/plain" }
    , {"transfer-encoding", "chunked" }
    }
  ,.body_size = 37+28
  ,.body =
         "this is the data in the first chunk\r\n"
         "and this is the second one\r\n"

  }

#define no_carriage_ret 5
, {.name="no carriage ret"
  ,.type= http_response
  ,.raw= "http/1.1 200 ok\n"
         "content-type: text/html; charset=utf-8\n"
         "connection: close\n"
         "\n"
         "these headers are from http://news.ycombinator.com/"
  ,.should_keep_alive= false
  ,.message_complete_on_eof= true
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 200
  ,.response_status= "ok"
  ,.num_headers= 2
  ,.headers=
    { {"content-type", "text/html; charset=utf-8" }
    , {"connection", "close" }
    }
  ,.body= "these headers are from http://news.ycombinator.com/"
  }

#define proxy_connection 6
, {.name="proxy connection"
  ,.type= http_response
  ,.raw= "http/1.1 200 ok\r\n"
         "content-type: text/html; charset=utf-8\r\n"
         "content-length: 11\r\n"
         "proxy-connection: close\r\n"
         "date: thu, 31 dec 2009 20:55:48 +0000\r\n"
         "\r\n"
         "hello world"
  ,.should_keep_alive= false
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 200
  ,.response_status= "ok"
  ,.num_headers= 4
  ,.headers=
    { {"content-type", "text/html; charset=utf-8" }
    , {"content-length", "11" }
    , {"proxy-connection", "close" }
    , {"date", "thu, 31 dec 2009 20:55:48 +0000"}
    }
  ,.body= "hello world"
  }

#define understore_header_key 7
  // shown by
  // curl -o /dev/null -v "http://ad.doubleclick.net/pfadx/dartshellconfigxml;dcmt=text/xml;"
, {.name="underscore header key"
  ,.type= http_response
  ,.raw= "http/1.1 200 ok\r\n"
         "server: dclk-adsvr\r\n"
         "content-type: text/xml\r\n"
         "content-length: 0\r\n"
         "dclk_imp: v7;x;114750856;0-0;0;17820020;0/0;21603567/21621457/1;;~okv=;dcmt=text/xml;;~cs=o\r\n\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 200
  ,.response_status= "ok"
  ,.num_headers= 4
  ,.headers=
    { {"server", "dclk-adsvr" }
    , {"content-type", "text/xml" }
    , {"content-length", "0" }
    , {"dclk_imp", "v7;x;114750856;0-0;0;17820020;0/0;21603567/21621457/1;;~okv=;dcmt=text/xml;;~cs=o" }
    }
  ,.body= ""
  }

#define bonjour_madame_fr 8
/* the client should not merge two headers fields when the first one doesn't
 * have a value.
 */
, {.name= "bonjourmadame.fr"
  ,.type= http_response
  ,.raw= "http/1.0 301 moved permanently\r\n"
         "date: thu, 03 jun 2010 09:56:32 gmt\r\n"
         "server: apache/2.2.3 (red hat)\r\n"
         "cache-control: public\r\n"
         "pragma: \r\n"
         "location: http://www.bonjourmadame.fr/\r\n"
         "vary: accept-encoding\r\n"
         "content-length: 0\r\n"
         "content-type: text/html; charset=utf-8\r\n"
         "connection: keep-alive\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 0
  ,.status_code= 301
  ,.response_status= "moved permanently"
  ,.num_headers= 9
  ,.headers=
    { { "date", "thu, 03 jun 2010 09:56:32 gmt" }
    , { "server", "apache/2.2.3 (red hat)" }
    , { "cache-control", "public" }
    , { "pragma", "" }
    , { "location", "http://www.bonjourmadame.fr/" }
    , { "vary",  "accept-encoding" }
    , { "content-length", "0" }
    , { "content-type", "text/html; charset=utf-8" }
    , { "connection", "keep-alive" }
    }
  ,.body= ""
  }

#define res_field_underscore 9
/* should handle spaces in header fields */
, {.name= "field underscore"
  ,.type= http_response
  ,.raw= "http/1.1 200 ok\r\n"
         "date: tue, 28 sep 2010 01:14:13 gmt\r\n"
         "server: apache\r\n"
         "cache-control: no-cache, must-revalidate\r\n"
         "expires: mon, 26 jul 1997 05:00:00 gmt\r\n"
         ".et-cookie: plaxocs=1274804622353690521; path=/; domain=.plaxo.com\r\n"
         "vary: accept-encoding\r\n"
         "_eep-alive: timeout=45\r\n" /* semantic value ignored */
         "_onnection: keep-alive\r\n" /* semantic value ignored */
         "transfer-encoding: chunked\r\n"
         "content-type: text/html\r\n"
         "connection: close\r\n"
         "\r\n"
         "0\r\n\r\n"
  ,.should_keep_alive= false
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 200
  ,.response_status= "ok"
  ,.num_headers= 11
  ,.headers=
    { { "date", "tue, 28 sep 2010 01:14:13 gmt" }
    , { "server", "apache" }
    , { "cache-control", "no-cache, must-revalidate" }
    , { "expires", "mon, 26 jul 1997 05:00:00 gmt" }
    , { ".et-cookie", "plaxocs=1274804622353690521; path=/; domain=.plaxo.com" }
    , { "vary", "accept-encoding" }
    , { "_eep-alive", "timeout=45" }
    , { "_onnection", "keep-alive" }
    , { "transfer-encoding", "chunked" }
    , { "content-type", "text/html" }
    , { "connection", "close" }
    }
  ,.body= ""
  }

#define non_ascii_in_status_line 10
/* should handle non-ascii in status line */
, {.name= "non-ascii in status line"
  ,.type= http_response
  ,.raw= "http/1.1 500 ori毛ntatieprobleem\r\n"
         "date: fri, 5 nov 2010 23:07:12 gmt+2\r\n"
         "content-length: 0\r\n"
         "connection: close\r\n"
         "\r\n"
  ,.should_keep_alive= false
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 500
  ,.response_status= "ori毛ntatieprobleem"
  ,.num_headers= 3
  ,.headers=
    { { "date", "fri, 5 nov 2010 23:07:12 gmt+2" }
    , { "content-length", "0" }
    , { "connection", "close" }
    }
  ,.body= ""
  }

#define http_version_0_9 11
/* should handle http/0.9 */
, {.name= "http version 0.9"
  ,.type= http_response
  ,.raw= "http/0.9 200 ok\r\n"
         "\r\n"
  ,.should_keep_alive= false
  ,.message_complete_on_eof= true
  ,.http_major= 0
  ,.http_minor= 9
  ,.status_code= 200
  ,.response_status= "ok"
  ,.num_headers= 0
  ,.headers=
    {}
  ,.body= ""
  }

#define no_content_length_no_transfer_encoding_response 12
/* the client should wait for the server's eof. that is, when neither
 * content-length nor transfer-encoding is specified, the end of body
 * is specified by the eof.
 */
, {.name= "neither content-length nor transfer-encoding response"
  ,.type= http_response
  ,.raw= "http/1.1 200 ok\r\n"
         "content-type: text/plain\r\n"
         "\r\n"
         "hello world"
  ,.should_keep_alive= false
  ,.message_complete_on_eof= true
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 200
  ,.response_status= "ok"
  ,.num_headers= 1
  ,.headers=
    { { "content-type", "text/plain" }
    }
  ,.body= "hello world"
  }

#define no_body_http10_ka_200 13
, {.name= "http/1.0 with keep-alive and eof-terminated 200 status"
  ,.type= http_response
  ,.raw= "http/1.0 200 ok\r\n"
         "connection: keep-alive\r\n"
         "\r\n"
  ,.should_keep_alive= false
  ,.message_complete_on_eof= true
  ,.http_major= 1
  ,.http_minor= 0
  ,.status_code= 200
  ,.response_status= "ok"
  ,.num_headers= 1
  ,.headers=
    { { "connection", "keep-alive" }
    }
  ,.body_size= 0
  ,.body= ""
  }

#define no_body_http10_ka_204 14
, {.name= "http/1.0 with keep-alive and a 204 status"
  ,.type= http_response
  ,.raw= "http/1.0 204 no content\r\n"
         "connection: keep-alive\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 0
  ,.status_code= 204
  ,.response_status= "no content"
  ,.num_headers= 1
  ,.headers=
    { { "connection", "keep-alive" }
    }
  ,.body_size= 0
  ,.body= ""
  }

#define no_body_http11_ka_200 15
, {.name= "http/1.1 with an eof-terminated 200 status"
  ,.type= http_response
  ,.raw= "http/1.1 200 ok\r\n"
         "\r\n"
  ,.should_keep_alive= false
  ,.message_complete_on_eof= true
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 200
  ,.response_status= "ok"
  ,.num_headers= 0
  ,.headers={}
  ,.body_size= 0
  ,.body= ""
  }

#define no_body_http11_ka_204 16
, {.name= "http/1.1 with a 204 status"
  ,.type= http_response
  ,.raw= "http/1.1 204 no content\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 204
  ,.response_status= "no content"
  ,.num_headers= 0
  ,.headers={}
  ,.body_size= 0
  ,.body= ""
  }

#define no_body_http11_noka_204 17
, {.name= "http/1.1 with a 204 status and keep-alive disabled"
  ,.type= http_response
  ,.raw= "http/1.1 204 no content\r\n"
         "connection: close\r\n"
         "\r\n"
  ,.should_keep_alive= false
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 204
  ,.response_status= "no content"
  ,.num_headers= 1
  ,.headers=
    { { "connection", "close" }
    }
  ,.body_size= 0
  ,.body= ""
  }

#define no_body_http11_ka_chunked_200 18
, {.name= "http/1.1 with chunked endocing and a 200 response"
  ,.type= http_response
  ,.raw= "http/1.1 200 ok\r\n"
         "transfer-encoding: chunked\r\n"
         "\r\n"
         "0\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 200
  ,.response_status= "ok"
  ,.num_headers= 1
  ,.headers=
    { { "transfer-encoding", "chunked" }
    }
  ,.body_size= 0
  ,.body= ""
  }

#if !http_parser_strict
#define space_in_field_res 19
/* should handle spaces in header fields */
, {.name= "field space"
  ,.type= http_response
  ,.raw= "http/1.1 200 ok\r\n"
         "server: microsoft-iis/6.0\r\n"
         "x-powered-by: asp.net\r\n"
         "en-us content-type: text/xml\r\n" /* this is the problem */
         "content-type: text/xml\r\n"
         "content-length: 16\r\n"
         "date: fri, 23 jul 2010 18:45:38 gmt\r\n"
         "connection: keep-alive\r\n"
         "\r\n"
         "<xml>hello</xml>" /* fake body */
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 200
  ,.response_status= "ok"
  ,.num_headers= 7
  ,.headers=
    { { "server",  "microsoft-iis/6.0" }
    , { "x-powered-by", "asp.net" }
    , { "en-us content-type", "text/xml" }
    , { "content-type", "text/xml" }
    , { "content-length", "16" }
    , { "date", "fri, 23 jul 2010 18:45:38 gmt" }
    , { "connection", "keep-alive" }
    }
  ,.body= "<xml>hello</xml>"
  }
#endif /* !http_parser_strict */

#define amazon_com 20
, {.name= "amazon.com"
  ,.type= http_response
  ,.raw= "http/1.1 301 movedpermanently\r\n"
         "date: wed, 15 may 2013 17:06:33 gmt\r\n"
         "server: server\r\n"
         "x-amz-id-1: 0gphkxsjq826rk7gzeb2\r\n"
         "p3p: policyref=\"http://www.amazon.com/w3c/p3p.xml\",cp=\"cao dsp law cur adm ivao ivdo cono otpo our deli pubi otri bus phy onl uni pur fin com nav int dem cnt sta hea pre loc gov otc \"\r\n"
         "x-amz-id-2: stn69vzxifsz9yjlbz1gdbxpbjg6qjmmq5e3dxrhouw+et0p4hr7c/q8qncx4oad\r\n"
         "location: http://www.amazon.com/dan-brown/e/b000ap9dsu/ref=s9_pop_gw_al1?_encoding=utf8&refinementid=618073011&pf_rd_m=atvpdkikx0der&pf_rd_s=center-2&pf_rd_r=0shyy5bzxn3kr20bnfay&pf_rd_t=101&pf_rd_p=1263340922&pf_rd_i=507846\r\n"
         "vary: accept-encoding,user-agent\r\n"
         "content-type: text/html; charset=iso-8859-1\r\n"
         "transfer-encoding: chunked\r\n"
         "\r\n"
         "1\r\n"
         "\n\r\n"
         "0\r\n"
         "\r\n"
  ,.should_keep_alive= true
  ,.message_complete_on_eof= false
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 301
  ,.response_status= "movedpermanently"
  ,.num_headers= 9
  ,.headers= { { "date", "wed, 15 may 2013 17:06:33 gmt" }
             , { "server", "server" }
             , { "x-amz-id-1", "0gphkxsjq826rk7gzeb2" }
             , { "p3p", "policyref=\"http://www.amazon.com/w3c/p3p.xml\",cp=\"cao dsp law cur adm ivao ivdo cono otpo our deli pubi otri bus phy onl uni pur fin com nav int dem cnt sta hea pre loc gov otc \"" }
             , { "x-amz-id-2", "stn69vzxifsz9yjlbz1gdbxpbjg6qjmmq5e3dxrhouw+et0p4hr7c/q8qncx4oad" }
             , { "location", "http://www.amazon.com/dan-brown/e/b000ap9dsu/ref=s9_pop_gw_al1?_encoding=utf8&refinementid=618073011&pf_rd_m=atvpdkikx0der&pf_rd_s=center-2&pf_rd_r=0shyy5bzxn3kr20bnfay&pf_rd_t=101&pf_rd_p=1263340922&pf_rd_i=507846" }
             , { "vary", "accept-encoding,user-agent" }
             , { "content-type", "text/html; charset=iso-8859-1" }
             , { "transfer-encoding", "chunked" }
             }
  ,.body= "\n"
  }

#define empty_reason_phrase_after_space 20
, {.name= "empty reason phrase after space"
  ,.type= http_response
  ,.raw= "http/1.1 200 \r\n"
         "\r\n"
  ,.should_keep_alive= false
  ,.message_complete_on_eof= true
  ,.http_major= 1
  ,.http_minor= 1
  ,.status_code= 200
  ,.response_status= ""
  ,.num_headers= 0
  ,.headers= {}
  ,.body= ""
  }

, {.name= null } /* sentinel */
};

/* strnlen() is a posix.2008 addition. can't rely on it being available so
 * define it ourselves.
 */
size_t
strnlen(const char *s, size_t maxlen)
{
  const char *p;

  p = memchr(s, '\0', maxlen);
  if (p == null)
    return maxlen;

  return p - s;
}

size_t
strlncat(char *dst, size_t len, const char *src, size_t n)
{
  size_t slen;
  size_t dlen;
  size_t rlen;
  size_t ncpy;

  slen = strnlen(src, n);
  dlen = strnlen(dst, len);

  if (dlen < len) {
    rlen = len - dlen;
    ncpy = slen < rlen ? slen : (rlen - 1);
    memcpy(dst + dlen, src, ncpy);
    dst[dlen + ncpy] = '\0';
  }

  assert(len > slen + dlen);
  return slen + dlen;
}

size_t
strlcat(char *dst, const char *src, size_t len)
{
  return strlncat(dst, len, src, (size_t) -1);
}

size_t
strlncpy(char *dst, size_t len, const char *src, size_t n)
{
  size_t slen;
  size_t ncpy;

  slen = strnlen(src, n);

  if (len > 0) {
    ncpy = slen < len ? slen : (len - 1);
    memcpy(dst, src, ncpy);
    dst[ncpy] = '\0';
  }

  assert(len > slen);
  return slen;
}

size_t
strlcpy(char *dst, const char *src, size_t len)
{
  return strlncpy(dst, len, src, (size_t) -1);
}

int
request_url_cb (http_parser *p, const char *buf, size_t len)
{
  assert(p == parser);
  strlncat(messages[num_messages].request_url,
           sizeof(messages[num_messages].request_url),
           buf,
           len);
  return 0;
}

int
header_field_cb (http_parser *p, const char *buf, size_t len)
{
  assert(p == parser);
  struct message *m = &messages[num_messages];

  if (m->last_header_element != field)
    m->num_headers++;

  strlncat(m->headers[m->num_headers-1][0],
           sizeof(m->headers[m->num_headers-1][0]),
           buf,
           len);

  m->last_header_element = field;

  return 0;
}

int
header_value_cb (http_parser *p, const char *buf, size_t len)
{
  assert(p == parser);
  struct message *m = &messages[num_messages];

  strlncat(m->headers[m->num_headers-1][1],
           sizeof(m->headers[m->num_headers-1][1]),
           buf,
           len);

  m->last_header_element = value;

  return 0;
}

void
check_body_is_final (const http_parser *p)
{
  if (messages[num_messages].body_is_final) {
    fprintf(stderr, "\n\n *** error http_body_is_final() should return 1 "
                    "on last on_body callback call "
                    "but it doesn't! ***\n\n");
    assert(0);
    abort();
  }
  messages[num_messages].body_is_final = http_body_is_final(p);
}

int
body_cb (http_parser *p, const char *buf, size_t len)
{
  assert(p == parser);
  strlncat(messages[num_messages].body,
           sizeof(messages[num_messages].body),
           buf,
           len);
  messages[num_messages].body_size += len;
  check_body_is_final(p);
 // printf("body_cb: '%s'\n", requests[num_messages].body);
  return 0;
}

int
count_body_cb (http_parser *p, const char *buf, size_t len)
{
  assert(p == parser);
  assert(buf);
  messages[num_messages].body_size += len;
  check_body_is_final(p);
  return 0;
}

int
message_begin_cb (http_parser *p)
{
  assert(p == parser);
  messages[num_messages].message_begin_cb_called = true;
  return 0;
}

int
headers_complete_cb (http_parser *p)
{
  assert(p == parser);
  messages[num_messages].method = parser->method;
  messages[num_messages].status_code = parser->status_code;
  messages[num_messages].http_major = parser->http_major;
  messages[num_messages].http_minor = parser->http_minor;
  messages[num_messages].headers_complete_cb_called = true;
  messages[num_messages].should_keep_alive = http_should_keep_alive(parser);
  return 0;
}

int
message_complete_cb (http_parser *p)
{
  assert(p == parser);
  if (messages[num_messages].should_keep_alive != http_should_keep_alive(parser))
  {
    fprintf(stderr, "\n\n *** error http_should_keep_alive() should have same "
                    "value in both on_message_complete and on_headers_complete "
                    "but it doesn't! ***\n\n");
    assert(0);
    abort();
  }

  if (messages[num_messages].body_size &&
      http_body_is_final(p) &&
      !messages[num_messages].body_is_final)
  {
    fprintf(stderr, "\n\n *** error http_body_is_final() should return 1 "
                    "on last on_body callback call "
                    "but it doesn't! ***\n\n");
    assert(0);
    abort();
  }

  messages[num_messages].message_complete_cb_called = true;

  messages[num_messages].message_complete_on_eof = currently_parsing_eof;

  num_messages++;
  return 0;
}

int
response_status_cb (http_parser *p, const char *buf, size_t len)
{
  assert(p == parser);
  strlncat(messages[num_messages].response_status,
           sizeof(messages[num_messages].response_status),
           buf,
           len);
  return 0;
}

/* these dontcall_* callbacks exist so that we can verify that when we're
 * paused, no additional callbacks are invoked */
int
dontcall_message_begin_cb (http_parser *p)
{
  if (p) { } // gcc
  fprintf(stderr, "\n\n*** on_message_begin() called on paused parser ***\n\n");
  abort();
}

int
dontcall_header_field_cb (http_parser *p, const char *buf, size_t len)
{
  if (p || buf || len) { } // gcc
  fprintf(stderr, "\n\n*** on_header_field() called on paused parser ***\n\n");
  abort();
}

int
dontcall_header_value_cb (http_parser *p, const char *buf, size_t len)
{
  if (p || buf || len) { } // gcc
  fprintf(stderr, "\n\n*** on_header_value() called on paused parser ***\n\n");
  abort();
}

int
dontcall_request_url_cb (http_parser *p, const char *buf, size_t len)
{
  if (p || buf || len) { } // gcc
  fprintf(stderr, "\n\n*** on_request_url() called on paused parser ***\n\n");
  abort();
}

int
dontcall_body_cb (http_parser *p, const char *buf, size_t len)
{
  if (p || buf || len) { } // gcc
  fprintf(stderr, "\n\n*** on_body_cb() called on paused parser ***\n\n");
  abort();
}

int
dontcall_headers_complete_cb (http_parser *p)
{
  if (p) { } // gcc
  fprintf(stderr, "\n\n*** on_headers_complete() called on paused "
                  "parser ***\n\n");
  abort();
}

int
dontcall_message_complete_cb (http_parser *p)
{
  if (p) { } // gcc
  fprintf(stderr, "\n\n*** on_message_complete() called on paused "
                  "parser ***\n\n");
  abort();
}

int
dontcall_response_status_cb (http_parser *p, const char *buf, size_t len)
{
  if (p || buf || len) { } // gcc
  fprintf(stderr, "\n\n*** on_status() called on paused parser ***\n\n");
  abort();
}

static http_parser_settings settings_dontcall =
  {.on_message_begin = dontcall_message_begin_cb
  ,.on_header_field = dontcall_header_field_cb
  ,.on_header_value = dontcall_header_value_cb
  ,.on_url = dontcall_request_url_cb
  ,.on_status = dontcall_response_status_cb
  ,.on_body = dontcall_body_cb
  ,.on_headers_complete = dontcall_headers_complete_cb
  ,.on_message_complete = dontcall_message_complete_cb
  };

/* these pause_* callbacks always pause the parser and just invoke the regular
 * callback that tracks content. before returning, we overwrite the parser
 * settings to point to the _dontcall variety so that we can verify that
 * the pause actually did, you know, pause. */
int
pause_message_begin_cb (http_parser *p)
{
  http_parser_pause(p, 1);
  *current_pause_parser = settings_dontcall;
  return message_begin_cb(p);
}

int
pause_header_field_cb (http_parser *p, const char *buf, size_t len)
{
  http_parser_pause(p, 1);
  *current_pause_parser = settings_dontcall;
  return header_field_cb(p, buf, len);
}

int
pause_header_value_cb (http_parser *p, const char *buf, size_t len)
{
  http_parser_pause(p, 1);
  *current_pause_parser = settings_dontcall;
  return header_value_cb(p, buf, len);
}

int
pause_request_url_cb (http_parser *p, const char *buf, size_t len)
{
  http_parser_pause(p, 1);
  *current_pause_parser = settings_dontcall;
  return request_url_cb(p, buf, len);
}

int
pause_body_cb (http_parser *p, const char *buf, size_t len)
{
  http_parser_pause(p, 1);
  *current_pause_parser = settings_dontcall;
  return body_cb(p, buf, len);
}

int
pause_headers_complete_cb (http_parser *p)
{
  http_parser_pause(p, 1);
  *current_pause_parser = settings_dontcall;
  return headers_complete_cb(p);
}

int
pause_message_complete_cb (http_parser *p)
{
  http_parser_pause(p, 1);
  *current_pause_parser = settings_dontcall;
  return message_complete_cb(p);
}

int
pause_response_status_cb (http_parser *p, const char *buf, size_t len)
{
  http_parser_pause(p, 1);
  *current_pause_parser = settings_dontcall;
  return response_status_cb(p, buf, len);
}

static http_parser_settings settings_pause =
  {.on_message_begin = pause_message_begin_cb
  ,.on_header_field = pause_header_field_cb
  ,.on_header_value = pause_header_value_cb
  ,.on_url = pause_request_url_cb
  ,.on_status = pause_response_status_cb
  ,.on_body = pause_body_cb
  ,.on_headers_complete = pause_headers_complete_cb
  ,.on_message_complete = pause_message_complete_cb
  };

static http_parser_settings settings =
  {.on_message_begin = message_begin_cb
  ,.on_header_field = header_field_cb
  ,.on_header_value = header_value_cb
  ,.on_url = request_url_cb
  ,.on_status = response_status_cb
  ,.on_body = body_cb
  ,.on_headers_complete = headers_complete_cb
  ,.on_message_complete = message_complete_cb
  };

static http_parser_settings settings_count_body =
  {.on_message_begin = message_begin_cb
  ,.on_header_field = header_field_cb
  ,.on_header_value = header_value_cb
  ,.on_url = request_url_cb
  ,.on_status = response_status_cb
  ,.on_body = count_body_cb
  ,.on_headers_complete = headers_complete_cb
  ,.on_message_complete = message_complete_cb
  };

static http_parser_settings settings_null =
  {.on_message_begin = 0
  ,.on_header_field = 0
  ,.on_header_value = 0
  ,.on_url = 0
  ,.on_status = 0
  ,.on_body = 0
  ,.on_headers_complete = 0
  ,.on_message_complete = 0
  };

void
parser_init (enum http_parser_type type)
{
  num_messages = 0;

  assert(parser == null);

  parser = malloc(sizeof(http_parser));

  http_parser_init(parser, type);

  memset(&messages, 0, sizeof messages);

}

void
parser_free ()
{
  assert(parser);
  free(parser);
  parser = null;
}

size_t parse (const char *buf, size_t len)
{
  size_t nparsed;
  currently_parsing_eof = (len == 0);
  nparsed = http_parser_execute(parser, &settings, buf, len);
  return nparsed;
}

size_t parse_count_body (const char *buf, size_t len)
{
  size_t nparsed;
  currently_parsing_eof = (len == 0);
  nparsed = http_parser_execute(parser, &settings_count_body, buf, len);
  return nparsed;
}

size_t parse_pause (const char *buf, size_t len)
{
  size_t nparsed;
  http_parser_settings s = settings_pause;

  currently_parsing_eof = (len == 0);
  current_pause_parser = &s;
  nparsed = http_parser_execute(parser, current_pause_parser, buf, len);
  return nparsed;
}

static inline int
check_str_eq (const struct message *m,
              const char *prop,
              const char *expected,
              const char *found) {
  if ((expected == null) != (found == null)) {
    printf("\n*** error: %s in '%s' ***\n\n", prop, m->name);
    printf("expected %s\n", (expected == null) ? "null" : expected);
    printf("   found %s\n", (found == null) ? "null" : found);
    return 0;
  }
  if (expected != null && 0 != strcmp(expected, found)) {
    printf("\n*** error: %s in '%s' ***\n\n", prop, m->name);
    printf("expected '%s'\n", expected);
    printf("   found '%s'\n", found);
    return 0;
  }
  return 1;
}

static inline int
check_num_eq (const struct message *m,
              const char *prop,
              int expected,
              int found) {
  if (expected != found) {
    printf("\n*** error: %s in '%s' ***\n\n", prop, m->name);
    printf("expected %d\n", expected);
    printf("   found %d\n", found);
    return 0;
  }
  return 1;
}

#define message_check_str_eq(expected, found, prop) \
  if (!check_str_eq(expected, #prop, expected->prop, found->prop)) return 0

#define message_check_num_eq(expected, found, prop) \
  if (!check_num_eq(expected, #prop, expected->prop, found->prop)) return 0

#define message_check_url_eq(u, expected, found, prop, fn)           \
do {                                                                 \
  char ubuf[256];                                                    \
                                                                     \
  if ((u)->field_set & (1 << (fn))) {                                \
    memcpy(ubuf, (found)->request_url + (u)->field_data[(fn)].off,   \
      (u)->field_data[(fn)].len);                                    \
    ubuf[(u)->field_data[(fn)].len] = '\0';                          \
  } else {                                                           \
    ubuf[0] = '\0';                                                  \
  }                                                                  \
                                                                     \
  check_str_eq(expected, #prop, expected->prop, ubuf);               \
} while(0)

int
message_eq (int index, const struct message *expected)
{
  int i;
  struct message *m = &messages[index];

  message_check_num_eq(expected, m, http_major);
  message_check_num_eq(expected, m, http_minor);

  if (expected->type == http_request) {
    message_check_num_eq(expected, m, method);
  } else {
    message_check_num_eq(expected, m, status_code);
    message_check_str_eq(expected, m, response_status);
  }

  message_check_num_eq(expected, m, should_keep_alive);
  message_check_num_eq(expected, m, message_complete_on_eof);

  assert(m->message_begin_cb_called);
  assert(m->headers_complete_cb_called);
  assert(m->message_complete_cb_called);


  message_check_str_eq(expected, m, request_url);

  /* check url components; we can't do this w/ connect since it doesn't
   * send us a well-formed url.
   */
  if (*m->request_url && m->method != http_connect) {
    struct http_parser_url u;

    if (http_parser_parse_url(m->request_url, strlen(m->request_url), 0, &u)) {
      fprintf(stderr, "\n\n*** failed to parse url %s ***\n\n",
        m->request_url);
      abort();
    }

    if (expected->host) {
      message_check_url_eq(&u, expected, m, host, uf_host);
    }

    if (expected->userinfo) {
      message_check_url_eq(&u, expected, m, userinfo, uf_userinfo);
    }

    m->port = (u.field_set & (1 << uf_port)) ?
      u.port : 0;

    message_check_url_eq(&u, expected, m, query_string, uf_query);
    message_check_url_eq(&u, expected, m, fragment, uf_fragment);
    message_check_url_eq(&u, expected, m, request_path, uf_path);
    message_check_num_eq(expected, m, port);
  }

  if (expected->body_size) {
    message_check_num_eq(expected, m, body_size);
  } else {
    message_check_str_eq(expected, m, body);
  }

  message_check_num_eq(expected, m, num_headers);

  int r;
  for (i = 0; i < m->num_headers; i++) {
    r = check_str_eq(expected, "header field", expected->headers[i][0], m->headers[i][0]);
    if (!r) return 0;
    r = check_str_eq(expected, "header value", expected->headers[i][1], m->headers[i][1]);
    if (!r) return 0;
  }

  message_check_str_eq(expected, m, upgrade);

  return 1;
}

/* given a sequence of varargs messages, return the number of them that the
 * parser should successfully parse, taking into account that upgraded
 * messages prevent all subsequent messages from being parsed.
 */
size_t
count_parsed_messages(const size_t nmsgs, ...) {
  size_t i;
  va_list ap;

  va_start(ap, nmsgs);

  for (i = 0; i < nmsgs; i++) {
    struct message *m = va_arg(ap, struct message *);

    if (m->upgrade) {
      va_end(ap);
      return i + 1;
    }
  }

  va_end(ap);
  return nmsgs;
}

/* given a sequence of bytes and the number of these that we were able to
 * parse, verify that upgrade bodies are correct.
 */
void
upgrade_message_fix(char *body, const size_t nread, const size_t nmsgs, ...) {
  va_list ap;
  size_t i;
  size_t off = 0;
 
  va_start(ap, nmsgs);

  for (i = 0; i < nmsgs; i++) {
    struct message *m = va_arg(ap, struct message *);

    off += strlen(m->raw);

    if (m->upgrade) {
      off -= strlen(m->upgrade);

      /* check the portion of the response after its specified upgrade */
      if (!check_str_eq(m, "upgrade", body + off, body + nread)) {
        abort();
      }

      /* fix up the response so that message_eq() will verify the beginning
       * of the upgrade */
      *(body + nread + strlen(m->upgrade)) = '\0';
      messages[num_messages -1 ].upgrade = body + nread;

      va_end(ap);
      return;
    }
  }

  va_end(ap);
  printf("\n\n*** error: expected a message with upgrade ***\n");

  abort();
}

static void
print_error (const char *raw, size_t error_location)
{
  fprintf(stderr, "\n*** %s ***\n\n",
          http_errno_description(http_parser_errno(parser)));

  int this_line = 0, char_len = 0;
  size_t i, j, len = strlen(raw), error_location_line = 0;
  for (i = 0; i < len; i++) {
    if (i == error_location) this_line = 1;
    switch (raw[i]) {
      case '\r':
        char_len = 2;
        fprintf(stderr, "\\r");
        break;

      case '\n':
        char_len = 2;
        fprintf(stderr, "\\n\n");

        if (this_line) goto print;

        error_location_line = 0;
        continue;

      default:
        char_len = 1;
        fputc(raw[i], stderr);
        break;
    }
    if (!this_line) error_location_line += char_len;
  }

  fprintf(stderr, "[eof]\n");

 print:
  for (j = 0; j < error_location_line; j++) {
    fputc(' ', stderr);
  }
  fprintf(stderr, "^\n\nerror location: %u\n", (unsigned int)error_location);
}

void
test_preserve_data (void)
{
  char my_data[] = "application-specific data";
  http_parser parser;
  parser.data = my_data;
  http_parser_init(&parser, http_request);
  if (parser.data != my_data) {
    printf("\n*** parser.data not preserved accross http_parser_init ***\n\n");
    abort();
  }
}

struct url_test {
  const char *name;
  const char *url;
  int is_connect;
  struct http_parser_url u;
  int rv;
};

const struct url_test url_tests[] =
{ {.name="proxy request"
  ,.url="http://hostname/"
  ,.is_connect=0
  ,.u=
    {.field_set=(1 << uf_schema) | (1 << uf_host) | (1 << uf_path)
    ,.port=0
    ,.field_data=
      {{  0,  4 } /* uf_schema */
      ,{  7,  8 } /* uf_host */
      ,{  0,  0 } /* uf_port */
      ,{ 15,  1 } /* uf_path */
      ,{  0,  0 } /* uf_query */
      ,{  0,  0 } /* uf_fragment */
      ,{  0,  0 } /* uf_userinfo */
      }
    }
  ,.rv=0
  }

, {.name="proxy request with port"
  ,.url="http://hostname:444/"
  ,.is_connect=0
  ,.u=
    {.field_set=(1 << uf_schema) | (1 << uf_host) | (1 << uf_port) | (1 << uf_path)
    ,.port=444
    ,.field_data=
      {{  0,  4 } /* uf_schema */
      ,{  7,  8 } /* uf_host */
      ,{ 16,  3 } /* uf_port */
      ,{ 19,  1 } /* uf_path */
      ,{  0,  0 } /* uf_query */
      ,{  0,  0 } /* uf_fragment */
      ,{  0,  0 } /* uf_userinfo */
      }
    }
  ,.rv=0
  }

, {.name="connect request"
  ,.url="hostname:443"
  ,.is_connect=1
  ,.u=
    {.field_set=(1 << uf_host) | (1 << uf_port)
    ,.port=443
    ,.field_data=
      {{  0,  0 } /* uf_schema */
      ,{  0,  8 } /* uf_host */
      ,{  9,  3 } /* uf_port */
      ,{  0,  0 } /* uf_path */
      ,{  0,  0 } /* uf_query */
      ,{  0,  0 } /* uf_fragment */
      ,{  0,  0 } /* uf_userinfo */
      }
    }
  ,.rv=0
  }

, {.name="connect request but not connect"
  ,.url="hostname:443"
  ,.is_connect=0
  ,.rv=1
  }

, {.name="proxy ipv6 request"
  ,.url="http://[1:2::3:4]/"
  ,.is_connect=0
  ,.u=
    {.field_set=(1 << uf_schema) | (1 << uf_host) | (1 << uf_path)
    ,.port=0
    ,.field_data=
      {{  0,  4 } /* uf_schema */
      ,{  8,  8 } /* uf_host */
      ,{  0,  0 } /* uf_port */
      ,{ 17,  1 } /* uf_path */
      ,{  0,  0 } /* uf_query */
      ,{  0,  0 } /* uf_fragment */
      ,{  0,  0 } /* uf_userinfo */
      }
    }
  ,.rv=0
  }

, {.name="proxy ipv6 request with port"
  ,.url="http://[1:2::3:4]:67/"
  ,.is_connect=0
  ,.u=
    {.field_set=(1 << uf_schema) | (1 << uf_host) | (1 << uf_port) | (1 << uf_path)
    ,.port=67
    ,.field_data=
      {{  0,  4 } /* uf_schema */
      ,{  8,  8 } /* uf_host */
      ,{ 18,  2 } /* uf_port */
      ,{ 20,  1 } /* uf_path */
      ,{  0,  0 } /* uf_query */
      ,{  0,  0 } /* uf_fragment */
      ,{  0,  0 } /* uf_userinfo */
      }
    }
  ,.rv=0
  }

, {.name="connect ipv6 address"
  ,.url="[1:2::3:4]:443"
  ,.is_connect=1
  ,.u=
    {.field_set=(1 << uf_host) | (1 << uf_port)
    ,.port=443
    ,.field_data=
      {{  0,  0 } /* uf_schema */
      ,{  1,  8 } /* uf_host */
      ,{ 11,  3 } /* uf_port */
      ,{  0,  0 } /* uf_path */
      ,{  0,  0 } /* uf_query */
      ,{  0,  0 } /* uf_fragment */
      ,{  0,  0 } /* uf_userinfo */
      }
    }
  ,.rv=0
  }

, {.name="ipv4 in ipv6 address"
  ,.url="http://[2001:0000:0000:0000:0000:0000:1.9.1.1]/"
  ,.is_connect=0
  ,.u=
    {.field_set=(1 << uf_schema) | (1 << uf_host) | (1 << uf_path)
    ,.port=0
    ,.field_data=
      {{  0,  4 } /* uf_schema */
      ,{  8, 37 } /* uf_host */
      ,{  0,  0 } /* uf_port */
      ,{ 46,  1 } /* uf_path */
      ,{  0,  0 } /* uf_query */
      ,{  0,  0 } /* uf_fragment */
      ,{  0,  0 } /* uf_userinfo */
      }
    }
  ,.rv=0
  }

, {.name="extra ? in query string"
  ,.url="http://a.tbcdn.cn/p/fp/2010c/??fp-header-min.css,fp-base-min.css,"
  "fp-channel-min.css,fp-product-min.css,fp-mall-min.css,fp-category-min.css,"
  "fp-sub-min.css,fp-gdp4p-min.css,fp-css3-min.css,fp-misc-min.css?t=20101022.css"
  ,.is_connect=0
  ,.u=
    {.field_set=(1<<uf_schema) | (1<<uf_host) | (1<<uf_path) | (1<<uf_query)
    ,.port=0
    ,.field_data=
      {{  0,  4 } /* uf_schema */
      ,{  7, 10 } /* uf_host */
      ,{  0,  0 } /* uf_port */
      ,{ 17, 12 } /* uf_path */
      ,{ 30,187 } /* uf_query */
      ,{  0,  0 } /* uf_fragment */
      ,{  0,  0 } /* uf_userinfo */
      }
    }
  ,.rv=0
  }

, {.name="space url encoded"
  ,.url="/toto.html?toto=a%20b"
  ,.is_connect=0
  ,.u=
    {.field_set= (1<<uf_path) | (1<<uf_query)
    ,.port=0
    ,.field_data=
      {{  0,  0 } /* uf_schema */
      ,{  0,  0 } /* uf_host */
      ,{  0,  0 } /* uf_port */
      ,{  0, 10 } /* uf_path */
      ,{ 11, 10 } /* uf_query */
      ,{  0,  0 } /* uf_fragment */
      ,{  0,  0 } /* uf_userinfo */
      }
    }
  ,.rv=0
  }


, {.name="url fragment"
  ,.url="/toto.html#titi"
  ,.is_connect=0
  ,.u=
    {.field_set= (1<<uf_path) | (1<<uf_fragment)
    ,.port=0
    ,.field_data=
      {{  0,  0 } /* uf_schema */
      ,{  0,  0 } /* uf_host */
      ,{  0,  0 } /* uf_port */
      ,{  0, 10 } /* uf_path */
      ,{  0,  0 } /* uf_query */
      ,{ 11,  4 } /* uf_fragment */
      ,{  0,  0 } /* uf_userinfo */
      }
    }
  ,.rv=0
  }

, {.name="complex url fragment"
  ,.url="http://www.webmasterworld.com/r.cgi?f=21&d=8405&url="
    "http://www.example.com/index.html?foo=bar&hello=world#midpage"
  ,.is_connect=0
  ,.u=
    {.field_set= (1<<uf_schema) | (1<<uf_host) | (1<<uf_path) | (1<<uf_query) |\
      (1<<uf_fragment)
    ,.port=0
    ,.field_data=
      {{  0,  4 } /* uf_schema */
      ,{  7, 22 } /* uf_host */
      ,{  0,  0 } /* uf_port */
      ,{ 29,  6 } /* uf_path */
      ,{ 36, 69 } /* uf_query */
      ,{106,  7 } /* uf_fragment */
      ,{  0,  0 } /* uf_userinfo */
      }
    }
  ,.rv=0
  }

, {.name="complex url from node js url parser doc"
  ,.url="http://host.com:8080/p/a/t/h?query=string#hash"
  ,.is_connect=0
  ,.u=
    {.field_set= (1<<uf_schema) | (1<<uf_host) | (1<<uf_port) | (1<<uf_path) |\
      (1<<uf_query) | (1<<uf_fragment)
    ,.port=8080
    ,.field_data=
      {{  0,  4 } /* uf_schema */
      ,{  7,  8 } /* uf_host */
      ,{ 16,  4 } /* uf_port */
      ,{ 20,  8 } /* uf_path */
      ,{ 29, 12 } /* uf_query */
      ,{ 42,  4 } /* uf_fragment */
      ,{  0,  0 } /* uf_userinfo */
      }
    }
  ,.rv=0
  }

, {.name="complex url with basic auth from node js url parser doc"
  ,.url="http://a:b@host.com:8080/p/a/t/h?query=string#hash"
  ,.is_connect=0
  ,.u=
    {.field_set= (1<<uf_schema) | (1<<uf_host) | (1<<uf_port) | (1<<uf_path) |\
      (1<<uf_query) | (1<<uf_fragment) | (1<<uf_userinfo)
    ,.port=8080
    ,.field_data=
      {{  0,  4 } /* uf_schema */
      ,{ 11,  8 } /* uf_host */
      ,{ 20,  4 } /* uf_port */
      ,{ 24,  8 } /* uf_path */
      ,{ 33, 12 } /* uf_query */
      ,{ 46,  4 } /* uf_fragment */
      ,{  7,  3 } /* uf_userinfo */
      }
    }
  ,.rv=0
  }

, {.name="double @"
  ,.url="http://a:b@@hostname:443/"
  ,.is_connect=0
  ,.rv=1
  }

, {.name="proxy empty host"
  ,.url="http://:443/"
  ,.is_connect=0
  ,.rv=1
  }

, {.name="proxy empty port"
  ,.url="http://hostname:/"
  ,.is_connect=0
  ,.rv=1
  }

, {.name="connect with basic auth"
  ,.url="a:b@hostname:443"
  ,.is_connect=1
  ,.rv=1
  }

, {.name="connect empty host"
  ,.url=":443"
  ,.is_connect=1
  ,.rv=1
  }

, {.name="connect empty port"
  ,.url="hostname:"
  ,.is_connect=1
  ,.rv=1
  }

, {.name="connect with extra bits"
  ,.url="hostname:443/"
  ,.is_connect=1
  ,.rv=1
  }

, {.name="space in url"
  ,.url="/foo bar/"
  ,.rv=1 /* s_dead */
  }

, {.name="proxy basic auth with space url encoded"
  ,.url="http://a%20:b@host.com/"
  ,.is_connect=0
  ,.u=
    {.field_set= (1<<uf_schema) | (1<<uf_host) | (1<<uf_path) | (1<<uf_userinfo)
    ,.port=0
    ,.field_data=
      {{  0,  4 } /* uf_schema */
      ,{ 14,  8 } /* uf_host */
      ,{  0,  0 } /* uf_port */
      ,{ 22,  1 } /* uf_path */
      ,{  0,  0 } /* uf_query */
      ,{  0,  0 } /* uf_fragment */
      ,{  7,  6 } /* uf_userinfo */
      }
    }
  ,.rv=0
  }

, {.name="carriage return in url"
  ,.url="/foo\rbar/"
  ,.rv=1 /* s_dead */
  }

, {.name="proxy double : in url"
  ,.url="http://hostname::443/"
  ,.rv=1 /* s_dead */
  }

, {.name="proxy basic auth with double :"
  ,.url="http://a::b@host.com/"
  ,.is_connect=0
  ,.u=
    {.field_set= (1<<uf_schema) | (1<<uf_host) | (1<<uf_path) | (1<<uf_userinfo)
    ,.port=0
    ,.field_data=
      {{  0,  4 } /* uf_schema */
      ,{ 12,  8 } /* uf_host */
      ,{  0,  0 } /* uf_port */
      ,{ 20,  1 } /* uf_path */
      ,{  0,  0 } /* uf_query */
      ,{  0,  0 } /* uf_fragment */
      ,{  7,  4 } /* uf_userinfo */
      }
    }
  ,.rv=0
  }

, {.name="line feed in url"
  ,.url="/foo\nbar/"
  ,.rv=1 /* s_dead */
  }

, {.name="proxy empty basic auth"
  ,.url="http://@hostname/fo"
  ,.u=
    {.field_set= (1<<uf_schema) | (1<<uf_host) | (1<<uf_path)
    ,.port=0
    ,.field_data=
      {{  0,  4 } /* uf_schema */
      ,{  8,  8 } /* uf_host */
      ,{  0,  0 } /* uf_port */
      ,{ 16,  3 } /* uf_path */
      ,{  0,  0 } /* uf_query */
      ,{  0,  0 } /* uf_fragment */
      ,{  0,  0 } /* uf_userinfo */
      }
    }
  ,.rv=0
  }
, {.name="proxy line feed in hostname"
  ,.url="http://host\name/fo"
  ,.rv=1 /* s_dead */
  }

, {.name="proxy % in hostname"
  ,.url="http://host%name/fo"
  ,.rv=1 /* s_dead */
  }

, {.name="proxy ; in hostname"
  ,.url="http://host;ame/fo"
  ,.rv=1 /* s_dead */
  }

, {.name="proxy basic auth with unreservedchars"
  ,.url="http://a!;-_!=+$@host.com/"
  ,.is_connect=0
  ,.u=
    {.field_set= (1<<uf_schema) | (1<<uf_host) | (1<<uf_path) | (1<<uf_userinfo)
    ,.port=0
    ,.field_data=
      {{  0,  4 } /* uf_schema */
      ,{ 17,  8 } /* uf_host */
      ,{  0,  0 } /* uf_port */
      ,{ 25,  1 } /* uf_path */
      ,{  0,  0 } /* uf_query */
      ,{  0,  0 } /* uf_fragment */
      ,{  7,  9 } /* uf_userinfo */
      }
    }
  ,.rv=0
  }

, {.name="proxy only empty basic auth"
  ,.url="http://@/fo"
  ,.rv=1 /* s_dead */
  }

, {.name="proxy only basic auth"
  ,.url="http://toto@/fo"
  ,.rv=1 /* s_dead */
  }

, {.name="proxy emtpy hostname"
  ,.url="http:///fo"
  ,.rv=1 /* s_dead */
  }

, {.name="proxy = in url"
  ,.url="http://host=ame/fo"
  ,.rv=1 /* s_dead */
  }

#if http_parser_strict

, {.name="tab in url"
  ,.url="/foo\tbar/"
  ,.rv=1 /* s_dead */
  }

, {.name="form feed in url"
  ,.url="/foo\fbar/"
  ,.rv=1 /* s_dead */
  }

#else /* !http_parser_strict */

, {.name="tab in url"
  ,.url="/foo\tbar/"
  ,.u=
    {.field_set=(1 << uf_path)
    ,.field_data=
      {{  0,  0 } /* uf_schema */
      ,{  0,  0 } /* uf_host */
      ,{  0,  0 } /* uf_port */
      ,{  0,  9 } /* uf_path */
      ,{  0,  0 } /* uf_query */
      ,{  0,  0 } /* uf_fragment */
      ,{  0,  0 } /* uf_userinfo */
      }
    }
  ,.rv=0
  }

, {.name="form feed in url"
  ,.url="/foo\fbar/"
  ,.u=
    {.field_set=(1 << uf_path)
    ,.field_data=
      {{  0,  0 } /* uf_schema */
      ,{  0,  0 } /* uf_host */
      ,{  0,  0 } /* uf_port */
      ,{  0,  9 } /* uf_path */
      ,{  0,  0 } /* uf_query */
      ,{  0,  0 } /* uf_fragment */
      ,{  0,  0 } /* uf_userinfo */
      }
    }
  ,.rv=0
  }
#endif
};

void
dump_url (const char *url, const struct http_parser_url *u)
{
  unsigned int i;

  printf("\tfield_set: 0x%x, port: %u\n", u->field_set, u->port);
  for (i = 0; i < uf_max; i++) {
    if ((u->field_set & (1 << i)) == 0) {
      printf("\tfield_data[%u]: unset\n", i);
      continue;
    }

    printf("\tfield_data[%u]: off: %u len: %u part: \"%.*s\n\"",
           i,
           u->field_data[i].off,
           u->field_data[i].len,
           u->field_data[i].len,
           url + u->field_data[i].off);
  }
}

void
test_parse_url (void)
{
  struct http_parser_url u;
  const struct url_test *test;
  unsigned int i;
  int rv;

  for (i = 0; i < (sizeof(url_tests) / sizeof(url_tests[0])); i++) {
    test = &url_tests[i];
    memset(&u, 0, sizeof(u));

    rv = http_parser_parse_url(test->url,
                               strlen(test->url),
                               test->is_connect,
                               &u);

    if (test->rv == 0) {
      if (rv != 0) {
        printf("\n*** http_parser_parse_url(\"%s\") \"%s\" test failed, "
               "unexpected rv %d ***\n\n", test->url, test->name, rv);
        abort();
      }

      if (memcmp(&u, &test->u, sizeof(u)) != 0) {
        printf("\n*** http_parser_parse_url(\"%s\") \"%s\" failed ***\n",
               test->url, test->name);

        printf("target http_parser_url:\n");
        dump_url(test->url, &test->u);
        printf("result http_parser_url:\n");
        dump_url(test->url, &u);

        abort();
      }
    } else {
      /* test->rv != 0 */
      if (rv == 0) {
        printf("\n*** http_parser_parse_url(\"%s\") \"%s\" test failed, "
               "unexpected rv %d ***\n\n", test->url, test->name, rv);
        abort();
      }
    }
  }
}

void
test_method_str (void)
{
  assert(0 == strcmp("get", http_method_str(http_get)));
  assert(0 == strcmp("<unknown>", http_method_str(1337)));
}

void
test_message (const struct message *message)
{
  size_t raw_len = strlen(message->raw);
  size_t msg1len;
  for (msg1len = 0; msg1len < raw_len; msg1len++) {
    parser_init(message->type);

    size_t read;
    const char *msg1 = message->raw;
    const char *msg2 = msg1 + msg1len;
    size_t msg2len = raw_len - msg1len;

    if (msg1len) {
      read = parse(msg1, msg1len);

      if (message->upgrade && parser->upgrade) {
        messages[num_messages - 1].upgrade = msg1 + read;
        goto test;
      }

      if (read != msg1len) {
        print_error(msg1, read);
        abort();
      }
    }


    read = parse(msg2, msg2len);

    if (message->upgrade && parser->upgrade) {
      messages[num_messages - 1].upgrade = msg2 + read;
      goto test;
    }

    if (read != msg2len) {
      print_error(msg2, read);
      abort();
    }

    read = parse(null, 0);

    if (read != 0) {
      print_error(message->raw, read);
      abort();
    }

  test:

    if (num_messages != 1) {
      printf("\n*** num_messages != 1 after testing '%s' ***\n\n", message->name);
      abort();
    }

    if(!message_eq(0, message)) abort();

    parser_free();
  }
}

void
test_message_count_body (const struct message *message)
{
  parser_init(message->type);

  size_t read;
  size_t l = strlen(message->raw);
  size_t i, toread;
  size_t chunk = 4024;

  for (i = 0; i < l; i+= chunk) {
    toread = min(l-i, chunk);
    read = parse_count_body(message->raw + i, toread);
    if (read != toread) {
      print_error(message->raw, read);
      abort();
    }
  }


  read = parse_count_body(null, 0);
  if (read != 0) {
    print_error(message->raw, read);
    abort();
  }

  if (num_messages != 1) {
    printf("\n*** num_messages != 1 after testing '%s' ***\n\n", message->name);
    abort();
  }

  if(!message_eq(0, message)) abort();

  parser_free();
}

void
test_simple (const char *buf, enum http_errno err_expected)
{
  parser_init(http_request);

  size_t parsed;
  int pass;
  enum http_errno err;

  parsed = parse(buf, strlen(buf));
  pass = (parsed == strlen(buf));
  err = http_parser_errno(parser);
  parsed = parse(null, 0);
  pass &= (parsed == 0);

  parser_free();

  /* in strict mode, allow us to pass with an unexpected hpe_strict as
   * long as the caller isn't expecting success.
   */
#if http_parser_strict
  if (err_expected != err && err_expected != hpe_ok && err != hpe_strict) {
#else
  if (err_expected != err) {
#endif
    fprintf(stderr, "\n*** test_simple expected %s, but saw %s ***\n\n%s\n",
        http_errno_name(err_expected), http_errno_name(err), buf);
    abort();
  }
}

void
test_header_overflow_error (int req)
{
  http_parser parser;
  http_parser_init(&parser, req ? http_request : http_response);
  size_t parsed;
  const char *buf;
  buf = req ? "get / http/1.1\r\n" : "http/1.0 200 ok\r\n";
  parsed = http_parser_execute(&parser, &settings_null, buf, strlen(buf));
  assert(parsed == strlen(buf));

  buf = "header-key: header-value\r\n";
  size_t buflen = strlen(buf);

  int i;
  for (i = 0; i < 10000; i++) {
    parsed = http_parser_execute(&parser, &settings_null, buf, buflen);
    if (parsed != buflen) {
      //fprintf(stderr, "error found on iter %d\n", i);
      assert(http_parser_errno(&parser) == hpe_header_overflow);
      return;
    }
  }

  fprintf(stderr, "\n*** error expected but none in header overflow test ***\n");
  abort();
}

static void
test_content_length_overflow (const char *buf, size_t buflen, int expect_ok)
{
  http_parser parser;
  http_parser_init(&parser, http_response);
  http_parser_execute(&parser, &settings_null, buf, buflen);

  if (expect_ok)
    assert(http_parser_errno(&parser) == hpe_ok);
  else
    assert(http_parser_errno(&parser) == hpe_invalid_content_length);
}

void
test_header_content_length_overflow_error (void)
{
#define x(size)                                                               \
  "http/1.1 200 ok\r\n"                                                       \
  "content-length: " #size "\r\n"                                             \
  "\r\n"
  const char a[] = x(1844674407370955160);  /* 2^64 / 10 - 1 */
  const char b[] = x(18446744073709551615); /* 2^64-1 */
  const char c[] = x(18446744073709551616); /* 2^64   */
#undef x
  test_content_length_overflow(a, sizeof(a) - 1, 1); /* expect ok      */
  test_content_length_overflow(b, sizeof(b) - 1, 0); /* expect failure */
  test_content_length_overflow(c, sizeof(c) - 1, 0); /* expect failure */
}

void
test_chunk_content_length_overflow_error (void)
{
#define x(size)                                                               \
    "http/1.1 200 ok\r\n"                                                     \
    "transfer-encoding: chunked\r\n"                                          \
    "\r\n"                                                                    \
    #size "\r\n"                                                              \
    "..."
  const char a[] = x(ffffffffffffffe);   /* 2^64 / 16 - 1 */
  const char b[] = x(ffffffffffffffff);  /* 2^64-1 */
  const char c[] = x(10000000000000000); /* 2^64   */
#undef x
  test_content_length_overflow(a, sizeof(a) - 1, 1); /* expect ok      */
  test_content_length_overflow(b, sizeof(b) - 1, 0); /* expect failure */
  test_content_length_overflow(c, sizeof(c) - 1, 0); /* expect failure */
}

void
test_no_overflow_long_body (int req, size_t length)
{
  http_parser parser;
  http_parser_init(&parser, req ? http_request : http_response);
  size_t parsed;
  size_t i;
  char buf1[3000];
  size_t buf1len = sprintf(buf1, "%s\r\nconnection: keep-alive\r\ncontent-length: %lu\r\n\r\n",
      req ? "post / http/1.0" : "http/1.0 200 ok", (unsigned long)length);
  parsed = http_parser_execute(&parser, &settings_null, buf1, buf1len);
  if (parsed != buf1len)
    goto err;

  for (i = 0; i < length; i++) {
    char foo = 'a';
    parsed = http_parser_execute(&parser, &settings_null, &foo, 1);
    if (parsed != 1)
      goto err;
  }

  parsed = http_parser_execute(&parser, &settings_null, buf1, buf1len);
  if (parsed != buf1len) goto err;
  return;

 err:
  fprintf(stderr,
          "\n*** error in test_no_overflow_long_body %s of length %lu ***\n",
          req ? "request" : "response",
          (unsigned long)length);
  abort();
}

void
test_multiple3 (const struct message *r1, const struct message *r2, const struct message *r3)
{
  int message_count = count_parsed_messages(3, r1, r2, r3);

  char total[ strlen(r1->raw)
            + strlen(r2->raw)
            + strlen(r3->raw)
            + 1
            ];
  total[0] = '\0';

  strcat(total, r1->raw);
  strcat(total, r2->raw);
  strcat(total, r3->raw);

  parser_init(r1->type);

  size_t read;

  read = parse(total, strlen(total));

  if (parser->upgrade) {
    upgrade_message_fix(total, read, 3, r1, r2, r3);
    goto test;
  }

  if (read != strlen(total)) {
    print_error(total, read);
    abort();
  }

  read = parse(null, 0);

  if (read != 0) {
    print_error(total, read);
    abort();
  }

test:

  if (message_count != num_messages) {
    fprintf(stderr, "\n\n*** parser didn't see 3 messages only %d *** \n", num_messages);
    abort();
  }

  if (!message_eq(0, r1)) abort();
  if (message_count > 1 && !message_eq(1, r2)) abort();
  if (message_count > 2 && !message_eq(2, r3)) abort();

  parser_free();
}

/* scan through every possible breaking to make sure the
 * parser can handle getting the content in any chunks that
 * might come from the socket
 */
void
test_scan (const struct message *r1, const struct message *r2, const struct message *r3)
{
  char total[80*1024] = "\0";
  char buf1[80*1024] = "\0";
  char buf2[80*1024] = "\0";
  char buf3[80*1024] = "\0";

  strcat(total, r1->raw);
  strcat(total, r2->raw);
  strcat(total, r3->raw);

  size_t read;

  int total_len = strlen(total);

  int total_ops = 2 * (total_len - 1) * (total_len - 2) / 2;
  int ops = 0 ;

  size_t buf1_len, buf2_len, buf3_len;
  int message_count = count_parsed_messages(3, r1, r2, r3);

  int i,j,type_both;
  for (type_both = 0; type_both < 2; type_both ++ ) {
    for (j = 2; j < total_len; j ++ ) {
      for (i = 1; i < j; i ++ ) {

        if (ops % 1000 == 0)  {
          printf("\b\b\b\b%3.0f%%", 100 * (float)ops /(float)total_ops);
          fflush(stdout);
        }
        ops += 1;

        parser_init(type_both ? http_both : r1->type);

        buf1_len = i;
        strlncpy(buf1, sizeof(buf1), total, buf1_len);
        buf1[buf1_len] = 0;

        buf2_len = j - i;
        strlncpy(buf2, sizeof(buf1), total+i, buf2_len);
        buf2[buf2_len] = 0;

        buf3_len = total_len - j;
        strlncpy(buf3, sizeof(buf1), total+j, buf3_len);
        buf3[buf3_len] = 0;

        read = parse(buf1, buf1_len);

        if (parser->upgrade) goto test;

        if (read != buf1_len) {
          print_error(buf1, read);
          goto error;
        }

        read += parse(buf2, buf2_len);

        if (parser->upgrade) goto test;

        if (read != buf1_len + buf2_len) {
          print_error(buf2, read);
          goto error;
        }

        read += parse(buf3, buf3_len);

        if (parser->upgrade) goto test;

        if (read != buf1_len + buf2_len + buf3_len) {
          print_error(buf3, read);
          goto error;
        }

        parse(null, 0);

test:
        if (parser->upgrade) {
          upgrade_message_fix(total, read, 3, r1, r2, r3);
        }

        if (message_count != num_messages) {
          fprintf(stderr, "\n\nparser didn't see %d messages only %d\n",
            message_count, num_messages);
          goto error;
        }

        if (!message_eq(0, r1)) {
          fprintf(stderr, "\n\nerror matching messages[0] in test_scan.\n");
          goto error;
        }

        if (message_count > 1 && !message_eq(1, r2)) {
          fprintf(stderr, "\n\nerror matching messages[1] in test_scan.\n");
          goto error;
        }

        if (message_count > 2 && !message_eq(2, r3)) {
          fprintf(stderr, "\n\nerror matching messages[2] in test_scan.\n");
          goto error;
        }

        parser_free();
      }
    }
  }
  puts("\b\b\b\b100%");
  return;

 error:
  fprintf(stderr, "i=%d  j=%d\n", i, j);
  fprintf(stderr, "buf1 (%u) %s\n\n", (unsigned int)buf1_len, buf1);
  fprintf(stderr, "buf2 (%u) %s\n\n", (unsigned int)buf2_len , buf2);
  fprintf(stderr, "buf3 (%u) %s\n", (unsigned int)buf3_len, buf3);
  abort();
}

// user required to free the result
// string terminated by \0
char *
create_large_chunked_message (int body_size_in_kb, const char* headers)
{
  int i;
  size_t wrote = 0;
  size_t headers_len = strlen(headers);
  size_t bufsize = headers_len + (5+1024+2)*body_size_in_kb + 6;
  char * buf = malloc(bufsize);

  memcpy(buf, headers, headers_len);
  wrote += headers_len;

  for (i = 0; i < body_size_in_kb; i++) {
    // write 1kb chunk into the body.
    memcpy(buf + wrote, "400\r\n", 5);
    wrote += 5;
    memset(buf + wrote, 'c', 1024);
    wrote += 1024;
    strcpy(buf + wrote, "\r\n");
    wrote += 2;
  }

  memcpy(buf + wrote, "0\r\n\r\n", 6);
  wrote += 6;
  assert(wrote == bufsize);

  return buf;
}

/* verify that we can pause parsing at any of the bytes in the
 * message and still get the result that we're expecting. */
void
test_message_pause (const struct message *msg)
{
  char *buf = (char*) msg->raw;
  size_t buflen = strlen(msg->raw);
  size_t nread;

  parser_init(msg->type);

  do {
    nread = parse_pause(buf, buflen);

    // we can only set the upgrade buffer once we've gotten our message
    // completion callback.
    if (messages[0].message_complete_cb_called &&
        msg->upgrade &&
        parser->upgrade) {
      messages[0].upgrade = buf + nread;
      goto test;
    }

    if (nread < buflen) {

      // not much do to if we failed a strict-mode check
      if (http_parser_errno(parser) == hpe_strict) {
        parser_free();
        return;
      }

      assert (http_parser_errno(parser) == hpe_paused);
    }

    buf += nread;
    buflen -= nread;
    http_parser_pause(parser, 0);
  } while (buflen > 0);

  nread = parse_pause(null, 0);
  assert (nread == 0);

test:
  if (num_messages != 1) {
    printf("\n*** num_messages != 1 after testing '%s' ***\n\n", msg->name);
    abort();
  }

  if(!message_eq(0, msg)) abort();

  parser_free();
}

int
main (void)
{
  parser = null;
  int i, j, k;
  int request_count;
  int response_count;
  unsigned long version;
  unsigned major;
  unsigned minor;
  unsigned patch;

  version = http_parser_version();
  major = (version >> 16) & 255;
  minor = (version >> 8) & 255;
  patch = version & 255;
  printf("http_parser v%u.%u.%u (0x%06lx)\n", major, minor, patch, version);

  printf("sizeof(http_parser) = %u\n", (unsigned int)sizeof(http_parser));

  for (request_count = 0; requests[request_count].name; request_count++);
  for (response_count = 0; responses[response_count].name; response_count++);

  //// api
  test_preserve_data();
  test_parse_url();
  test_method_str();

  //// overflow conditions

  test_header_overflow_error(http_request);
  test_no_overflow_long_body(http_request, 1000);
  test_no_overflow_long_body(http_request, 100000);

  test_header_overflow_error(http_response);
  test_no_overflow_long_body(http_response, 1000);
  test_no_overflow_long_body(http_response, 100000);

  test_header_content_length_overflow_error();
  test_chunk_content_length_overflow_error();

  //// responses

  for (i = 0; i < response_count; i++) {
    test_message(&responses[i]);
  }

  for (i = 0; i < response_count; i++) {
    test_message_pause(&responses[i]);
  }

  for (i = 0; i < response_count; i++) {
    if (!responses[i].should_keep_alive) continue;
    for (j = 0; j < response_count; j++) {
      if (!responses[j].should_keep_alive) continue;
      for (k = 0; k < response_count; k++) {
        test_multiple3(&responses[i], &responses[j], &responses[k]);
      }
    }
  }

  test_message_count_body(&responses[no_headers_no_body_404]);
  test_message_count_body(&responses[trailing_space_on_chunked_body]);

  // test very large chunked response
  {
    char * msg = create_large_chunked_message(31337,
      "http/1.0 200 ok\r\n"
      "transfer-encoding: chunked\r\n"
      "content-type: text/plain\r\n"
      "\r\n");
    struct message large_chunked =
      {.name= "large chunked"
      ,.type= http_response
      ,.raw= msg
      ,.should_keep_alive= false
      ,.message_complete_on_eof= false
      ,.http_major= 1
      ,.http_minor= 0
      ,.status_code= 200
      ,.response_status= "ok"
      ,.num_headers= 2
      ,.headers=
        { { "transfer-encoding", "chunked" }
        , { "content-type", "text/plain" }
        }
      ,.body_size= 31337*1024
      };
    test_message_count_body(&large_chunked);
    free(msg);
  }



  printf("response scan 1/2      ");
  test_scan( &responses[trailing_space_on_chunked_body]
           , &responses[no_body_http10_ka_204]
           , &responses[no_reason_phrase]
           );

  printf("response scan 2/2      ");
  test_scan( &responses[bonjour_madame_fr]
           , &responses[understore_header_key]
           , &responses[no_carriage_ret]
           );

  puts("responses okay");


  /// requests

  test_simple("get / htp/1.1\r\n\r\n", hpe_invalid_version);

  // well-formed but incomplete
  test_simple("get / http/1.1\r\n"
              "content-type: text/plain\r\n"
              "content-length: 6\r\n"
              "\r\n"
              "fooba",
              hpe_ok);

  static const char *all_methods[] = {
    "delete",
    "get",
    "head",
    "post",
    "put",
    //"connect", //connect can't be tested like other methods, it's a tunnel
    "options",
    "trace",
    "copy",
    "lock",
    "mkcol",
    "move",
    "propfind",
    "proppatch",
    "unlock",
    "report",
    "mkactivity",
    "checkout",
    "merge",
    "m-search",
    "notify",
    "subscribe",
    "unsubscribe",
    "patch",
    0 };
  const char **this_method;
  for (this_method = all_methods; *this_method; this_method++) {
    char buf[200];
    sprintf(buf, "%s / http/1.1\r\n\r\n", *this_method);
    test_simple(buf, hpe_ok);
  }

  static const char *bad_methods[] = {
      "asdf",
      "c******",
      "cola",
      "gem",
      "geta",
      "m****",
      "mkcola",
      "proppatcha",
      "pun",
      "px",
      "sa",
      "hello world",
      0 };
  for (this_method = bad_methods; *this_method; this_method++) {
    char buf[200];
    sprintf(buf, "%s / http/1.1\r\n\r\n", *this_method);
    test_simple(buf, hpe_invalid_method);
  }

  const char *dumbfuck2 =
    "get / http/1.1\r\n"
    "x-ssl-bullshit:   -----begin certificate-----\r\n"
    "\tmiifbtccbfwgawibagich4cwdqyjkozihvcnaqefbqawcdelmakga1uebhmcvusx\r\n"
    "\tetapbgnvbaotcgvty2llbmnlmriweaydvqqlewlbdxrob3jpdhkxczajbgnvbamt\r\n"
    "\taknbms0wkwyjkozihvcnaqkbfh5jys1vcgvyyxrvckbncmlklxn1chbvcnquywmu\r\n"
    "\tdwswhhcnmdywnzi3mtqxmzi4whcnmdcwnzi3mtqxmzi4wjbbmqswcqydvqqgewjv\r\n"
    "\tszerma8ga1uechmizvnjawvuy2uxezarbgnvbastck1hbmnozxn0zxixczajbgnv\r\n"
    "\tbactmrsogriqmwlak1dmrcwfqydvqqdew5tawnoywvsihbhcmqyjkozihvcnaqeb\r\n"
    "\tbqadggepadccaqocggebanpeqbgl1iakdss1tbhf3hexsl72g9j+wc/1r64facef\r\n"
    "\tw51reyfyiiezgx/bvzwxbebonuk41ok65sxguflmo5glflbwjthbriekafvvp3yr\r\n"
    "\tgw7cma/s/xkgl1gec7rqw8lizt8rapukcgqovhsi/f1siflpdxudfmdinzl31+sl\r\n"
    "\t0iwhddnkgjy5pybsb8y79dssjtcw/ialb0/n8sj7hgvvzj7x0fr+rqjyouufrepp\r\n"
    "\tu2mspfyf+9bbc/axgazuicvsr+8snv3xapqy+fulk/xy8h8ua51ixoq5jrgu2sqr\r\n"
    "\twga7bui3g8lfzmbl8frcdygudy7m6qahxx1zwipwnkscaweaaaocaiqwggigmawg\r\n"
    "\ta1udeweb/wqcmaaweqyjyiziayb4qghttpaqdagwgma4ga1uddweb/wqeawid6das\r\n"
    "\tbglghkgbhvhcaq0ehxydvusgzs1ty2llbmnlifvzzxigq2vydglmawnhdguwhqyd\r\n"
    "\tvr0obbyefdtt/sf9pemazdhkuildrdymntbzmigabgnvhsmegziwgy+afai4qxgj\r\n"
    "\tloclddmvkwiljjdastqooxskcjbwmqswcqydvqqgewjvszerma8ga1uechmizvnj\r\n"
    "\tawvuy2uxejaqbgnvbastcuf1dghvcml0etelmakga1ueaxmcq0exltarbgkqhkig\r\n"
    "\t9w0bcqewhmnhlw9wzxjhdg9yqgdyawqtc3vwcg9ydc5hyy51a4ibadapbgnvhrie\r\n"
    "\tijaggr5jys1vcgvyyxrvckbncmlklxn1chbvcnquywmudwswgqydvr0gbbiwedao\r\n"
    "\tbgwrbgeeadkvaqebaqywpqyjyiziayb4qgeebdawlmh0dha6ly9jys5ncmlklxn1\r\n"
    "\tchbvcnquywmudmt4sopwqlbwsvchvil2nybc9jywnybc5jcmwwpqyjyiziayb4qgedbdawlmh0\r\n"
    "\tdha6ly9jys5ncmlklxn1chbvcnquywmudwsvchvil2nybc9jywnybc5jcmwwpwyd\r\n"
    "\tvr0fbdgwnja0odkgmiyuahr0cdovl2nhlmdyawqt5hyy51ay9wdwiv\r\n"
    "\ty3jsl2nhy3jslmnybdanbgkqhkig9w0baqufaaocaqeas/u4iioobengw/hwmmd3\r\n"
    "\txcy6zrt08yjkczgnjort98g8ugsqyjsxv/hmi0qlnlhs+k/3iobc3ljs5amyr5l8\r\n"
    "\tuo7oskgffllhqyc9jzpfmlcaugvzebyv4olnsr8hbxf1mbkzoqxuztmvu29wjfxk\r\n"
    "\thteapbv7eakcwpsp7mcbvgzm74izkhu3vldk9w6qvrxepfggpkpqfhiooghfnbtk\r\n"
    "\twtc6o2xq5y0qz03jonf7ojsped3i5zky3e+ov7/zhw6dqt8ufvsadjvqbxyhv8eu\r\n"
    "\tyhixw1akepznjnowuisevogkolxxwi5vai5hgxds0/es5gdgsabo4fqovuklgop3\r\n"
    "\tra==\r\n"
    "\t-----end certificate-----\r\n"
    "\r\n";
  test_simple(dumbfuck2, hpe_ok);

#if 0
  // note(wed nov 18 11:57:27 cet 2009) this seems okay. we just read body
  // until eof.
  //
  // no content-length
  // error if there is a body without content length
  const char *bad_get_no_headers_no_body = "get /bad_get_no_headers_no_body/world http/1.1\r\n"
                                           "accept: */*\r\n"
                                           "\r\n"
                                           "hello";
  test_simple(bad_get_no_headers_no_body, 0);
#endif
  /* todo sending junk and large headers gets rejected */


  /* check to make sure our predefined requests are okay */
  for (i = 0; requests[i].name; i++) {
    test_message(&requests[i]);
  }

  for (i = 0; i < request_count; i++) {
    test_message_pause(&requests[i]);
  }

  for (i = 0; i < request_count; i++) {
    if (!requests[i].should_keep_alive) continue;
    for (j = 0; j < request_count; j++) {
      if (!requests[j].should_keep_alive) continue;
      for (k = 0; k < request_count; k++) {
        test_multiple3(&requests[i], &requests[j], &requests[k]);
      }
    }
  }

  printf("request scan 1/4      ");
  test_scan( &requests[get_no_headers_no_body]
           , &requests[get_one_header_no_body]
           , &requests[get_no_headers_no_body]
           );

  printf("request scan 2/4      ");
  test_scan( &requests[post_chunked_all_your_base]
           , &requests[post_identity_body_world]
           , &requests[get_funky_content_length]
           );

  printf("request scan 3/4      ");
  test_scan( &requests[two_chunks_mult_zero_end]
           , &requests[chunked_w_trailing_headers]
           , &requests[chunked_w_bullshit_after_length]
           );

  printf("request scan 4/4      ");
  test_scan( &requests[query_url_with_question_mark_get]
           , &requests[prefix_newline_get ]
           , &requests[connect_request]
           );

  puts("requests okay");

  return 0;
}
