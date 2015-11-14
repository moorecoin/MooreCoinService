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

#ifndef beast_basicnativeheaders_h_included
#define beast_basicnativeheaders_h_included

#include <beast/config.h>

#undef t

//==============================================================================
#if beast_mac || beast_ios

 #if beast_ios
  #import <foundation/foundation.h>
  #import <uikit/uikit.h>
  #import <coredata/coredata.h>
  #import <mobilecoreservices/mobilecoreservices.h>
  #include <sys/fcntl.h>
 #else
  #define point carbondummypointname
  #define component carbondummycompname
  #import <cocoa/cocoa.h>
  #import <coreaudio/hosttime.h>
  #undef point
  #undef component
  #include <sys/dir.h>
 #endif

 #include <sys/socket.h>
 #include <sys/sysctl.h>
 #include <sys/stat.h>
 #include <sys/param.h>
 #include <sys/mount.h>
 #include <sys/utsname.h>
 #include <sys/mman.h>
 #include <fnmatch.h>
 #include <utime.h>
 #include <dlfcn.h>
 #include <ifaddrs.h>
 #include <net/if_dl.h>
 #include <mach/mach_time.h>
 #include <mach-o/dyld.h>
 #include <objc/runtime.h>
 #include <objc/objc.h>
 #include <objc/message.h>

//==============================================================================
#elif beast_windows
 #if beast_msvc
  #ifndef _cpprtti
   #error "beast requires rtti!"
  #endif

  #ifndef _cppunwind
   #error "beast requires rtti!"
  #endif

  #pragma warning (push)
  #pragma warning (disable : 4100 4201 4514 4312 4995)
 #endif

 #define strict 1
 #define win32_lean_and_mean 1
 #ifndef _win32_winnt
  #if beast_mingw
   #define _win32_winnt 0x0501
  #else
   #define _win32_winnt 0x0600
  #endif
 #endif
 #define _unicode 1
 #define unicode 1
 #ifndef _win32_ie
  #define _win32_ie 0x0400
 #endif

 #include <windows.h>
 #include <shellapi.h>
 #include <tchar.h>
 #include <stddef.h>
 #include <ctime>
 #include <wininet.h>
 #include <nb30.h>
 #include <iphlpapi.h>
 #include <mapi.h>
 #include <float.h>
 #include <process.h>
 #include <shlobj.h>
 #include <shlwapi.h>
 #include <mmsystem.h>

 #if beast_mingw
  #include <basetyps.h>
 #else
  #include <crtdbg.h>
  #include <comutil.h>
 #endif

 #undef packed

 #if beast_msvc
  #pragma warning (pop)
  #pragma warning (4: 4511 4512 4100 /*4365*/)  // (enable some warnings that are turned off in vc8)
 #endif

 #if beast_msvc && ! beast_dont_autolink_to_win32_libraries
  #pragma comment (lib, "kernel32.lib")
  #pragma comment (lib, "user32.lib")
  #pragma comment (lib, "wininet.lib")
  #pragma comment (lib, "advapi32.lib")
  #pragma comment (lib, "ws2_32.lib")
  #pragma comment (lib, "version.lib")
  #pragma comment (lib, "shlwapi.lib")
  #pragma comment (lib, "winmm.lib")

  #ifdef _native_wchar_t_defined
   #ifdef _debug
    #pragma comment (lib, "comsuppwd.lib")
   #else
    #pragma comment (lib, "comsuppw.lib")
   #endif
  #else
   #ifdef _debug
    #pragma comment (lib, "comsuppd.lib")
   #else
    #pragma comment (lib, "comsupp.lib")
   #endif
  #endif
 #endif

//==============================================================================
#elif beast_linux || beast_bsd
 #include <sched.h>
 #include <pthread.h>
 #include <sys/time.h>
 #include <errno.h>
 #include <sys/stat.h>
 #include <sys/ptrace.h>
 #include <sys/wait.h>
 #include <sys/mman.h>
 #include <fnmatch.h>
 #include <utime.h>
 #include <pwd.h>
 #include <fcntl.h>
 #include <dlfcn.h>
 #include <netdb.h>
 #include <arpa/inet.h>
 #include <netinet/in.h>
 #include <sys/types.h>
 #include <sys/ioctl.h>
 #include <sys/socket.h>
 #include <net/if.h>
 #include <sys/file.h>
 #include <signal.h>
 #include <stddef.h>

 #if beast_bsd
  #include <dirent.h>
  #include <ifaddrs.h>
  #include <net/if_dl.h>
  #include <kvm.h>
  #include <langinfo.h>
  #include <sys/cdefs.h>
  #include <sys/param.h>
  #include <sys/mount.h>
  #include <sys/types.h>
  #include <sys/sysctl.h>

  // this has to be in the global namespace
  extern char** environ;

 #else
  #include <sys/dir.h>
  #include <sys/vfs.h>
  #include <sys/sysinfo.h>
  #include <sys/prctl.h>
 #endif

//==============================================================================
#elif beast_android
 #include <jni.h>
 #include <pthread.h>
 #include <sched.h>
 #include <sys/time.h>
 #include <utime.h>
 #include <errno.h>
 #include <fcntl.h>
 #include <dlfcn.h>
 #include <sys/stat.h>
 #include <sys/statfs.h>
 #include <sys/ptrace.h>
 #include <sys/sysinfo.h>
 #include <sys/mman.h>
 #include <pwd.h>
 #include <dirent.h>
 #include <fnmatch.h>
 #include <sys/wait.h>
#endif

// need to clear various moronic redefinitions made by system headers..
#undef max
#undef min
#undef direct
#undef check

#endif   // beast_basicnativeheaders_h_included
