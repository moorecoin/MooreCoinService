boost_lib_path		?= /usr/local/lib
boost_include_path ?= /usr/local/include

cflags = -o2 -i$(boost_include_path)
ldflags = -l$(boost_lib_path)

cxx		?= c++
shared  ?= "1"

ifeq ($(shared), 1)
	ldflags := $(ldflags) -lboost_system -lboost_date_time -lboost_regex -lboost_unit_test_framework -lwebsocketpp
else
	ldflags := $(ldflags) $(boost_lib_path)/libboost_system.a $(boost_lib_path)/libboost_date_time.a $(boost_lib_path)/libboost_regex.a $(boost_lib_path)/libboost_unit_test_framework.a ../../libwebsocketpp.a
endif

tests: hybi_util.cpp
	$(cxx) $(cflags) $^ -o $@ $(ldflags)

perf: uri_perf.cpp
	$(cxx) $(cflags) $^ -o $@ $(ldflags)

%.o: %.cpp
	$(cxx) -c $(cflags) -o $@ $^

# cleanup by removing generated files
#
.phony:		clean
clean:
		rm -f *.o tests
