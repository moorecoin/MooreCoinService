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

#ifndef beast_asio_system_boostincludes_h_included
#define beast_asio_system_boostincludes_h_included

// make sure we take care of fixing boost::bind oddities first.
#if !defined(beast_core_h_included)
#error core.h must be included before including this file
#endif

// these should have already been set in your project, but
// if you forgot then we will be optimistic and choose the latest.
//
#if beast_win32
# ifndef _win32_winnt
#  pragma message ("warning: _win32_winnt was not set in your project")
#  define _win32_winnt 0x0600
# endif
# ifndef _variadic_max
#  define _variadic_max 10
# endif
#endif

// work-around for broken <boost/get_pointer.hpp>
#include <beast/boost/get_pointer.h>

#endif
