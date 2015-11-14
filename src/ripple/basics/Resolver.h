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

#ifndef ripple_basics_resolver_h_included
#define ripple_basics_resolver_h_included

#include <vector>
#include <functional>

#include <beast/boost/errorcode.h>
#include <beast/net/ipendpoint.h>

namespace ripple {

class resolver
{
public:
    typedef std::function <
        void (std::string,
            std::vector <beast::ip::endpoint>) >
        handlertype;

    virtual ~resolver () = 0;

    /** issue an asynchronous stop request. */
    virtual void stop_async () = 0;

    /** issue a synchronous stop request. */
    virtual void stop () = 0;

    /** issue a synchronous start request. */
    virtual void start () = 0;

    /** resolve all hostnames on the list
        @param names the names to be resolved
        @param handler the handler to call
    */
    /** @{ */
    template <class handler>
    void resolve (std::vector <std::string> const& names, handler handler)
    {
        resolve (names, handlertype (handler));
    }

    virtual void resolve (
        std::vector <std::string> const& names,
        handlertype const& handler) = 0;
    /** @} */
};

}

#endif
