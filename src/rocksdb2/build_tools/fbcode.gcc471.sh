#!/bin/sh
#
# set environment variables so that we can compile leveldb using
# fbcode settings.  it uses the latest g++ compiler and also
# uses jemalloc

toolchain_rev=fbe3b095a4cc4a3713730050d182b7b4a80c342f
toolchain_executables="/mnt/gvfs/third-party/$toolchain_rev/centos5.2-native"
toolchain_lib_base="/mnt/gvfs/third-party/$toolchain_rev/gcc-4.7.1-glibc-2.14.1"
tool_jemalloc=jemalloc-3.3.1/9202ce3

# location of libhdfs libraries
if test "$use_hdfs"; then
  java_home="/usr/local/jdk-6u22-64"
  jinclude="-i$java_home/include -i$java_home/include/linux"
  glibc_runtime_path="/usr/local/fbcode/gcc-4.7.1-glibc-2.14.1"
  hdfslib=" -wl,--no-whole-archive hdfs/libhdfs.a -l$java_home/jre/lib/amd64 "
  hdfslib+=" -l$java_home/jre/lib/amd64/server -l$glibc_runtime_path/lib "
  hdfslib+=" -ldl -lverify -ljava -ljvm "
fi

# location of libgcc
libgcc_include=" -i $toolchain_lib_base/libgcc/libgcc-4.7.1/afc21dc/include"
libgcc_libs=" -l $toolchain_lib_base/libgcc/libgcc-4.7.1/afc21dc/libs"

# location of glibc
glibc_include=" -i $toolchain_lib_base/glibc/glibc-2.14.1/99df8fc/include"
glibc_libs=" -l $toolchain_lib_base/glibc/glibc-2.14.1/99df8fc/lib"

# location of snappy headers and libraries
snappy_include=" -i $toolchain_lib_base/snappy/snappy-1.0.3/7518bbe/include"
snappy_libs=" $toolchain_lib_base/snappy/snappy-1.0.3/7518bbe/lib/libsnappy.a"

# location of zlib headers and libraries
zlib_include=" -i $toolchain_lib_base/zlib/zlib-1.2.5/91ddd43/include"
zlib_libs=" $toolchain_lib_base/zlib/zlib-1.2.5/91ddd43/lib/libz.a"

# location of bzip headers and libraries
bzip_include=" -i $toolchain_lib_base/bzip2/bzip2-1.0.6/91ddd43/include"
bzip_libs=" $toolchain_lib_base/bzip2/bzip2-1.0.6/91ddd43/lib/libbz2.a"

# location of gflags headers and libraries
gflags_include=" -i $toolchain_lib_base/gflags/gflags-1.6/91ddd43/include"
gflags_libs=" $toolchain_lib_base/gflags/gflags-1.6/91ddd43/lib/libgflags.a"

# use intel sse support for checksum calculations
export use_sse=" -msse -msse4.2 "

cc="$toolchain_executables/gcc/gcc-4.7.1-glibc-2.14.1/bin/gcc"
cxx="$toolchain_executables/gcc/gcc-4.7.1-glibc-2.14.1/bin/g++ $jinclude $snappy_include $zlib_include $bzip_include $gflags_include"
ar=$toolchain_executables/binutils/binutils-2.21.1/da39a3e/bin/ar
ranlib=$toolchain_executables/binutils/binutils-2.21.1/da39a3e/bin/ranlib

cflags="-b$toolchain_executables/binutils/binutils-2.21.1/da39a3e/bin/gold -m64 -mtune=generic"
cflags+=" -i $toolchain_lib_base/jemalloc/$tool_jemalloc/include -dhave_jemalloc"
cflags+=" $libgcc_include $glibc_include"
cflags+=" -drocksdb_platform_posix -drocksdb_atomic_present -drocksdb_fallocate_present"
cflags+=" -dsnappy -dgflags=google -dzlib -dbzip2"

exec_ldflags=" -wl,--whole-archive $toolchain_lib_base/jemalloc/$tool_jemalloc/lib/libjemalloc.a"
exec_ldflags+=" -wl,--no-whole-archive $toolchain_lib_base/libunwind/libunwind-1.0.1/350336c/lib/libunwind.a"
exec_ldflags+=" $hdfslib $snappy_libs $zlib_libs $bzip_libs $gflags_libs"

platform_ldflags="$libgcc_libs $glibc_libs "

exec_ldflags_shared="$snappy_libs $zlib_libs $bzip_libs $gflags_libs"

valgrind_ver="$toolchain_lib_base/valgrind/valgrind-3.8.1/91ddd43/bin/"

export cc cxx ar ranlib cflags exec_ldflags exec_ldflags_shared valgrind_ver
