boost_libs=boost_system boost_thread boost_regex boost_random

include ../common.mk

ldflags := $(ldflags) -lpthread

chat_client: chat_client.o chat_client_handler.o
	$(cxx) $(cflags) $^ -o $@ $(ldflags)

%.o: %.cpp
	$(cxx) -c $(cflags) -o $@ $^

# cleanup by removing generated files
#
.phony:		clean
clean:
		rm -f *.o chat_client
