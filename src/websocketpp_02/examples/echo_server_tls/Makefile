boost_libs=boost_system boost_date_time boost_thread boost_regex

include ../common.mk

ldflags := $(ldflags) -lcrypto -lssl -lpthread

echo_server_tls: echo_server_tls.o
	$(cxx) $(cflags) $^ -o $@ $(ldflags)

%.o: %.cpp
	$(cxx) -c $(cflags) -o $@ $^

# cleanup by removing generated files
#
.phony:		clean
clean:
		rm -f *.o echo_server_tls
