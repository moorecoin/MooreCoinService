# copyright (c) 2013
# all rights reserved.
#
# redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * neither the name of hyperleveldb nor the names of its contributors may
#       be used to endorse or promote products derived from this software
#       without specific prior written permission.
#
# this software is provided by the copyright holders and contributors "as is"
# and any express or implied warranties, including, but not limited to, the
# implied warranties of merchantability and fitness for a particular purpose
# are disclaimed. in no event shall the copyright owner or contributors be
# liable for any direct, indirect, incidental, special, exemplary, or
# consequential damages (including, but not limited to, procurement of
# substitute goods or services; loss of use, data, or profits; or business
# interruption) however caused and on any theory of liability, whether in
# contract, strict liability, or tort (including negligence or otherwise)
# arising in any way out of the use of this software, even if advised of the
# possibility of such damage.
#
#                                               -*- autoconf -*-
#
# process this file with autoconf to produce a configure script.

ac_prereq([2.62])
ac_init([hyperleveldb], [1.0.dev], [robert@hyperdex.org])
am_init_automake([foreign subdir-objects dist-bzip2])
m4_ifdef([am_silent_rules], [am_silent_rules([yes])])
lt_prereq([2.2])
lt_init
ac_config_srcdir([hyperleveldb/db.h])
ac_config_headers([config.h])
ac_config_macro_dir([m4])

# checks for programs.
ac_prog_cxx
ac_prog_cc
ac_lang(c++)

anal_warnings_cxx

# checks for libraries.

# checks for header files.

# checks for typedefs, structures, and compiler characteristics.

# checks for library functions.
ac_func_error_at_line
ac_func_mmap
ac_check_funcs([alarm clock_gettime mach_absolute_time ftruncate memmove mkdir munmap rmdir socket])

# optional components
snappy_detect_hdr=yes
snappy_detect_lib=yes
ac_check_lib([snappy], [snappy_compress], [], [snappy_detect_hdr=no])
ac_check_header([snappy.h],,[snappy_detect_lib=no])
ac_arg_enable([snappy], [as_help_string([--enable-snappy],
              [build with snappy @<:@default: auto@:>@])],
              [snappy=${enableval}], [snappy=no])
if test x"${snappy}" = xyes; then
    if test x"${snappy_detect_hdr}" != xyes; then
        ac_msg_error([
-------------------------------------------------
leveldb configured with the snappy library.
libsnappy.so not found
please install snappy to continue.
-------------------------------------------------])
    fi
    if test x"${snappy_detect_lib}" != xyes; then
        ac_msg_error([
-------------------------------------------------
leveldb configured with the snappy library.
snappy.h not found
please install snappy to continue.
-------------------------------------------------])
    fi
fi
if test x"${snappy_detect_hdr}" = xyes -a x"${snappy_detect_lib}" = xyes; then
snappy_flags=-dsnappy
snappy_libs=-lsnappy
else
snappy_flags=
snappy_libs=
fi
ac_subst(snappy_flags)
ac_subst(snappy_libs)

ac_config_files([makefile libhyperleveldb.pc hyperleveldb.upack])
ac_output
