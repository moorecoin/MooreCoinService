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

#ifndef beast_config_configcheck_h_included
#define beast_config_configcheck_h_included

//
// apply sensible defaults for the configuration settings
//

#ifndef beast_force_debug
#define beast_force_debug 0
#endif

#ifndef beast_log_assertions
# if beast_android
#  define beast_log_assertions 1
# else
#  define beast_log_assertions 0
# endif
#endif

#if beast_debug && ! defined (beast_check_memory_leaks)
#define beast_check_memory_leaks 1
#endif

#ifndef beast_disable_contract_checks
#define beast_disable_contract_checks 0
#endif

//------------------------------------------------------------------------------

#ifndef beast_dont_autolink_to_win32_libraries
#define beast_dont_autolink_to_win32_libraries 0
#endif

#ifndef beast_include_zlib_code
#define beast_include_zlib_code 1
#endif

#ifndef beast_zlib_include_path
#define beast_zlib_include_path <zlib.h>
#endif

#ifndef beast_sqlite_force_ndebug
#define beast_sqlite_force_ndebug 0
#endif

#ifndef beast_string_utf_type
#define beast_string_utf_type 8
#endif

//------------------------------------------------------------------------------

#ifndef beast_use_boost_features
#define beast_use_boost_features 0
#endif

#endif
