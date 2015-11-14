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

#ifndef beast_config_platformconfig_h_included
#define beast_config_platformconfig_h_included

//==============================================================================
/*  this file figures out which platform is being built, and defines some macros
    that the rest of the code can use for os-specific compilation.

    macros that will be set here are:

    - one of beast_windows, beast_mac beast_linux, beast_ios, beast_android, etc.
    - either beast_32bit or beast_64bit, depending on the architecture.
    - either beast_little_endian or beast_big_endian.
    - either beast_intel or beast_ppc
    - either beast_gcc or beast_msvc
*/

//==============================================================================
#if (defined (_win32) || defined (_win64))
  #define       beast_win32 1
  #define       beast_windows 1
#elif defined (beast_android)
  #undef        beast_android
  #define       beast_android 1
#elif defined (linux) || defined (__linux__)
  #define     beast_linux 1
#elif defined (__apple_cpp__) || defined(__apple_cc__)
  #define point carbondummypointname // (workaround to avoid definition of "point" by old carbon headers)
  #define component carbondummycompname
  #include <corefoundation/corefoundation.h> // (needed to find out what platform we're using)
  #undef point
  #undef component

  #if target_os_iphone || target_iphone_simulator
    #define     beast_iphone 1
    #define     beast_ios 1
  #else
    #define     beast_mac 1
  #endif
#elif defined (__freebsd__)
  #define beast_bsd 1
#else
  #error "unknown platform!"
#endif

//==============================================================================
#if beast_windows
  #ifdef _msc_ver
    #ifdef _win64
      #define beast_64bit 1
    #else
      #define beast_32bit 1
    #endif
  #endif

  #ifdef _debug
    #define beast_debug 1
  #endif

  #ifdef __mingw32__
    #define beast_mingw 1
    #ifdef __mingw64__
      #define beast_64bit 1
    #else
      #define beast_32bit 1
    #endif
  #endif

  /** if defined, this indicates that the processor is little-endian. */
  #define beast_little_endian 1

  #define beast_intel 1
#endif

//==============================================================================
#if beast_mac || beast_ios

  #if defined (debug) || defined (_debug) || ! (defined (ndebug) || defined (_ndebug))
    #define beast_debug 1
  #endif

  #ifdef __little_endian__
    #define beast_little_endian 1
  #else
    #define beast_big_endian 1
  #endif
#endif

#if beast_mac

  #if defined (__ppc__) || defined (__ppc64__)
    #define beast_ppc 1
  #elif defined (__arm__)
    #define beast_arm 1
  #else
    #define beast_intel 1
  #endif

  #ifdef __lp64__
    #define beast_64bit 1
  #else
    #define beast_32bit 1
  #endif

  #if mac_os_x_version_min_required < mac_os_x_version_10_4
    #error "building for osx 10.3 is no longer supported!"
  #endif

  #ifndef mac_os_x_version_10_5
    #error "to build with 10.4 compatibility, use a 10.5 or 10.6 sdk and set the deployment target to 10.4"
  #endif

#endif

//==============================================================================
#if beast_linux || beast_android || beast_bsd

  #ifdef _debug
    #define beast_debug 1
  #endif

  // allow override for big-endian linux platforms
  #if defined (__little_endian__) || ! defined (beast_big_endian)
    #define beast_little_endian 1
    #undef beast_big_endian
  #else
    #undef beast_little_endian
    #define beast_big_endian 1
  #endif

  #if defined (__lp64__) || defined (_lp64)
    #define beast_64bit 1
  #else
    #define beast_32bit 1
  #endif

  #if __mmx__ || __sse__ || __amd64__
    #ifdef __arm__
      #define beast_arm 1
    #else
      #define beast_intel 1
    #endif
  #endif
#endif

//==============================================================================
// compiler type macros.

#ifdef __clang__
 #define beast_clang 1
#elif defined (__gnuc__)
  #define beast_gcc 1
#elif defined (_msc_ver)
  #define beast_msvc 1

  #if _msc_ver < 1500
    #define beast_vc8_or_earlier 1

    #if _msc_ver < 1400
      #define beast_vc7_or_earlier 1

      #if _msc_ver < 1300
        #warning "msvc 6.0 is no longer supported!"
      #endif
    #endif
  #endif

  #if beast_64bit || ! beast_vc7_or_earlier
    #define beast_use_intrinsics 1
  #endif
#else
  #error unknown compiler
#endif

//------------------------------------------------------------------------------

// handy macro that lets pragma warnings be clicked in the output window
//
// usage: #pragma message(beast_fileandline_ "advertise here!")
//
//        note that a space following the macro is mandatory for c++11.
//
// this is here so it can be used in c compilations that include this directly.
//
#define beast_pp_str2_(x) #x
#define beast_pp_str1_(x) beast_pp_str2_(x)
#define beast_fileandline_ __file__ "(" beast_pp_str1_(__line__) "): warning:"

#endif
