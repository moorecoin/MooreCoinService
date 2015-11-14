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

#ifndef ripple_overlay_overlayimpl_h_included
#define ripple_overlay_overlayimpl_h_included

#include <ripple/overlay/overlay.h>
#include <ripple/server/handoff.h>
#include <ripple/server/serverhandler.h>
#include <ripple/basics/resolver.h>
#include <ripple/basics/seconds_clock.h>
#include <ripple/basics/unorderedcontainers.h>
#include <ripple/peerfinder/manager.h>
#include <ripple/resource/manager.h>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/container/flat_map.hpp>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <beast/cxx14/memory.h> // <memory>
#include <mutex>
#include <unordered_map>

namespace ripple {

class peerimp;

class overlayimpl : public overlay
{
public:
    class child
    {
    protected:
        overlayimpl& overlay_;

        explicit
        child (overlayimpl& overlay);

        virtual ~child();

    public:
        virtual void stop() = 0;
    };

private:
    using clock_type = std::chrono::steady_clock;
    using socket_type = boost::asio::ip::tcp::socket;
    using address_type = boost::asio::ip::address;
    using endpoint_type = boost::asio::ip::tcp::endpoint;
    using error_code = boost::system::error_code;

    struct timer
        : child
        , std::enable_shared_from_this<timer>
    {
        boost::asio::basic_waitable_timer <clock_type> timer_;

        explicit
        timer (overlayimpl& overlay);

        void
        stop() override;

        void
        run();

        void
        on_timer (error_code ec);
    };

    boost::asio::io_service& io_service_;
    boost::optional<boost::asio::io_service::work> work_;
    boost::asio::io_service::strand strand_;

    std::recursive_mutex mutex_; // vfalco use std::mutex
    std::condition_variable_any cond_;
    std::weak_ptr<timer> timer_;
    boost::container::flat_map<
        child*, std::weak_ptr<child>> list_;

    setup setup_;
    beast::journal journal_;
    serverhandler& serverhandler_;

    resource::manager& m_resourcemanager;

    std::unique_ptr <peerfinder::manager> m_peerfinder;

    hash_map <peerfinder::slot::ptr,
        std::weak_ptr <peerimp>> m_peers;

    hash_map<rippleaddress, std::weak_ptr<peerimp>> m_publickeymap;

    hash_map<peer::id_t, std::weak_ptr<peerimp>> m_shortidmap;

    resolver& m_resolver;

    std::atomic <peer::id_t> next_id_;

    //--------------------------------------------------------------------------

public:
    overlayimpl (setup const& setup, stoppable& parent,
        serverhandler& serverhandler, resource::manager& resourcemanager,
            beast::file const& pathtodbfileordirectory,
                resolver& resolver, boost::asio::io_service& io_service);

    ~overlayimpl();

    overlayimpl (overlayimpl const&) = delete;
    overlayimpl& operator= (overlayimpl const&) = delete;

    peerfinder::manager&
    peerfinder()
    {
        return *m_peerfinder;
    }

    resource::manager&
    resourcemanager()
    {
        return m_resourcemanager;
    }

    serverhandler&
    serverhandler()
    {
        return serverhandler_;
    }

    setup const&
    setup() const
    {
        return setup_;
    }

    void
    onlegacypeerhello (std::unique_ptr<beast::asio::ssl_bundle>&& ssl_bundle,
        boost::asio::const_buffer buffer,
            endpoint_type remote_endpoint) override;

    handoff
    onhandoff (std::unique_ptr <beast::asio::ssl_bundle>&& bundle,
        beast::http::message&& request,
            endpoint_type remote_endpoint) override;

    peersequence
    getactivepeers() override;

    peer::ptr
    findpeerbyshortid (peer::id_t const& id) override;

    void
    add_active (std::shared_ptr<peerimp> const& peer);

    void
    remove (peerfinder::slot::ptr const& slot);

    /** called when a peer has connected successfully
        this is called after the peer handshake has been completed and during
        peer activation. at this point, the peer address and the public key
        are known.
    */
    void
    activate (std::shared_ptr<peerimp> const& peer);

    /** called when an active peer is destroyed. */
    void
    onpeerdeactivate (peer::id_t id, rippleaddress const& publickey);

    static
    bool
    ispeerupgrade (beast::http::message const& request);

    static
    std::string
    makeprefix (std::uint32_t id);

private:
    std::shared_ptr<http::writer>
    makeredirectresponse (peerfinder::slot::ptr const& slot,
        beast::http::message const& request, address_type remote_address);

    void
    connect (beast::ip::endpoint const& remote_endpoint) override;

    std::size_t
    size() override;

    json::value
    crawl() override;

    json::value
    json() override;

    bool
    processrequest (beast::http::message const& req,
        handoff& handoff);

    //--------------------------------------------------------------------------

    //
    // stoppable
    //

    void
    checkstopped();

    void
    onprepare() override;

    void
    onstart() override;

    void
    onstop() override;

    void
    onchildrenstopped() override;

    //
    // propertystream
    //

    void
    onwrite (beast::propertystream::map& stream);

    //--------------------------------------------------------------------------

    void
    add (std::shared_ptr<peerimp> const& peer);

    void
    remove (child& child);

    void
    stop();

    void
    autoconnect();

    void
    sendendpoints();
};

} // ripple

#endif
