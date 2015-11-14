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

#ifndef ripple_server_serverhandler_h_included
#define ripple_server_serverhandler_h_included

#include <ripple/basics/basicconfig.h>
#include <ripple/server/port.h>
#include <ripple/overlay/overlay.h>
#include <ripple/rpc/yield.h>
#include <beast/utility/journal.h>
#include <beast/utility/propertystream.h>
#include <beast/cxx14/memory.h> // <memory>
#include <boost/asio/ip/address.hpp>
#include <vector>

namespace ripple {

class serverhandler
    : public beast::stoppable
    , public beast::propertystream::source
{
protected:
    serverhandler (stoppable& parent);

public:
    struct setup
    {
        std::vector<http::port> ports;

        // memberspace
        struct client_t
        {
            bool secure;
            std::string ip;
            std::uint16_t port;
            std::string user;
            std::string password;
            std::string admin_user;
            std::string admin_password;
        };

        // configuration when acting in client role
        client_t client;

        // configuration for the overlay
        struct overlay_t
        {
            boost::asio::ip::address ip;
            std::uint16_t port = 0;
        };

        overlay_t overlay;
        rpc::yieldstrategy yieldstrategy;

        void
        makecontexts();
    };

    virtual
    ~serverhandler() = default;

    /** opens listening ports based on the config settings
        this is implemented outside the constructor to support
        two-stage initialization in the application object.
    */
    virtual
    void
    setup (setup const& setup, beast::journal journal) = 0;

    /** returns the setup associated with the handler. */
    virtual
    setup const&
    setup() const = 0;

    /** fills in boilerplate http header field values. */
    static
    void
    appendstandardfields (beast::http::message& message);
};

//------------------------------------------------------------------------------

serverhandler::setup
setup_serverhandler (basicconfig const& c, std::ostream& log);

} // ripple

#endif
