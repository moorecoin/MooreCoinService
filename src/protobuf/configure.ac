## process this file with autoconf to produce configure.
## in general, the safest way to proceed is to run ./autogen.sh

ac_prereq(2.59)

# note:  if you change the version, you must also update it in:
# * java/pom.xml
# * python/setup.py
# * src/google/protobuf/stubs/common.h
# * src/makefile.am (update -version-info for ldflags if needed)
#
# in the svn trunk, the version should always be the next anticipated release
# version with the "-pre" suffix.  (we used to use "-snapshot" but this pushed
# the size of one file name in the dist tarfile over the 99-char limit.)
ac_init([protocol buffers],[2.5.1-pre],[protobuf@googlegroups.com],[protobuf])

am_maintainer_mode([enable])

ac_config_srcdir(src/google/protobuf/message.cc)
ac_config_headers([config.h])
ac_config_macro_dir([m4])

# autoconf's default cxxflags are usually "-g -o2".  these aren't necessarily
# the best choice for libprotobuf.
as_if([test "x${ac_cv_env_cflags_set}" = "x"],
      [cflags=""])
as_if([test "x${ac_cv_env_cxxflags_set}" = "x"],
      [cxxflags=""])

ac_canonical_target

am_init_automake

ac_arg_with([zlib],
  [as_help_string([--with-zlib],
    [include classes for streaming compressed data in and out @<:@default=check@:>@])],
  [],[with_zlib=check])

ac_arg_with([protoc],
  [as_help_string([--with-protoc=command],
    [use the given protoc command instead of building a new one when building tests (useful for cross-compiling)])],
  [],[with_protoc=no])

# checks for programs.
ac_prog_cc
ac_prog_cxx
ac_lang([c++])
acx_use_system_extensions
am_conditional(gcc, test "$gcc" = yes)   # let the makefile know if we're gcc

# test_util.cc takes forever to compile with gcc and optimization turned on.
ac_msg_checking([c++ compiler flags...])
as_if([test "x${ac_cv_env_cxxflags_set}" = "x"],[
  as_if([test "$gcc" = "yes"],[
    protobuf_opt_flag="-o2"
    cxxflags="${cxxflags} -g"
  ])

  # protocol buffers contains several checks that are intended to be used only
  # for debugging and which might hurt performance.  most users are probably
  # end users who don't want these checks, so add -dndebug by default.
  cxxflags="$cxxflags -dndebug"

  ac_msg_result([use default: $protobuf_opt_flag $cxxflags])
],[
  ac_msg_result([use user-supplied: $cxxflags])
])

ac_subst(protobuf_opt_flag)

acx_check_suncc

# have to do libtool after suncc, other wise it "helpfully" adds crun cstd
# to the link
ac_prog_libtool

# checks for header files.
ac_header_stdc
ac_check_headers([fcntl.h inttypes.h limits.h stdlib.h unistd.h])

# checks for library functions.
ac_func_memcmp
ac_func_strtod
ac_check_funcs([ftruncate memset mkdir strchr strerror strtol])

# check for zlib.
have_zlib=0
as_if([test "$with_zlib" != no], [
  ac_msg_checking([zlib version])

  # first check the zlib header version.
  ac_compile_ifelse(
    [ac_lang_program([[
        #include <zlib.h>
        #if !defined(zlib_vernum) || (zlib_vernum < 0x1204)
        # error zlib version too old
        #endif
        ]], [])], [
    ac_msg_result([ok (1.2.0.4 or later)])

    # also need to add -lz to the linker flags and make sure this succeeds.
    ac_search_libs([zlibversion], [z], [
      ac_define([have_zlib], [1], [enable classes using zlib compression.])
      have_zlib=1
    ], [
      as_if([test "$with_zlib" != check], [
        ac_msg_failure([--with-zlib was given, but no working zlib library was found])
      ])
    ])
  ], [
    as_if([test "$with_zlib" = check], [
      ac_msg_result([headers missing or too old (requires 1.2.0.4)])
    ], [
      ac_msg_failure([--with-zlib was given, but zlib headers were not present or were too old (requires 1.2.0.4)])
    ])
  ])
])
am_conditional([have_zlib], [test $have_zlib = 1])

as_if([test "$with_protoc" != "no"], [
  protoc=$with_protoc
  as_if([test "$with_protoc" = "yes"], [
    # no argument given.  use system protoc.
    protoc=protoc
  ])
  as_if([echo "$protoc" | grep -q '^@<:@^/@:>@.*/'], [
    # does not start with a slash, but contains a slash.  so, it's a relative
    # path (as opposed to an absolute path or an executable in $path).
    # since it will actually be executed from the src directory, prefix with
    # the current directory.  we also insert $ac_top_build_prefix in case this
    # is a nested package and --with-protoc was actually given on the outer
    # package's configure script.
    protoc=`pwd`/${ac_top_build_prefix}$protoc
  ])
  ac_subst([protoc])
])
am_conditional([use_external_protoc], [test "$with_protoc" != "no"])

acx_pthread
ac_cxx_stl_hash

# hack:  make gtest's configure script pick up our copy of cflags and cxxflags,
#   since the flags added by acx_check_suncc must be used when compiling gtest
#   too.
export cflags
export cxxflags
ac_config_subdirs([gtest])

ac_config_files([makefile src/makefile protobuf.pc protobuf-lite.pc])
ac_output
