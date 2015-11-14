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

/* dump what the parser finds to stdout as it happen */

#include "http_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int on_message_begin(http_parser* _) {
  (void)_;
  printf("\n***message begin***\n\n");
  return 0;
}

int on_headers_complete(http_parser* _) {
  (void)_;
  printf("\n***headers complete***\n\n");
  return 0;
}

int on_message_complete(http_parser* _) {
  (void)_;
  printf("\n***message complete***\n\n");
  return 0;
}

int on_url(http_parser* _, const char* at, size_t length) {
  (void)_;
  printf("url: %.*s\n", (int)length, at);
  return 0;
}

int on_header_field(http_parser* _, const char* at, size_t length) {
  (void)_;
  printf("header field: %.*s\n", (int)length, at);
  return 0;
}

int on_header_value(http_parser* _, const char* at, size_t length) {
  (void)_;
  printf("header value: %.*s\n", (int)length, at);
  return 0;
}

int on_body(http_parser* _, const char* at, size_t length) {
  (void)_;
  printf("body: %.*s\n", (int)length, at);
  return 0;
}

void usage(const char* name) {
  fprintf(stderr,
          "usage: %s $type $filename\n"
          "  type: -x, where x is one of {r,b,q}\n"
          "  parses file as a response, request, or both\n",
          name);
  exit(exit_failure);
}

int main(int argc, char* argv[]) {
  enum http_parser_type file_type;

  if (argc != 3) {
    usage(argv[0]);
  }

  char* type = argv[1];
  if (type[0] != '-') {
    usage(argv[0]);
  }

  switch (type[1]) {
    /* in the case of "-", type[1] will be nul */
    case 'r':
      file_type = http_response;
      break;
    case 'q':
      file_type = http_request;
      break;
    case 'b':
      file_type = http_both;
      break;
    default:
      usage(argv[0]);
  }

  char* filename = argv[2];
  file* file = fopen(filename, "r");
  if (file == null) {
    perror("fopen");
    return exit_failure;
  }

  fseek(file, 0, seek_end);
  long file_length = ftell(file);
  if (file_length == -1) {
    perror("ftell");
    return exit_failure;
  }
  fseek(file, 0, seek_set);

  char* data = malloc(file_length);
  if (fread(data, 1, file_length, file) != (size_t)file_length) {
    fprintf(stderr, "couldn't read entire file\n");
    free(data);
    return exit_failure;
  }

  http_parser_settings settings;
  memset(&settings, 0, sizeof(settings));
  settings.on_message_begin = on_message_begin;
  settings.on_url = on_url;
  settings.on_header_field = on_header_field;
  settings.on_header_value = on_header_value;
  settings.on_headers_complete = on_headers_complete;
  settings.on_body = on_body;
  settings.on_message_complete = on_message_complete;

  http_parser parser;
  http_parser_init(&parser, file_type);
  size_t nparsed = http_parser_execute(&parser, &settings, data, file_length);
  free(data);

  if (nparsed != (size_t)file_length) {
    fprintf(stderr,
            "error: %s (%s)\n",
            http_errno_description(http_parser_errno(&parser)),
            http_errno_name(http_parser_errno(&parser)));
    return exit_failure;
  }

  return exit_success;
}
