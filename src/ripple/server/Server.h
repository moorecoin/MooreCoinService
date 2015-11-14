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

#ifndef ripple_server_server_h_included
#define ripple_server_server_h_included

#include <ripple/server/port.h>
#include <beast/utility/journal.h>
#include <beast/utility/propertystream.h>

namespace ripple {
namespace http {

/** multi-threaded, asynchronous http server. */
class server
{
public:
    /** destroy the server.
        the server is closed if it is not already closed. this call
        blocks until the server has stopped.
    */
    virtual
    ~server() = default;

    /** returns the journal associated with the server. */
    virtual
    beast::journal
    journal() = 0;

    /** set the listening port settings.
        this may only be called once.
    */
    virtual
    void
    ports (std::vector<port> const& v) = 0;

    virtual
    void
    onwrite (beast::propertystream::map& map) = 0;

    /** close the server.
        the close is performed asynchronously. the handler will be notified
        when the server has stopped. the server is considered stopped when
        there are no pending i/o completion handlers and all connections
        have closed.
        thread safety:
            safe to call concurrently from any thread.
    */
    virtual
    void
    close() = 0;
};

} // http
} // ripple

#endif
