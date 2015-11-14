# ##########################################################################
# lz4 programs - makefile
# copyright (c) yann collet 2011-2015
#
# gpl v2 license
#
# this program is free software; you can redistribute it and/or modify
# it under the terms of the gnu general public license as published by
# the free software foundation; either version 2 of the license, or
# (at your option) any later version.
#
# this program is distributed in the hope that it will be useful,
# but without any warranty; without even the implied warranty of
# merchantability or fitness for a particular purpose.  see the
# gnu general public license for more details.
#
# you should have received a copy of the gnu general public license along
# with this program; if not, write to the free software foundation, inc.,
# 51 franklin street, fifth floor, boston, ma 02110-1301 usa.
#
# you can contact the author at :
#  - lz4 source repository : http://code.google.com/p/lz4/
#  - lz4 forum froup : https://groups.google.com/forum/#!forum/lz4c
# ##########################################################################
# lz4 : command line utility, supporting gzip-like arguments
# lz4c  : clu, supporting also legacy lz4demo arguments
# lz4c32: same as lz4c, but forced to compile in 32-bits mode
# fuzzer  : test tool, to check lz4 integrity on target platform
# fuzzer32: same as fuzzer, but forced to compile in 32-bits mode
# fullbench  : precisely measure speed for each lz4 function variant
# fullbench32: same as fullbench, but forced to compile in 32-bits mode
# ##########################################################################

release?= r126

destdir?=
prefix ?= /usr
cflags ?= -o3
cflags += -std=c99 -wall -wextra -wundef -wshadow -wcast-align -wstrict-prototypes -pedantic -dlz4_version=\"$(release)\"
flags   = -i../lib $(cppflags) $(cflags) $(ldflags)

bindir=$(prefix)/bin
mandir=$(prefix)/share/man/man1
lz4dir=../lib

test_files = copying
test_targets=test-native


# define *.exe as extension for windows systems
ifneq (,$(filter windows%,$(os)))
ext =.exe
void = nul
else
ext =
void = /dev/null
endif


# select test target for travis ci's build matrix
travis_target=$(lz4_travis_ci_env)


default: lz4 lz4c

all: lz4 lz4c lz4c32 fullbench fullbench32 fuzzer fuzzer32 frametest frametest32 datagen

lz4: $(lz4dir)/lz4.c $(lz4dir)/lz4hc.c $(lz4dir)/lz4frame.c $(lz4dir)/xxhash.c bench.c lz4io.c lz4cli.c
	$(cc)      $(flags) $^ -o $@$(ext)

lz4c  : $(lz4dir)/lz4.c $(lz4dir)/lz4hc.c $(lz4dir)/lz4frame.c $(lz4dir)/xxhash.c bench.c lz4io.c lz4cli.c
	$(cc)      $(flags) -denable_lz4c_legacy_options $^ -o $@$(ext)

lz4c32: $(lz4dir)/lz4.c $(lz4dir)/lz4hc.c $(lz4dir)/lz4frame.c $(lz4dir)/xxhash.c bench.c lz4io.c lz4cli.c
	$(cc) -m32 $(flags) -denable_lz4c_legacy_options $^ -o $@$(ext)

fullbench  : $(lz4dir)/lz4.c $(lz4dir)/lz4hc.c $(lz4dir)/lz4frame.c $(lz4dir)/xxhash.c fullbench.c
	$(cc)      $(flags) $^ -o $@$(ext)

fullbench32: $(lz4dir)/lz4.c $(lz4dir)/lz4hc.c $(lz4dir)/lz4frame.c $(lz4dir)/xxhash.c fullbench.c
	$(cc) -m32 $(flags) $^ -o $@$(ext)

fuzzer  : $(lz4dir)/lz4.c $(lz4dir)/lz4hc.c $(lz4dir)/xxhash.c fuzzer.c
	$(cc)      $(flags) $^ -o $@$(ext)

fuzzer32: $(lz4dir)/lz4.c $(lz4dir)/lz4hc.c $(lz4dir)/xxhash.c fuzzer.c
	$(cc) -m32 $(flags) $^ -o $@$(ext)

frametest: $(lz4dir)/lz4frame.c $(lz4dir)/lz4.c $(lz4dir)/lz4hc.c $(lz4dir)/xxhash.c frametest.c
	$(cc)      $(flags) $^ -o $@$(ext)

frametest32: $(lz4dir)/lz4frame.c $(lz4dir)/lz4.c $(lz4dir)/lz4hc.c $(lz4dir)/xxhash.c frametest.c
	$(cc) -m32 $(flags) $^ -o $@$(ext)

datagen : datagen.c
	$(cc)      $(flags) $^ -o $@$(ext)


clean:
	@rm -f core *.o *.test \
        lz4$(ext) lz4c$(ext) lz4c32$(ext) \
        fullbench$(ext) fullbench32$(ext) \
        fuzzer$(ext) fuzzer32$(ext) \
        frametest$(ext) frametest32$(ext) \
        datagen$(ext)
	@echo cleaning completed


#------------------------------------------------------------------------
#make install is validated only for linux, osx, kfreebsd and hurd targets
ifneq (,$(filter $(shell uname),linux darwin gnu/kfreebsd gnu))

install: lz4 lz4c
	@echo installing binaries
	@install -d -m 755 $(destdir)$(bindir)/ $(destdir)$(mandir)/
	@install -m 755 lz4$(ext) $(destdir)$(bindir)/lz4$(ext)
	@ln -sf lz4$(ext) $(destdir)$(bindir)/lz4cat
	@install -m 755 lz4c$(ext) $(destdir)$(bindir)/lz4c$(ext)
	@echo installing man pages
	@install -m 644 lz4.1 $(destdir)$(mandir)/lz4.1
	@install -m 644 lz4c.1 $(destdir)$(mandir)/lz4c.1
	@install -m 644 lz4cat.1 $(destdir)$(mandir)/lz4cat.1
	@echo lz4 installation completed

uninstall:
	rm -f $(destdir)$(bindir)/lz4cat
	[ -x $(destdir)$(bindir)/lz4$(ext) ] && rm -f $(destdir)$(bindir)/lz4$(ext)
	[ -x $(destdir)$(bindir)/lz4c$(ext) ] && rm -f $(destdir)$(bindir)/lz4c$(ext)
	[ -f $(destdir)$(mandir)/lz4.1 ] && rm -f $(destdir)$(mandir)/lz4.1
	[ -f $(destdir)$(mandir)/lz4c.1 ] && rm -f $(destdir)$(mandir)/lz4c.1
	[ -f $(destdir)$(mandir)/lz4cat.1 ] && rm -f $(destdir)$(mandir)/lz4cat.1
	@echo lz4 programs successfully uninstalled

test: test-lz4 test-lz4c test-frametest test-fullbench test-fuzzer test-mem

test32: test-lz4c32 test-frametest32 test-fullbench32 test-fuzzer32 test-mem32

test-all: test test32

test-travis: $(travis_target)

test-lz4: lz4 datagen
	./datagen -g16kb  | ./lz4 -9     | ./lz4 -vdq > $(void)
	./datagen         | ./lz4        | ./lz4 -vdq > $(void)
	./datagen -g6m -p100 | ./lz4 -9bd | ./lz4 -vdq > $(void)
	./datagen -g256mb | ./lz4 -vqb4d | ./lz4 -vdq > $(void)
	./datagen -g6gb   | ./lz4 -vqb5d | ./lz4 -vdq > $(void)
# test frame concatenation with null-length frame
	@echo -n > empty.test
	@echo hi > nonempty.test
	cat nonempty.test empty.test nonempty.test > orig.test
	@./lz4 -zq empty.test > empty.lz4.test
	@./lz4 -zq nonempty.test > nonempty.lz4.test
	cat nonempty.lz4.test empty.lz4.test nonempty.lz4.test > concat.lz4.test
	./lz4 -d concat.lz4.test > result.test
	sdiff orig.test result.test
	@rm *.test
	@echo frame concatenation test completed
# test frame concatenation with null-length frame


test-lz4c: lz4c datagen
	./datagen -g256mb | ./lz4c -l -v    | ./lz4c   -vdq > $(void)

test-lz4c32: lz4 lz4c32 lz4 datagen
	./datagen -g16kb  | ./lz4c32 -9     | ./lz4c32 -vdq > $(void)
	./datagen -g16kb  | ./lz4c32 -9     | ./lz4    -vdq > $(void)
	./datagen         | ./lz4c32        | ./lz4c32 -vdq > $(void)
	./datagen         | ./lz4c32        | ./lz4    -vdq > $(void)
	./datagen -g256mb | ./lz4c32 -vqb4d | ./lz4c32 -vdq > $(void)
	./datagen -g256mb | ./lz4c32 -vqb4d | ./lz4    -vdq > $(void)
	./datagen -g6gb   | ./lz4c32 -vqb5d | ./lz4c32 -vdq > $(void)

test-fullbench: fullbench
	./fullbench --no-prompt $(test_files)

test-fullbench32: fullbench32
	./fullbench32 --no-prompt $(test_files)

test-fuzzer: fuzzer
	./fuzzer

test-fuzzer32: fuzzer32
	./fuzzer32

test-frametest: frametest
	./frametest

test-frametest32: frametest32
	./frametest32

test-mem: lz4 datagen fuzzer frametest
	./datagen -g16kb > tmp
	valgrind --leak-check=yes ./lz4 -9 -bd -f tmp /dev/null
	./datagen -g16mb > tmp
	valgrind --leak-check=yes ./lz4 -9 -b5d -f tmp /dev/null
	./datagen -g256mb > tmp
	valgrind --leak-check=yes ./lz4 -b4d -f -vq tmp /dev/null
	rm tmp
	valgrind --leak-check=yes ./fuzzer -i64 -t1
	valgrind --leak-check=yes ./frametest -i256

test-mem32: lz4c32 datagen
# unfortunately, valgrind doesn't seem to work with non-native binary. if someone knows how to do a valgrind-test on a 32-bits exe with a 64-bits system...

endif
