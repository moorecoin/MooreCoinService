boost_libs=boost_system boost_date_time boost_regex boost_thread boost_random boost_chrono boost_program_options

include ../common.mk

ldflags := $(ldflags) -lcrypto -lssl -lpthread
cflags := -wall -o3 $(cflags)

os=$(shell uname)

ifneq ($(os), darwin)
	ldflags := $(ldflags) -lrt -lpthread
endif

wsperf: wsperf.o request.o case.o generic.o wscmd.o stress_aggregate.o stress_handler.o
	$(cxx) $(cflags) $^ -o $@ $(ldflags)

%.o: %.cpp
	$(cxx) -c $(cflags) -o $@ $^

# cleanup by removing generated files
#
.phony:		clean
clean:
		rm -f *.o wsperf wsperf_client
