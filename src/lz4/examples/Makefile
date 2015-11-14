# ##########################################################################
# lz4 examples - makefile
# copyright (c) yann collet 2011-2014
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

cc     := $(cc)
cflags ?= -o3
cflags += -std=c99 -wall -wextra -wundef -wshadow -wcast-align -wstrict-prototypes -wno-missing-braces   # wno-missing-braces required due to gcc <4.8.3 bug
flags   = -i../lib $(cppflags) $(cflags) $(ldflags)

testfile= makefile
lz4dir  = ../lib


# minimize test target for travis ci's build matrix
ifeq ($(lz4_travis_ci_env),-m32)
cflags += -m32
else ifeq ($(lz4_travis_ci_env),-m64)
endif


# define *.exe as extension for windows systems
ifneq (,$(filter windows%,$(os)))
ext =.exe
void = nul
else
ext =
void = /dev/null
endif


default: all

all: printversion doublebuffer ringbuffer ringbufferhc linecompress

printversion: $(lz4dir)/lz4.c printversion.c
	$(cc)      $(flags) $^ -o $@$(ext)

doublebuffer: $(lz4dir)/lz4.c blockstreaming_doublebuffer.c
	$(cc)      $(flags) $^ -o $@$(ext)

ringbuffer  : $(lz4dir)/lz4.c blockstreaming_ringbuffer.c
	$(cc)      $(flags) $^ -o $@$(ext)

ringbufferhc: $(lz4dir)/lz4.c $(lz4dir)/lz4hc.c hcstreaming_ringbuffer.c
	$(cc)      $(flags) $^ -o $@$(ext)

linecompress: $(lz4dir)/lz4.c blockstreaming_linebyline.c
	$(cc)      $(flags) $^ -o $@$(ext)

test : all
	./printversion$(ext)
	./doublebuffer$(ext) $(testfile)
	./ringbuffer$(ext)   $(testfile)
	./ringbufferhc$(ext) $(testfile)
	./linecompress$(ext) $(testfile)

clean:
	@rm -f core *.o *.dec *-0 *-9 *-8192 *.lz4s \
        printversion$(ext) doublebuffer$(ext) ringbuffer$(ext) ringbufferhc$(ext) linecompress$(ext)
	@echo cleaning completed

