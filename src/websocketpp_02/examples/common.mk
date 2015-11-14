boost_prefix ?= /usr/local
boost_lib_path		?= $(boost_prefix)/lib
boost_include_path ?= $(boost_prefix)/include

cpp11               ?= 

cflags = -wall -o2 $(cpp11) -i$(boost_include_path)
ldflags = -l$(boost_lib_path)

cxx		?= c++
shared  ?= 0

ifeq ($(shared), 1)
	ldflags := $(ldflags) -lwebsocketpp
	ldflags := $(ldflags) $(boost_libs:%=-l%)
else
	ldflags := $(ldflags) ../../libwebsocketpp.a
	ldflags := $(ldflags) $(boost_libs:%=$(boost_lib_path)/lib%.a)
endif
