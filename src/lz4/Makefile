# ################################################################
# lz4 - makefile
# copyright (c) yann collet 2011-2015
# all rights reserved.
# 
# bsd license
# redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
# 
# * redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
# 
# * redistributions in binary form must reproduce the above copyright notice, this
#   list of conditions and the following disclaimer in the documentation and/or
#   other materials provided with the distribution.
# 
# this software is provided by the copyright holders and contributors "as is" and
# any express or implied warranties, including, but not limited to, the implied
# warranties of merchantability and fitness for a particular purpose are
# disclaimed. in no event shall the copyright holder or contributors be liable for
# any direct, indirect, incidental, special, exemplary, or consequential damages
# (including, but not limited to, procurement of substitute goods or services;
# loss of use, data, or profits; or business interruption) however caused and on
# any theory of liability, whether in contract, strict liability, or tort
# (including negligence or otherwise) arising in any way out of the use of this
# software, even if advised of the possibility of such damage.
# 
# you can contact the author at :
#  - lz4 source repository : http://code.google.com/p/lz4/
#  - lz4 forum froup : https://groups.google.com/forum/#!forum/lz4c
# ################################################################

# version number
export version=126
export release=r$(version)

destdir?=
prefix ?= /usr

libdir ?= $(prefix)/lib
includedir=$(prefix)/include
prgdir  = programs
lz4dir  = lib
distribname=lz4-$(release).tar.gz

text =  $(lz4dir)/lz4.c $(lz4dir)/lz4.h $(lz4dir)/lz4hc.c $(lz4dir)/lz4hc.h \
	$(lz4dir)/lz4frame.c $(lz4dir)/lz4frame.h $(lz4dir)/lz4frame_static.h \
	$(lz4dir)/xxhash.c $(lz4dir)/xxhash.h \
	$(lz4dir)/liblz4.pc.in $(lz4dir)/makefile $(lz4dir)/license \
	makefile lz4_block_format.txt lz4_frame_format.html news readme.md \
	cmake_unofficial/cmakelists.txt \
	$(prgdir)/fullbench.c $(prgdir)/lz4cli.c \
	$(prgdir)/datagen.c $(prgdir)/fuzzer.c \
	$(prgdir)/lz4io.c $(prgdir)/lz4io.h \
	$(prgdir)/bench.c $(prgdir)/bench.h \
	$(prgdir)/lz4.1 $(prgdir)/lz4c.1 $(prgdir)/lz4cat.1 \
	$(prgdir)/makefile $(prgdir)/copying	
nontext = images/image00.png images/image01.png images/image02.png \
	images/image03.png images/image04.png images/image05.png \
	images/image06.png
sources = $(text) $(nontext)


# select test target for travis ci's build matrix
ifneq (,$(filter test-%,$(lz4_travis_ci_env)))
travis_target=prg-travis
else
travis_target=$(lz4_travis_ci_env)
endif


default: lz4programs

all: 
	@cd $(lz4dir); $(make) -e all
	@cd $(prgdir); $(make) -e all

lz4programs:
	@cd $(prgdir); $(make) -e

clean:
	@rm -f $(distribname) *.sha1
	@cd $(prgdir); $(make) clean
	@cd $(lz4dir); $(make) clean
	@cd examples; $(make) clean
	@echo cleaning completed


#------------------------------------------------------------------------
#make install is validated only for linux, osx, kfreebsd and hurd targets
ifneq (,$(filter $(shell uname),linux darwin gnu/kfreebsd gnu))

install:
	@cd $(lz4dir); $(make) -e install
	@cd $(prgdir); $(make) -e install

uninstall:
	@cd $(lz4dir); $(make) uninstall
	@cd $(prgdir); $(make) uninstall

travis-install:
	sudo $(make) install

dist: clean
	@install -dd -m 700 lz4-$(release)/lib/
	@install -dd -m 700 lz4-$(release)/programs/
	@install -dd -m 700 lz4-$(release)/cmake_unofficial/
	@install -dd -m 700 lz4-$(release)/images/
	@for f in $(text); do \
		tr -d '\r' < $$f > .tmp; \
		install -m 600 .tmp lz4-$(release)/$$f; \
	done
	@rm .tmp
	@for f in $(nontext); do \
		install -m 600 $$f lz4-$(release)/$$f; \
	done
	@tar -czf $(distribname) lz4-$(release)/
	@rm -rf lz4-$(release)
	@sha1sum $(distribname) > $(distribname).sha1
	@echo distribution $(distribname) built

test:
	@cd $(prgdir); $(make) -e test

test-travis: $(travis_target)

cmake:
	@cd cmake_unofficial; cmake cmakelists.txt; $(make)

streaming-examples:
	cd examples; $(make) -e test

prg-travis:
	@cd $(prgdir); $(make) -e test-travis

endif
