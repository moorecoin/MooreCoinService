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

#if beast_include_beastconfig
#include <beastconfig.h>
#endif

//==============================================================================
#include <beast/module/core/native/basicnativeheaders.h>
#include <beast/module/core/core.h>

#include <locale>
#include <cctype>

#if ! beast_bsd
 #include <sys/timeb.h>
#endif

#if ! beast_android
 #include <cwctype>
#endif

#if beast_windows
 #include <ctime>
 #include <winsock2.h>
 #include <ws2tcpip.h>

 #if ! beast_mingw
  #include <dbghelp.h>

  #if ! beast_dont_autolink_to_win32_libraries
   #pragma comment (lib, "dbghelp.lib")
  #endif
 #endif

 #if beast_mingw
  #include <ws2spi.h>
 #endif

#else
 #if beast_linux || beast_android
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <sys/errno.h>
  #include <unistd.h>
  #include <netinet/in.h>
 #endif

 #if beast_linux
  #include <langinfo.h>
 #endif

 #include <pwd.h>
 #include <fcntl.h>
 #include <netdb.h>
 #include <arpa/inet.h>
 #include <netinet/tcp.h>
 #include <sys/time.h>
 #include <net/if.h>
 #include <sys/ioctl.h>

 #if ! beast_android && ! beast_bsd
  #include <execinfo.h>
 #endif
#endif

#if beast_mac || beast_ios
 #include <xlocale.h>
 #include <mach/mach.h>
#endif

#if beast_android
 #include <android/log.h>
#endif

//------------------------------------------------------------------------------

// if the msvc debug heap headers were included, disable
// the macros during the juce include since they conflict.
#ifdef _crtdbg_map_alloc
#pragma push_macro("calloc")
#pragma push_macro("free")
#pragma push_macro("malloc")
#pragma push_macro("realloc")
#pragma push_macro("_recalloc")
#pragma push_macro("_aligned_free")
#pragma push_macro("_aligned_malloc")
#pragma push_macro("_aligned_offset_malloc")
#pragma push_macro("_aligned_realloc")
#pragma push_macro("_aligned_recalloc")
#pragma push_macro("_aligned_offset_realloc")
#pragma push_macro("_aligned_offset_recalloc")
#pragma push_macro("_aligned_msize")
#undef calloc
#undef free
#undef malloc
#undef realloc
#undef _recalloc
#undef _aligned_free
#undef _aligned_malloc
#undef _aligned_offset_malloc
#undef _aligned_realloc
#undef _aligned_recalloc
#undef _aligned_offset_realloc
#undef _aligned_offset_recalloc
#undef _aligned_msize
#endif

#include <beast/module/core/diagnostic/fatalerror.cpp>
#include <beast/module/core/diagnostic/semanticversion.cpp>
#include <beast/module/core/diagnostic/unittestutilities.cpp>

#include <beast/module/core/files/directoryiterator.cpp>
#include <beast/module/core/files/file.cpp>
#include <beast/module/core/files/fileinputstream.cpp>
#include <beast/module/core/files/fileoutputstream.cpp>

#include <beast/module/core/maths/random.cpp>

#include <beast/module/core/memory/memoryblock.cpp>

#include <beast/module/core/misc/result.cpp>

#include <beast/module/core/streams/fileinputsource.cpp>
#include <beast/module/core/streams/inputstream.cpp>
#include <beast/module/core/streams/memoryoutputstream.cpp>
#include <beast/module/core/streams/outputstream.cpp>

#include <beast/module/core/system/systemstats.cpp>

#include <beast/module/core/text/lexicalcast.cpp>
#include <beast/module/core/text/stringarray.cpp>
#include <beast/module/core/text/stringpairarray.cpp>

#include <beast/module/core/thread/deadlinetimer.cpp>
#include <beast/module/core/thread/workers.cpp>

#include <beast/module/core/time/atexithook.cpp>
#include <beast/module/core/time/time.cpp>

#if beast_mac || beast_ios
#include <beast/module/core/native/osx_objchelpers.h>
#endif

#if beast_android
#include "native/android_jnihelpers.h"
#endif

#if ! beast_windows
#include <beast/module/core/native/posix_sharedcode.h>
#endif

#if beast_mac || beast_ios
#include <beast/module/core/native/mac_files.mm>
#include <beast/module/core/native/mac_strings.mm>
#include <beast/module/core/native/mac_systemstats.mm>
#include <beast/module/core/native/mac_threads.mm>

#elif beast_windows
#include <beast/module/core/native/win32_files.cpp>
#include <beast/module/core/native/win32_systemstats.cpp>
#include <beast/module/core/native/win32_threads.cpp>

#elif beast_linux
#include <beast/module/core/native/linux_files.cpp>
#include <beast/module/core/native/linux_systemstats.cpp>
#include <beast/module/core/native/linux_threads.cpp>

#elif beast_bsd
#include <beast/module/core/native/bsd_files.cpp>
#include <beast/module/core/native/bsd_systemstats.cpp>
#include <beast/module/core/native/bsd_threads.cpp>

#elif beast_android
#include "native/android_files.cpp"
#include "native/android_misc.cpp"
#include "native/android_systemstats.cpp"
#include "native/android_threads.cpp"

#endif

// has to be outside the beast namespace
extern "c" {
void beast_reportfatalerror (char const* message, char const* filename, int linenumber)
{
    if (beast::beast_isrunningunderdebugger())
        beast_breakdebugger;
    beast::fatalerror (message, filename, linenumber);
    beast_analyzer_noreturn
}
}

#ifdef _crtdbg_map_alloc
#pragma pop_macro("calloc")
#pragma pop_macro("free")
#pragma pop_macro("malloc")
#pragma pop_macro("realloc")
#pragma pop_macro("_recalloc")
#pragma pop_macro("_aligned_free")
#pragma pop_macro("_aligned_malloc")
#pragma pop_macro("_aligned_offset_malloc")
#pragma pop_macro("_aligned_realloc")
#pragma pop_macro("_aligned_recalloc")
#pragma pop_macro("_aligned_offset_realloc")
#pragma pop_macro("_aligned_offset_recalloc")
#pragma pop_macro("_aligned_msize")
#endif
