m4_define([snappy_major], [1])
m4_define([snappy_minor], [1])
m4_define([snappy_patchlevel], [2])

# libtool shared library interface versions (current:revision:age)
# update this value for every release!  (a:b:c will map to foo.so.(a-c).c.b)
# http://www.gnu.org/software/libtool/manual/html_node/updating-version-info.html
m4_define([snappy_ltversion], [3:1:2])

ac_init([snappy], [snappy_major.snappy_minor.snappy_patchlevel])
ac_config_macro_dir([m4])

# these are flags passed to automake (though they look like gcc flags!)
am_init_automake([-wall])

lt_init
ac_subst([libtool_deps])
ac_prog_cxx
ac_lang([c++])
ac_c_bigendian
ac_type_size_t
ac_type_ssize_t
ac_check_headers([stdint.h stddef.h sys/mman.h sys/resource.h windows.h byteswap.h sys/byteswap.h sys/endian.h sys/time.h])

# don't use ac_func_mmap, as it checks for mappings of already-mapped memory,
# which we don't need (and does not exist on windows).
ac_check_func([mmap])

gtest_lib_check([], [true], [true # ignore; we can live without it.])

ac_arg_with([gflags],
  [as_help_string(
    [--with-gflags],
    [use google flags package to enhance the unit test @<:@default=check@:>@])],
    [],
    [with_gflags=check])

if test "x$with_gflags" != "xno"; then
  pkg_check_modules(
    [gflags],
    [libgflags],
    [ac_define([have_gflags], [1], [use the gflags package for command-line parsing.])],
    [if test "x$with_gflags" != "xcheck"; then
      ac_msg_failure([--with-gflags was given, but test for gflags failed])
    fi])
fi

# see if we have __builtin_expect.
# todo: use ac_cache.
ac_msg_checking([if the compiler supports __builtin_expect])
 
ac_try_compile(, [
    return __builtin_expect(1, 1) ? 1 : 0
], [
    snappy_have_builtin_expect=yes
    ac_msg_result([yes])
], [
    snappy_have_builtin_expect=no
    ac_msg_result([no])
])
if test x$snappy_have_builtin_expect = xyes ; then
    ac_define([have_builtin_expect], [1], [define to 1 if the compiler supports __builtin_expect.])
fi

# see if we have working count-trailing-zeros intrinsics.
# todo: use ac_cache.
ac_msg_checking([if the compiler supports __builtin_ctzll])

ac_try_compile(, [
    return (__builtin_ctzll(0x100000000ll) == 32) ? 1 : 0
], [
    snappy_have_builtin_ctz=yes
    ac_msg_result([yes])
], [
    snappy_have_builtin_ctz=no
    ac_msg_result([no])
])
if test x$snappy_have_builtin_ctz = xyes ; then
    ac_define([have_builtin_ctz], [1], [define to 1 if the compiler supports __builtin_ctz and friends.])
fi

# other compression libraries; the unit test can use these for comparison
# if they are available. if they are not found, just ignore.
unittest_libs=""
ac_defun([check_ext_compression_lib], [
  ah_check_lib([$1])
  ac_check_lib(
    [$1],
    [$2],
    [
      ac_define_unquoted(as_tr_cpp(have_lib$1))
      unittest_libs="-l$1 $unittest_libs"
    ],
    [true]
  )
])
check_ext_compression_lib([z], [zlibversion])
check_ext_compression_lib([lzo2], [lzo1x_1_15_compress])
check_ext_compression_lib([lzf], [lzf_compress])
check_ext_compression_lib([fastlz], [fastlz_compress])
check_ext_compression_lib([quicklz], [qlz_compress])
ac_subst([unittest_libs])

# these are used by snappy-stubs-public.h.in.
if test "$ac_cv_header_stdint_h" = "yes"; then
    ac_subst([ac_cv_have_stdint_h], [1])
else
    ac_subst([ac_cv_have_stdint_h], [0])
fi
if test "$ac_cv_header_stddef_h" = "yes"; then
    ac_subst([ac_cv_have_stddef_h], [1])
else
    ac_subst([ac_cv_have_stddef_h], [0])
fi
if test "$ac_cv_header_sys_uio_h" = "yes"; then
    ac_subst([ac_cv_have_sys_uio_h], [1])
else
    ac_subst([ac_cv_have_sys_uio_h], [0])
fi

# export the version to snappy-stubs-public.h.
snappy_major="snappy_major"
snappy_minor="snappy_minor"
snappy_patchlevel="snappy_patchlevel"

ac_subst([snappy_major])
ac_subst([snappy_minor])
ac_subst([snappy_patchlevel])
ac_subst([snappy_ltversion], snappy_ltversion)

ac_config_headers([config.h])
ac_config_files([makefile snappy-stubs-public.h])
ac_output
