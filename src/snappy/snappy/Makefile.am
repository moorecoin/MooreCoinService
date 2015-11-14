aclocal_amflags = -i m4

# library.
lib_ltlibraries = libsnappy.la
libsnappy_la_sources = snappy.cc snappy-sinksource.cc snappy-stubs-internal.cc snappy-c.cc
libsnappy_la_ldflags = -version-info $(snappy_ltversion)

include_headers = snappy.h snappy-sinksource.h snappy-stubs-public.h snappy-c.h
noinst_headers = snappy-internal.h snappy-stubs-internal.h snappy-test.h

# unit tests and benchmarks.
snappy_unittest_cppflags = $(gflags_cflags) $(gtest_cppflags)
snappy_unittest_sources = snappy_unittest.cc snappy-test.cc
snappy_unittest_ldflags = $(gtest_ldflags)
snappy_unittest_ldadd = libsnappy.la $(unittest_libs) $(gflags_libs) $(gtest_libs)
tests = snappy_unittest
noinst_programs = $(tests)

extra_dist = autogen.sh testdata/alice29.txt testdata/asyoulik.txt testdata/baddata1.snappy testdata/baddata2.snappy testdata/baddata3.snappy testdata/geo.protodata testdata/fireworks.jpeg testdata/html testdata/html_x_4 testdata/kppkn.gtb testdata/lcet10.txt testdata/paper-100k.pdf testdata/plrabn12.txt testdata/urls.10k
dist_doc_data = changelog copying install news readme format_description.txt framing_format.txt

libtool: $(libtool_deps)
	$(shell) ./config.status --recheck
