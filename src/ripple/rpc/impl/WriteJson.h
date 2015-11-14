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

#ifndef rippled_ripple_rpc_impl_writelegacyjson_h
#define rippled_ripple_rpc_impl_writelegacyjson_h

namespace ripple {
namespace rpc {

/** writes a minimal representation of a json value to an output in o(n) time.

    data is streamed right to the output, so only a marginal amount of memory is
    used.  this can be very important for a very large json::value.
 */
void writejson (json::value const&, output const&);

/** return the minimal string representation of a json::value in o(n) time.

    this requires a memory allocation for the full size of the output.
    if possible, use write().
 */
std::string jsonasstring (json::value const&);

} // rpc
} // ripple

#endif
