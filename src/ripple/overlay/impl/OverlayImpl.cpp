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
#include <ripple/basics/log.h>
#include <ripple/basics/make_sslcontext.h>
#include <ripple/server/jsonwriter.h>
#include <ripple/overlay/impl/connectattempt.h>
#include <ripple/overlay/impl/overlayimpl.h>
#include <ripple/overlay/impl/peerimp.h>
#include <ripple/overlay/impl/tmhello.h>
#include <ripple/peerfinder/make_manager.h>
#include <beast/byteorder.h>
#include <beast/crypto/base64.h>
#include <beast/http/rfc2616.h>
#include <beast/utility/ci_char_traits.h>
#include <beast/utility/wrappedsink.h>

namespace ripple {

/** a functor to visit all active peers and retrieve their json data */
struct get_peer_json
{
    typedef json::value return_type;

    json::value json;

    get_peer_json ()
    { }

    void operator() (peer::ptr const& peer)
    {
        json.append (peer->json ());
    }

    json::value operator() ()
    {
        return json;
    }
};

//------------------------------------------------------------------------------

overlayimpl::child::child (overlayimpl& overlay)
    : overlay_(overlay)
{
}

overlayimpl::child::~child()
{
    overlay_.remove(*this);
}

//------------------------------------------------------------------------------

overlayimpl::timer::timer (overlayimpl& overlay)
    : child(overlay)
    , timer_(overlay_.io_service_)
{
}

void
overlayimpl::timer::stop()
{
    error_code ec;
    timer_.cancel(ec);
}

void
overlayimpl::timer::run()
{
    error_code ec;
    timer_.expires_from_now (std::chrono::seconds(1));
    timer_.async_wait(overlay_.strand_.wrap(
        std::bind(&timer::on_timer, shared_from_this(),
            beast::asio::placeholders::error)));
}

void
overlayimpl::timer::on_timer (error_code ec)
{
    if (ec || overlay_.isstopping())
    {
        if (ec && ec != boost::asio::error::operation_aborted)
            if (overlay_.journal_.error) overlay_.journal_.error <<
                "on_timer: " << ec.message();
        return;
    }

    overlay_.m_peerfinder->once_per_second();
    overlay_.sendendpoints();
    overlay_.autoconnect();

    timer_.expires_from_now (std::chrono::seconds(1));
    timer_.async_wait(overlay_.strand_.wrap(std::bind(
        &timer::on_timer, shared_from_this(),
            beast::asio::placeholders::error)));
}

//------------------------------------------------------------------------------

overlayimpl::overlayimpl (
    setup const& setup,
    stoppable& parent,
    serverhandler& serverhandler,
    resource::manager& resourcemanager,
    beast::file const& pathtodbfileordirectory,
    resolver& resolver,
    boost::asio::io_service& io_service)
    : overlay (parent)
    , io_service_ (io_service)
    , work_ (boost::in_place(std::ref(io_service_)))
    , strand_ (io_service_)
    , setup_(setup)
    , journal_ (deprecatedlogs().journal("overlay"))
    , serverhandler_(serverhandler)
    , m_resourcemanager (resourcemanager)
    , m_peerfinder (peerfinder::make_manager (*this, io_service,
        pathtodbfileordirectory, get_seconds_clock(),
            deprecatedlogs().journal("peerfinder")))
    , m_resolver (resolver)
    , next_id_(1)
{
    beast::propertystream::source::add (m_peerfinder.get());
}

overlayimpl::~overlayimpl ()
{
    stop();

    // block until dependent objects have been destroyed.
    // this is just to catch improper use of the stoppable api.
    //
    std::unique_lock <decltype(mutex_)> lock (mutex_);
    cond_.wait (lock, [this] { return list_.empty(); });
}

//------------------------------------------------------------------------------

void
overlayimpl::onlegacypeerhello (
    std::unique_ptr<beast::asio::ssl_bundle>&& ssl_bundle,
        boost::asio::const_buffer buffer, endpoint_type remote_endpoint)
{
    error_code ec;
    auto const local_endpoint (ssl_bundle->socket.local_endpoint(ec));
    if (ec)
        return;

    auto const slot = m_peerfinder->new_inbound_slot (
        beast::ipaddressconversion::from_asio(local_endpoint),
            beast::ipaddressconversion::from_asio(remote_endpoint));

    if (slot == nullptr)
        // self connect, close
        return;

    auto const peer = std::make_shared<peerimp>(next_id_++,
        remote_endpoint, slot, boost::asio::const_buffers_1(buffer),
            std::move(ssl_bundle),  *this);
    {
        // as we are not on the strand, run() must be called
        // while holding the lock, otherwise new i/o can be
        // queued after a call to stop().
        std::lock_guard <decltype(mutex_)> lock (mutex_);
        add(peer);    
        peer->run();
    }
}

handoff
overlayimpl::onhandoff (std::unique_ptr <beast::asio::ssl_bundle>&& ssl_bundle,
    beast::http::message&& request,
        endpoint_type remote_endpoint)
{
    auto const id = next_id_++;
    beast::wrappedsink sink (deprecatedlogs()["peer"], makeprefix(id));
    beast::journal journal (sink);

    handoff handoff;
    if (processrequest(request, handoff))
        return handoff;
    if (! ispeerupgrade(request))
        return handoff;

    handoff.moved = true;

    if (journal.trace) journal.trace <<
        "peer connection upgrade from " << remote_endpoint;

    error_code ec;
    auto const local_endpoint (ssl_bundle->socket.local_endpoint(ec));
    if (ec)
    {
        if (journal.trace) journal.trace <<
            remote_endpoint << " failed: " << ec.message();
        return handoff;
    }

    auto consumer = m_resourcemanager.newinboundendpoint(
        beast::ipaddressconversion::from_asio(remote_endpoint));
    if (consumer.disconnect())
        return handoff;

    auto const slot = m_peerfinder->new_inbound_slot (
        beast::ipaddressconversion::from_asio(local_endpoint),
            beast::ipaddressconversion::from_asio(remote_endpoint));

    if (slot == nullptr)
    {
        // self-connect, close
        handoff.moved = false;
        return handoff;
    }

    // todo validate http request

    {
        auto const types = beast::rfc2616::split_commas(
            request.headers["connect-as"]);
        if (std::find_if(types.begin(), types.end(),
                [](std::string const& s)
                {
                    return beast::ci_equal(s,
                        std::string("peer"));
                }) == types.end())
        {
            handoff.moved = false;
            handoff.response = makeredirectresponse(slot, request,
                remote_endpoint.address());
            handoff.keep_alive = request.keep_alive();
            return handoff;
        }
    }

    handoff.moved = true;
    bool success = true;

    protocol::tmhello hello;
    std::tie(hello, success) = parsehello (request, journal);
    if(! success)
        return handoff;

    uint256 sharedvalue;
    std::tie(sharedvalue, success) = makesharedvalue(
        ssl_bundle->stream.native_handle(), journal);
    if(! success)
        return handoff;

    rippleaddress publickey;
    std::tie(publickey, success) = verifyhello (hello,
        sharedvalue, journal, getapp());
    if(! success)
        return handoff;

    std::string name;
    bool const cluster = getapp().getunl().nodeincluster(
        publickey, name);
    
    auto const result = m_peerfinder->activate (slot,
        publickey.topublickey(), cluster);
    if (result != peerfinder::result::success)
    {
        if (journal.trace) journal.trace <<
            "peer " << remote_endpoint << " redirected, slots full";
        handoff.moved = false;
        handoff.response = makeredirectresponse(slot, request,
            remote_endpoint.address());
        handoff.keep_alive = request.keep_alive();
        return handoff;
    }

    auto const peer = std::make_shared<peerimp>(id,
        remote_endpoint, slot, std::move(request), hello, publickey,
            consumer, std::move(ssl_bundle), *this);
    {
        // as we are not on the strand, run() must be called
        // while holding the lock, otherwise new i/o can be
        // queued after a call to stop().
        std::lock_guard <decltype(mutex_)> lock (mutex_);
        add(peer);
        peer->run();
    }
    handoff.moved = true;
    return handoff;
}

//------------------------------------------------------------------------------

bool
overlayimpl::ispeerupgrade(beast::http::message const& request)
{
    if (! request.upgrade())
        return false;
    auto const versions = parse_protocolversions(
        request.headers["upgrade"]);
    if (versions.size() == 0)
        return false;
    if (! request.request() && request.status() != 101)
        return false;
    return true;
}

std::string
overlayimpl::makeprefix (std::uint32_t id)
{
    std::stringstream ss;
    ss << "[" << std::setfill('0') << std::setw(3) << id << "] ";
    return ss.str();
}

std::shared_ptr<http::writer>
overlayimpl::makeredirectresponse (peerfinder::slot::ptr const& slot,
    beast::http::message const& request, address_type remote_address)
{
    json::value json(json::objectvalue);
    {
        auto const result = m_peerfinder->redirect(slot);
        json::value& ips = (json["peer-ips"] = json::arrayvalue);
        for (auto const& _ : m_peerfinder->redirect(slot))
            ips.append(_.address.to_string());
    }

    beast::http::message m;
    m.request(false);
    m.status(503);
    m.reason("service unavailable");
    m.headers.append("remote-address", remote_address.to_string());
    m.version(request.version());
    if (request.version() == std::make_pair(1, 0))
    {
        //?
    }
    auto const response = http::make_jsonwriter (m, json);
    return response;
}

//------------------------------------------------------------------------------

void
overlayimpl::connect (beast::ip::endpoint const& remote_endpoint)
{
    assert(work_);

    auto usage = resourcemanager().newoutboundendpoint (remote_endpoint);
    if (usage.disconnect())
    {
        if (journal_.info) journal_.info <<
            "over resource limit: " << remote_endpoint;
        return;
    }
    
    auto const slot = peerfinder().new_outbound_slot(remote_endpoint);
    if (slot == nullptr)
    {
        if (journal_.debug) journal_.debug <<
            "connect: no slot for " << remote_endpoint;
        return;
    }

    auto const p = std::make_shared<connectattempt>(
        io_service_, beast::ipaddressconversion::to_asio_endpoint(remote_endpoint),
            usage, setup_.context, next_id_++, slot,
                deprecatedlogs().journal("peer"), *this);

    std::lock_guard<decltype(mutex_)> lock(mutex_);
    list_.emplace(p.get(), p);
    p->run();
}

//------------------------------------------------------------------------------

// adds a peer that is already handshaked and active
void
overlayimpl::add_active (std::shared_ptr<peerimp> const& peer)
{
    std::lock_guard <decltype(mutex_)> lock (mutex_);

    {
        auto const result =
            m_peers.emplace (peer->slot(), peer);
        assert (result.second);
        (void) result.second;
    }

    // now track this peer
    {
        auto const result (m_shortidmap.emplace (
            std::piecewise_construct,
                std::make_tuple (peer->id()),
                    std::make_tuple (peer)));
        assert(result.second);
        (void) result.second;
    }

    {
        auto const result (m_publickeymap.emplace(
            peer->getnodepublic(), peer));
        assert(result.second);
        (void) result.second;
    }

    list_.emplace(peer.get(), peer);

    journal_.debug <<
        "activated " << peer->getremoteaddress() <<
        " (" << peer->id() <<
        ":" << peer->getnodepublic().topublickey() << ")";

    // as we are not on the strand, run() must be called
    // while holding the lock, otherwise new i/o can be
    // queued after a call to stop().
    peer->run();
}

void
overlayimpl::remove (peerfinder::slot::ptr const& slot)
{
    std::lock_guard <decltype(mutex_)> lock (mutex_);
    auto const iter = m_peers.find (slot);
    assert(iter != m_peers.end ());
    m_peers.erase (iter);
}

//------------------------------------------------------------------------------
//
// stoppable
//
//------------------------------------------------------------------------------

// caller must hold the mutex
void
overlayimpl::checkstopped ()
{
    if (isstopping() && arechildrenstopped () && list_.empty())
        stopped();
}

void
overlayimpl::onprepare()
{
    peerfinder::config config;

    if (getconfig ().peers_max != 0)
        config.maxpeers = getconfig ().peers_max;

    config.outpeers = config.calcoutpeers();

    auto const port = serverhandler_.setup().overlay.port;

    config.peerprivate = getconfig().peer_private;
    config.wantincoming =
        (! config.peerprivate) && (port != 0);
    // if it's a private peer or we are running as standalone
    // automatic connections would defeat the purpose.
    config.autoconnect =
        !getconfig().run_standalone &&
        !getconfig().peer_private;
    config.listeningport = port;
    config.features = "";

    // enforce business rules
    config.applytuning();

    m_peerfinder->setconfig (config);

    auto bootstrapips (getconfig ().ips);

    // if no ips are specified, use the ripple labs round robin
    // pool to get some servers to insert into the boot cache.
    //if (bootstrapips.empty ())
    //    bootstrapips.push_back ("r.ripple.com 51235");

    if (!bootstrapips.empty ())
    {
        m_resolver.resolve (bootstrapips,
            [this](std::string const& name,
                std::vector <beast::ip::endpoint> const& addresses)
            {
                std::vector <std::string> ips;
                ips.reserve(addresses.size());
                for (auto const& addr : addresses)
                    ips.push_back (to_string (addr));
                std::string const base ("config: ");
                if (!ips.empty ())
                    m_peerfinder->addfallbackstrings (base + name, ips);
            });
    }

    // add the ips_fixed from the rippled.cfg file
    if (! getconfig ().run_standalone && !getconfig ().ips_fixed.empty ())
    {
        m_resolver.resolve (getconfig ().ips_fixed,
            [this](
                std::string const& name,
                std::vector <beast::ip::endpoint> const& addresses)
            {
                if (!addresses.empty ())
                    m_peerfinder->addfixedpeer (name, addresses);
            });
    }
}

void
overlayimpl::onstart ()
{
    auto const timer = std::make_shared<timer>(*this);
    std::lock_guard <decltype(mutex_)> lock (mutex_);
    list_.emplace(timer.get(), timer);
    timer_ = timer;
    timer->run();
}

void
overlayimpl::onstop ()
{
    strand_.dispatch(std::bind(&overlayimpl::stop, this));
}

void
overlayimpl::onchildrenstopped ()
{
    std::lock_guard <decltype(mutex_)> lock (mutex_);
    checkstopped ();
}

//------------------------------------------------------------------------------
//
// propertystream
//
//------------------------------------------------------------------------------

void
overlayimpl::onwrite (beast::propertystream::map& stream)
{
}

//------------------------------------------------------------------------------
/** a peer has connected successfully
    this is called after the peer handshake has been completed and during
    peer activation. at this point, the peer address and the public key
    are known.
*/
void
overlayimpl::activate (std::shared_ptr<peerimp> const& peer)
{
    std::lock_guard <decltype(mutex_)> lock (mutex_);

    // now track this peer
    {
        auto const result (m_shortidmap.emplace (
            std::piecewise_construct,
                std::make_tuple (peer->id()),
                    std::make_tuple (peer)));
        assert(result.second);
        (void) result.second;
    }

    {
        auto const result (m_publickeymap.emplace(
            peer->getnodepublic(), peer));
        assert(result.second);
        (void) result.second;
    }

    journal_.debug <<
        "activated " << peer->getremoteaddress() <<
        " (" << peer->id() <<
        ":" << peer->getnodepublic().topublickey() << ")";

    // we just accepted this peer so we have non-zero active peers
    assert(size() != 0);
}

void
overlayimpl::onpeerdeactivate (peer::id_t id,
    rippleaddress const& publickey)
{
    std::lock_guard <decltype(mutex_)> lock (mutex_);
    m_shortidmap.erase(id);
    m_publickeymap.erase(publickey);
}

/** the number of active peers on the network
    active peers are only those peers that have completed the handshake
    and are running the ripple protocol.
*/
std::size_t
overlayimpl::size()
{
    std::lock_guard <decltype(mutex_)> lock (mutex_);
    return m_publickeymap.size ();
}

json::value
overlayimpl::crawl()
{
    json::value jv;
    auto& av = jv["active"] = json::value(json::arrayvalue);
    std::lock_guard <decltype(mutex_)> lock (mutex_);
    for (auto const& e : m_publickeymap)
    {
        auto const sp = e.second.lock();
        if (sp)
        {
            auto& pv = av.append(json::value(json::objectvalue));
            pv["type"] = "peer";
            pv["public_key"] = beast::base64_encode(
                sp->getnodepublic().getnodepublic().data(),
                    sp->getnodepublic().getnodepublic().size());
            if (sp->crawl())
            {
                if (sp->slot()->inbound())
                    pv["ip"] = sp->getremoteaddress().address().to_string();
                else
                    pv["ip"] = sp->getremoteaddress().to_string();
            }
        }
    }
    return jv;
}

// returns information on verified peers.
json::value
overlayimpl::json ()
{
    return foreach (get_peer_json());
}

bool
overlayimpl::processrequest (beast::http::message const& req,
    handoff& handoff)
{
    if (req.url() != "/crawl")
        return false;

    beast::http::message resp;
    resp.request(false);
    resp.status(200);
    resp.reason("ok");
    json::value v;
    v["overlay"] = crawl();
    handoff.response = http::make_jsonwriter(resp, v);
    return true;
}

overlay::peersequence
overlayimpl::getactivepeers()
{
    overlay::peersequence ret;
    std::lock_guard <decltype(mutex_)> lock (mutex_);
    ret.reserve (m_publickeymap.size ());
    for (auto const& e : m_publickeymap)
    {
        auto const sp = e.second.lock();
        if (sp)
            ret.push_back(sp);
    }

    return ret;
}

peer::ptr
overlayimpl::findpeerbyshortid (peer::id_t const& id)
{
    std::lock_guard <decltype(mutex_)> lock (mutex_);
    auto const iter = m_shortidmap.find (id);
    if (iter != m_shortidmap.end ())
        return iter->second.lock();
    return peer::ptr();
}

//------------------------------------------------------------------------------

void
overlayimpl::add (std::shared_ptr<peerimp> const& peer)
{
    {
        auto const result =
            m_peers.emplace (peer->slot(), peer);
        assert (result.second);
        (void) result.second;
    }
    list_.emplace(peer.get(), peer);
}

void
overlayimpl::remove (child& child)
{
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    list_.erase(&child);
    if (list_.empty())
        checkstopped();
}

void
overlayimpl::stop()
{
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    if (work_)
    {
        work_ = boost::none;
        for (auto& _ : list_)
        {
            auto const child = _.second.lock();
            // happens when the child is about to be destroyed
            if (child != nullptr)
                child->stop();
        }
    }
}

void
overlayimpl::autoconnect()
{
    auto const result = m_peerfinder->autoconnect();
    for (auto addr : result)
        connect (addr);
}

void
overlayimpl::sendendpoints()
{
    auto const result = m_peerfinder->buildendpointsforpeers();
    for (auto const& e : result)
    {
        std::shared_ptr<peerimp> peer;
        {
            std::lock_guard <decltype(mutex_)> lock (mutex_);
            auto const iter = m_peers.find (e.first);
            if (iter != m_peers.end())
                peer = iter->second.lock();
        }
        if (peer)
            peer->sendendpoints (e.second.begin(), e.second.end());
    }
}


//------------------------------------------------------------------------------

overlay::setup
setup_overlay (basicconfig const& config)
{
    overlay::setup setup;
    auto const& section = config.section("overlay");
    set (setup.http_handshake, "http_handshake", section);
    set (setup.auto_connect, "auto_connect", section);
    std::string promote;
    set (promote, "become_superpeer", section);
    if (promote == "never")
        setup.promote = overlay::promote::never;
    else if (promote == "always")
        setup.promote = overlay::promote::always;
    else
        setup.promote = overlay::promote::automatic;
    setup.context = make_sslcontext();
    return setup;
}

std::unique_ptr <overlay>
make_overlay (
    overlay::setup const& setup,
    beast::stoppable& parent,
    serverhandler& serverhandler,
    resource::manager& resourcemanager,
    beast::file const& pathtodbfileordirectory,
    resolver& resolver,
    boost::asio::io_service& io_service)
{
    return std::make_unique <overlayimpl> (setup, parent, serverhandler,
        resourcemanager, pathtodbfileordirectory, resolver, io_service);
}

}
