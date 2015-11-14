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

//  chrono_io
//
//  (c) copyright howard hinnant
//  use, modification and distribution are subject to the boost software license,
//  version 1.0. (see accompanying file license_1_0.txt or copy at
//  http://www.boost.org/license_1_0.txt).

#include <beast/chrono/chrono_io.h>

//_libcpp_begin_namespace_std
namespace std {

namespace chrono
{

locale::id
durationpunct::id;

}  // chrono

//_libcpp_end_namespace_std
}
