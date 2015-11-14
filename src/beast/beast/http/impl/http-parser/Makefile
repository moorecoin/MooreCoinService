# copyright joyent, inc. and other node contributors. all rights reserved.
#
# permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "software"), to
# deal in the software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the software, and to permit persons to whom the software is
# furnished to do so, subject to the following conditions:
#
# the above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the software.
#
# the software is provided "as is", without warranty of any kind, express or
# implied, including but not limited to the warranties of merchantability,
# fitness for a particular purpose and noninfringement. in no event shall the
# authors or copyright holders be liable for any claim, damages or other
# liability, whether in an action of contract, tort or otherwise, arising
# from, out of or in connection with the software or the use or other dealings
# in the software.

platform ?= $(shell sh -c 'uname -s | tr "[a-z]" "[a-z]"')
soname ?= libhttp_parser.so.2.2.1

cc?=gcc
ar?=ar

cppflags += -i.
cppflags_debug = $(cppflags) -dhttp_parser_strict=1
cppflags_debug += $(cppflags_debug_extra)
cppflags_fast = $(cppflags) -dhttp_parser_strict=0
cppflags_fast += $(cppflags_fast_extra)

cflags += -wall -wextra -werror
cflags_debug = $(cflags) -o0 -g $(cflags_debug_extra)
cflags_fast = $(cflags) -o3 $(cflags_fast_extra)
cflags_lib = $(cflags_fast) -fpic

ldflags_lib = $(ldflags) -shared

ifneq (darwin,$(platform))
# todo(bnoordhuis) the native sunos linker expects -h rather than -soname...
ldflags_lib += -wl,-soname=$(soname)
endif

test: test_g test_fast
	./test_g
	./test_fast

test_g: http_parser_g.o test_g.o
	$(cc) $(cflags_debug) $(ldflags) http_parser_g.o test_g.o -o $@

test_g.o: test.c http_parser.h makefile
	$(cc) $(cppflags_debug) $(cflags_debug) -c test.c -o $@

http_parser_g.o: http_parser.c http_parser.h makefile
	$(cc) $(cppflags_debug) $(cflags_debug) -c http_parser.c -o $@

test_fast: http_parser.o test.o http_parser.h
	$(cc) $(cflags_fast) $(ldflags) http_parser.o test.o -o $@

test.o: test.c http_parser.h makefile
	$(cc) $(cppflags_fast) $(cflags_fast) -c test.c -o $@

http_parser.o: http_parser.c http_parser.h makefile
	$(cc) $(cppflags_fast) $(cflags_fast) -c http_parser.c

test-run-timed: test_fast
	while(true) do time ./test_fast > /dev/null; done

test-valgrind: test_g
	valgrind ./test_g

libhttp_parser.o: http_parser.c http_parser.h makefile
	$(cc) $(cppflags_fast) $(cflags_lib) -c http_parser.c -o libhttp_parser.o

library: libhttp_parser.o
	$(cc) $(ldflags_lib) -o $(soname) $<

package: http_parser.o
	$(ar) rcs libhttp_parser.a http_parser.o

url_parser: http_parser.o contrib/url_parser.c
	$(cc) $(cppflags_fast) $(cflags_fast) $^ -o $@

url_parser_g: http_parser_g.o contrib/url_parser.c
	$(cc) $(cppflags_debug) $(cflags_debug) $^ -o $@

parsertrace: http_parser.o contrib/parsertrace.c
	$(cc) $(cppflags_fast) $(cflags_fast) $^ -o parsertrace

parsertrace_g: http_parser_g.o contrib/parsertrace.c
	$(cc) $(cppflags_debug) $(cflags_debug) $^ -o parsertrace_g

tags: http_parser.c http_parser.h test.c
	ctags $^

clean:
	rm -f *.o *.a tags test test_fast test_g \
		http_parser.tar libhttp_parser.so.* \
		url_parser url_parser_g parsertrace parsertrace_g

contrib/url_parser.c:	http_parser.h
contrib/parsertrace.c:	http_parser.h

.phony: clean package test-run test-run-timed test-valgrind
