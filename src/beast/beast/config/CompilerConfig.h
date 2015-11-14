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

#ifndef beast_config_compilerconfig_h_included
#define beast_config_compilerconfig_h_included

#include <assert.h>
#include <beast/config/platformconfig.h>

// this file defines miscellaneous macros for debugging, assertions, etc.

#if beast_force_debug
# undef beast_debug
# define beast_debug 1
#endif

/** this macro defines the c calling convention used as the standard for beast calls.
*/
#if beast_msvc
# define beast_cdecl      __cdecl
#else
# define beast_cdecl
#endif

/** this macro fixes c++'s constexpr for vs2012, which doesn't understand it.
*/
#if beast_msvc
# define beast_constexpr const
#else
# define beast_constexpr constexpr
#endif


// debugging and assertion macros

#if beast_log_assertions || beast_debug
#define beast_logcurrentassertion    beast::logassertion (__file__, __line__);
#else
#define beast_logcurrentassertion
#endif

#if beast_ios || beast_linux || beast_android || beast_ppc
/** this will try to break into the debugger if the app is currently being debugged.
    if called by an app that's not being debugged, the behaiour isn't defined - it may crash or not, depending
    on the platform.
    @see bassert()
*/
# define beast_breakdebugger        { ::kill (0, sigtrap); }
#elif beast_use_intrinsics
# ifndef __intel_compiler
#  pragma intrinsic (__debugbreak)
# endif
# define beast_breakdebugger        { __debugbreak(); }
#elif beast_gcc || beast_mac
# if beast_no_inline_asm
#  define beast_breakdebugger       { }
# else
#  define beast_breakdebugger       { asm ("int $3"); }
# endif
#else
#  define beast_breakdebugger       { __asm int 3 }
#endif

#if beast_clang && defined (__has_feature) && ! defined (beast_analyzer_noreturn)
# if __has_feature (attribute_analyzer_noreturn)
   inline void __attribute__((analyzer_noreturn)) beast_assert_noreturn() {}
#  define beast_analyzer_noreturn beast_assert_noreturn();
# endif
#endif

#ifndef beast_analyzer_noreturn
#define beast_analyzer_noreturn
#endif

//------------------------------------------------------------------------------

#ifdef __cplusplus
extern "c" {
#endif
/** report a fatal error message and terminate the application.
    normally you won't call this directly.
*/
extern void beast_reportfatalerror (char const* message, char const* filename, int linenumber);
#ifdef __cplusplus
}
#endif

#if beast_debug || doxygen

/** writes a string to the standard error stream.
    this is only compiled in a debug build.
*/
#define bdbg(dbgtext) { \
    beast::string tempdbgbuf; \
    tempdbgbuf << dbgtext; \
    beast::outputdebugstring (tempdbgbuf.tostdstring ()); \
}

#if 0
/** this will always cause an assertion failure.
    it is only compiled in a debug build, (unless beast_log_assertions is enabled for your build).
    @see bassert
*/
#define bassertfalse          { beast_logcurrentassertion; if (beast::beast_isrunningunderdebugger()) beast_breakdebugger; beast_analyzer_noreturn }

/** platform-independent assertion macro.
    this macro gets turned into a no-op when you're building with debugging turned off, so be
    careful that the expression you pass to it doesn't perform any actions that are vital for the
    correct behaviour of your program!
    @see bassertfalse
  */
#define bassert(expression)   { if (! (expression)) beast_reportfatalerror(#expression,__file__,__line__); }
#else

#define bassertfalse assert(false)
#define bassert(expression) assert(expression)
#endif

#else

// if debugging is disabled, these dummy debug and assertion macros are used..

#define bdbg(dbgtext)
#define bassertfalse              { beast_logcurrentassertion }

# if beast_log_assertions
#  define bassert(expression)      { if (! (expression)) bassertfalse; }
# else
#  define bassert(a)               {}
# endif

#endif

//------------------------------------------------------------------------------

#if ! doxygen
 #define beast_join_macro_helper(a, b) a ## b
 #define beast_stringify_macro_helper(a) #a
#endif

/** a good old-fashioned c macro concatenation helper.
    this combines two items (which may themselves be macros) into a single string,
    avoiding the pitfalls of the ## macro operator.
*/
#define beast_join_macro(item1, item2)  beast_join_macro_helper (item1, item2)

/** a handy c macro for stringifying any symbol, rather than just a macro parameter.
*/
#define beast_stringify(item)  beast_stringify_macro_helper (item)

//------------------------------------------------------------------------------

#if beast_msvc || doxygen
/** this can be placed before a stack or member variable declaration to tell
    the compiler to align it to the specified number of bytes.
*/
#define beast_align(bytes) __declspec (align (bytes))
#else
#define beast_align(bytes) __attribute__ ((aligned (bytes)))
#endif

//------------------------------------------------------------------------------

// cross-compiler deprecation macros..
#ifdef doxygen
 /** this macro can be used to wrap a function which has been deprecated. */
 #define beast_deprecated(functiondef)
#elif beast_msvc && ! beast_no_deprecation_warnings
 #define beast_deprecated(functiondef) __declspec(deprecated) functiondef
#elif beast_gcc && ! beast_no_deprecation_warnings
 #define beast_deprecated(functiondef) functiondef __attribute__ ((deprecated))
#else
 #define beast_deprecated(functiondef) functiondef
#endif

//------------------------------------------------------------------------------

#if beast_android && ! doxygen
# define beast_modal_loops_permitted 0
#elif ! defined (beast_modal_loops_permitted)
 /** some operating environments don't provide a modal loop mechanism, so this
     flag can be used to disable any functions that try to run a modal loop.
 */
 #define beast_modal_loops_permitted 1
#endif

//------------------------------------------------------------------------------

#if beast_gcc
# define beast_packed __attribute__((packed))
#elif ! doxygen
# define beast_packed
#endif

//------------------------------------------------------------------------------

// here, we'll check for c++11 compiler support, and if it's not available, define
// a few workarounds, so that we can still use some of the newer language features.
#if defined (__gxx_experimental_cxx0x__) && defined (__gnuc__) && (__gnuc__ * 100 + __gnuc_minor__) >= 405
# define beast_compiler_supports_noexcept 1
# define beast_compiler_supports_nullptr 1
# define beast_compiler_supports_move_semantics 1
# if (__gnuc__ * 100 + __gnuc_minor__) >= 407 && ! defined (beast_compiler_supports_override_and_final)
#  define beast_compiler_supports_override_and_final 1
# endif
#endif

#if beast_clang && defined (__has_feature)
# if __has_feature (cxx_nullptr)
#  define beast_compiler_supports_nullptr 1
# endif
# if __has_feature (cxx_noexcept)
#  define beast_compiler_supports_noexcept 1
# endif
# if __has_feature (cxx_rvalue_references)
#  define beast_compiler_supports_move_semantics 1
# endif
# ifndef beast_compiler_supports_override_and_final
#  define beast_compiler_supports_override_and_final 1
# endif
# ifndef beast_compiler_supports_arc
#  define beast_compiler_supports_arc 1
# endif
#endif

#if defined (_msc_ver) && _msc_ver >= 1600
# define beast_compiler_supports_nullptr 1
# define beast_compiler_supports_move_semantics 1
#endif

#if defined (_msc_ver) && _msc_ver >= 1700
# define beast_compiler_supports_override_and_final 1
#endif

#if beast_compiler_supports_move_semantics
# define beast_move_arg(type) type&&
# define beast_move_cast(type) static_cast<type&&>
#else
# define beast_move_arg(type) type
# define beast_move_cast(type) type
#endif

#ifdef __cplusplus
namespace beast {
bool beast_isrunningunderdebugger();
void logassertion (char const* file, int line);
}
#endif

#endif
