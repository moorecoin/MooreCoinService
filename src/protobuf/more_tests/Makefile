# additional tests to run before releasing a package.
#
# run like:
#   make package=/path/to/protobuf-version.tar.gz
#
# some of these tests require tools or make assumptions that may not be
# available on end-user machines, so these cannot be part of "make check".  for
# example, we test that the headers compile with strict warning settings, but
# since different compilers produce wildly different warnings we cannot assume
# that this test will pass everywhere.  if we ran it as part of "make check",
# it could unnecessarily block users from running the real tests just because
# their compiler produces some extra warnings that probably aren't a big deal.
# so we run it separately. 

all: header_warning_test

clean:
	rm -rf src target header_warning_test.cc header_warning_test.o header_warning_test

# unpack the package into src, then install it into target.
package=protobuf.tar.gz

src: $(package)
	tar zxvf $(package)
	mv `basename $(package) .tar.gz` src

target: src
	(cd src && ./configure --prefix=$$pwd/../target --disable-shared)
	(cd src && make -j4 check)
	(cd src && make install)

# verify that headers produce no warnings even under strict settings.
header_warning_test.cc: target
	( (cd target/include && find google/protobuf -name '*.h') | \
	  awk '{print "#include \""$$1"\""} ' > header_warning_test.cc )

header_warning_test: header_warning_test.cc
	# todo(kenton):  consider adding -pedantic and -weffc++.  currently these
	#   produce tons of extra warnings so we'll need to do some work first.
	g++ -itarget/include -wall -werror -wsign-compare -o2 -c header_warning_test.cc
	touch header_warning_test
