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
#include <ripple/app/main/application.h>
#include <ripple/json/json_reader.h>
#include <ripple/server/jsonwriter.h>
#include <ripple/server/make_serverhandler.h>
#include <ripple/server/impl/jsonrpcutil.h>
#include <ripple/server/impl/serverhandlerimp.h>
#include <ripple/basics/log.h>
#include <ripple/basics/make_sslcontext.h>
#include <ripple/core/jobqueue.h>
#include <ripple/server/make_server.h>
#include <ripple/overlay/overlay.h>
#include <ripple/resource/manager.h>
#include <ripple/resource/fees.h>
#include <ripple/rpc/coroutine.h>
#include <beast/crypto/base64.h>
#include <beast/cxx14/algorithm.h> // <algorithm>
#include <beast/http/rfc2616.h>
#include <boost/algorithm/string.hpp>
#include <boost/type_traits.hpp>
#include <boost/optional.hpp>
#include <boost/regex.hpp>
#include <algorithm>
#include <stdexcept>

namespace ripple {

serverhandler::serverhandler (stoppable& parent)
    : stoppable ("serverhandler", parent)
    , source ("server")
{
}

//------------------------------------------------------------------------------

serverhandlerimp::serverhandlerimp (stoppable& parent,
    boost::asio::io_service& io_service, jobqueue& jobqueue,
        networkops& networkops, resource::manager& resourcemanager)
    : serverhandler (parent)
    , m_resourcemanager (resourcemanager)
    , m_journal (deprecatedlogs().journal("server"))
    , m_jobqueue (jobqueue)
    , m_networkops (networkops)
    , m_server (http::make_server(
        *this, io_service, deprecatedlogs().journal("server")))
{
}

serverhandlerimp::~serverhandlerimp()
{
    m_server = nullptr;
}

void
serverhandlerimp::setup (setup const& setup, beast::journal journal)
{
    setup_ = setup;
    m_server->ports (setup.ports);
}

//------------------------------------------------------------------------------

void
serverhandlerimp::onstop()
{
    m_server->close();
}

//------------------------------------------------------------------------------

void
serverhandlerimp::onaccept (http::session& session)
{
}

bool
serverhandlerimp::onaccept (http::session& session,
    boost::asio::ip::tcp::endpoint endpoint)
{
    return true;
}

void
serverhandlerimp::onlegacypeerhello (
    std::unique_ptr<beast::asio::ssl_bundle>&& ssl_bundle,
        boost::asio::const_buffer buffer,
            boost::asio::ip::tcp::endpoint remote_address)
{
    // vfalco todo inject overlay
    getapp().overlay().onlegacypeerhello(std::move(ssl_bundle),
        buffer, remote_address);
}

auto
serverhandlerimp::onhandoff (http::session& session,
    std::unique_ptr <beast::asio::ssl_bundle>&& bundle,
        beast::http::message&& request,
            boost::asio::ip::tcp::endpoint remote_address) ->
    handoff
{
    if (session.port().protocol.count("wss") > 0 &&
        iswebsocketupgrade (request))
    {
        // pass to websockets
        handoff handoff;
        // handoff.moved = true;
        return handoff;
    }
    if (session.port().protocol.count("peer") > 0)
        return getapp().overlay().onhandoff (std::move(bundle),
            std::move(request), remote_address);
    // pass through to legacy onrequest
    return handoff{};
}

auto
serverhandlerimp::onhandoff (http::session& session,
    boost::asio::ip::tcp::socket&& socket,
        beast::http::message&& request,
            boost::asio::ip::tcp::endpoint remote_address) ->
    handoff
{
    if (session.port().protocol.count("ws") > 0 &&
        iswebsocketupgrade (request))
    {
        // pass to websockets
        handoff handoff;
        // handoff.moved = true;
        return handoff;
    }
    // pass through to legacy onrequest
    return handoff{};
}

static inline
rpc::output makeoutput (http::session& session)
{
    return [&](boost::string_ref const& b)
    {
        session.write (b.data(), b.size());
    };
}

namespace {

void runcoroutine (rpc::coroutine coroutine, jobqueue& jobqueue)
{
    if (!coroutine)
        return;
    coroutine();
    if (!coroutine)
        return;

    // reschedule the job on the job queue.
    jobqueue.addjob (
        jtclient, "rpc-coroutine",
        [coroutine, &jobqueue] (job&)
        {
            runcoroutine (coroutine, jobqueue);
        });
}

} // namespace

void
serverhandlerimp::onrequest (http::session& session)
{
    // make sure rpc is enabled on the port
    if (session.port().protocol.count("http") == 0 &&
        session.port().protocol.count("https") == 0)
    {
        httpreply (403, "forbidden", makeoutput (session));
        session.close (true);
        return;
    }

    // check user/password authorization
    if (! authorized (session.port(),
        build_map(session.request().headers)))
    {
        httpreply (403, "forbidden", makeoutput (session));
        session.close (true);
        return;
    }

    auto detach = session.detach();

    if (setup_.yieldstrategy.usecoroutines ==
        rpc::yieldstrategy::usecoroutines::yes)
    {
        rpc::coroutine::yieldfunction yieldfunction =
                [this, detach] (yield const& y) { processsession (detach, y); };
        runcoroutine (rpc::coroutine (yieldfunction), m_jobqueue);
    }
    else
    {
        m_jobqueue.addjob (
            jtclient, "rpc-client",
            [=] (job&) { processsession (detach, rpc::yield{}); });
    }
}

void
serverhandlerimp::onclose (http::session& session,
    boost::system::error_code const&)
{
}

void
serverhandlerimp::onstopped (http::server&)
{
    stopped();
}

//------------------------------------------------------------------------------

// dispatched on the job queue
void
serverhandlerimp::processsession (
    std::shared_ptr<http::session> const& session, yield const& yield)
{
    auto output = makeoutput (*session);
    if (auto byteyieldcount = setup_.yieldstrategy.byteyieldcount)
        output = rpc::chunkedyieldingoutput (output, yield, byteyieldcount);

    processrequest (
        session->port(),
        to_string (session->body()),
        session->remoteaddress().at_port (0),
        output,
        yield);

    if (session->request().keep_alive())
        session->complete();
    else
        session->close (true);
}

void
serverhandlerimp::processrequest (
    http::port const& port,
    std::string const& request,
    beast::ip::endpoint const& remoteipaddress,
    output output,
    yield yield)
{
    json::value jsonrpc;
    {
        json::reader reader;
        if ((request.size () > 1000000) ||
            ! reader.parse (request, jsonrpc) ||
            jsonrpc.isnull () ||
            ! jsonrpc.isobject ())
        {
            httpreply (400, "unable to parse request", output);
            return;
        }
    }

    auto const& admin_allow = getconfig().rpc_admin_allow;
    auto role = role::forbid;
    if (jsonrpc.isobject() && jsonrpc.ismember("params") &&
            jsonrpc["params"].isarray() && jsonrpc["params"].size() > 0 &&
                jsonrpc["params"][json::uint(0)].isobject())
        role = adminrole(port, jsonrpc["params"][json::uint(0)],
            remoteipaddress, admin_allow);
    else
        role = adminrole(port, json::objectvalue,
            remoteipaddress, admin_allow);

    resource::consumer usage;

    if (role == role::admin)
        usage = m_resourcemanager.newadminendpoint (remoteipaddress.to_string());
    else
        usage = m_resourcemanager.newinboundendpoint(remoteipaddress);

    if (usage.disconnect ())
    {
        httpreply (503, "server is overloaded", output);
        return;
    }

    // parse id now so errors from here on will have the id
    //
    // vfalco note except that "id" isn't included in the following errors.
    //
    json::value const id = jsonrpc ["id"];
    json::value const method = jsonrpc ["method"];

    if (method.isnull ())
    {
        httpreply (400, "null method", output);
        return;
    }

    if (! method.isstring ())
    {
        httpreply (400, "method is not string", output);
        return;
    }

    std::string strmethod = method.asstring ();
    if (strmethod.empty())
    {
        httpreply (400, "method is empty", output);
        return;
    }

    // extract request parameters from the request json as `params`.
    //
    // if the field "params" is empty, `params` is an empty object.
    //
    // otherwise, that field must be an array of length 1 (why?)
    // and we take that first entry and validate that it's an object.
    json::value params = jsonrpc ["params"];

    if (params.isnull () || params.empty())
        params = json::value (json::objectvalue);

    else if (!params.isarray () || params.size() != 1)
    {
        httpreply (400, "params unparseable", output);
        return;
    }
    else
    {
        params = std::move (params[0u]);
        if (!params.isobject())
        {
            httpreply (400, "params unparseable", output);
            return;
        }
    }

    // vfalco todo shouldn't we handle this earlier?
    //
    if (role == role::forbid)
    {
        // vfalco todo needs implementing
        // fixme needs implementing
        // xxx this needs rate limiting to prevent brute forcing password.
        httpreply (403, "forbidden", output);
        return;
    }

    resource::charge loadtype = resource::feereferencerpc;

    m_journal.info << "query: " << strmethod << to_string(params);

    // provide the json-rpc method as the field "command" in the request.
    params[jss::command] = strmethod;
    writelog (lstrace, rpchandler)
        << "dorpccommand:" << strmethod << ":" << params;

    rpc::context context {params, loadtype, m_networkops, role, nullptr, yield};
    std::string response;

    if (setup_.yieldstrategy.streaming == rpc::yieldstrategy::streaming::yes)
    {
        executerpc (context, response, setup_.yieldstrategy);
    }
    else
    {
        json::value result;
        rpc::docommand (context, result, setup_.yieldstrategy);

        // always report "status".  on an error report the request as received.
        if (result.ismember ("error"))
        {
            result[jss::status] = jss::error;
            result[jss::request] = params;
            writelog (lsdebug, rpcerr) <<
                "rpcerror: " << result ["error"] <<
                ": " << result ["error_message"];
        }
        else
        {
            result[jss::status]  = jss::success;
        }

        json::value reply (json::objectvalue);
        reply[jss::result] = std::move (result);
        response = to_string (reply);
    }

    response += '\n';
    usage.charge (loadtype);

    if (m_journal.info.active())
    {
        static const int maxsize = 10000;
        if (response.size() <= maxsize)
            m_journal.info << "reply: " << response;
        else
            m_journal.info << "reply: " << response.substr (0, maxsize);
    }

    httpreply (200, response, output);
}

//------------------------------------------------------------------------------

// returns `true` if the http request is a websockets upgrade
// http://en.wikipedia.org/wiki/http/1.1_upgrade_header#use_with_websockets
bool
serverhandlerimp::iswebsocketupgrade (beast::http::message const& request)
{
    if (request.upgrade())
        return request.headers["upgrade"] == "websocket";
    return false;
}

// vfalco todo rewrite to use beast::http::headers
bool
serverhandlerimp::authorized (http::port const& port,
    std::map<std::string, std::string> const& h)
{
    if (port.user.empty() || port.password.empty())
        return true;

    auto const it = h.find ("authorization");
    if ((it == h.end ()) || (it->second.substr (0, 6) != "basic "))
        return false;
    std::string struserpass64 = it->second.substr (6);
    boost::trim (struserpass64);
    std::string struserpass = beast::base64_decode (struserpass64);
    std::string::size_type ncolon = struserpass.find (":");
    if (ncolon == std::string::npos)
        return false;
    std::string struser = struserpass.substr (0, ncolon);
    std::string strpassword = struserpass.substr (ncolon + 1);
    return struser == port.user && strpassword == port.password;
}

//------------------------------------------------------------------------------

void
serverhandlerimp::onwrite (beast::propertystream::map& map)
{
    m_server->onwrite (map);
}

//------------------------------------------------------------------------------

// copied from config::getadminrole and modified to use the port
role
adminrole (http::port const& port,
    json::value const& params, beast::ip::endpoint const& remoteip)
{
    role role (role::forbid);

    bool const bpasswordsupplied =
        params.ismember ("admin_user") ||
        params.ismember ("admin_password");

    bool const bpasswordrequired =
        ! port.admin_user.empty() || ! port.admin_password.empty();

    bool bpasswordwrong;

    if (bpasswordsupplied)
    {
        if (bpasswordrequired)
        {
            // required, and supplied, check match
            bpasswordwrong =
                (port.admin_user !=
                    (params.ismember ("admin_user") ? params["admin_user"].asstring () : ""))
                ||
                (port.admin_password !=
                    (params.ismember ("admin_user") ? params["admin_password"].asstring () : ""));
        }
        else
        {
            // not required, but supplied
            bpasswordwrong = false;
        }
    }
    else
    {
        // required but not supplied,
        bpasswordwrong = bpasswordrequired;
    }

    // meets ip restriction for admin.
    beast::ip::endpoint const remote_addr (remoteip.at_port (0));
    bool badminip = false;

    for (auto const& allow_addr : getconfig().rpc_admin_allow)
    {
        if (allow_addr == remote_addr)
        {
            badminip = true;
            break;
        }
    }

    if (bpasswordwrong                          // wrong
            || (bpasswordsupplied && !badminip))    // supplied and doesn't meet ip filter.
    {
        role   = role::forbid;
    }
    // if supplied, password is correct.
    else
    {
        // allow admin, if from admin ip and no password is required or it was supplied and correct.
        role = badminip && (!bpasswordrequired || bpasswordsupplied) ? role::admin : role::guest;
    }

    return role;
}

//------------------------------------------------------------------------------

void
serverhandler::appendstandardfields (beast::http::message& message)
{
}

//------------------------------------------------------------------------------

void
serverhandler::setup::makecontexts()
{
    for(auto& p : ports)
    {
        if (p.secure())
        {
            if (p.ssl_key.empty() && p.ssl_cert.empty() &&
                    p.ssl_chain.empty())
                p.context = make_sslcontext();
            else
                p.context = make_sslcontextauthed (
                    p.ssl_key, p.ssl_cert, p.ssl_chain);
        }
        else
        {
            p.context = std::make_shared<
                boost::asio::ssl::context>(
                    boost::asio::ssl::context::sslv23);
        }
    }
}

namespace detail {

// intermediate structure used for parsing
struct parsedport
{
    std::string name;
    std::set<std::string, beast::ci_less> protocol;
    std::string user;
    std::string password;
    std::string admin_user;
    std::string admin_password;
    std::string ssl_key;
    std::string ssl_cert;
    std::string ssl_chain;

    boost::optional<boost::asio::ip::address> ip;
    boost::optional<std::uint16_t> port;
    boost::optional<bool> allow_admin;
};

void
parse_port (parsedport& port, section const& section, std::ostream& log)
{
    {
        auto result = section.find("ip");
        if (result.second)
        {
            try
            {
                port.ip = boost::asio::ip::address::from_string(result.first);
            }
            catch(...)
            {
                log << "invalid value '" << result.first <<
                    "' for key 'ip' in [" << section.name() << "]\n";
                throw std::exception();
            }
        }
    }

    {
        auto const result = section.find("port");
        if (result.second)
        {
            auto const ul = std::stoul(result.first);
            if (ul > std::numeric_limits<std::uint16_t>::max())
            {
                log <<
                    "value '" << result.first << "' for key 'port' is out of range\n";
                throw std::exception();
            }
            if (ul == 0)
            {
                log <<
                    "value '0' for key 'port' is invalid\n";
                throw std::exception();
            }
            port.port = static_cast<std::uint16_t>(ul);
        }
    }

    {
        auto const result = section.find("protocol");
        if (result.second)
        {
            for (auto const& s : beast::rfc2616::split_commas(
                    result.first.begin(), result.first.end()))
                port.protocol.insert(s);
        }
    }

    {
        auto const result = section.find("admin");
        if (result.second)
        {
            if (result.first == "no")
            {
                port.allow_admin = false;
            }
            else if (result.first == "allow")
            {
                port.allow_admin = true;
            }
            else
            {
                log << "invalid value '" << result.first <<
                    "' for key 'admin' in [" << section.name() << "]\n";
                throw std::exception();
            }
        }
    }

    set(port.user, "user", section);
    set(port.password, "password", section);
    set(port.admin_user, "admin_user", section);
    set(port.admin_password, "admin_password", section);
    set(port.ssl_key, "ssl_key", section);
    set(port.ssl_cert, "ssl_cert", section);
    set(port.ssl_chain, "ssl_chain", section);
}

http::port
to_port(parsedport const& parsed, std::ostream& log)
{
    http::port p;
    p.name = parsed.name;

    if (! parsed.ip)
    {
        log << "missing 'ip' in [" << p.name << "]\n";
        throw std::exception();
    }
    p.ip = *parsed.ip;

    if (! parsed.port)
    {
        log << "missing 'port' in [" << p.name << "]\n";
        throw std::exception();
    }
    else if (*parsed.port == 0)
    {
        log << "port " << *parsed.port << "in [" << p.name << "] is invalid\n";
        throw std::exception();
    }
    p.port = *parsed.port;

    if (! parsed.allow_admin)
        p.allow_admin = false;
    else
        p.allow_admin = *parsed.allow_admin;

    if (parsed.protocol.empty())
    {
        log << "missing 'protocol' in [" << p.name << "]\n";
        throw std::exception();
    }
    p.protocol = parsed.protocol;
    if (p.websockets() &&
        (parsed.protocol.count("peer") > 0 ||
        parsed.protocol.count("http") > 0 ||
        parsed.protocol.count("https") > 0))
    {
        log << "invalid protocol combination in [" << p.name << "]\n";
        throw std::exception();
    }

    p.user = parsed.user;
    p.password = parsed.password;
    p.admin_user = parsed.admin_user;
    p.admin_password = parsed.admin_password;
    p.ssl_key = parsed.ssl_key;
    p.ssl_cert = parsed.ssl_cert;
    p.ssl_chain = parsed.ssl_chain;

    return p;
}

std::vector<http::port>
parse_ports (basicconfig const& config, std::ostream& log)
{
    std::vector<http::port> result;

    if (! config.exists("server"))
    {
        log <<
            "required section [server] is missing\n";
        throw std::exception();
    }

    parsedport common;
    parse_port (common, config["server"], log);

    auto const& names = config.section("server").values();
    result.reserve(names.size());
    for (auto const& name : names)
    {
        if (! config.exists(name))
        {
            log <<
                "missing section: [" << name << "]\n";
            throw std::exception();
        }
        parsedport parsed = common;
        parsed.name = name;
        parse_port(parsed, config[name], log);
        result.push_back(to_port(parsed, log));
    }

    std::size_t count = 0;
    for (auto const& p : result)
        if (p.protocol.count("peer") > 0)
            ++count;
    if (count > 1)
    {
        log << "error: more than one peer protocol configured in [server]\n";
        throw std::exception();
    }
    if (count == 0)
        log << "warning: no peer protocol configured\n";

    return result;
}

// fill out the client portion of the setup
void
setup_client (serverhandler::setup& setup)
{
    decltype(setup.ports)::const_iterator iter;
    for (iter = setup.ports.cbegin();
            iter != setup.ports.cend(); ++iter)
        if (iter->protocol.count("http") > 0 ||
                iter->protocol.count("https") > 0)
            break;
    if (iter == setup.ports.cend())
        return;
    setup.client.secure =
        iter->protocol.count("https") > 0;
    setup.client.ip = iter->ip.to_string();
    // vfalco hack! to make localhost work
    if (setup.client.ip == "0.0.0.0")
        setup.client.ip = "127.0.0.1";
    setup.client.port = iter->port;
    setup.client.user = iter->user;
    setup.client.password = iter->password;
    setup.client.admin_user = iter->admin_user;
    setup.client.admin_password = iter->admin_password;
}

// fill out the overlay portion of the setup
void
setup_overlay (serverhandler::setup& setup)
{
    auto const iter = std::find_if(setup.ports.cbegin(), setup.ports.cend(),
        [](http::port const& port)
        {
            return port.protocol.count("peer") > 0;
        });
    if (iter == setup.ports.cend())
    {
        setup.overlay.port = 0;
        return;
    }
    setup.overlay.ip = iter->ip;
    setup.overlay.port = iter->port;
}

}

serverhandler::setup
setup_serverhandler (basicconfig const& config, std::ostream& log)
{
    serverhandler::setup setup;
    setup.ports = detail::parse_ports (config, log);
    setup.yieldstrategy = rpc::makeyieldstrategy (config["server"]);
    detail::setup_client(setup);
    detail::setup_overlay(setup);

    return setup;
}

std::unique_ptr <serverhandler>
make_serverhandler (beast::stoppable& parent,
    boost::asio::io_service& io_service, jobqueue& jobqueue,
        networkops& networkops, resource::manager& resourcemanager)
{
    return std::make_unique <serverhandlerimp> (parent, io_service,
        jobqueue, networkops, resourcemanager);
}

}
