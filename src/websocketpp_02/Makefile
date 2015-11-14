################################################################################
# 
#  copyright (c) 2011, peter thorson. all rights reserved.
# 
#  redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#      * redistributions of source code must retain the above copyright
#        notice, this list of conditions and the following disclaimer.
#      * redistributions in binary form must reproduce the above copyright
#        notice, this list of conditions and the following disclaimer in the
#        documentation and/or other materials provided with the distribution.
#      * neither the name of the websocket++ project nor the
#        names of its contributors may be used to endorse or promote products
#        derived from this software without specific prior written permission.
#  
#  this software is provided by the copyright holders and contributors "as is" 
#  and any express or implied warranties, including, but not limited to, the 
#  implied warranties of merchantability and fitness for a particular purpose 
#  are disclaimed. in no event shall peter thorson be liable for any
#  direct, indirect, incidental, special, exemplary, or consequential damages
#  (including, but not limited to, procurement of substitute goods or services;
#  loss of use, data, or profits; or business interruption) however caused and
#  on any theory of liability, whether in contract, strict liability, or tort
#  (including negligence or otherwise) arising in any way out of the use of this
#  software, even if advised of the possibility of such damage.
#  
#  this makefile was derived from a similar one included in the libjson project
#  it's authors were jonathan wallace and bernhard fluehmann.


objects = network_utilities.o sha1.o base64.o md5.o uri.o hybi_header.o hybi_util.o data.o

boost_prefix ?= /usr/local
boost_lib_path		?= $(boost_prefix)/lib
boost_include_path  ?= $(boost_prefix)/include

libs = -l$(boost_lib_path) -lboost_system -lboost_date_time -lboost_regex -lboost_random -lboost_program_options -lboost_thread

//libs_static = $(boost_path)/boost_system.a $(boost_path)/boost_regex.a

os=$(shell uname)

# cpp11 build
ifeq ($(cpp11), 1)
	cpp11_    = -std=c++0x -stdlib=libc++
else
	cpp11_    ?=
endif

# defaults
ifeq ($(os), darwin)
	cxxflags_default = -c $(cpp11_) -wall -o2 -dndebug -i$(boost_include_path)
else
	cxxflags_default = -c -wall -o2 -dndebug -i$(boost_include_path)
endif
cxxflags_small   = -c 
cxxflags_debug   = -c -g -o0
cxxflags_shared  = -f$(pic)
libname          = libwebsocketpp
libname_hdr      = websocketpp
libname_debug    = $(libname)
suffix_shared    = so
suffix_shared_darwin = dylib
suffix_static    = a
major_version    = 0
minor_version    = 2.0
objdir           = objs

# variables
prefix          ?= /usr/local
exec_prefix     ?= $(prefix)
libdir          ?= lib
includedir      ?= include
srcdir          ?= src
cxx             ?= c++
ar              ?= ar
pic             ?= pic
build_type      ?= default
shared          ?= 0


# internal variables
inst_path        = $(exec_prefix)/$(libdir)
include_path     = $(prefix)/$(includedir)

# build_type specific settings
ifeq ($(build_type), debug)
	cxxflags     = $(cxxflags_debug)
	libname     := $(libname_debug)
else
	cxxflags    ?= $(cxxflags_default)
endif



# shared specific settings
ifeq ($(shared), 1)
	ifeq ($(os), darwin)
		libname_shared           = $(libname).$(suffix_shared_darwin)
	else
		libname_shared           = $(libname).$(suffix_shared)
	endif
	libname_shared_major_version = $(libname_shared).$(major_version)
	lib_target                   = $(libname_shared_major_version).$(minor_version)
	objdir                      := $(objdir)_shared
	cxxflags                    := $(cxxflags) $(cxxflags_shared)
else
	lib_target                   = $(libname).$(suffix_static)
	objdir                      := $(objdir)_static
endif

# phony targets
.phony: all banner installdirs install install_headers clean uninstall \
        uninstall_headers

# targets
all: $(lib_target)
	@echo "============================================================"
	@echo "done"
	@echo "============================================================"

banner:
	@echo "============================================================"
	@echo "libwebsocketpp version: "$(major_version).$(minor_version) "target: "$(target) "os: "$(os)
	@echo "============================================================"

installdirs: banner
	mkdir -p $(objdir)

# libraries
ifeq ($(shared),1)
$(lib_target): banner installdirs $(addprefix $(objdir)/, $(objects))
	@echo "link "
	cd $(objdir) ; \
	if test "$(os)" = "darwin" ; then \
		$(cxx) -dynamiclib $(libs) -wl,-dylib_install_name -wl,$(libname_shared_major_version) -o $@ $(objects) ; \
	else \
		$(cxx) -shared $(libs) -wl,-soname,$(libname_shared_major_version) -o $@ $(objects) ; \
	fi ; \
	mv -f $@ ../
	@echo "link: done"
else
$(lib_target): banner installdirs $(addprefix $(objdir)/, $(objects))
	@echo "archive"
	cd $(objdir) ; \
	$(ar) -cvq $@ $(objects) ; \
	mv -f $@ ../
	@echo "archive: done"
endif

# compile object files
$(objdir)/sha1.o: $(srcdir)/sha1/sha1.cpp
	$(cxx) $< -c -o $@ $(cxxflags)
	
$(objdir)/base64.o: $(srcdir)/base64/base64.cpp
	$(cxx) $< -c -o $@ $(cxxflags)

$(objdir)/hybi_header.o: $(srcdir)/processors/hybi_header.cpp
	$(cxx) $< -c -o $@ $(cxxflags)

$(objdir)/hybi_util.o: $(srcdir)/processors/hybi_util.cpp
	$(cxx) $< -c -o $@ $(cxxflags)

$(objdir)/data.o: $(srcdir)/messages/data.cpp
	$(cxx) $< -c -o $@ $(cxxflags)

$(objdir)/md5.o: $(srcdir)/md5/md5.c
	$(cxx) $< -c -o $@ $(cxxflags)

$(objdir)/%.o: $(srcdir)/%.cpp
	$(cxx) $< -c -o $@ $(cxxflags)

ifeq ($(shared),1)
install: banner install_headers $(lib_target)
	@echo "install shared library"
	mkdir -p $(inst_path)
	cp -f ./$(lib_target) $(inst_path)
	cd $(inst_path) ; \
	ln -sf $(lib_target) $(libname_shared_major_version) ; \
	ln -sf $(libname_shared_major_version) $(libname_shared)
	if test "$(os)" != "darwin" ; then \
		ldconfig ; \
	fi
	@echo "install shared library: done."
else
install: banner install_headers $(lib_target)
	@echo "install static library"
	mkdir -p $(inst_path)
	cp -f ./$(lib_target) $(inst_path)
	@echo "install static library: done."
endif

install_headers: banner
	@echo "install header files"
	mkdir -p $(include_path)/$(libname_hdr)
#	cp -f ./*.hpp $(include_path)/$(libname_hdr)
	cp -f ./$(srcdir)/*.hpp $(include_path)/$(libname_hdr)
	mkdir -p $(include_path)/$(libname_hdr)/base64
	cp -f ./$(srcdir)/base64/base64.h $(include_path)/$(libname_hdr)/base64
	mkdir -p $(include_path)/$(libname_hdr)/sha1
	cp -f ./$(srcdir)/sha1/sha1.h $(include_path)/$(libname_hdr)/sha1
	mkdir -p $(include_path)/$(libname_hdr)/http
	cp -f ./$(srcdir)/http/*.hpp $(include_path)/$(libname_hdr)/http
	mkdir -p $(include_path)/$(libname_hdr)/logger
	cp -f ./$(srcdir)/logger/*.hpp $(include_path)/$(libname_hdr)/logger
	mkdir -p $(include_path)/$(libname_hdr)/md5
	cp -f ./$(srcdir)/md5/md5.h $(include_path)/$(libname_hdr)/md5
	cp -f ./$(srcdir)/md5/md5.hpp $(include_path)/$(libname_hdr)/md5
	mkdir -p $(include_path)/$(libname_hdr)/messages
	cp -f ./$(srcdir)/messages/*.hpp $(include_path)/$(libname_hdr)/messages
	mkdir -p $(include_path)/$(libname_hdr)/processors
	cp -f ./$(srcdir)/processors/*.hpp $(include_path)/$(libname_hdr)/processors
	mkdir -p $(include_path)/$(libname_hdr)/rng
	cp -f ./$(srcdir)/rng/*.hpp $(include_path)/$(libname_hdr)/rng
	mkdir -p $(include_path)/$(libname_hdr)/roles
	cp -f ./$(srcdir)/roles/*.hpp $(include_path)/$(libname_hdr)/roles
	mkdir -p $(include_path)/$(libname_hdr)/sockets
	cp -f ./$(srcdir)/sockets/*.hpp $(include_path)/$(libname_hdr)/sockets
	mkdir -p $(include_path)/$(libname_hdr)/utf8_validator
	cp -f ./$(srcdir)/utf8_validator/*.hpp $(include_path)/$(libname_hdr)/utf8_validator
	chmod -r a+r $(include_path)/$(libname_hdr)
	find  $(include_path)/$(libname_hdr) -type d -exec chmod a+x {} \;
	@echo "install header files: done."

clean: banner
	@echo "clean library and object folder"
	rm -rf $(objdir)
	rm -f $(lib_target)
	@echo "clean library and object folder: done"

ifeq ($(shared),1)
uninstall: banner uninstall_headers
	@echo "uninstall shared library"
	rm -f $(inst_path)/$(libname_shared)
	rm -f $(inst_path)/$(libname_shared_major_version)
	rm -f $(inst_path)/$(lib_target)
	ldconfig
	@echo "uninstall shared library: done"
else
uninstall: banner uninstall_headers
	@echo "uninstall static library"
	rm -f $(inst_path)/$(lib_target)
	@echo "uninstall static library: done"
endif

uninstall_headers: banner
	@echo "uninstall header files"
	rm -rf $(include_path)/$(libname)
	@echo "uninstall header files: done"
