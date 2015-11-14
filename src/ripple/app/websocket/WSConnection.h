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

#ifndef ripple_wsconnection_h
#define ripple_wsconnection_h

#include <ripple/app/misc/networkops.h>
#include <ripple/core/config.h>
#include <ripple/net/infosub.h>
#include <ripple/resource/manager.h>
#include <ripple/server/port.h>
#include <ripple/json/to_string.h>
#include <ripple/unity/websocket.h>
#include <beast/asio/placeholders.h>
#include <memory>

namespace ripple {

/** a ripple websocket connection handler.
    this handles everything that is independent of the endpint_type.
*/
class wsconnection
    : public std::enable_shared_from_this <wsconnection>
    , public infosub
    , public countedobject <wsconnection>
{
public:
    static char const* getcountedobjectname () { return "wsconnection"; }

protected:
    typedef websocketpp_02::message::data::ptr message_ptr;

    wsconnection (http::port const& port,
        resource::manager& resourcemanager, resource::consumer usage,
            infosub::source& source, bool ispublic,
                beast::ip::endpoint const& remoteaddress,
                    boost::asio::io_service& io_service);

    wsconnection(wsconnection const&) = delete;
    wsconnection& operator= (wsconnection const&) = delete;

    virtual ~wsconnection ();

    virtual void predestroy () = 0;
    virtual void disconnect () = 0;

public:
    void onpong (std::string const&);
    void rcvmessage (message_ptr msg, bool& msgrejected, bool& runqueue);
    message_ptr getmessage ();
    bool checkmessage ();
    void returnmessage (message_ptr ptr);
    json::value invokecommand (json::value& jvrequest);

protected:
    http::port const& port_;
    resource::manager& m_resourcemanager;
    resource::consumer m_usage;
    bool const m_ispublic;
    beast::ip::endpoint const m_remoteaddress;
    locktype m_receivequeuemutex;
    std::deque <message_ptr> m_receivequeue;
    networkops& m_netops;
    boost::asio::deadline_timer m_pingtimer;
    bool m_sentping;
    bool m_receivequeuerunning;
    bool m_isdead;
    boost::asio::io_service& m_io_service;
};

//------------------------------------------------------------------------------

template <typename endpoint_type>
class wsserverhandler;

/** a ripple websocket connection handler for a specific endpoint_type.
*/
template <typename endpoint_type>
class wsconnectiontype
    : public wsconnection
{
public:
    typedef typename endpoint_type::connection_type connection;
    typedef typename boost::shared_ptr<connection> connection_ptr;
    typedef typename boost::weak_ptr<connection> weak_connection_ptr;
    typedef wsserverhandler <endpoint_type> server_type;

private:
    server_type& m_serverhandler;
    weak_connection_ptr m_connection;

public:
    wsconnectiontype (resource::manager& resourcemanager,
                      infosub::source& source,
                      server_type& serverhandler,
                      connection_ptr const& cpconnection)
        : wsconnection (
            serverhandler.port(),
            resourcemanager,
            resourcemanager.newinboundendpoint (
                cpconnection->get_socket ().remote_endpoint ()),
            source,
            serverhandler.getpublic (),
            cpconnection->get_socket ().remote_endpoint (),
            cpconnection->get_io_service ())
        , m_serverhandler (serverhandler)
        , m_connection (cpconnection)
    {
        setpingtimer ();
    }

    void predestroy ()
    {
        // sever connection
        m_pingtimer.cancel ();
        m_connection.reset ();

        {
            scopedlocktype sl (m_receivequeuemutex);
            m_isdead = true;
        }
    }

    static void destroy (std::shared_ptr <wsconnectiontype <endpoint_type> >)
    {
        // just discards the reference
    }

    // implement overridden functions from base class:
    void send (json::value const& jvobj, bool broadcast)
    {
        connection_ptr ptr = m_connection.lock ();

        if (ptr)
            m_serverhandler.send (ptr, jvobj, broadcast);
    }

    void send (json::value const& jvobj, std::string const& sobj, bool broadcast)
    {
        connection_ptr ptr = m_connection.lock ();

        if (ptr)
            m_serverhandler.send (ptr, sobj, broadcast);
    }

    void disconnect ()
    {
        connection_ptr ptr = m_connection.lock ();

        if (ptr)
            m_io_service.dispatch (ptr->get_strand ().wrap (std::bind (
                &wsconnectiontype <endpoint_type>::handle_disconnect,
                    m_connection)));
    }

    static void handle_disconnect(weak_connection_ptr c)
    {
        connection_ptr ptr = c.lock ();

        if (ptr)
            ptr->close (websocketpp_02::close::status::protocol_error, "overload");
    }

    bool onpingtimer (std::string&)
    {
        if (m_sentping)
            return true; // causes connection to close

        m_sentping = true;
        setpingtimer ();
        return false; // causes ping to be sent
    }

    //--------------------------------------------------------------------------

    static void pingtimer (weak_connection_ptr c, server_type* h,
        boost::system::error_code const& e)
    {
        if (e)
            return;

        connection_ptr ptr = c.lock ();

        if (ptr)
            h->pingtimer (ptr);
    }

    void setpingtimer ()
    {
        connection_ptr ptr = m_connection.lock ();

        if (ptr)
        {
            m_pingtimer.expires_from_now (boost::posix_time::seconds
                (getconfig ().websocket_ping_freq));

            m_pingtimer.async_wait (ptr->get_strand ().wrap (
                std::bind (&wsconnectiontype <endpoint_type>::pingtimer,
                    m_connection, &m_serverhandler,
                           beast::asio::placeholders::error)));
        }
    }
};

} // ripple

#endif
