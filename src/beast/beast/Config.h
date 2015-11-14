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

#ifndef beast_config_h_included
#define beast_config_h_included

// vfalco note this is analogous to <boost/config.hpp>

#if !defined(beast_compiler_config) && !defined(beast_no_compiler_config) && !defined(beast_no_config)
#include <beast/config/selectcompilerconfig.h>
#endif
#ifdef   beast_compiler_config
#include beast_compiler_config
#endif

#if !defined(beast_stdlib_config) && !defined(beast_no_stdlib_config) && !defined(beast_no_config) && defined(__cplusplus)
#include <beast/config/selectstdlibconfig.h>
#endif
#ifdef   beast_stdlib_config
#include beast_stdlib_config
#endif

#if !defined(beast_platform_config) && !defined(beast_no_platform_config) && !defined(beast_no_config)
#include <beast/config/selectcompilerconfig.h>
#endif
#ifdef   beast_platform_config
#include beast_platform_config
#endif

// legacy
#include <beast/version.h>
#include <beast/config/platformconfig.h>
#include <beast/config/compilerconfig.h>
#include <beast/config/standardconfig.h>
#include <beast/config/configcheck.h>

// suffix
#include <beast/config/suffix.h>
    
#endif
