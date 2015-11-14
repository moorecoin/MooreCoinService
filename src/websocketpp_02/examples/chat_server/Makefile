boost_libs=boost_system boost_date_time boost_program_options boost_thread boost_regex

include ../common.mk

ldflags := $(ldflags) -lpthread

chat_server: chat_server.o chat.o
	$(cxx) $(cflags) $^ -o $@ $(ldflags)

%.o: %.cpp
	$(cxx) -c $(cflags) -o $@ $^

# cleanup by removing generated files
#
.phony:		clean
clean:
		rm -f *.o chat_server
