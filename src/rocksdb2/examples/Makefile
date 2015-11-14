include ../build_config.mk

all: simple_example column_families_example

simple_example: simple_example.cc
	$(cxx) $(cxxflags) $@.cc -o$@ ../librocksdb.a -i../include -o2 -std=c++11 $(platform_ldflags) $(platform_cxxflags) $(exec_ldflags)

column_families_example: column_families_example.cc
	$(cxx) $(cxxflags) $@.cc -o$@ ../librocksdb.a -i../include -o2 -std=c++11 $(platform_ldflags) $(platform_cxxflags) $(exec_ldflags)
