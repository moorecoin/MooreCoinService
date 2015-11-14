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
#ifndef http_parser_h
#define http_parser_h
#ifdef __cplusplus
extern "c" {
#endif

/* also update soname in the makefile whenever you change these. */
#define http_parser_version_major 2
#define http_parser_version_minor 2
#define http_parser_version_patch 1

#include <sys/types.h>
#if defined(_win32) && !defined(__mingw32__) && (!defined(_msc_ver) || _msc_ver<1600)
#include <basetsd.h>
#include <stddef.h>
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

/* compile with -dhttp_parser_strict=0 to make less checks, but run
 * faster
 */
#ifndef http_parser_strict
# define http_parser_strict 1
#endif

/* maximium header size allowed */
#define http_max_header_size (80*1024)


typedef struct http_parser http_parser;
typedef struct http_parser_settings http_parser_settings;


/* callbacks should return non-zero to indicate an error. the parser will
 * then halt execution.
 *
 * the one exception is on_headers_complete. in a http_response parser
 * returning '1' from on_headers_complete will tell the parser that it
 * should not expect a body. this is used when receiving a response to a
 * head request which may contain 'content-length' or 'transfer-encoding:
 * chunked' headers that indicate the presence of a body.
 *
 * http_data_cb does not return data chunks. it will be call arbitrarally
 * many times for each string. e.g. you might get 10 callbacks for "on_url"
 * each providing just a few characters more data.
 */
typedef int (*http_data_cb) (http_parser*, const char *at, size_t length);
typedef int (*http_cb) (http_parser*);


/* request methods */
#define http_method_map(xx)         \
  xx(0,  delete,      delete)       \
  xx(1,  get,         get)          \
  xx(2,  head,        head)         \
  xx(3,  post,        post)         \
  xx(4,  put,         put)          \
  /* pathological */                \
  xx(5,  connect,     connect)      \
  xx(6,  options,     options)      \
  xx(7,  trace,       trace)        \
  /* webdav */                      \
  xx(8,  copy,        copy)         \
  xx(9,  lock,        lock)         \
  xx(10, mkcol,       mkcol)        \
  xx(11, move,        move)         \
  xx(12, propfind,    propfind)     \
  xx(13, proppatch,   proppatch)    \
  xx(14, search,      search)       \
  xx(15, unlock,      unlock)       \
  /* subversion */                  \
  xx(16, report,      report)       \
  xx(17, mkactivity,  mkactivity)   \
  xx(18, checkout,    checkout)     \
  xx(19, merge,       merge)        \
  /* upnp */                        \
  xx(20, msearch,     m-search)     \
  xx(21, notify,      notify)       \
  xx(22, subscribe,   subscribe)    \
  xx(23, unsubscribe, unsubscribe)  \
  /* rfc-5789 */                    \
  xx(24, patch,       patch)        \
  xx(25, purge,       purge)        \

enum http_method
  {
#define xx(num, name, string) http_##name = num,
  http_method_map(xx)
#undef xx
  };


enum http_parser_type { http_request, http_response, http_both };


/* flag values for http_parser.flags field */
enum flags
  { f_chunked               = 1 << 0
  , f_connection_keep_alive = 1 << 1
  , f_connection_close      = 1 << 2
  , f_trailing              = 1 << 3
  , f_upgrade               = 1 << 4
  , f_skipbody              = 1 << 5
  };


/* map for errno-related constants
 * 
 * the provided argument should be a macro that takes 2 arguments.
 */
#define http_errno_map(xx)                                           \
  /* no error */                                                     \
  xx(ok, "success")                                                  \
                                                                     \
  /* callback-related errors */                                      \
  xx(cb_message_begin, "the on_message_begin callback failed")       \
  xx(cb_url, "the on_url callback failed")                           \
  xx(cb_header_field, "the on_header_field callback failed")         \
  xx(cb_header_value, "the on_header_value callback failed")         \
  xx(cb_headers_complete, "the on_headers_complete callback failed") \
  xx(cb_body, "the on_body callback failed")                         \
  xx(cb_message_complete, "the on_message_complete callback failed") \
  xx(cb_status, "the on_status callback failed")                     \
                                                                     \
  /* parsing-related errors */                                       \
  xx(invalid_eof_state, "stream ended at an unexpected time")        \
  xx(header_overflow,                                                \
     "too many header bytes seen; overflow detected")                \
  xx(closed_connection,                                              \
     "data received after completed connection: close message")      \
  xx(invalid_version, "invalid http version")                        \
  xx(invalid_status, "invalid http status code")                     \
  xx(invalid_method, "invalid http method")                          \
  xx(invalid_url, "invalid url")                                     \
  xx(invalid_host, "invalid host")                                   \
  xx(invalid_port, "invalid port")                                   \
  xx(invalid_path, "invalid path")                                   \
  xx(invalid_query_string, "invalid query string")                   \
  xx(invalid_fragment, "invalid fragment")                           \
  xx(lf_expected, "lf character expected")                           \
  xx(invalid_header_token, "invalid character in header")            \
  xx(invalid_content_length,                                         \
     "invalid character in content-length header")                   \
  xx(invalid_chunk_size,                                             \
     "invalid character in chunk size header")                       \
  xx(invalid_constant, "invalid constant string")                    \
  xx(invalid_internal_state, "encountered unexpected internal state")\
  xx(strict, "strict mode assertion failed")                         \
  xx(paused, "parser is paused")                                     \
  xx(unknown, "an unknown error occurred")


/* define hpe_* values for each errno value above */
#define http_errno_gen(n, s) hpe_##n,
enum http_errno {
  http_errno_map(http_errno_gen)
};
#undef http_errno_gen


/* get an http_errno value from an http_parser */
#define http_parser_errno(p)            ((enum http_errno) (p)->http_errno)


struct http_parser {
  /** private **/
  unsigned int type : 2;         /* enum http_parser_type */
  unsigned int flags : 6;        /* f_* values from 'flags' enum; semi-public */
  unsigned int state : 8;        /* enum state from http_parser.c */
  unsigned int header_state : 8; /* enum header_state from http_parser.c */
  unsigned int index : 8;        /* index into current matcher */

  uint32_t nread;          /* # bytes read in various scenarios */
  uint64_t content_length; /* # bytes in body (0 if no content-length header) */

  /** read-only **/
  unsigned short http_major;
  unsigned short http_minor;
  unsigned int status_code : 16; /* responses only */
  unsigned int method : 8;       /* requests only */
  unsigned int http_errno : 7;

  /* 1 = upgrade header was present and the parser has exited because of that.
   * 0 = no upgrade header present.
   * should be checked when http_parser_execute() returns in addition to
   * error checking.
   */
  unsigned int upgrade : 1;

  /** public **/
  void *data; /* a pointer to get hook to the "connection" or "socket" object */
};


struct http_parser_settings {
  http_cb      on_message_begin;
  http_data_cb on_url;
  http_data_cb on_status;
  http_data_cb on_header_field;
  http_data_cb on_header_value;
  http_cb      on_headers_complete;
  http_data_cb on_body;
  http_cb      on_message_complete;
};


enum http_parser_url_fields
  { uf_schema           = 0
  , uf_host             = 1
  , uf_port             = 2
  , uf_path             = 3
  , uf_query            = 4
  , uf_fragment         = 5
  , uf_userinfo         = 6
  , uf_max              = 7
  };


/* result structure for http_parser_parse_url().
 *
 * callers should index into field_data[] with uf_* values iff field_set
 * has the relevant (1 << uf_*) bit set. as a courtesy to clients (and
 * because we probably have padding left over), we convert any port to
 * a uint16_t.
 */
struct http_parser_url {
  uint16_t field_set;           /* bitmask of (1 << uf_*) values */
  uint16_t port;                /* converted uf_port string */

  struct {
    uint16_t off;               /* offset into buffer in which field starts */
    uint16_t len;               /* length of run in buffer */
  } field_data[uf_max];
};


/* returns the library version. bits 16-23 contain the major version number,
 * bits 8-15 the minor version number and bits 0-7 the patch level.
 * usage example:
 *
 *   unsigned long version = http_parser_version();
 *   unsigned major = (version >> 16) & 255;
 *   unsigned minor = (version >> 8) & 255;
 *   unsigned patch = version & 255;
 *   printf("http_parser v%u.%u.%u\n", major, minor, version);
 */
unsigned long http_parser_version(void);

void http_parser_init(http_parser *parser, enum http_parser_type type);


size_t http_parser_execute(http_parser *parser,
                           const http_parser_settings *settings,
                           const char *data,
                           size_t len);


/* if http_should_keep_alive() in the on_headers_complete or
 * on_message_complete callback returns 0, then this should be
 * the last message on the connection.
 * if you are the server, respond with the "connection: close" header.
 * if you are the client, close the connection.
 */
int http_should_keep_alive(const http_parser *parser);

/* returns a string version of the http method. */
const char *http_method_str(enum http_method m);

/* return a string name of the given error */
const char *http_errno_name(enum http_errno err);

/* return a string description of the given error */
const char *http_errno_description(enum http_errno err);

/* parse a url; return nonzero on failure */
int http_parser_parse_url(const char *buf, size_t buflen,
                          int is_connect,
                          struct http_parser_url *u);

/* pause or un-pause the parser; a nonzero value pauses */
void http_parser_pause(http_parser *parser, int paused);

/* checks if this is the final chunk of the body. */
int http_body_is_final(const http_parser *parser);

#ifdef __cplusplus
}
#endif
#endif
