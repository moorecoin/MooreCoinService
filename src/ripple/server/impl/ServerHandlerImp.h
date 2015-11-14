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

#ifndef ripple_server_serverhandlerimp_h_included
#define ripple_server_serverhandlerimp_h_included

#include <ripple/core/job.h>
#include <ripple/server/serverhandler.h>
#include <ripple/server/session.h>
#include <ripple/rpc/output.h>
#include <ripple/rpc/rpchandler.h>

namespace ripple {

// private implementation
class serverhandlerimp
    : public serverhandler
    , public beast::leakchecked <serverhandlerimp>
    , public http::handler
{
private:
    resource::manager& m_resourcemanager;
    beast::journal m_journal;
    jobqueue& m_jobqueue;
    networkops& m_networkops;
    std::unique_ptr<http::server> m_server;
    setup setup_;

public:
    serverhandlerimp (stoppable& parent, boost::asio::io_service& io_service,
        jobqueue& jobqueue, networkops& networkops,
            resource::manager& resourcemanager);

    ~serverhandlerimp();

private:
    using output = rpc::output;
    using yield = rpc::yield;

    void
    setup (setup const& setup, beast::journal journal) override;

    setup const&
    setup() const override
    {
        return setup_;
    }

    //
    // stoppable
    //

    void
    onstop() override;

    //
    // http::handler
    //

    void
    onaccept (http::session& session) override;

    bool
    onaccept (http::session& session,
        boost::asio::ip::tcp::endpoint endpoint) override;

    void
    onlegacypeerhello (std::unique_ptr<beast::asio::ssl_bundle>&& ssl_bundle,
        boost::asio::const_buffer buffer,
            boost::asio::ip::tcp::endpoint remote_address) override;

    handoff
    onhandoff (http::session& session,
        std::unique_ptr <beast::asio::ssl_bundle>&& bundle,
            beast::http::message&& request,
                boost::asio::ip::tcp::endpoint remote_address) override;

    handoff
    onhandoff (http::session& session, boost::asio::ip::tcp::socket&& socket,
        beast::http::message&& request,
            boost::asio::ip::tcp::endpoint remote_address) override;
    void
    onrequest (http::session& session) override;

    void
    onclose (http::session& session,
        boost::system::error_code const&) override;

    void
    onstopped (http::server&) override;

    //--------------------------------------------------------------------------

    void
    processsession (std::shared_ptr<http::session> const&, yield const&);

    void
    processrequest (http::port const& port, std::string const& request,
        beast::ip::endpoint const& remoteipaddress, output, yield);

    //
    // propertystream
    //

    void
    onwrite (beast::propertystream::map& map) override;

private:
    bool
    iswebsocketupgrade (beast::http::message const& request);

    bool
    authorized (http::port const& port,
        std::map<std::string, std::string> const& h);
};

}

#endif
