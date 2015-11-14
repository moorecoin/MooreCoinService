boost_libs=boost_system boost_date_time boost_regex boost_thread

include ../common.mk

ldflags := $(ldflags) -lcrypto -lssl -lpthread

fuzzing_server: fuzzing_server_tls.o
	$(cxx) $(cflags) $^ -o $@ $(ldflags)

%.o: %.cpp
	$(cxx) -c $(cflags) -o $@ $^

# cleanup by removing generated files
#
.phony:		clean
clean:
		rm -f *.o fuzzing_server
