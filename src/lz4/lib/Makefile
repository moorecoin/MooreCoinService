# ################################################################
# lz4 library - makefile
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
#  - lz4 source mirror : https://github.com/cyan4973/lz4
#  - lz4 forum froup : https://groups.google.com/forum/#!forum/lz4c
# ################################################################

# version numbers
version ?= 126
libver_major=`sed -n '/define lz4_version_major/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < lz4.h`
libver_minor=`sed -n '/define lz4_version_minor/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < lz4.h`
libver_patch=`sed -n '/define lz4_version_release/s/.*[[:blank:]]\([0-9][0-9]*\).*/\1/p' < lz4.h`
libver=$(libver_major).$(libver_minor).$(libver_patch)

destdir?=
prefix ?= /usr
cflags ?= -o3
cflags += -i. -std=c99 -wall -wextra -wundef -wshadow -wcast-align -wstrict-prototypes -pedantic

libdir?= $(prefix)/lib
includedir=$(prefix)/include


# os x linker doesn't support -soname, and use different extension
# see : https://developer.apple.com/library/mac/documentation/developertools/conceptual/dynamiclibraries/100-articles/dynamiclibrarydesignguidelines.html
ifeq ($(shell uname), darwin)
	shared_ext = dylib
	shared_ext_major = $(libver_major).$(shared_ext)
	shared_ext_ver = $(libver).$(shared_ext)
	soname_flags = -install_name $(prefix)/lib/liblz4.$(shared_ext_major) -compatibility_version $(libver_major) -current_version $(libver)
else
	soname_flags = -wl,-soname=liblz4.$(shared_ext).$(libver_major)
	shared_ext = so
	shared_ext_major = $(shared_ext).$(libver_major)
	shared_ext_ver = $(shared_ext).$(libver)
endif

default: liblz4

all: liblz4

liblz4: lz4.c lz4hc.c lz4frame.c xxhash.c
	@echo compiling static library
	@$(cc) $(cppflags) $(cflags) -c $^
	@$(ar) rcs liblz4.a lz4.o lz4hc.o lz4frame.o xxhash.o
	@echo compiling dynamic library $(libver)
	@$(cc) $(cppflags) $(cflags) $(ldflags) -shared $^ -fpic $(soname_flags) -o $@.$(shared_ext_ver)
	@echo creating versioned links
	@ln -sf $@.$(shared_ext_ver) $@.$(shared_ext_major)
	@ln -sf $@.$(shared_ext_ver) $@.$(shared_ext)

clean:
	@rm -f core *.o *.a *.$(shared_ext) *.$(shared_ext).* liblz4.pc
	@echo cleaning library completed


#------------------------------------------------------------------------
#make install is validated only for linux, osx, kfreebsd and hurd targets
ifneq (,$(filter $(shell uname),linux darwin gnu/kfreebsd gnu))

liblz4.pc: liblz4.pc.in makefile
	@echo creating pkgconfig
	@sed -e 's|@prefix@|$(prefix)|' \
            -e 's|@libdir@|$(libdir)|' \
            -e 's|@includedir@|$(includedir)|' \
            -e 's|@version@|$(version)|' \
             $< >$@

install: liblz4 liblz4.pc
	@install -d -m 755 $(destdir)$(libdir)/pkgconfig/ $(destdir)$(includedir)/
	@install -m 755 liblz4.$(shared_ext_ver) $(destdir)$(libdir)/liblz4.$(shared_ext_ver)
	@cp -a liblz4.$(shared_ext_major) $(destdir)$(libdir)
	@cp -a liblz4.$(shared_ext) $(destdir)$(libdir)
	@cp -a liblz4.pc $(destdir)$(libdir)/pkgconfig/
	@install -m 644 liblz4.a $(destdir)$(libdir)/liblz4.a
	@install -m 644 lz4.h $(destdir)$(includedir)/lz4.h
	@install -m 644 lz4hc.h $(destdir)$(includedir)/lz4hc.h
	@install -m 644 lz4frame.h $(destdir)$(includedir)/lz4frame.h
	@echo lz4 static and shared library installed

uninstall:
	@rm -f $(destdir)$(libdir)/liblz4.$(shared_ext)
	@rm -f $(destdir)$(libdir)/liblz4.$(shared_ext_major)
	@rm -f $(destdir)$(libdir)/pkgconfig/liblz4.pc
	@[ -x $(destdir)$(libdir)/liblz4.$(shared_ext_ver) ] && rm -f $(destdir)$(libdir)/liblz4.$(shared_ext_ver)
	@[ -f $(destdir)$(libdir)/liblz4.a ] && rm -f $(destdir)$(libdir)/liblz4.a
	@[ -f $(destdir)$(includedir)/lz4.h ] && rm -f $(destdir)$(includedir)/lz4.h
	@[ -f $(destdir)$(includedir)/lz4hc.h ] && rm -f $(destdir)$(includedir)/lz4hc.h
	@echo lz4 libraries successfully uninstalled

endif
