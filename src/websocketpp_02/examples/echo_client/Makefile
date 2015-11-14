boost_libs=boost_system boost_date_time boost_program_options boost_thread boost_regex boost_random

include ../common.mk

ldflags := $(ldflags) -lpthread

echo_client: echo_client.o
	$(cxx) $(cflags) $^ -o $@ $(ldflags)

%.o: %.cpp
	$(cxx) -c $(cflags) -o $@ $^

# cleanup by removing generated files
#
.phony:		clean
clean:
		rm -f *.o echo_client
