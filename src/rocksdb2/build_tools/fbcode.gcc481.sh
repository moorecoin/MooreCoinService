#!/bin/sh
#
# set environment variables so that we can compile rocksdb using
# fbcode settings.  it uses the latest g++ compiler and also
# uses jemalloc

toolchain_rev=53dc1fe83f84e9145b9ffb81b81aa7f6a49c87cc
centos_version=`rpm -q --qf "%{version}" $(rpm -q --whatprovides redhat-release)`
if [ "$centos_version" = "6" ]; then
  toolchain_executables="/mnt/gvfs/third-party/$toolchain_rev/centos6-native"
else
  toolchain_executables="/mnt/gvfs/third-party/$toolchain_rev/centos5.2-native"
fi
toolchain_lib_base="/mnt/gvfs/third-party/$toolchain_rev/gcc-4.8.1-glibc-2.17"

# location of libhdfs libraries
if test "$use_hdfs"; then
  java_home="/usr/local/jdk-6u22-64"
  jinclude="-i$java_home/include -i$java_home/include/linux"
  glibc_runtime_path="/usr/local/fbcode/gcc-4.8.1-glibc-2.17"
  hdfslib=" -wl,--no-whole-archive hdfs/libhdfs.a -l$java_home/jre/lib/amd64 "
  hdfslib+=" -l$java_home/jre/lib/amd64/server -l$glibc_runtime_path/lib "
  hdfslib+=" -ldl -lverify -ljava -ljvm "
fi

# location of libgcc
libgcc_include=" -i $toolchain_lib_base/libgcc/libgcc-4.8.1/8aac7fc/include"
libgcc_libs=" -l $toolchain_lib_base/libgcc/libgcc-4.8.1/8aac7fc/libs"

# location of glibc
glibc_include=" -i $toolchain_lib_base/glibc/glibc-2.17/99df8fc/include"
glibc_libs=" -l $toolchain_lib_base/glibc/glibc-2.17/99df8fc/lib"

# location of snappy headers and libraries
snappy_include=" -i $toolchain_lib_base/snappy/snappy-1.0.3/43d84e2/include"
snappy_libs=" $toolchain_lib_base/snappy/snappy-1.0.3/43d84e2/lib/libsnappy.a"

# location of zlib headers and libraries
zlib_include=" -i $toolchain_lib_base/zlib/zlib-1.2.5/c3f970a/include"
zlib_libs=" $toolchain_lib_base/zlib/zlib-1.2.5/c3f970a/lib/libz.a"

# location of bzip headers and libraries
bzip_include=" -i $toolchain_lib_base/bzip2/bzip2-1.0.6/c3f970a/include"
bzip_libs=" $toolchain_lib_base/bzip2/bzip2-1.0.6/c3f970a/lib/libbz2.a"

lz4_rev=065ec7e38fe83329031f6668c43bef83eff5808b
lz4_include=" -i /mnt/gvfs/third-party2/lz4/$lz4_rev/r108/gcc-4.8.1-glibc-2.17/c3f970a/include"
lz4_libs=" /mnt/gvfs/third-party2/lz4/$lz4_rev/r108/gcc-4.8.1-glibc-2.17/c3f970a/lib/liblz4.a"

# location of gflags headers and libraries
gflags_include=" -i $toolchain_lib_base/gflags/gflags-1.6/c3f970a/include"
gflags_libs=" $toolchain_lib_base/gflags/gflags-1.6/c3f970a/lib/libgflags.a"

# location of jemalloc
jemalloc_include=" -i $toolchain_lib_base/jemalloc/jemalloc-3.4.1/4d53c6f/include/"
jemalloc_lib=" -wl,--whole-archive $toolchain_lib_base/jemalloc/jemalloc-3.4.1/4d53c6f/lib/libjemalloc.a"

# location of numa
numa_rev=829d10dac0230f99cd7e1778869d2adf3da24b65
numa_include=" -i /mnt/gvfs/third-party2/numa/$numa_rev/2.0.8/gcc-4.8.1-glibc-2.17/c3f970a/include/"
numa_lib=" /mnt/gvfs/third-party2/numa/$numa_rev/2.0.8/gcc-4.8.1-glibc-2.17/c3f970a/lib/libnuma.a"

# use intel sse support for checksum calculations
export use_sse=" -msse -msse4.2 "

cc="$toolchain_executables/gcc/gcc-4.8.1/cc6c9dc/bin/gcc"
cxx="$toolchain_executables/gcc/gcc-4.8.1/cc6c9dc/bin/g++ $jinclude $snappy_include $zlib_include $bzip_include $lz4_include $gflags_include $numa_include"
ar=$toolchain_executables/binutils/binutils-2.21.1/da39a3e/bin/ar
ranlib=$toolchain_executables/binutils/binutils-2.21.1/da39a3e/bin/ranlib

cflags="-b$toolchain_executables/binutils/binutils-2.21.1/da39a3e/bin/gold -m64 -mtune=generic"
cflags+=" $libgcc_include $glibc_include"
cflags+=" -drocksdb_platform_posix -drocksdb_atomic_present -drocksdb_fallocate_present"
cflags+=" -dsnappy -dgflags=google -dzlib -dbzip2 -dlz4 -dnuma"

exec_ldflags="-wl,--dynamic-linker,/usr/local/fbcode/gcc-4.8.1-glibc-2.17/lib/ld.so"
exec_ldflags+=" -wl,--no-whole-archive $toolchain_lib_base/libunwind/libunwind-1.0.1/675d945/lib/libunwind.a"
exec_ldflags+=" $hdfslib $snappy_libs $zlib_libs $bzip_libs $lz4_libs $gflags_libs $numa_lib"

platform_ldflags="$libgcc_libs $glibc_libs "

exec_ldflags_shared="$snappy_libs $zlib_libs $bzip_libs $lz4_libs $gflags_libs"

valgrind_ver="$toolchain_lib_base/valgrind/valgrind-3.8.1/c3f970a/bin/"

export cc cxx ar ranlib cflags exec_ldflags exec_ldflags_shared valgrind_ver jemalloc_lib jemalloc_include
