boost_libs=boost_system boost_date_time boost_thread boost_regex boost_random

include ../common.mk

ldflags := $(ldflags) -lpthread

fuzzing_client: fuzzing_client.o
	$(cxx) $(cflags) $^ -o $@ $(ldflags)

%.o: %.cpp
	$(cxx) -c $(cflags) -o $@ $^

# cleanup by removing generated files
#
.phony:		clean
clean:
		rm -f *.o fuzzing_client
