/* based on src/http/ngx_http_parse.c from nginx copyright igor sysoev
 *
 * additional changes are licensed under the same terms as nginx and
 * copyright joyent, inc. and other node contributors. all rights reserved.
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
#include <assert.h>
#include <stddef.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifndef ullong_max
# define ullong_max ((uint64_t) -1) /* 2^64-1 */
#endif

#ifndef min
# define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef array_size
# define array_size(a) (sizeof(a) / sizeof((a)[0]))
#endif

#ifndef bit_at
# define bit_at(a, i)                                                \
  (!!((unsigned int) (a)[(unsigned int) (i) >> 3] &                  \
   (1 << ((unsigned int) (i) & 7))))
#endif

#ifndef elem_at
# define elem_at(a, i, v) ((unsigned int) (i) < array_size(a) ? (a)[(i)] : (v))
#endif

#define set_errno(e)                                                 \
do {                                                                 \
  parser->http_errno = (e);                                          \
} while(0)


/* run the notify callback for, returning er if it fails */
#define callback_notify_(for, er)                                    \
do {                                                                 \
  assert(http_parser_errno(parser) == hpe_ok);                       \
                                                                     \
  if (settings->on_##for) {                                          \
    if (0 != settings->on_##for(parser)) {                           \
      set_errno(hpe_cb_##for);                                       \
    }                                                                \
                                                                     \
    /* we either errored above or got paused; get out */             \
    if (http_parser_errno(parser) != hpe_ok) {                       \
      return (er);                                                   \
    }                                                                \
  }                                                                  \
} while (0)

/* run the notify callback for and consume the current byte */
#define callback_notify(for)            callback_notify_(for, p - data + 1)

/* run the notify callback for and don't consume the current byte */
#define callback_notify_noadvance(for)  callback_notify_(for, p - data)

/* run data callback for with len bytes, returning er if it fails */
#define callback_data_(for, len, er)                                 \
do {                                                                 \
  assert(http_parser_errno(parser) == hpe_ok);                       \
                                                                     \
  if (for##_mark) {                                                  \
    if (settings->on_##for) {                                        \
      if (0 != settings->on_##for(parser, for##_mark, (len))) {      \
        set_errno(hpe_cb_##for);                                     \
      }                                                              \
                                                                     \
      /* we either errored above or got paused; get out */           \
      if (http_parser_errno(parser) != hpe_ok) {                     \
        return (er);                                                 \
      }                                                              \
    }                                                                \
    for##_mark = null;                                               \
  }                                                                  \
} while (0)
  
/* run the data callback for and consume the current byte */
#define callback_data(for)                                           \
    callback_data_(for, p - for##_mark, p - data + 1)

/* run the data callback for and don't consume the current byte */
#define callback_data_noadvance(for)                                 \
    callback_data_(for, p - for##_mark, p - data)

/* set the mark for; non-destructive if mark is already set */
#define mark(for)                                                    \
do {                                                                 \
  if (!for##_mark) {                                                 \
    for##_mark = p;                                                  \
  }                                                                  \
} while (0)


#define proxy_connection "proxy-connection"
#define connection "connection"
#define content_length "content-length"
#define transfer_encoding "transfer-encoding"
#define upgrade "upgrade"
#define chunked "chunked"
#define keep_alive "keep-alive"
#define close "close"


static const char *method_strings[] =
  {
#define xx(num, name, string) #string,
  http_method_map(xx)
#undef xx
  };


/* tokens as defined by rfc 2616. also lowercases them.
 *        token       = 1*<any char except ctls or separators>
 *     separators     = "(" | ")" | "<" | ">" | "@"
 *                    | "," | ";" | ":" | "\" | <">
 *                    | "/" | "[" | "]" | "?" | "="
 *                    | "{" | "}" | sp | ht
 */
static const char tokens[256] = {
/*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
        0,       0,       0,       0,       0,       0,       0,       0,
/*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
        0,       0,       0,       0,       0,       0,       0,       0,
/*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23 etb */
        0,       0,       0,       0,       0,       0,       0,       0,
/*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
        0,       0,       0,       0,       0,       0,       0,       0,
/*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
        0,      '!',      0,      '#',     '$',     '%',     '&',    '\'',
/*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
        0,       0,      '*',     '+',      0,      '-',     '.',      0,
/*  48  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7  */
       '0',     '1',     '2',     '3',     '4',     '5',     '6',     '7',
/*  56  8    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?  */
       '8',     '9',      0,       0,       0,       0,       0,       0,
/*  64  @    65  a    66  b    67  c    68  d    69  e    70  f    71  g  */
        0,      'a',     'b',     'c',     'd',     'e',     'f',     'g',
/*  72  h    73  i    74  j    75  k    76  l    77  m    78  n    79  o  */
       'h',     'i',     'j',     'k',     'l',     'm',     'n',     'o',
/*  80  p    81  q    82  r    83  s    84  t    85  u    86  v    87  w  */
       'p',     'q',     'r',     's',     't',     'u',     'v',     'w',
/*  88  x    89  y    90  z    91  [    92  \    93  ]    94  ^    95  _  */
       'x',     'y',     'z',      0,       0,       0,      '^',     '_',
/*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
       '`',     'a',     'b',     'c',     'd',     'e',     'f',     'g',
/* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
       'h',     'i',     'j',     'k',     'l',     'm',     'n',     'o',
/* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
       'p',     'q',     'r',     's',     't',     'u',     'v',     'w',
/* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127 del */
       'x',     'y',     'z',      0,      '|',      0,      '~',       0 };


static const int8_t unhex[256] =
  {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  , 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1
  ,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1
  ,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
  };


#if http_parser_strict
# define t(v) 0
#else
# define t(v) v
#endif


static const uint8_t normal_url_char[32] = {
/*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
        0    |   0    |   0    |   0    |   0    |   0    |   0    |   0,
/*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
        0    | t(2)   |   0    |   0    | t(16)  |   0    |   0    |   0,
/*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23 etb */
        0    |   0    |   0    |   0    |   0    |   0    |   0    |   0,
/*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
        0    |   0    |   0    |   0    |   0    |   0    |   0    |   0,
/*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
        0    |   2    |   4    |   0    |   16   |   32   |   64   |  128,
/*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
/*  48  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7  */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
/*  56  8    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?  */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |   0,
/*  64  @    65  a    66  b    67  c    68  d    69  e    70  f    71  g  */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
/*  72  h    73  i    74  j    75  k    76  l    77  m    78  n    79  o  */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
/*  80  p    81  q    82  r    83  s    84  t    85  u    86  v    87  w  */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
/*  88  x    89  y    90  z    91  [    92  \    93  ]    94  ^    95  _  */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
/*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
/* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
/* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |  128,
/* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127 del */
        1    |   2    |   4    |   8    |   16   |   32   |   64   |   0, };

#undef t

enum state
  { s_dead = 1 /* important that this is > 0 */

  , s_start_req_or_res
  , s_res_or_resp_h
  , s_start_res
  , s_res_h
  , s_res_ht
  , s_res_htt
  , s_res_http
  , s_res_first_http_major
  , s_res_http_major
  , s_res_first_http_minor
  , s_res_http_minor
  , s_res_first_status_code
  , s_res_status_code
  , s_res_status_start
  , s_res_status
  , s_res_line_almost_done

  , s_start_req

  , s_req_method
  , s_req_spaces_before_url
  , s_req_schema
  , s_req_schema_slash
  , s_req_schema_slash_slash
  , s_req_server_start
  , s_req_server
  , s_req_server_with_at
  , s_req_path
  , s_req_query_string_start
  , s_req_query_string
  , s_req_fragment_start
  , s_req_fragment
  , s_req_http_start
  , s_req_http_h
  , s_req_http_ht
  , s_req_http_htt
  , s_req_http_http
  , s_req_first_http_major
  , s_req_http_major
  , s_req_first_http_minor
  , s_req_http_minor
  , s_req_line_almost_done

  , s_header_field_start
  , s_header_field
  , s_header_value_start
  , s_header_value
  , s_header_value_lws

  , s_header_almost_done

  , s_chunk_size_start
  , s_chunk_size
  , s_chunk_parameters
  , s_chunk_size_almost_done

  , s_headers_almost_done
  , s_headers_done

  /* important: 's_headers_done' must be the last 'header' state. all
   * states beyond this must be 'body' states. it is used for overflow
   * checking. see the parsing_header() macro.
   */

  , s_chunk_data
  , s_chunk_data_almost_done
  , s_chunk_data_done

  , s_body_identity
  , s_body_identity_eof

  , s_message_done
  };


#define parsing_header(state) (state <= s_headers_done)


enum header_states
  { h_general = 0
  , h_c
  , h_co
  , h_con

  , h_matching_connection
  , h_matching_proxy_connection
  , h_matching_content_length
  , h_matching_transfer_encoding
  , h_matching_upgrade

  , h_connection
  , h_content_length
  , h_transfer_encoding
  , h_upgrade

  , h_matching_transfer_encoding_chunked
  , h_matching_connection_keep_alive
  , h_matching_connection_close

  , h_transfer_encoding_chunked
  , h_connection_keep_alive
  , h_connection_close
  };

enum http_host_state
  {
    s_http_host_dead = 1
  , s_http_userinfo_start
  , s_http_userinfo
  , s_http_host_start
  , s_http_host_v6_start
  , s_http_host
  , s_http_host_v6
  , s_http_host_v6_end
  , s_http_host_port_start
  , s_http_host_port
};

/* macros for character classes; depends on strict-mode  */
#define cr                  '\r'
#define lf                  '\n'
#define lower(c)            (unsigned char)(c | 0x20)
#define is_alpha(c)         (lower(c) >= 'a' && lower(c) <= 'z')
#define is_num(c)           ((c) >= '0' && (c) <= '9')
#define is_alphanum(c)      (is_alpha(c) || is_num(c))
#define is_hex(c)           (is_num(c) || (lower(c) >= 'a' && lower(c) <= 'f'))
#define is_mark(c)          ((c) == '-' || (c) == '_' || (c) == '.' || \
  (c) == '!' || (c) == '~' || (c) == '*' || (c) == '\'' || (c) == '(' || \
  (c) == ')')
#define is_userinfo_char(c) (is_alphanum(c) || is_mark(c) || (c) == '%' || \
  (c) == ';' || (c) == ':' || (c) == '&' || (c) == '=' || (c) == '+' || \
  (c) == '$' || (c) == ',')

#if http_parser_strict
#define token(c)            (tokens[(unsigned char)c])
#define is_url_char(c)      (bit_at(normal_url_char, (unsigned char)c))
#define is_host_char(c)     (is_alphanum(c) || (c) == '.' || (c) == '-')
#else
#define token(c)            ((c == ' ') ? ' ' : tokens[(unsigned char)c])
#define is_url_char(c)                                                         \
  (bit_at(normal_url_char, (unsigned char)c) || ((c) & 0x80))
#define is_host_char(c)                                                        \
  (is_alphanum(c) || (c) == '.' || (c) == '-' || (c) == '_')
#endif


#define start_state (parser->type == http_request ? s_start_req : s_start_res)


#if http_parser_strict
# define strict_check(cond)                                          \
do {                                                                 \
  if (cond) {                                                        \
    set_errno(hpe_strict);                                           \
    goto error;                                                      \
  }                                                                  \
} while (0)
# define new_message() (http_should_keep_alive(parser) ? start_state : s_dead)
#else
# define strict_check(cond)
# define new_message() start_state
#endif


/* map errno values to strings for human-readable output */
#define http_strerror_gen(n, s) { "hpe_" #n, s },
static struct {
  const char *name;
  const char *description;
} http_strerror_tab[] = {
  http_errno_map(http_strerror_gen)
};
#undef http_strerror_gen

int http_message_needs_eof(const http_parser *parser);

/* our url parser.
 *
 * this is designed to be shared by http_parser_execute() for url validation,
 * hence it has a state transition + byte-for-byte interface. in addition, it
 * is meant to be embedded in http_parser_parse_url(), which does the dirty
 * work of turning state transitions url components for its api.
 *
 * this function should only be invoked with non-space characters. it is
 * assumed that the caller cares about (and can detect) the transition between
 * url and non-url states by looking for these.
 */
static enum state
parse_url_char(enum state s, const char ch)
{
  if (ch == ' ' || ch == '\r' || ch == '\n') {
    return s_dead;
  }

#if http_parser_strict
  if (ch == '\t' || ch == '\f') {
    return s_dead;
  }
#endif

  switch (s) {
    case s_req_spaces_before_url:
      /* proxied requests are followed by scheme of an absolute uri (alpha).
       * all methods except connect are followed by '/' or '*'.
       */

      if (ch == '/' || ch == '*') {
        return s_req_path;
      }

      if (is_alpha(ch)) {
        return s_req_schema;
      }

      break;

    case s_req_schema:
      if (is_alpha(ch)) {
        return s;
      }

      if (ch == ':') {
        return s_req_schema_slash;
      }

      break;

    case s_req_schema_slash:
      if (ch == '/') {
        return s_req_schema_slash_slash;
      }

      break;

    case s_req_schema_slash_slash:
      if (ch == '/') {
        return s_req_server_start;
      }

      break;

    case s_req_server_with_at:
      if (ch == '@') {
        return s_dead;
      }

    /* fallthrough */
    case s_req_server_start:
    case s_req_server:
      if (ch == '/') {
        return s_req_path;
      }

      if (ch == '?') {
        return s_req_query_string_start;
      }

      if (ch == '@') {
        return s_req_server_with_at;
      }

      if (is_userinfo_char(ch) || ch == '[' || ch == ']') {
        return s_req_server;
      }

      break;

    case s_req_path:
      if (is_url_char(ch)) {
        return s;
      }

      switch (ch) {
        case '?':
          return s_req_query_string_start;

        case '#':
          return s_req_fragment_start;
      }

      break;

    case s_req_query_string_start:
    case s_req_query_string:
      if (is_url_char(ch)) {
        return s_req_query_string;
      }

      switch (ch) {
        case '?':
          /* allow extra '?' in query string */
          return s_req_query_string;

        case '#':
          return s_req_fragment_start;
      }

      break;

    case s_req_fragment_start:
      if (is_url_char(ch)) {
        return s_req_fragment;
      }

      switch (ch) {
        case '?':
          return s_req_fragment;

        case '#':
          return s;
      }

      break;

    case s_req_fragment:
      if (is_url_char(ch)) {
        return s;
      }

      switch (ch) {
        case '?':
        case '#':
          return s;
      }

      break;

    default:
      break;
  }

  /* we should never fall out of the switch above unless there's an error */
  return s_dead;
}

size_t http_parser_execute (http_parser *parser,
                            const http_parser_settings *settings,
                            const char *data,
                            size_t len)
{
  char c, ch;
  int8_t unhex_val;
  const char *p = data;
  const char *header_field_mark = 0;
  const char *header_value_mark = 0;
  const char *url_mark = 0;
  const char *body_mark = 0;
  const char *status_mark = 0;

  /* we're in an error state. don't bother doing anything. */
  if (http_parser_errno(parser) != hpe_ok) {
    return 0;
  }

  if (len == 0) {
    switch (parser->state) {
      case s_body_identity_eof:
        /* use of callback_notify() here would erroneously return 1 byte read if
         * we got paused.
         */
        callback_notify_noadvance(message_complete);
        return 0;

      case s_dead:
      case s_start_req_or_res:
      case s_start_res:
      case s_start_req:
        return 0;

      default:
        set_errno(hpe_invalid_eof_state);
        return 1;
    }
  }


  if (parser->state == s_header_field)
    header_field_mark = data;
  if (parser->state == s_header_value)
    header_value_mark = data;
  switch (parser->state) {
  case s_req_path:
  case s_req_schema:
  case s_req_schema_slash:
  case s_req_schema_slash_slash:
  case s_req_server_start:
  case s_req_server:
  case s_req_server_with_at:
  case s_req_query_string_start:
  case s_req_query_string:
  case s_req_fragment_start:
  case s_req_fragment:
    url_mark = data;
    break;
  case s_res_status:
    status_mark = data;
    break;
  }

  for (p=data; p != data + len; p++) {
    ch = *p;

    if (parsing_header(parser->state)) {
      ++parser->nread;
      /* don't allow the total size of the http headers (including the status
       * line) to exceed http_max_header_size.  this check is here to protect
       * embedders against denial-of-service attacks where the attacker feeds
       * us a never-ending header that the embedder keeps buffering.
       *
       * this check is arguably the responsibility of embedders but we're doing
       * it on the embedder's behalf because most won't bother and this way we
       * make the web a little safer.  http_max_header_size is still far bigger
       * than any reasonable request or response so this should never affect
       * day-to-day operation.
       */
      if (parser->nread > http_max_header_size) {
        set_errno(hpe_header_overflow);
        goto error;
      }
    }

    reexecute_byte:
    switch (parser->state) {

      case s_dead:
        /* this state is used after a 'connection: close' message
         * the parser will error out if it reads another message
         */
        if (ch == cr || ch == lf)
          break;

        set_errno(hpe_closed_connection);
        goto error;

      case s_start_req_or_res:
      {
        if (ch == cr || ch == lf)
          break;
        parser->flags = 0;
        parser->content_length = ullong_max;

        if (ch == 'h') {
          parser->state = s_res_or_resp_h;

          callback_notify(message_begin);
        } else {
          parser->type = http_request;
          parser->state = s_start_req;
          goto reexecute_byte;
        }

        break;
      }

      case s_res_or_resp_h:
        if (ch == 't') {
          parser->type = http_response;
          parser->state = s_res_ht;
        } else {
          if (ch != 'e') {
            set_errno(hpe_invalid_constant);
            goto error;
          }

          parser->type = http_request;
          parser->method = http_head;
          parser->index = 2;
          parser->state = s_req_method;
        }
        break;

      case s_start_res:
      {
        parser->flags = 0;
        parser->content_length = ullong_max;

        switch (ch) {
          case 'h':
            parser->state = s_res_h;
            break;

          case cr:
          case lf:
            break;

          default:
            set_errno(hpe_invalid_constant);
            goto error;
        }

        callback_notify(message_begin);
        break;
      }

      case s_res_h:
        strict_check(ch != 't');
        parser->state = s_res_ht;
        break;

      case s_res_ht:
        strict_check(ch != 't');
        parser->state = s_res_htt;
        break;

      case s_res_htt:
        strict_check(ch != 'p');
        parser->state = s_res_http;
        break;

      case s_res_http:
        strict_check(ch != '/');
        parser->state = s_res_first_http_major;
        break;

      case s_res_first_http_major:
        if (ch < '0' || ch > '9') {
          set_errno(hpe_invalid_version);
          goto error;
        }

        parser->http_major = ch - '0';
        parser->state = s_res_http_major;
        break;

      /* major http version or dot */
      case s_res_http_major:
      {
        if (ch == '.') {
          parser->state = s_res_first_http_minor;
          break;
        }

        if (!is_num(ch)) {
          set_errno(hpe_invalid_version);
          goto error;
        }

        parser->http_major *= 10;
        parser->http_major += ch - '0';

        if (parser->http_major > 999) {
          set_errno(hpe_invalid_version);
          goto error;
        }

        break;
      }

      /* first digit of minor http version */
      case s_res_first_http_minor:
        if (!is_num(ch)) {
          set_errno(hpe_invalid_version);
          goto error;
        }

        parser->http_minor = ch - '0';
        parser->state = s_res_http_minor;
        break;

      /* minor http version or end of request line */
      case s_res_http_minor:
      {
        if (ch == ' ') {
          parser->state = s_res_first_status_code;
          break;
        }

        if (!is_num(ch)) {
          set_errno(hpe_invalid_version);
          goto error;
        }

        parser->http_minor *= 10;
        parser->http_minor += ch - '0';

        if (parser->http_minor > 999) {
          set_errno(hpe_invalid_version);
          goto error;
        }

        break;
      }

      case s_res_first_status_code:
      {
        if (!is_num(ch)) {
          if (ch == ' ') {
            break;
          }

          set_errno(hpe_invalid_status);
          goto error;
        }
        parser->status_code = ch - '0';
        parser->state = s_res_status_code;
        break;
      }

      case s_res_status_code:
      {
        if (!is_num(ch)) {
          switch (ch) {
            case ' ':
              parser->state = s_res_status_start;
              break;
            case cr:
              parser->state = s_res_line_almost_done;
              break;
            case lf:
              parser->state = s_header_field_start;
              break;
            default:
              set_errno(hpe_invalid_status);
              goto error;
          }
          break;
        }

        parser->status_code *= 10;
        parser->status_code += ch - '0';

        if (parser->status_code > 999) {
          set_errno(hpe_invalid_status);
          goto error;
        }

        break;
      }

      case s_res_status_start:
      {
        if (ch == cr) {
          parser->state = s_res_line_almost_done;
          break;
        }

        if (ch == lf) {
          parser->state = s_header_field_start;
          break;
        }

        mark(status);
        parser->state = s_res_status;
        parser->index = 0;
        break;
      }

      case s_res_status:
        if (ch == cr) {
          parser->state = s_res_line_almost_done;
          callback_data(status);
          break;
        }

        if (ch == lf) {
          parser->state = s_header_field_start;
          callback_data(status);
          break;
        }

        break;

      case s_res_line_almost_done:
        strict_check(ch != lf);
        parser->state = s_header_field_start;
        break;

      case s_start_req:
      {
        if (ch == cr || ch == lf)
          break;
        parser->flags = 0;
        parser->content_length = ullong_max;

        if (!is_alpha(ch)) {
          set_errno(hpe_invalid_method);
          goto error;
        }

        parser->method = (enum http_method) 0;
        parser->index = 1;
        switch (ch) {
          case 'c': parser->method = http_connect; /* or copy, checkout */ break;
          case 'd': parser->method = http_delete; break;
          case 'g': parser->method = http_get; break;
          case 'h': parser->method = http_head; break;
          case 'l': parser->method = http_lock; break;
          case 'm': parser->method = http_mkcol; /* or move, mkactivity, merge, m-search */ break;
          case 'n': parser->method = http_notify; break;
          case 'o': parser->method = http_options; break;
          case 'p': parser->method = http_post;
            /* or propfind|proppatch|put|patch|purge */
            break;
          case 'r': parser->method = http_report; break;
          case 's': parser->method = http_subscribe; /* or search */ break;
          case 't': parser->method = http_trace; break;
          case 'u': parser->method = http_unlock; /* or unsubscribe */ break;
          default:
            set_errno(hpe_invalid_method);
            goto error;
        }
        parser->state = s_req_method;

        callback_notify(message_begin);

        break;
      }

      case s_req_method:
      {
        const char *matcher;
        if (ch == '\0') {
          set_errno(hpe_invalid_method);
          goto error;
        }

        matcher = method_strings[parser->method];
        if (ch == ' ' && matcher[parser->index] == '\0') {
          parser->state = s_req_spaces_before_url;
        } else if (ch == matcher[parser->index]) {
          ; /* nada */
        } else if (parser->method == http_connect) {
          if (parser->index == 1 && ch == 'h') {
            parser->method = http_checkout;
          } else if (parser->index == 2  && ch == 'p') {
            parser->method = http_copy;
          } else {
            set_errno(hpe_invalid_method);
            goto error;
          }
        } else if (parser->method == http_mkcol) {
          if (parser->index == 1 && ch == 'o') {
            parser->method = http_move;
          } else if (parser->index == 1 && ch == 'e') {
            parser->method = http_merge;
          } else if (parser->index == 1 && ch == '-') {
            parser->method = http_msearch;
          } else if (parser->index == 2 && ch == 'a') {
            parser->method = http_mkactivity;
          } else {
            set_errno(hpe_invalid_method);
            goto error;
          }
        } else if (parser->method == http_subscribe) {
          if (parser->index == 1 && ch == 'e') {
            parser->method = http_search;
          } else {
            set_errno(hpe_invalid_method);
            goto error;
          }
        } else if (parser->index == 1 && parser->method == http_post) {
          if (ch == 'r') {
            parser->method = http_propfind; /* or http_proppatch */
          } else if (ch == 'u') {
            parser->method = http_put; /* or http_purge */
          } else if (ch == 'a') {
            parser->method = http_patch;
          } else {
            set_errno(hpe_invalid_method);
            goto error;
          }
        } else if (parser->index == 2) {
          if (parser->method == http_put) {
            if (ch == 'r') {
              parser->method = http_purge;
            } else {
              set_errno(hpe_invalid_method);
              goto error;
            }
          } else if (parser->method == http_unlock) {
            if (ch == 's') {
              parser->method = http_unsubscribe;
            } else {
              set_errno(hpe_invalid_method);
              goto error;
            }
          } else {
            set_errno(hpe_invalid_method);
            goto error;
          }
        } else if (parser->index == 4 && parser->method == http_propfind && ch == 'p') {
          parser->method = http_proppatch;
        } else {
          set_errno(hpe_invalid_method);
          goto error;
        }

        ++parser->index;
        break;
      }

      case s_req_spaces_before_url:
      {
        if (ch == ' ') break;

        mark(url);
        if (parser->method == http_connect) {
          parser->state = s_req_server_start;
        }

        parser->state = parse_url_char((enum state)parser->state, ch);
        if (parser->state == s_dead) {
          set_errno(hpe_invalid_url);
          goto error;
        }

        break;
      }

      case s_req_schema:
      case s_req_schema_slash:
      case s_req_schema_slash_slash:
      case s_req_server_start:
      {
        switch (ch) {
          /* no whitespace allowed here */
          case ' ':
          case cr:
          case lf:
            set_errno(hpe_invalid_url);
            goto error;
          default:
            parser->state = parse_url_char((enum state)parser->state, ch);
            if (parser->state == s_dead) {
              set_errno(hpe_invalid_url);
              goto error;
            }
        }

        break;
      }

      case s_req_server:
      case s_req_server_with_at:
      case s_req_path:
      case s_req_query_string_start:
      case s_req_query_string:
      case s_req_fragment_start:
      case s_req_fragment:
      {
        switch (ch) {
          case ' ':
            parser->state = s_req_http_start;
            callback_data(url);
            break;
          case cr:
          case lf:
            parser->http_major = 0;
            parser->http_minor = 9;
            parser->state = (ch == cr) ?
              s_req_line_almost_done :
              s_header_field_start;
            callback_data(url);
            break;
          default:
            parser->state = parse_url_char((enum state)parser->state, ch);
            if (parser->state == s_dead) {
              set_errno(hpe_invalid_url);
              goto error;
            }
        }
        break;
      }

      case s_req_http_start:
        switch (ch) {
          case 'h':
            parser->state = s_req_http_h;
            break;
          case ' ':
            break;
          default:
            set_errno(hpe_invalid_constant);
            goto error;
        }
        break;

      case s_req_http_h:
        strict_check(ch != 't');
        parser->state = s_req_http_ht;
        break;

      case s_req_http_ht:
        strict_check(ch != 't');
        parser->state = s_req_http_htt;
        break;

      case s_req_http_htt:
        strict_check(ch != 'p');
        parser->state = s_req_http_http;
        break;

      case s_req_http_http:
        strict_check(ch != '/');
        parser->state = s_req_first_http_major;
        break;

      /* first digit of major http version */
      case s_req_first_http_major:
        if (ch < '1' || ch > '9') {
          set_errno(hpe_invalid_version);
          goto error;
        }

        parser->http_major = ch - '0';
        parser->state = s_req_http_major;
        break;

      /* major http version or dot */
      case s_req_http_major:
      {
        if (ch == '.') {
          parser->state = s_req_first_http_minor;
          break;
        }

        if (!is_num(ch)) {
          set_errno(hpe_invalid_version);
          goto error;
        }

        parser->http_major *= 10;
        parser->http_major += ch - '0';

        if (parser->http_major > 999) {
          set_errno(hpe_invalid_version);
          goto error;
        }

        break;
      }

      /* first digit of minor http version */
      case s_req_first_http_minor:
        if (!is_num(ch)) {
          set_errno(hpe_invalid_version);
          goto error;
        }

        parser->http_minor = ch - '0';
        parser->state = s_req_http_minor;
        break;

      /* minor http version or end of request line */
      case s_req_http_minor:
      {
        if (ch == cr) {
          parser->state = s_req_line_almost_done;
          break;
        }

        if (ch == lf) {
          parser->state = s_header_field_start;
          break;
        }

        /* xxx allow spaces after digit? */

        if (!is_num(ch)) {
          set_errno(hpe_invalid_version);
          goto error;
        }

        parser->http_minor *= 10;
        parser->http_minor += ch - '0';

        if (parser->http_minor > 999) {
          set_errno(hpe_invalid_version);
          goto error;
        }

        break;
      }

      /* end of request line */
      case s_req_line_almost_done:
      {
        if (ch != lf) {
          set_errno(hpe_lf_expected);
          goto error;
        }

        parser->state = s_header_field_start;
        break;
      }

      case s_header_field_start:
      {
        if (ch == cr) {
          parser->state = s_headers_almost_done;
          break;
        }

        if (ch == lf) {
          /* they might be just sending \n instead of \r\n so this would be
           * the second \n to denote the end of headers*/
          parser->state = s_headers_almost_done;
          goto reexecute_byte;
        }

        c = token(ch);

        if (!c) {
          set_errno(hpe_invalid_header_token);
          goto error;
        }

        mark(header_field);

        parser->index = 0;
        parser->state = s_header_field;

        switch (c) {
          case 'c':
            parser->header_state = h_c;
            break;

          case 'p':
            parser->header_state = h_matching_proxy_connection;
            break;

          case 't':
            parser->header_state = h_matching_transfer_encoding;
            break;

          case 'u':
            parser->header_state = h_matching_upgrade;
            break;

          default:
            parser->header_state = h_general;
            break;
        }
        break;
      }

      case s_header_field:
      {
        c = token(ch);

        if (c) {
          switch (parser->header_state) {
            case h_general:
              break;

            case h_c:
              parser->index++;
              parser->header_state = (c == 'o' ? h_co : h_general);
              break;

            case h_co:
              parser->index++;
              parser->header_state = (c == 'n' ? h_con : h_general);
              break;

            case h_con:
              parser->index++;
              switch (c) {
                case 'n':
                  parser->header_state = h_matching_connection;
                  break;
                case 't':
                  parser->header_state = h_matching_content_length;
                  break;
                default:
                  parser->header_state = h_general;
                  break;
              }
              break;

            /* connection */

            case h_matching_connection:
              parser->index++;
              if (parser->index > sizeof(connection)-1
                  || c != connection[parser->index]) {
                parser->header_state = h_general;
              } else if (parser->index == sizeof(connection)-2) {
                parser->header_state = h_connection;
              }
              break;

            /* proxy-connection */

            case h_matching_proxy_connection:
              parser->index++;
              if (parser->index > sizeof(proxy_connection)-1
                  || c != proxy_connection[parser->index]) {
                parser->header_state = h_general;
              } else if (parser->index == sizeof(proxy_connection)-2) {
                parser->header_state = h_connection;
              }
              break;

            /* content-length */

            case h_matching_content_length:
              parser->index++;
              if (parser->index > sizeof(content_length)-1
                  || c != content_length[parser->index]) {
                parser->header_state = h_general;
              } else if (parser->index == sizeof(content_length)-2) {
                parser->header_state = h_content_length;
              }
              break;

            /* transfer-encoding */

            case h_matching_transfer_encoding:
              parser->index++;
              if (parser->index > sizeof(transfer_encoding)-1
                  || c != transfer_encoding[parser->index]) {
                parser->header_state = h_general;
              } else if (parser->index == sizeof(transfer_encoding)-2) {
                parser->header_state = h_transfer_encoding;
              }
              break;

            /* upgrade */

            case h_matching_upgrade:
              parser->index++;
              if (parser->index > sizeof(upgrade)-1
                  || c != upgrade[parser->index]) {
                parser->header_state = h_general;
              } else if (parser->index == sizeof(upgrade)-2) {
                parser->header_state = h_upgrade;
              }
              break;

            case h_connection:
            case h_content_length:
            case h_transfer_encoding:
            case h_upgrade:
              if (ch != ' ') parser->header_state = h_general;
              break;

            default:
              assert(0 && "unknown header_state");
              break;
          }
          break;
        }

        if (ch == ':') {
          parser->state = s_header_value_start;
          callback_data(header_field);
          break;
        }

        if (ch == cr) {
          parser->state = s_header_almost_done;
          callback_data(header_field);
          break;
        }

        if (ch == lf) {
          parser->state = s_header_field_start;
          callback_data(header_field);
          break;
        }

        set_errno(hpe_invalid_header_token);
        goto error;
      }

      case s_header_value_start:
      {
        if (ch == ' ' || ch == '\t') break;

        mark(header_value);

        parser->state = s_header_value;
        parser->index = 0;

        if (ch == cr) {
          parser->header_state = h_general;
          parser->state = s_header_almost_done;
          callback_data(header_value);
          break;
        }

        if (ch == lf) {
          parser->state = s_header_field_start;
          callback_data(header_value);
          break;
        }

        c = lower(ch);

        switch (parser->header_state) {
          case h_upgrade:
            parser->flags |= f_upgrade;
            parser->header_state = h_general;
            break;

          case h_transfer_encoding:
            /* looking for 'transfer-encoding: chunked' */
            if ('c' == c) {
              parser->header_state = h_matching_transfer_encoding_chunked;
            } else {
              parser->header_state = h_general;
            }
            break;

          case h_content_length:
            if (!is_num(ch)) {
              set_errno(hpe_invalid_content_length);
              goto error;
            }

            parser->content_length = ch - '0';
            break;

          case h_connection:
            /* looking for 'connection: keep-alive' */
            if (c == 'k') {
              parser->header_state = h_matching_connection_keep_alive;
            /* looking for 'connection: close' */
            } else if (c == 'c') {
              parser->header_state = h_matching_connection_close;
            } else {
              parser->header_state = h_general;
            }
            break;

          default:
            parser->header_state = h_general;
            break;
        }
        break;
      }

      case s_header_value:
      {

        if (ch == cr) {
          parser->state = s_header_almost_done;
          callback_data(header_value);
          break;
        }

        if (ch == lf) {
          parser->state = s_header_almost_done;
          callback_data_noadvance(header_value);
          goto reexecute_byte;
        }

        c = lower(ch);

        switch (parser->header_state) {
          case h_general:
            break;

          case h_connection:
          case h_transfer_encoding:
            assert(0 && "shouldn't get here.");
            break;

          case h_content_length:
          {
            uint64_t t;

            if (ch == ' ') break;

            if (!is_num(ch)) {
              set_errno(hpe_invalid_content_length);
              goto error;
            }

            t = parser->content_length;
            t *= 10;
            t += ch - '0';

            /* overflow? test against a conservative limit for simplicity. */
            if ((ullong_max - 10) / 10 < parser->content_length) {
              set_errno(hpe_invalid_content_length);
              goto error;
            }

            parser->content_length = t;
            break;
          }

          /* transfer-encoding: chunked */
          case h_matching_transfer_encoding_chunked:
            parser->index++;
            if (parser->index > sizeof(chunked)-1
                || c != chunked[parser->index]) {
              parser->header_state = h_general;
            } else if (parser->index == sizeof(chunked)-2) {
              parser->header_state = h_transfer_encoding_chunked;
            }
            break;

          /* looking for 'connection: keep-alive' */
          case h_matching_connection_keep_alive:
            parser->index++;
            if (parser->index > sizeof(keep_alive)-1
                || c != keep_alive[parser->index]) {
              parser->header_state = h_general;
            } else if (parser->index == sizeof(keep_alive)-2) {
              parser->header_state = h_connection_keep_alive;
            }
            break;

          /* looking for 'connection: close' */
          case h_matching_connection_close:
            parser->index++;
            if (parser->index > sizeof(close)-1 || c != close[parser->index]) {
              parser->header_state = h_general;
            } else if (parser->index == sizeof(close)-2) {
              parser->header_state = h_connection_close;
            }
            break;

          case h_transfer_encoding_chunked:
          case h_connection_keep_alive:
          case h_connection_close:
            if (ch != ' ') parser->header_state = h_general;
            break;

          default:
            parser->state = s_header_value;
            parser->header_state = h_general;
            break;
        }
        break;
      }

      case s_header_almost_done:
      {
        strict_check(ch != lf);

        parser->state = s_header_value_lws;

        switch (parser->header_state) {
          case h_connection_keep_alive:
            parser->flags |= f_connection_keep_alive;
            break;
          case h_connection_close:
            parser->flags |= f_connection_close;
            break;
          case h_transfer_encoding_chunked:
            parser->flags |= f_chunked;
            break;
          default:
            break;
        }

        break;
      }

      case s_header_value_lws:
      {
        if (ch == ' ' || ch == '\t')
          parser->state = s_header_value_start;
        else
        {
          parser->state = s_header_field_start;
          goto reexecute_byte;
        }
        break;
      }

      case s_headers_almost_done:
      {
        strict_check(ch != lf);

        if (parser->flags & f_trailing) {
          /* end of a chunked request */
          parser->state = new_message();
          callback_notify(message_complete);
          break;
        }

        parser->state = s_headers_done;

        /* set this here so that on_headers_complete() callbacks can see it */
        parser->upgrade =
          (parser->flags & f_upgrade || parser->method == http_connect);

        /* here we call the headers_complete callback. this is somewhat
         * different than other callbacks because if the user returns 1, we
         * will interpret that as saying that this message has no body. this
         * is needed for the annoying case of recieving a response to a head
         * request.
         *
         * we'd like to use callback_notify_noadvance() here but we cannot, so
         * we have to simulate it by handling a change in errno below.
         */
        if (settings->on_headers_complete) {
          switch (settings->on_headers_complete(parser)) {
            case 0:
              break;

            case 1:
              parser->flags |= f_skipbody;
              break;

            default:
              set_errno(hpe_cb_headers_complete);
              return p - data; /* error */
          }
        }

        if (http_parser_errno(parser) != hpe_ok) {
          return p - data;
        }

        goto reexecute_byte;
      }

      case s_headers_done:
      {
        strict_check(ch != lf);

        parser->nread = 0;

        /* exit, the rest of the connect is in a different protocol. */
        if (parser->upgrade) {
          parser->state = new_message();
          callback_notify(message_complete);
          return (p - data) + 1;
        }

        if (parser->flags & f_skipbody) {
          parser->state = new_message();
          callback_notify(message_complete);
        } else if (parser->flags & f_chunked) {
          /* chunked encoding - ignore content-length header */
          parser->state = s_chunk_size_start;
        } else {
          if (parser->content_length == 0) {
            /* content-length header given but zero: content-length: 0\r\n */
            parser->state = new_message();
            callback_notify(message_complete);
          } else if (parser->content_length != ullong_max) {
            /* content-length header given and non-zero */
            parser->state = s_body_identity;
          } else {
            if (parser->type == http_request ||
                !http_message_needs_eof(parser)) {
              /* assume content-length 0 - read the next */
              parser->state = new_message();
              callback_notify(message_complete);
            } else {
              /* read body until eof */
              parser->state = s_body_identity_eof;
            }
          }
        }

        break;
      }

      case s_body_identity:
      {
        uint64_t to_read = min(parser->content_length,
                               (uint64_t) ((data + len) - p));

        assert(parser->content_length != 0
            && parser->content_length != ullong_max);

        /* the difference between advancing content_length and p is because
         * the latter will automaticaly advance on the next loop iteration.
         * further, if content_length ends up at 0, we want to see the last
         * byte again for our message complete callback.
         */
        mark(body);
        parser->content_length -= to_read;
        p += to_read - 1;

        if (parser->content_length == 0) {
          parser->state = s_message_done;

          /* mimic callback_data_noadvance() but with one extra byte.
           *
           * the alternative to doing this is to wait for the next byte to
           * trigger the data callback, just as in every other case. the
           * problem with this is that this makes it difficult for the test
           * harness to distinguish between complete-on-eof and
           * complete-on-length. it's not clear that this distinction is
           * important for applications, but let's keep it for now.
           */
          callback_data_(body, p - body_mark + 1, p - data);
          goto reexecute_byte;
        }

        break;
      }

      /* read until eof */
      case s_body_identity_eof:
        mark(body);
        p = data + len - 1;

        break;

      case s_message_done:
        parser->state = new_message();
        callback_notify(message_complete);
        break;

      case s_chunk_size_start:
      {
        assert(parser->nread == 1);
        assert(parser->flags & f_chunked);

        unhex_val = unhex[(unsigned char)ch];
        if (unhex_val == -1) {
          set_errno(hpe_invalid_chunk_size);
          goto error;
        }

        parser->content_length = unhex_val;
        parser->state = s_chunk_size;
        break;
      }

      case s_chunk_size:
      {
        uint64_t t;

        assert(parser->flags & f_chunked);

        if (ch == cr) {
          parser->state = s_chunk_size_almost_done;
          break;
        }

        unhex_val = unhex[(unsigned char)ch];

        if (unhex_val == -1) {
          if (ch == ';' || ch == ' ') {
            parser->state = s_chunk_parameters;
            break;
          }

          set_errno(hpe_invalid_chunk_size);
          goto error;
        }

        t = parser->content_length;
        t *= 16;
        t += unhex_val;

        /* overflow? test against a conservative limit for simplicity. */
        if ((ullong_max - 16) / 16 < parser->content_length) {
          set_errno(hpe_invalid_content_length);
          goto error;
        }

        parser->content_length = t;
        break;
      }

      case s_chunk_parameters:
      {
        assert(parser->flags & f_chunked);
        /* just ignore this shit. todo check for overflow */
        if (ch == cr) {
          parser->state = s_chunk_size_almost_done;
          break;
        }
        break;
      }

      case s_chunk_size_almost_done:
      {
        assert(parser->flags & f_chunked);
        strict_check(ch != lf);

        parser->nread = 0;

        if (parser->content_length == 0) {
          parser->flags |= f_trailing;
          parser->state = s_header_field_start;
        } else {
          parser->state = s_chunk_data;
        }
        break;
      }

      case s_chunk_data:
      {
        uint64_t to_read = min(parser->content_length,
                               (uint64_t) ((data + len) - p));

        assert(parser->flags & f_chunked);
        assert(parser->content_length != 0
            && parser->content_length != ullong_max);

        /* see the explanation in s_body_identity for why the content
         * length and data pointers are managed this way.
         */
        mark(body);
        parser->content_length -= to_read;
        p += to_read - 1;

        if (parser->content_length == 0) {
          parser->state = s_chunk_data_almost_done;
        }

        break;
      }

      case s_chunk_data_almost_done:
        assert(parser->flags & f_chunked);
        assert(parser->content_length == 0);
        strict_check(ch != cr);
        parser->state = s_chunk_data_done;
        callback_data(body);
        break;

      case s_chunk_data_done:
        assert(parser->flags & f_chunked);
        strict_check(ch != lf);
        parser->nread = 0;
        parser->state = s_chunk_size_start;
        break;

      default:
        assert(0 && "unhandled state");
        set_errno(hpe_invalid_internal_state);
        goto error;
    }
  }

  /* run callbacks for any marks that we have leftover after we ran our of
   * bytes. there should be at most one of these set, so it's ok to invoke
   * them in series (unset marks will not result in callbacks).
   *
   * we use the noadvance() variety of callbacks here because 'p' has already
   * overflowed 'data' and this allows us to correct for the off-by-one that
   * we'd otherwise have (since callback_data() is meant to be run with a 'p'
   * value that's in-bounds).
   */

  assert(((header_field_mark ? 1 : 0) +
          (header_value_mark ? 1 : 0) +
          (url_mark ? 1 : 0)  +
          (body_mark ? 1 : 0) +
          (status_mark ? 1 : 0)) <= 1);

  callback_data_noadvance(header_field);
  callback_data_noadvance(header_value);
  callback_data_noadvance(url);
  callback_data_noadvance(body);
  callback_data_noadvance(status);

  return len;

error:
  if (http_parser_errno(parser) == hpe_ok) {
    set_errno(hpe_unknown);
  }

  return (p - data);
}


/* does the parser need to see an eof to find the end of the message? */
int
http_message_needs_eof (const http_parser *parser)
{
  if (parser->type == http_request) {
    return 0;
  }

  /* see rfc 2616 section 4.4 */
  if (parser->status_code / 100 == 1 || /* 1xx e.g. continue */
      parser->status_code == 204 ||     /* no content */
      parser->status_code == 304 ||     /* not modified */
      parser->flags & f_skipbody) {     /* response to a head request */
    return 0;
  }

  if ((parser->flags & f_chunked) || parser->content_length != ullong_max) {
    return 0;
  }

  return 1;
}


int
http_should_keep_alive (const http_parser *parser)
{
  if (parser->http_major > 0 && parser->http_minor > 0) {
    /* http/1.1 */
    if (parser->flags & f_connection_close) {
      return 0;
    }
  } else {
    /* http/1.0 or earlier */
    if (!(parser->flags & f_connection_keep_alive)) {
      return 0;
    }
  }

  return !http_message_needs_eof(parser);
}


const char *
http_method_str (enum http_method m)
{
  return elem_at(method_strings, m, "<unknown>");
}


void
http_parser_init (http_parser *parser, enum http_parser_type t)
{
  void *data = parser->data; /* preserve application data */
  memset(parser, 0, sizeof(*parser));
  parser->data = data;
  parser->type = t;
  parser->state = (t == http_request ? s_start_req : (t == http_response ? s_start_res : s_start_req_or_res));
  parser->http_errno = hpe_ok;
}

const char *
http_errno_name(enum http_errno err) {
  assert(err < (sizeof(http_strerror_tab)/sizeof(http_strerror_tab[0])));
  return http_strerror_tab[err].name;
}

const char *
http_errno_description(enum http_errno err) {
  assert(err < (sizeof(http_strerror_tab)/sizeof(http_strerror_tab[0])));
  return http_strerror_tab[err].description;
}

static enum http_host_state
http_parse_host_char(enum http_host_state s, const char ch) {
  switch(s) {
    case s_http_userinfo:
    case s_http_userinfo_start:
      if (ch == '@') {
        return s_http_host_start;
      }

      if (is_userinfo_char(ch)) {
        return s_http_userinfo;
      }
      break;

    case s_http_host_start:
      if (ch == '[') {
        return s_http_host_v6_start;
      }

      if (is_host_char(ch)) {
        return s_http_host;
      }

      break;

    case s_http_host:
      if (is_host_char(ch)) {
        return s_http_host;
      }

    /* fallthrough */
    case s_http_host_v6_end:
      if (ch == ':') {
        return s_http_host_port_start;
      }

      break;

    case s_http_host_v6:
      if (ch == ']') {
        return s_http_host_v6_end;
      }

    /* fallthrough */
    case s_http_host_v6_start:
      if (is_hex(ch) || ch == ':' || ch == '.') {
        return s_http_host_v6;
      }

      break;

    case s_http_host_port:
    case s_http_host_port_start:
      if (is_num(ch)) {
        return s_http_host_port;
      }

      break;

    default:
      break;
  }
  return s_http_host_dead;
}

static int
http_parse_host(const char * buf, struct http_parser_url *u, int found_at) {
  enum http_host_state s;

  const char *p;
  size_t buflen = u->field_data[uf_host].off + u->field_data[uf_host].len;

  u->field_data[uf_host].len = 0;

  s = found_at ? s_http_userinfo_start : s_http_host_start;

  for (p = buf + u->field_data[uf_host].off; p < buf + buflen; p++) {
    enum http_host_state new_s = http_parse_host_char(s, *p);

    if (new_s == s_http_host_dead) {
      return 1;
    }

    switch(new_s) {
      case s_http_host:
        if (s != s_http_host) {
          u->field_data[uf_host].off = p - buf;
        }
        u->field_data[uf_host].len++;
        break;

      case s_http_host_v6:
        if (s != s_http_host_v6) {
          u->field_data[uf_host].off = p - buf;
        }
        u->field_data[uf_host].len++;
        break;

      case s_http_host_port:
        if (s != s_http_host_port) {
          u->field_data[uf_port].off = p - buf;
          u->field_data[uf_port].len = 0;
          u->field_set |= (1 << uf_port);
        }
        u->field_data[uf_port].len++;
        break;

      case s_http_userinfo:
        if (s != s_http_userinfo) {
          u->field_data[uf_userinfo].off = p - buf ;
          u->field_data[uf_userinfo].len = 0;
          u->field_set |= (1 << uf_userinfo);
        }
        u->field_data[uf_userinfo].len++;
        break;

      default:
        break;
    }
    s = new_s;
  }

  /* make sure we don't end somewhere unexpected */
  switch (s) {
    case s_http_host_start:
    case s_http_host_v6_start:
    case s_http_host_v6:
    case s_http_host_port_start:
    case s_http_userinfo:
    case s_http_userinfo_start:
      return 1;
    default:
      break;
  }

  return 0;
}

int
http_parser_parse_url(const char *buf, size_t buflen, int is_connect,
                      struct http_parser_url *u)
{
  enum state s;
  const char *p;
  enum http_parser_url_fields uf, old_uf;
  int found_at = 0;

  u->port = u->field_set = 0;
  s = is_connect ? s_req_server_start : s_req_spaces_before_url;
  uf = old_uf = uf_max;

  for (p = buf; p < buf + buflen; p++) {
    s = parse_url_char(s, *p);

    /* figure out the next field that we're operating on */
    switch (s) {
      case s_dead:
        return 1;

      /* skip delimeters */
      case s_req_schema_slash:
      case s_req_schema_slash_slash:
      case s_req_server_start:
      case s_req_query_string_start:
      case s_req_fragment_start:
        continue;

      case s_req_schema:
        uf = uf_schema;
        break;

      case s_req_server_with_at:
        found_at = 1;

      /* falltrough */
      case s_req_server:
        uf = uf_host;
        break;

      case s_req_path:
        uf = uf_path;
        break;

      case s_req_query_string:
        uf = uf_query;
        break;

      case s_req_fragment:
        uf = uf_fragment;
        break;

      default:
        assert(!"unexpected state");
        return 1;
    }

    /* nothing's changed; soldier on */
    if (uf == old_uf) {
      u->field_data[uf].len++;
      continue;
    }

    u->field_data[uf].off = p - buf;
    u->field_data[uf].len = 1;

    u->field_set |= (1 << uf);
    old_uf = uf;
  }

  /* host must be present if there is a schema */
  /* parsing http:///toto will fail */
  if ((u->field_set & ((1 << uf_schema) | (1 << uf_host))) != 0) {
    if (http_parse_host(buf, u, found_at) != 0) {
      return 1;
    }
  }

  /* connect requests can only contain "hostname:port" */
  if (is_connect && u->field_set != ((1 << uf_host)|(1 << uf_port))) {
    return 1;
  }

  if (u->field_set & (1 << uf_port)) {
    /* don't bother with endp; we've already validated the string */
    unsigned long v = strtoul(buf + u->field_data[uf_port].off, null, 10);

    /* ports have a max value of 2^16 */
    if (v > 0xffff) {
      return 1;
    }

    u->port = (uint16_t) v;
  }

  return 0;
}

void
http_parser_pause(http_parser *parser, int paused) {
  /* users should only be pausing/unpausing a parser that is not in an error
   * state. in non-debug builds, there's not much that we can do about this
   * other than ignore it.
   */
  if (http_parser_errno(parser) == hpe_ok ||
      http_parser_errno(parser) == hpe_paused) {
    set_errno((paused) ? hpe_paused : hpe_ok);
  } else {
    assert(0 && "attempting to pause parser in error state");
  }
}

int
http_body_is_final(const struct http_parser *parser) {
    return parser->state == s_message_done;
}

unsigned long
http_parser_version(void) {
  return http_parser_version_major * 0x10000 |
         http_parser_version_minor * 0x00100 |
         http_parser_version_patch * 0x00001;
}
