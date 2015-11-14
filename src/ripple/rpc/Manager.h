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

#ifndef ripple_rpc_manager_h_included
#define ripple_rpc_manager_h_included

#include <ripple/rpc/request.h>
#include <memory>

namespace ripple {
namespace rpc {

/** processes rpc commands. */
class manager
{
public:
    typedef std::function <void (request&)> handler_type;

    virtual ~manager () = 0;

    /** add a handler for the specified json-rpc command. */
    template <class handler>
    void add (std::string const& method)
    {
        add (method, handler_type (
        [](request& req)
        {
            handler h;
            h (req);
        }));
    }

    virtual void add (std::string const& method, handler_type&& handler) = 0;

    /** dispatch the json-rpc request.
        @return `true` if the command was found.
    */
    virtual bool dispatch (request& req) = 0;
};

std::unique_ptr <manager> make_manager (beast::journal journal);

} // rpc
} // ripple

#endif
