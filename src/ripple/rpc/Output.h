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

#ifndef rippled_ripple_basics_types_output_h
#define rippled_ripple_basics_types_output_h

#include <boost/utility/string_ref.hpp>

namespace ripple {
namespace rpc {

using output = std::function <void (boost::string_ref const&)>;

inline
output stringoutput (std::string& s)
{
    return [&](boost::string_ref const& b) { s.append (b.data(), b.size()); };
}

} // rpc
} // ripple

#endif
