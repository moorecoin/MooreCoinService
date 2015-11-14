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

#ifndef ripple_overlay_peerimp_h_included
#define ripple_overlay_peerimp_h_included

#include <ripple/app/ledger/ledgerproposal.h>
#include <ripple/basics/log.h> // deprecated
#include <ripple/nodestore/database.h>
#include <ripple/overlay/predicates.h>
#include <ripple/overlay/impl/protocolmessage.h>
#include <ripple/overlay/impl/overlayimpl.h>
#include <ripple/core/config.h>
#include <ripple/core/job.h>
#include <ripple/core/loadfeetrack.h>
#include <ripple/core/loadevent.h>
#include <ripple/protocol/protocol.h>
#include <ripple/protocol/sttx.h>
#include <ripple/validators/manager.h>
#include <beast/byteorder.h>
#include <beast/asio/ipaddressconversion.h>
#include <beast/asio/placeholders.h>
#include <beast/asio/streambuf.h>
#include <beast/asio/ssl_bundle.h>
#include <beast/http/message.h>
#include <beast/http/parser.h>
#include <beast/utility/wrappedsink.h>
#include <cstdint>
#include <queue>

namespace ripple {

class peerimp
    : public peer
    , public std::enable_shared_from_this <peerimp>
    , public overlayimpl::child
{
public:
    /** type of connection.
        the affects how messages are routed.
    */
    enum class type
    {
        legacy,
        leaf,
        peer
    };

    /** current state */
    enum class state
    {
        /** a connection is being established (outbound) */
        connecting

        /** connection has been successfully established */
        ,connected

        /** handshake has been received from this peer */
        ,handshaked

        /** running the ripple protocol actively */
        ,active
    };

    typedef std::shared_ptr <peerimp> ptr;

private:
    using clock_type = std::chrono::steady_clock;
    using error_code= boost::system::error_code ;
    using socket_type = boost::asio::ip::tcp::socket;
    using stream_type = boost::asio::ssl::stream <socket_type&>;
    using address_type = boost::asio::ip::address;
    using endpoint_type = boost::asio::ip::tcp::endpoint;

    // the length of the smallest valid finished message
    static const size_t sslminimumfinishedlength = 12;

    id_t const id_;
    beast::wrappedsink sink_;
    beast::wrappedsink p_sink_;
    beast::journal journal_;
    beast::journal p_journal_;
    std::unique_ptr<beast::asio::ssl_bundle> ssl_bundle_;
    socket_type& socket_;
    stream_type& stream_;
    boost::asio::io_service::strand strand_;
    boost::asio::basic_waitable_timer<
        std::chrono::steady_clock> timer_;

    //type type_ = type::legacy;

    // updated at each stage of the connection process to reflect
    // the current conditions as closely as possible.
    beast::ip::endpoint remote_address_;

    // these are up here to prevent warnings about order of initializations
    //
    overlayimpl& overlay_;
    bool m_inbound;
    state state_;          // current state
    bool detaching_ = false;
    // node public key of peer.
    rippleaddress publickey_;
    std::string name_;
    uint256 sharedvalue_;

    // the indices of the smallest and largest ledgers this peer has available
    //
    ledgerindex minledger_ = 0;
    ledgerindex maxledger_ = 0;
    uint256 closedledgerhash_;
    uint256 previousledgerhash_;
    std::list<uint256> recentledgers_;
    std::list<uint256> recenttxsets_;
    mutable std::mutex recentlock_;
    protocol::tmstatuschange last_status_;
    protocol::tmhello hello_;
    resource::consumer usage_;
    peerfinder::slot::ptr slot_;
    beast::asio::streambuf read_buffer_;
    beast::http::message http_message_;
    beast::http::body http_body_;
    beast::asio::streambuf write_buffer_;
    std::queue<message::pointer> send_queue_;
    bool gracefulclose_ = false;
    std::unique_ptr <loadevent> load_event_;
    std::unique_ptr<validators::connection> validatorsconnection_;

    //--------------------------------------------------------------------------

public:
    peerimp (peerimp const&) = delete;
    peerimp& operator= (peerimp const&) = delete;

    /** create an active incoming peer from an established ssl connection. */
    peerimp (id_t id, endpoint_type remote_endpoint,
        peerfinder::slot::ptr const& slot, beast::http::message&& request,
            protocol::tmhello const& hello, rippleaddress const& publickey,
                resource::consumer consumer,
                    std::unique_ptr<beast::asio::ssl_bundle>&& ssl_bundle,
                        overlayimpl& overlay);

    /** create an incoming legacy peer from an established ssl connection. */
    template <class constbuffersequence>
    peerimp (id_t id, endpoint_type remote_endpoint,
        peerfinder::slot::ptr const& slot, constbuffersequence const& buffer,
            std::unique_ptr<beast::asio::ssl_bundle>&& ssl_bundle,
                overlayimpl& overlay);

    /** create outgoing, handshaked peer. */
    // vfalco legacypublickey should be implied by the slot
    template <class buffers>
    peerimp (std::unique_ptr<beast::asio::ssl_bundle>&& ssl_bundle,
        buffers const& buffers, peerfinder::slot::ptr&& slot,
            resource::consumer usage, protocol::tmhello&& hello,
                rippleaddress const& legacypublickey, id_t id,
                    overlayimpl& overlay);

    virtual
    ~peerimp();

    peerfinder::slot::ptr const&
    slot()
    {
        return slot_;
    }

    // work-around for calling shared_from_this in constructors
    void
    run();

    // called when overlay gets a stop request.
    void
    stop() override;

    //
    // network
    //

    void
    send (message::pointer const& m) override;

    /** send a set of peerfinder endpoints as a protocol message. */
    template <class fwdit, class = typename std::enable_if_t<std::is_same<
        typename std::iterator_traits<fwdit>::value_type,
            peerfinder::endpoint>::value>>
    void
    sendendpoints (fwdit first, fwdit last);

    beast::ip::endpoint
    getremoteaddress() const override
    {
        return remote_address_;
    }

    void
    charge (resource::charge const& fee) override;

    //
    // identity
    //

    peer::id_t
    id() const override
    {
        return id_;
    }

    bool
    crawl() const;

    bool
    cluster() const override
    {
        return slot_->cluster();
    }

    rippleaddress const&
    getnodepublic () const override
    {
        return publickey_;
    }

    json::value
    json() override;

    //
    // ledger
    //

    uint256 const&
    getclosedledgerhash () const override
    {
        return closedledgerhash_;
    }

    bool
    hasledger (uint256 const& hash, std::uint32_t seq) const override;

    void
    ledgerrange (std::uint32_t& minseq, std::uint32_t& maxseq) const override;

    bool
    hastxset (uint256 const& hash) const override;

    void
    cyclestatus () override;

    bool
    supportsversion (int version) override;

    bool
    hasrange (std::uint32_t umin, std::uint32_t umax) override;

private:
    void
    close();

    void
    fail(std::string const& reason);

    void
    fail(std::string const& name, error_code ec);

    void
    gracefulclose();

    void
    settimer();

    void
    canceltimer();

    static
    std::string
    makeprefix(id_t id);

    // called when the timer wait completes
    void
    ontimer (boost::system::error_code const& ec);

    // called when ssl shutdown completes
    void
    onshutdown (error_code ec);

    void
    doaccept();

    void
    dolegacyaccept();

    static
    beast::http::message
    makeresponse (bool crawl, beast::http::message const& req,
        uint256 const& sharedvalue);

    void
    onwriteresponse (error_code ec, std::size_t bytes_transferred);

    //
    // protocol message loop
    //

    // starts the protocol message loop
    void
    doprotocolstart (bool legacy);

    // called when protocol message bytes are received
    void
    onreadmessage (error_code ec, std::size_t bytes_transferred);

    // called when protocol messages bytes are sent
    void
    onwritemessage (error_code ec, std::size_t bytes_transferred);

public:
    //--------------------------------------------------------------------------
    //
    // protocolstream
    //
    //--------------------------------------------------------------------------

    static
    error_code
    invalid_argument_error()
    {
        return boost::system::errc::make_error_code (
            boost::system::errc::invalid_argument);
    }

    error_code
    onmessageunknown (std::uint16_t type);

    error_code
    onmessagebegin (std::uint16_t type,
        std::shared_ptr <::google::protobuf::message> const& m);

    void
    onmessageend (std::uint16_t type,
        std::shared_ptr <::google::protobuf::message> const& m);

    void onmessage (std::shared_ptr <protocol::tmhello> const& m);
    void onmessage (std::shared_ptr <protocol::tmping> const& m);
    void onmessage (std::shared_ptr <protocol::tmcluster> const& m);
    void onmessage (std::shared_ptr <protocol::tmgetpeers> const& m);
    void onmessage (std::shared_ptr <protocol::tmpeers> const& m);
    void onmessage (std::shared_ptr <protocol::tmendpoints> const& m);
    void onmessage (std::shared_ptr <protocol::tmtransaction> const& m);
    void onmessage (std::shared_ptr <protocol::tmgetledger> const& m);
    void onmessage (std::shared_ptr <protocol::tmledgerdata> const& m);
    void onmessage (std::shared_ptr <protocol::tmproposeset> const& m);
    void onmessage (std::shared_ptr <protocol::tmstatuschange> const& m);
    void onmessage (std::shared_ptr <protocol::tmhavetransactionset> const& m);
    void onmessage (std::shared_ptr <protocol::tmvalidation> const& m);
    void onmessage (std::shared_ptr <protocol::tmgetobjectbyhash> const& m);

    //--------------------------------------------------------------------------

private:
    state state() const
    {
        return state_;
    }

    void state (state new_state)
    {
        state_ = new_state;
    }

    //--------------------------------------------------------------------------

    void
    sendgetpeers();

    /** perform a secure handshake with the peer at the other end.
        if this function returns false then we cannot guarantee that there
        is no active man-in-the-middle attack taking place and the link
        must be disconnected.
        @return true if successful, false otherwise.
    */
    bool
    sendhello();

    void
    addledger (uint256 const& hash);

    void
    addtxset (uint256 const& hash);

    void
    dofetchpack (const std::shared_ptr<protocol::tmgetobjectbyhash>& packet);

    void
    checktransaction (job&, int flags, sttx::pointer stx);

    void
    checkpropose (job& job,
        std::shared_ptr<protocol::tmproposeset> const& packet,
            ledgerproposal::pointer proposal, uint256 consensuslcl);

    void
    checkvalidation (job&, stvalidation::pointer val,
        bool istrusted, std::shared_ptr<protocol::tmvalidation> const& packet);

    void
    getledger (std::shared_ptr<protocol::tmgetledger> const&packet);

    // called when we receive tx set data.
    void
    peertxdata (job&, uint256 const& hash,
        std::shared_ptr <protocol::tmledgerdata> const& ppacket,
            beast::journal journal);
};

//------------------------------------------------------------------------------

template <class constbuffersequence>
peerimp::peerimp (id_t id, endpoint_type remote_endpoint,
    peerfinder::slot::ptr const& slot, constbuffersequence const& buffers,
        std::unique_ptr<beast::asio::ssl_bundle>&& ssl_bundle,
            overlayimpl& overlay)
    : child (overlay)
    , id_ (id)
    , sink_ (deprecatedlogs().journal("peer"), makeprefix(id))
    , p_sink_ (deprecatedlogs().journal("protocol"), makeprefix(id))
    , journal_ (sink_)
    , p_journal_ (p_sink_)
    , ssl_bundle_ (std::move(ssl_bundle))
    , socket_ (ssl_bundle_->socket)
    , stream_ (ssl_bundle_->stream)
    , strand_ (socket_.get_io_service())
    , timer_ (socket_.get_io_service())
    , remote_address_ (
        beast::ipaddressconversion::from_asio(remote_endpoint))
    , overlay_ (overlay)
    , m_inbound (true)
    , state_ (state::connected)
    , slot_ (slot)
    , validatorsconnection_(getapp().getvalidators().newconnection(id))
{
    read_buffer_.commit(boost::asio::buffer_copy(read_buffer_.prepare(
        boost::asio::buffer_size(buffers)), buffers));
}

template <class buffers>
peerimp::peerimp (std::unique_ptr<beast::asio::ssl_bundle>&& ssl_bundle,
    buffers const& buffers, peerfinder::slot::ptr&& slot,
        resource::consumer usage, protocol::tmhello&& hello,
            rippleaddress const& legacypublickey, id_t id,
                overlayimpl& overlay)
    : child (overlay)
    , id_ (id)
    , sink_ (deprecatedlogs().journal("peer"), makeprefix(id))
    , p_sink_ (deprecatedlogs().journal("protocol"), makeprefix(id))
    , journal_ (sink_)
    , p_journal_ (p_sink_)
    , ssl_bundle_(std::move(ssl_bundle))
    , socket_ (ssl_bundle_->socket)
    , stream_ (ssl_bundle_->stream)
    , strand_ (socket_.get_io_service())
    , timer_ (socket_.get_io_service())
    , remote_address_ (slot->remote_endpoint())
    , overlay_ (overlay)
    , m_inbound (false)
    , state_ (state::active)
    , publickey_ (legacypublickey)
    , hello_ (std::move(hello))
    , usage_ (usage)
    , slot_ (std::move(slot))
    , validatorsconnection_(getapp().getvalidators().newconnection(id))
{
    read_buffer_.commit (boost::asio::buffer_copy(read_buffer_.prepare(
        boost::asio::buffer_size(buffers)), buffers));
}

template <class fwdit, class>
void
peerimp::sendendpoints (fwdit first, fwdit last)
{
    protocol::tmendpoints tm;
    for (;first != last; ++first)
    {
        auto const& ep = *first;
        protocol::tmendpoint& tme (*tm.add_endpoints());
        if (ep.address.is_v4())
            tme.mutable_ipv4()->set_ipv4(
                beast::tonetworkbyteorder (ep.address.to_v4().value));
        else
            tme.mutable_ipv4()->set_ipv4(0);
        tme.mutable_ipv4()->set_ipv4port (ep.address.port());
        tme.set_hops (ep.hops);
    }
    tm.set_version (1);

    send (std::make_shared <message> (tm, protocol::mtendpoints));
}

}

#endif
