//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012, 2013 ripple labs inc.

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

#ifndef json_to_string_h_included
#define json_to_string_h_included

#include <ripple/json/json_config.h>
#include <string>
#include <ostream>

namespace json {

class value;

/** writes a json::value to an std::string. */
std::string to_string (value const&);

/** output using the styledstreamwriter. @see json::operator>>(). */
std::ostream& operator<< (std::ostream&, const value& root);

} // json

#endif // json_to_string_h_included
