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

#ifndef beast_core_fatalerror_h_included
#define beast_core_fatalerror_h_included

#include <beast/strings/string.h>

namespace beast
{

/** signal a fatal error.

    a fatal error indicates that the program has encountered an unexpected
    situation and cannot continue safely. reasons for raising a fatal error
    would be to protect data integrity, prevent valuable resources from being
    wasted, or to ensure that the user does not experience undefined behavior.

    if multiple threads raise an error, only one will succeed while the others
    will be blocked before the process terminates.
*/
void
fatalerror (char const* message, char const* file = nullptr, int line = 0);

} // beast

#endif
