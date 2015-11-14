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

#include <beastconfig.h>
#include <ripple/app/websocket/wsdoor.h>
#include <ripple/app/websocket/wsserverhandler.h>
#include <ripple/unity/websocket.h>
#include <beast/threads/thread.h>
#include <beast/cxx14/memory.h> // <memory>
#include <mutex>

namespace ripple {

//
// this is a light weight, untrusted interface for web clients.
// for now we don't provide proof.  later we will.
//
// might need to support this header for browsers: access-control-allow-origin: *
// - https://developer.mozilla.org/en-us/docs/http_access_control
//

//
// strategy:
// - we only talk to networkops (so we will work even in thin mode)
// - networkops is smart enough to subscribe and or pass back messages
//
// vfalco note networkops isn't used here...
//

class wsdoorimp
    : public wsdoor
    , protected beast::thread
    , beast::leakchecked <wsdoorimp>
{
private:
    using locktype = std::recursive_mutex;
    using scopedlocktype = std::lock_guard <locktype>;

    std::shared_ptr<http::port> port_;
    resource::manager& m_resourcemanager;
    infosub::source& m_source;
    locktype m_endpointlock;
    std::shared_ptr<websocketpp_02::server_autotls> m_endpoint;

public:
    wsdoorimp (http::port const& port, resource::manager& resourcemanager,
        infosub::source& source)
        : wsdoor (source)
        , thread ("websocket")
        , port_(std::make_shared<http::port>(port))
        , m_resourcemanager (resourcemanager)
        , m_source (source)
    {
        startthread ();
    }

    ~wsdoorimp ()
    {
        stopthread ();
    }

private:
    void run ()
    {
        writelog (lsinfo, wsdoor) <<
            "websocket: '" << port_->name << "' listening on " <<
                port_->ip.to_string() << ":" << std::to_string(port_->port) <<
                    (port_->allow_admin ? "(admin)" : "");

        websocketpp_02::server_autotls::handler::ptr handler (
            new wsserverhandler <websocketpp_02::server_autotls> (
                port_, m_resourcemanager, m_source));

        {
            scopedlocktype lock (m_endpointlock);

            m_endpoint = std::make_shared<websocketpp_02::server_autotls> (
                handler);
        }

        // call the main-event-loop of the websocket server.
        try
        {
            m_endpoint->listen (port_->ip, port_->port);
        }
        catch (websocketpp_02::exception& e)
        {
            writelog (lswarning, wsdoor) << "websocketpp_02 exception: "
                                         << e.what ();

            // temporary workaround for websocketpp_02 throwing exceptions on
            // access/close races
            for (;;)
            {
                // https://github.com/zaphoyd/websocketpp_02/issues/98
                try
                {
                    m_endpoint->get_io_service ().run ();
                    break;
                }
                catch (websocketpp_02::exception& e)
                {
                    writelog (lswarning, wsdoor) << "websocketpp_02 exception: "
                                                 << e.what ();
                }
            }
        }

        {
            scopedlocktype lock (m_endpointlock);

            m_endpoint.reset();
        }

        stopped ();
    }

    void onstop ()
    {
        std::shared_ptr<websocketpp_02::server_autotls> endpoint;

        {
            scopedlocktype lock (m_endpointlock);

             endpoint = m_endpoint;
        }

        // vfalco note we probably dont want to block here
        //             but websocketpp is deficient and broken.
        //
        if (endpoint)
            endpoint->stop ();

        signalthreadshouldexit ();
    }
};

//------------------------------------------------------------------------------

wsdoor::wsdoor (stoppable& parent)
    : stoppable ("wsdoor", parent)
{
}

//------------------------------------------------------------------------------

std::unique_ptr<wsdoor>
make_wsdoor (http::port const& port, resource::manager& resourcemanager,
    infosub::source& source)
{
    std::unique_ptr<wsdoor> door;

    try
    {
        door = std::make_unique <wsdoorimp> (port, resourcemanager, source);
    }
    catch (...)
    {
    }

    return door;
}

}
