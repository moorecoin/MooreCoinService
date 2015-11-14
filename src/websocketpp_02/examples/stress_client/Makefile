boost_libs=boost_system boost_date_time boost_thread boost_random boost_regex

include ../common.mk

ldflags := $(ldflags) -lcrypto -lssl -lpthread

stress_client: stress_client.o
	$(cxx) $(cflags) $^ -o $@ $(ldflags)

%.o: %.cpp
	$(cxx) -c $(cflags) -o $@ $^

# cleanup by removing generated files
#
.phony:		clean
clean:
		rm -f *.o stress_client
