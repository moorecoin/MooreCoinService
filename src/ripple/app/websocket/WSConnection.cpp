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
#include <ripple/app/websocket/wsconnection.h>
#include <ripple/net/rpcerr.h>
#include <ripple/app/main/application.h>
#include <ripple/protocol/jsonfields.h>
#include <ripple/resource/fees.h>
#include <ripple/rpc/rpchandler.h>
#include <ripple/server/role.h>

namespace ripple {

wsconnection::wsconnection (http::port const& port,
    resource::manager& resourcemanager, resource::consumer usage,
        infosub::source& source, bool ispublic,
            beast::ip::endpoint const& remoteaddress,
                boost::asio::io_service& io_service)
    : infosub (source, usage)
    , port_(port)
    , m_resourcemanager (resourcemanager)
    , m_ispublic (ispublic)
    , m_remoteaddress (remoteaddress)
    , m_netops (getapp ().getops ())
    , m_pingtimer (io_service)
    , m_sentping (false)
    , m_receivequeuerunning (false)
    , m_isdead (false)
    , m_io_service (io_service)
{
    writelog (lsdebug, wsconnection) <<
        "websocket connection from " << remoteaddress;
}

wsconnection::~wsconnection ()
{
}

void wsconnection::onpong (std::string const&)
{
    m_sentping = false;
}

void wsconnection::rcvmessage (
    message_ptr msg, bool& msgrejected, bool& runqueue)
{
    scopedlocktype sl (m_receivequeuemutex);

    if (m_isdead)
    {
        msgrejected = false;
        runqueue = false;
        return;
    }

    if ((m_receivequeue.size () >= 1000) ||
        (msg->get_payload().size() > 1000000))
    {
        msgrejected = true;
        runqueue = false;
    }
    else
    {
        msgrejected = false;
        m_receivequeue.push_back (msg);

        if (m_receivequeuerunning)
            runqueue = false;
        else
        {
            runqueue = true;
            m_receivequeuerunning = true;
        }
    }
}

bool wsconnection::checkmessage ()
{
    scopedlocktype sl (m_receivequeuemutex);

    assert (m_receivequeuerunning);

    if (m_isdead || m_receivequeue.empty ())
    {
        m_receivequeuerunning = false;
        return false;
    }

    return true;
}

wsconnection::message_ptr wsconnection::getmessage ()
{
    scopedlocktype sl (m_receivequeuemutex);

    if (m_isdead || m_receivequeue.empty ())
    {
        m_receivequeuerunning = false;
        return message_ptr ();
    }

    message_ptr m = m_receivequeue.front ();
    m_receivequeue.pop_front ();
    return m;
}

void wsconnection::returnmessage (message_ptr ptr)
{
    scopedlocktype sl (m_receivequeuemutex);

    if (!m_isdead)
    {
        m_receivequeue.push_front (ptr);
        m_receivequeuerunning = false;
    }
}

json::value wsconnection::invokecommand (json::value& jvrequest)
{
    if (getconsumer().disconnect ())
    {
        disconnect ();
        return rpcerror (rpcslow_down);
    }

    // requests without "command" are invalid.
    //
    if (!jvrequest.ismember (jss::command))
    {
        json::value jvresult (json::objectvalue);

        jvresult[jss::type]    = jss::response;
        jvresult[jss::status]  = jss::error;
        jvresult[jss::error]   = jss::missingcommand;
        jvresult[jss::request] = jvrequest;

        if (jvrequest.ismember (jss::id))
        {
            jvresult[jss::id]  = jvrequest[jss::id];
        }

        getconsumer().charge (resource::feeinvalidrpc);

        return jvresult;
    }

    resource::charge loadtype = resource::feereferencerpc;
    json::value jvresult (json::objectvalue);

    role const role = port_.allow_admin ? adminrole (port_, jvrequest,
        m_remoteaddress, getconfig().rpc_admin_allow) : role::guest;

    if (role::forbid == role)
    {
        jvresult[jss::result]  = rpcerror (rpcforbidden);
    }
    else
    {
        rpc::context context {
            jvrequest, loadtype, m_netops, role,
            std::dynamic_pointer_cast<infosub> (this->shared_from_this ())};
        rpc::docommand (context, jvresult[jss::result]);
    }

    getconsumer().charge (loadtype);
    if (getconsumer().warn ())
    {
        jvresult[jss::warning] = jss::load;
    }

    // currently we will simply unwrap errors returned by the rpc
    // api, in the future maybe we can make the responses
    // consistent.
    //
    // regularize result. this is duplicate code.
    if (jvresult[jss::result].ismember (jss::error))
    {
        jvresult               = jvresult[jss::result];
        jvresult[jss::status]  = jss::error;
        jvresult[jss::request] = jvrequest;

    }
    else
    {
        jvresult[jss::status] = jss::success;
    }

    if (jvrequest.ismember (jss::id))
    {
        jvresult[jss::id] = jvrequest[jss::id];
    }

    jvresult[jss::type] = jss::response;

    return jvresult;
}

} // ripple
