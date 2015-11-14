#
#  makefile
#  
#  copyright (c) 1998, 2009
#  paul e. jones <paulej@packetizer.com>
#  all rights reserved.
#
#############################################################################
#  $id: makefile 12 2009-06-22 19:34:25z paulej $
#############################################################################
#
#  description:
#	this is a makefile for unix to build the programs sha, shacmp, and
#	shatest
#
#

cc	= g++

cflags	= -c -o2 -wall -d_file_offset_bits=64

libs	=

objs	= sha1.o

all: sha shacmp shatest

sha: sha.o $(objs)
	$(cc) -o $@ sha.o $(objs) $(libs)

shacmp: shacmp.o $(objs)
	$(cc) -o $@ shacmp.o $(objs) $(libs)

shatest: shatest.o $(objs)
	$(cc) -o $@ shatest.o $(objs) $(libs)

%.o: %.cpp
	$(cc) $(cflags) -o $@ $<

clean:
	$(rm) *.o sha shacmp shatest
