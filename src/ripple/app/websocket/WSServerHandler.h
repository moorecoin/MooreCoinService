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

#ifndef ripple_wsserverhandler_h_included
#define ripple_wsserverhandler_h_included

#include <ripple/app/main/application.h>
#include <ripple/app/websocket/wsconnection.h>
#include <ripple/protocol/jsonfields.h>
#include <ripple/server/port.h>
#include <ripple/json/json_reader.h>
#include <ripple/unity/websocket.h>
#include <memory>
#include <unordered_map>

namespace ripple {

extern bool serverokay (std::string& reason);

template <typename endpoint_type>
class wsconnectiontype;

// caution: on_* functions are called by the websocket code while holding a lock

struct wsserverhandlerlog;

// this tag helps with mutex tracking
struct wsserverhandlerbase
{
};

// a single instance of this object is made.
// this instance dispatches all events.  there is no per connection persistence.

template <typename endpoint_type>
class wsserverhandler
    : public wsserverhandlerbase
    , public endpoint_type::handler
    , public beast::leakchecked <wsserverhandler <endpoint_type> >
{
public:
    typedef typename endpoint_type::handler::connection_ptr     connection_ptr;
    typedef typename endpoint_type::handler::message_ptr        message_ptr;
    typedef std::shared_ptr< wsconnectiontype <endpoint_type> >    wsc_ptr;

    // private reasons to close.
    enum
    {
        crtooslow   = 4000,     // client is too slow.
    };

private:
    std::shared_ptr<http::port> port_;
    resource::manager& m_resourcemanager;
    infosub::source& m_source;

protected:
    // vfalco todo make this private.
    typedef ripplemutex locktype;
    typedef std::lock_guard <locktype> scopedlocktype;
    locktype mlock;

    // for each connection maintain an associated object to track subscriptions.
    using maptype = hash_map <connection_ptr, wsc_ptr>;
    maptype mmap;

public:
    wsserverhandler (std::shared_ptr<http::port> const& port,
        resource::manager& resourcemanager, infosub::source& source)
        : port_(port)
        , m_resourcemanager (resourcemanager)
        , m_source (source)
    {
    }

    wsserverhandler(wsserverhandler const&) = delete;
    wsserverhandler& operator= (wsserverhandler const&) = delete;

    http::port const&
    port() const
    {
        return *port_;
    }

    bool getpublic()
    {
        return port_->allow_admin;
    };

    static void ssend (connection_ptr cpclient, message_ptr mpmessage)
    {
        try
        {
            cpclient->send (mpmessage->get_payload (), mpmessage->get_opcode ());
        }
        catch (...)
        {
            cpclient->close (websocketpp_02::close::status::value (crtooslow),
                             std::string ("client is too slow."));
        }
    }

    static void ssendb (connection_ptr cpclient, std::string const& strmessage,
                        bool broadcast)
    {
        try
        {
            writelog (broadcast ? lstrace : lsinfo, wsserverhandlerlog)
                    << "ws:: sending '" << strmessage << "'";

            cpclient->send (strmessage);
        }
        catch (...)
        {
            cpclient->close (websocketpp_02::close::status::value (crtooslow),
                             std::string ("client is too slow."));
        }
    }

    void send (connection_ptr cpclient, message_ptr mpmessage)
    {
        cpclient->get_strand ().post (
            std::bind (
                &wsserverhandler<endpoint_type>::ssend,
                cpclient, mpmessage));
    }

    void send (connection_ptr cpclient, std::string const& strmessage,
               bool broadcast)
    {
        cpclient->get_strand ().post (
            std::bind (
                &wsserverhandler<endpoint_type>::ssendb, cpclient, strmessage,
                broadcast));
    }

    void send (connection_ptr cpclient, json::value const& jvobj, bool broadcast)
    {
        send (cpclient, to_string (jvobj), broadcast);
    }

    void pingtimer (connection_ptr cpclient)
    {
        wsc_ptr ptr;
        {
            scopedlocktype sl (mlock);
            auto it = mmap.find (cpclient);

            if (it == mmap.end ())
                return;

            ptr = it->second;
        }
        std::string data ("ping");

        if (ptr->onpingtimer (data))
        {
            cpclient->terminate (false);
            try
            {
                writelog (lsdebug, wsserverhandlerlog) <<
                    "ws:: ping_out(" <<
                     cpclient->get_socket ().remote_endpoint ().to_string () <<
                     ")";
            }
            catch (...)
            {
            }
        }
        else
            cpclient->ping (data);
    }

    void on_send_empty (connection_ptr cpclient)
    {
        wsc_ptr ptr;
        {
            scopedlocktype sl (mlock);
            auto it = mmap.find (cpclient);

            if (it == mmap.end ())
                return;

            ptr = it->second;
        }

        ptr->onsendempty ();
    }

    void on_open (connection_ptr cpclient)
    {
        scopedlocktype   sl (mlock);

        try
        {
            std::pair <typename maptype::iterator, bool> const result (
                mmap.emplace (cpclient,
                    std::make_shared < wsconnectiontype <endpoint_type> > (
                        std::ref(m_resourcemanager),
                        std::ref (m_source),
                        std::ref(*this),
                        std::cref(cpclient))));
            assert (result.second);
            (void) result.second;
            writelog (lsdebug, wsserverhandlerlog) <<
                "ws:: on_open(" <<
                cpclient->get_socket ().remote_endpoint() << ")";
        }
        catch (...)
        {
        }
    }

    void on_pong (connection_ptr cpclient, std::string data)
    {
        wsc_ptr ptr;
        {
            scopedlocktype   sl (mlock);
            auto it = mmap.find (cpclient);

            if (it == mmap.end ())
                return;

            ptr = it->second;
        }
        try
        {
            writelog (lsdebug, wsserverhandlerlog) <<
           "ws:: on_pong(" << cpclient->get_socket ().remote_endpoint() << ")";
        }
        catch (...)
        {
        }
        ptr->onpong (data);
    }

    void on_close (connection_ptr cpclient)
    {
        doclose (cpclient, "on_close");
    }

    void on_fail (connection_ptr cpclient)
    {
        doclose (cpclient, "on_fail");
    }

    void doclose (connection_ptr const& cpclient, char const* reason)
    {
        // we cannot destroy the connection while holding the map lock or we
        // deadlock with publedger
        wsc_ptr ptr;
        {
            scopedlocktype   sl (mlock);
            auto it = mmap.find (cpclient);

            if (it == mmap.end ())
            {
                try
                {
                    writelog (lsdebug, wsserverhandlerlog) <<
                        "ws:: " << reason << "(" <<
                           cpclient->get_socket ().remote_endpoint() <<
                           ") not found";
                }
                catch (...)
                {
                }
                return;
            }

            ptr = it->second;
            // prevent the wsconnection from being destroyed until we release
            // the lock
            mmap.erase (it);
        }
        ptr->predestroy (); // must be done before we return
        try
        {
            writelog (lsdebug, wsserverhandlerlog) <<
                "ws:: " << reason << "(" <<
                   cpclient->get_socket ().remote_endpoint () << ") found";
        }
        catch (...)
        {
        }

        // must be done without holding the websocket send lock
        getapp().getjobqueue ().addjob (
            jtclient,
            "wsclient::destroy",
            std::bind (&wsconnectiontype <endpoint_type>::destroy, ptr));
    }

    void on_message (connection_ptr cpclient, message_ptr mpmessage)
    {
        wsc_ptr ptr;
        {
            scopedlocktype   sl (mlock);
            auto it = mmap.find (cpclient);

            if (it == mmap.end ())
                return;

            ptr = it->second;
        }

        bool brejected, brunq;
        ptr->rcvmessage (mpmessage, brejected, brunq);

        if (brejected)
        {
            try
            {
                writelog (lsdebug, wsserverhandlerlog) <<
                    "ws:: rejected(" <<
                    cpclient->get_socket().remote_endpoint() <<
                    ") '" << mpmessage->get_payload () << "'";
            }
            catch (...)
            {
            }
        }

        if (brunq)
            getapp().getjobqueue ().addjob (jtclient, "wsclient::command",
                      std::bind (&wsserverhandler<endpoint_type>::do_messages,
                                 this, std::placeholders::_1, cpclient));
    }

    void do_messages (job& job, connection_ptr cpclient)
    {
        wsc_ptr ptr;
        {
            scopedlocktype   sl (mlock);
            auto it = mmap.find (cpclient);

            if (it == mmap.end ())
                return;

            ptr = it->second;
        }

        // this loop prevents a single thread from handling more
        // than 3 operations for the same client, otherwise a client
        // can monopolize resources.
        //
        for (int i = 0; i < 3; ++i)
        {
            message_ptr msg = ptr->getmessage ();

            if (!msg)
                return;

            if (!do_message (job, cpclient, ptr, msg))
            {
                ptr->returnmessage(msg);
                return;
            }
        }

        if (ptr->checkmessage ())
            getapp().getjobqueue ().addjob (
                jtclient, "wsclient::more",
                std::bind (&wsserverhandler<endpoint_type>::do_messages, this,
                           std::placeholders::_1, cpclient));
    }

    bool do_message (job& job, const connection_ptr& cpclient,
                     const wsc_ptr& conn, const message_ptr& mpmessage)
    {
        json::value     jvrequest;
        json::reader    jrreader;

        try
        {
            writelog (lsinfo, wsserverhandlerlog) <<
                "ws:: receiving(" << cpclient->get_socket ().remote_endpoint () <<
                ") '" << mpmessage->get_payload () << "'";
        }
        catch (...)
        {
        }

        if (mpmessage->get_opcode () != websocketpp_02::frame::opcode::text)
        {
            json::value jvresult (json::objectvalue);

            jvresult[jss::type]    = jss::error;
            jvresult[jss::error]   = "wstextrequired";
            // we only accept text messages.

            send (cpclient, jvresult, false);
        }
        else if (!jrreader.parse (mpmessage->get_payload (), jvrequest) ||
                 jvrequest.isnull () || !jvrequest.isobject ())
        {
            json::value jvresult (json::objectvalue);

            jvresult[jss::type]    = jss::error;
            jvresult[jss::error]   = "jsoninvalid";    // received invalid json.
            jvresult[jss::value]   = mpmessage->get_payload ();

            send (cpclient, jvresult, false);
        }
        else
        {
            if (jvrequest.ismember (jss::command))
            {
                json::value& jcmd = jvrequest[jss::command];
                if (jcmd.isstring())
                    job.rename (std::string ("wsclient::") + jcmd.asstring());
            }

            send (cpclient, conn->invokecommand (jvrequest), false);
        }

        return true;
    }

    boost::asio::ssl::context&
    get_ssl_context ()
    {
        return *port_->context;
    }

    bool
    plain_only()
    {
        return port_->protocol.count("wss") == 0;
    }

    bool
    secure_only()
    {
        return port_->protocol.count("ws") == 0;
    }

    // respond to http requests.
    bool http (connection_ptr cpclient)
    {
        std::string reason;

        if (!serverokay (reason))
        {
            cpclient->set_body (
                "<html><body>server cannot accept clients: " +
                reason + "</body></html>");
            return false;
        }

        cpclient->set_body (
            "<!doctype html><html><head><title>" + systemname () +
            " test</title></head>" + "<body><h1>" + systemname () +
            " test</h1><p>this page shows http(s) connectivity is working.</p></body></html>");
        return true;
    }
};

} // ripple

#endif
