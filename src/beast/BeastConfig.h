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

#ifndef beast_beastconfig_h_included
#define beast_beastconfig_h_included

/** configuration file for beast.
    this sets various configurable options for beast. in order to compile you
    must place a copy of this file in a location where your build environment
    can find it, and then customize its contents to suit your needs.
    @file beastconfig.h
*/

//------------------------------------------------------------------------------
//
// unit tests
//
//------------------------------------------------------------------------------

/** config: beast_no_unit_test_inline
    prevents unit test definitions from being inserted into a global table.
*/
#ifndef beast_no_unit_test_inline
#define beast_no_unit_test_inline 0
#endif

//------------------------------------------------------------------------------
//
// diagnostics
//
//------------------------------------------------------------------------------

/** config: beast_force_debug
    normally, beast_debug is set to 1 or 0 based on compiler and project
    settings, but if you define this value, you can override this to force it
    to be true or false.
*/
#ifndef   beast_force_debug
//#define beast_force_debug 1
#endif

/** config: beast_check_memory_leaks
    enables a memory-leak check for certain objects when the app terminates.
    see the leakchecked class for more details about enabling leak checking for
    specific classes.
*/
#ifndef   beast_check_memory_leaks
//#define beast_check_memory_leaks 0
#endif

//------------------------------------------------------------------------------
//
// libraries
//
//------------------------------------------------------------------------------

/** config: beast_dont_autolink_to_win32_libraries
    in a visual c++  build, this can be used to stop the required system libs
    being automatically added to the link stage.
*/
#ifndef   beast_dont_autolink_to_win32_libraries
//#define beast_dont_autolink_to_win32_libraries 1
#endif

/** config: beast_include_zlib_code
    this can be used to disable beast's embedded 3rd-party zlib code.
    you might need to tweak this if you're linking to an external zlib library
    in your app, but for normal apps, this option should be left alone.

    if you disable this, you might also want to set a value for
    beast_zlib_include_path, to specify the path where your zlib headers live.
*/
#ifndef   beast_include_zlib_code
//#define beast_include_zlib_code 1
#endif

/** config: beast_zlib_include_path
    this is included when beast_include_zlib_code is set to zero.
*/
#ifndef beast_zlib_include_path
#define beast_zlib_include_path <zlib.h>
#endif

/** config: beast_sqlite_force_ndebug
    setting this option forces sqlite into release mode even if ndebug is not set
*/
#ifndef   beast_sqlite_force_ndebug
//#define beast_sqlite_force_ndebug 1
#endif

/** config: beast_functional_uses_###
    <functional> source configuration.
    set one of these to manually force a particular implementation of bind().
    if nothing is chosen then beast will use whatever is appropriate for your
    environment based on what is available.
    if you override these, set one to 1 and the rest to 0
*/
#ifndef   beast_functional_uses_std
//#define beast_functional_uses_std    0
#endif
#ifndef   beast_functional_uses_tr1
//#define beast_functional_uses_tr1    0
#endif
#ifndef   beast_functional_uses_boost
//#define beast_functional_uses_boost  0
#endif

//------------------------------------------------------------------------------
//
// boost
//
//------------------------------------------------------------------------------

/** config: beast_use_boost_features
    this activates boost specific features and improvements. if this is
    turned on, the include paths for your build environment must be set
    correctly to find the boost headers.
*/
#ifndef   beast_use_boost_features
//#define beast_use_boost_features 1
#endif

#endif
