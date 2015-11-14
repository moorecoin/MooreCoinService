//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

    portions of this file are from juce.
    copyright (c) 2013 - raw material software ltd.
    please visit http://www.juce.com

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

#ifndef beast_config_standardconfig_h_included
#define beast_config_standardconfig_h_included

#ifndef beast_config_compilerconfig_h_included
#error "compilerconfig.h must be included first"
#endif

// now we'll include some common os headers..
#if beast_msvc
#pragma warning (push)
#pragma warning (disable: 4514 4245 4100)
#endif

#if beast_use_intrinsics
#include <intrin.h>
#endif

#if beast_mac || beast_ios
#include <libkern/osatomic.h>
#endif

#if beast_linux
#include <signal.h>
# if __intel_compiler
#  if __ia64__
#include <ia64intrin.h>
#  else
#include <ia32intrin.h>
#  endif
# endif
#endif

#if beast_msvc && beast_debug
#include <crtdbg.h>
#include <stdlib.h>
#include <malloc.h>
#endif

#if beast_msvc
#pragma warning (pop)
#endif

#if beast_android
 #include <sys/atomics.h>
 #include <byteswap.h>
#endif

// undef symbols that are sometimes set by misguided 3rd-party headers..
#undef check
#undef type_bool
#undef max
#undef min

#endif
