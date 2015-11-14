//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

    permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    the  software is provided "as is" and the author disclaims all warranties
    with  regard  to  this  software  including  all  implied  warranties  of
    merchantability  and  fitness. in no event shall the author be liable for
    any  special ,  direct, indirect, or consequential damages or any damages
    whatsoever  resulting  from  loss  of use, data or profits, whether in an
    action  of  contract, negligence or other tortious action, arising out of
    or in connection with the use or performance of this software.
*/
//==============================================================================

// ideas from boost

// android, which must be manually set by defining beast_android
#if defined(beast_android)
#define beast_platform_config "config/platform/android.h"

// linux, also other platforms (hurd etc) that use glibc
#elif (defined(linux) || defined(__linux) || defined(__linux__) || defined(__gnu__) || defined(__glibc__)) && !defined(_crayc)
#define beast_platform_config "config/platform/linux.h"

// bsd
#elif defined(__freebsd__) || defined(__netbsd__) || defined(__openbsd__) || defined(__dragonfly__)
#define beast_platform_config "config/platform/bsd.h"

// win32
#elif defined(_win32) || defined(__win32__) || defined(win32) || defined(_win64)
#define beast_platform_config "config/platform/win32.h"

// macos
#elif defined(macintosh) || defined(__apple__) || defined(__apple_cc__) || defined(__apple_cpp__)
#define beast_platform_config "config/platform/macos.h"

#else
#error "unsupported platform."
#endif
