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
#include <ripple/overlay/impl/tmhello.h>
#include <ripple/overlay/impl/peerimp.h>
#include <ripple/overlay/impl/tuning.h>
#include <ripple/app/ledger/inboundledgers.h>
#include <ripple/app/ledger/ledgermaster.h>
#include <ripple/app/misc/ihashrouter.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/app/peers/clusternodestatus.h>
#include <ripple/app/peers/uniquenodelist.h>
#include <ripple/basics/stringutilities.h>
#include <ripple/basics/uptimetimer.h>
#include <ripple/core/jobqueue.h>
#include <ripple/json/json_reader.h>
#include <ripple/resource/fees.h>
#include <ripple/server/serverhandler.h>
#include <ripple/protocol/buildinfo.h>
#include <beast/streams/debug_ostream.h>
#include <beast/weak_fn.h>
#include <boost/asio/io_service.hpp>
#include <functional>
#include <beast/cxx14/memory.h> // <memory>
#include <sstream>

namespace ripple {

peerimp::peerimp (id_t id, endpoint_type remote_endpoint,
    peerfinder::slot::ptr const& slot, beast::http::message&& request,
        protocol::tmhello const& hello, rippleaddress const& publickey,
            resource::consumer consumer,
                std::unique_ptr<beast::asio::ssl_bundle>&& ssl_bundle,
                    overlayimpl& overlay)
    : child (overlay)
    , id_(id)
    , sink_(deprecatedlogs().journal("peer"), makeprefix(id))
    , p_sink_(deprecatedlogs().journal("protocol"), makeprefix(id))
    , journal_ (sink_)
    , p_journal_(p_sink_)
    , ssl_bundle_(std::move(ssl_bundle))
    , socket_ (ssl_bundle_->socket)
    , stream_ (ssl_bundle_->stream)
    , strand_ (socket_.get_io_service())
    , timer_ (socket_.get_io_service())
    , remote_address_ (
        beast::ipaddressconversion::from_asio(remote_endpoint))
    , overlay_ (overlay)
    , m_inbound (true)
    , state_ (state::active)
    , publickey_(publickey)
    , hello_(hello)
    , usage_(consumer)
    , slot_ (slot)
    , http_message_(std::move(request))
    , validatorsconnection_(getapp().getvalidators().newconnection(id))
{
}

peerimp::~peerimp ()
{
    if (cluster())
        if (journal_.warning) journal_.warning <<
            name_ << " left cluster";
    if (state_ == state::active)
    {
        assert(publickey_.isset());
        assert(publickey_.isvalid());
        overlay_.onpeerdeactivate(id_, publickey_);
    }
    overlay_.peerfinder().on_closed (slot_);
    overlay_.remove (slot_);
}

void
peerimp::run()
{
    if(! strand_.running_in_this_thread())
        return strand_.post(std::bind (
            &peerimp::run, shared_from_this()));
    if (m_inbound)
    {
        if (read_buffer_.size() > 0)
            dolegacyaccept();
        else
            doaccept();
    }
    else
    {
        assert (state_ == state::active);
        // xxx set timer: connection is in grace period to be useful.
        // xxx set timer: connection idle (idle may vary depending on connection type.)
        if ((hello_.has_ledgerclosed ()) && (
            hello_.ledgerclosed ().size () == (256 / 8)))
        {
            memcpy (closedledgerhash_.begin (),
                hello_.ledgerclosed ().data (), 256 / 8);
            if ((hello_.has_ledgerprevious ()) &&
                (hello_.ledgerprevious ().size () == (256 / 8)))
            {
                memcpy (previousledgerhash_.begin (),
                    hello_.ledgerprevious ().data (), 256 / 8);
                addledger (previousledgerhash_);
            }
            else
            {
                previousledgerhash_.zero();
            }
        }
        doprotocolstart(false);
    }
}

void
peerimp::stop()
{
    if(! strand_.running_in_this_thread())
        return strand_.post(std::bind (
            &peerimp::stop, shared_from_this()));
    if (socket_.is_open())
    {
        // the rationale for using different severity levels is that
        // outbound connections are under our control and may be logged
        // at a higher level, but inbound connections are more numerous and
        // uncontrolled so to prevent log flooding the severity is reduced.
        //
        if(m_inbound)
        {
            if(journal_.debug) journal_.debug <<
                "stop";
        }
        else
        {
            if(journal_.info) journal_.info <<
                "stop";
        }
    }
    close();
}

//------------------------------------------------------------------------------

void
peerimp::send (message::pointer const& m)
{
    if (! strand_.running_in_this_thread())
        return strand_.post(std::bind (
            &peerimp::send, shared_from_this(), m));
    if(gracefulclose_)
        return;
    if(detaching_)
        return;
    send_queue_.push(m);
    if(send_queue_.size() > 1)
        return;
    settimer();
    boost::asio::async_write (stream_, boost::asio::buffer(
        send_queue_.front()->getbuffer()), strand_.wrap(std::bind(
            &peerimp::onwritemessage, shared_from_this(),
                beast::asio::placeholders::error,
                    beast::asio::placeholders::bytes_transferred)));
}

void
peerimp::charge (resource::charge const& fee)
{
    if ((usage_.charge(fee) == resource::drop) && usage_.disconnect())
        fail("charge: resources");
}

//------------------------------------------------------------------------------

bool
peerimp::crawl() const
{
    auto const iter = http_message_.headers.find("crawl");
    if (iter == http_message_.headers.end())
        return false;
    return beast::ci_equal(iter->second, "public");
}

json::value
peerimp::json()
{
    json::value ret (json::objectvalue);

    ret["public_key"]   = publickey_.tostring ();
    ret["address"]      = remote_address_.to_string();

    if (m_inbound)
        ret["inbound"] = true;

    if (cluster())
    {
        ret["cluster"] = true;

        if (!name_.empty ())
            ret["name"] = name_;
    }

    if (hello_.has_fullversion ())
        ret["version"] = hello_.fullversion ();

    if (hello_.has_protoversion ())
    {
        auto protocol = buildinfo::make_protocol (hello_.protoversion ());

        if (protocol != buildinfo::getcurrentprotocol())
            ret["protocol"] = to_string (protocol);
    }

    std::uint32_t minseq, maxseq;
    ledgerrange(minseq, maxseq);

    if ((minseq != 0) || (maxseq != 0))
        ret["complete_ledgers"] = boost::lexical_cast<std::string>(minseq) +
            " - " + boost::lexical_cast<std::string>(maxseq);

    if (closedledgerhash_ != zero)
        ret["ledger"] = to_string (closedledgerhash_);

    if (last_status_.has_newstatus ())
    {
        switch (last_status_.newstatus ())
        {
        case protocol::nsconnecting:
            ret["status"] = "connecting";
            break;

        case protocol::nsconnected:
            ret["status"] = "connected";
            break;

        case protocol::nsmonitoring:
            ret["status"] = "monitoring";
            break;

        case protocol::nsvalidating:
            ret["status"] = "validating";
            break;

        case protocol::nsshutting:
            ret["status"] = "shutting";
            break;

        default:
            // fixme: do we really want this?
            p_journal_.warning <<
                "unknown status: " << last_status_.newstatus ();
        }
    }

    return ret;
}

//------------------------------------------------------------------------------

bool
peerimp::hasledger (uint256 const& hash, std::uint32_t seq) const
{
    std::lock_guard<std::mutex> sl(recentlock_);
    if ((seq != 0) && (seq >= minledger_) && (seq <= maxledger_))
        return true;
    return std::find (recentledgers_.begin(),
        recentledgers_.end(), hash) != recentledgers_.end();
}

void
peerimp::ledgerrange (std::uint32_t& minseq,
    std::uint32_t& maxseq) const
{
    std::lock_guard<std::mutex> sl(recentlock_);

    minseq = minledger_;
    maxseq = maxledger_;
}

bool
peerimp::hastxset (uint256 const& hash) const
{
    std::lock_guard<std::mutex> sl(recentlock_);
    return std::find (recenttxsets_.begin(),
        recenttxsets_.end(), hash) != recenttxsets_.end();
}

void
peerimp::cyclestatus ()
{
    previousledgerhash_ = closedledgerhash_;
    closedledgerhash_.zero ();
}

bool
peerimp::supportsversion (int version)
{
    return hello_.has_protoversion () && (hello_.protoversion () >= version);
}

bool
peerimp::hasrange (std::uint32_t umin, std::uint32_t umax)
{
    return (umin >= minledger_) && (umax <= maxledger_);
}

//------------------------------------------------------------------------------

void
peerimp::close()
{
    assert(strand_.running_in_this_thread());
    if (socket_.is_open())
    {
        detaching_ = true; // deprecated
        error_code ec;
        timer_.cancel(ec);
        socket_.close(ec);
        if(m_inbound)
        {
            if(journal_.debug) journal_.debug <<
                "closed";
        }
        else
        {
            if(journal_.info) journal_.info <<
                "closed";
        }
    }
}

void
peerimp::fail(std::string const& reason)
{
    assert(strand_.running_in_this_thread());
    if (socket_.is_open())
        if (journal_.debug) journal_.debug <<
            reason;
    close();
}

void
peerimp::fail(std::string const& name, error_code ec)
{
    assert(strand_.running_in_this_thread());
    if (socket_.is_open())
        if (journal_.debug) journal_.debug <<
            name << ": " << ec.message();
    close();
}

void
peerimp::gracefulclose()
{
    assert(strand_.running_in_this_thread());
    assert(socket_.is_open());
    assert(! gracefulclose_);
    gracefulclose_ = true;
#if 0
    // flush messages
    while(send_queue_.size() > 1)
        send_queue_.pop_back();
#endif
    if (send_queue_.size() > 0)
        return;
    settimer();
    stream_.async_shutdown(strand_.wrap(std::bind(&peerimp::onshutdown,
        shared_from_this(), beast::asio::placeholders::error)));
}

void
peerimp::settimer()
{
    error_code ec;
    timer_.expires_from_now(std::chrono::seconds(15), ec);
    if (ec)
    {
        if (journal_.error) journal_.error <<
            "settimer: " << ec.message();
        return;
    }
    timer_.async_wait(strand_.wrap(std::bind(&peerimp::ontimer,
        shared_from_this(), beast::asio::placeholders::error)));
}

// convenience for ignoring the error code
void
peerimp::canceltimer()
{
    error_code ec;
    timer_.cancel(ec);
}

//------------------------------------------------------------------------------

std::string
peerimp::makeprefix(id_t id)
{
    std::stringstream ss;
    ss << "[" << std::setfill('0') << std::setw(3) << id << "] ";
    return ss.str();
}

void
peerimp::ontimer (error_code const& ec)
{
    if (! socket_.is_open())
        return;

    if (ec == boost::asio::error::operation_aborted)
        return;

    if (ec)
    {
        // this should never happen
        if (journal_.error) journal_.error <<
            "ontimer: " << ec.message();
        return close();
    }

    fail("timeout");
}

void
peerimp::onshutdown(error_code ec)
{
    canceltimer();
    // if we don't get eof then something went wrong
    if (! ec)
    {
        if (journal_.error) journal_.error <<
            "onshutdown: expected error condition";
        return close();
    }
    if (ec != boost::asio::error::eof)
        return fail("onshutdown", ec);
    close();
}

//------------------------------------------------------------------------------

void peerimp::dolegacyaccept()
{
    assert(read_buffer_.size() > 0);
    if(journal_.debug) journal_.debug <<
        "dolegacyaccept: " << remote_address_;
    usage_ = overlay_.resourcemanager().newinboundendpoint (remote_address_);
    if (usage_.disconnect ())
        return fail("dolegacyaccept: resources");
    doprotocolstart(true);
}

void peerimp::doaccept()
{
    assert(read_buffer_.size() == 0);
    assert(http_message_.upgrade());

    if(journal_.debug) journal_.debug <<
        "doaccept: " << remote_address_;

    bool success;
    uint256 sharedvalue;
    std::tie(sharedvalue, success) = makesharedvalue(
        ssl_bundle_->stream.native_handle(), journal_);
    // this shouldn't fail since we already computed
    // the shared value successfully in overlayimpl
    if(! success)
        return fail("makesharedvalue: unexpected failure");

    // todo apply headers to connection state.

    auto resp = makeresponse(
        ! overlay_.peerfinder().config().peerprivate,
            http_message_, sharedvalue);
    beast::http::write (write_buffer_, resp);

    auto const protocol = buildinfo::make_protocol(hello_.protoversion());
    if(journal_.info) journal_.info <<
        "protocol: " << to_string(protocol);
    if(journal_.info) journal_.info <<
        "public key: " << publickey_.humannodepublic();
    bool const cluster = getapp().getunl().nodeincluster(publickey_, name_);
    if (cluster)
        if (journal_.info) journal_.info <<
            "cluster name: " << name_;

    overlay_.activate(shared_from_this());

    // xxx set timer: connection is in grace period to be useful.
    // xxx set timer: connection idle (idle may vary depending on connection type.)
    if ((hello_.has_ledgerclosed ()) && (
        hello_.ledgerclosed ().size () == (256 / 8)))
    {
        memcpy (closedledgerhash_.begin (),
            hello_.ledgerclosed ().data (), 256 / 8);
        if ((hello_.has_ledgerprevious ()) &&
            (hello_.ledgerprevious ().size () == (256 / 8)))
        {
            memcpy (previousledgerhash_.begin (),
                hello_.ledgerprevious ().data (), 256 / 8);
            addledger (previousledgerhash_);
        }
        else
        {
            previousledgerhash_.zero();
        }
    }

    onwriteresponse(error_code(), 0);
}

beast::http::message
peerimp::makeresponse (bool crawl,
    beast::http::message const& req, uint256 const& sharedvalue)
{
    beast::http::message resp;
    resp.request(false);
    resp.status(101);
    resp.reason("switching protocols");
    resp.version(req.version());
    resp.headers.append("connection", "upgrade");
    resp.headers.append("upgrade", "rtxp/1.2");
    resp.headers.append("connect-as", "peer");
    resp.headers.append("server", buildinfo::getfullversionstring());
    resp.headers.append ("crawl", crawl ? "public" : "private");
    protocol::tmhello hello = buildhello(sharedvalue, getapp());
    appendhello(resp, hello);
    return resp;
}

// called repeatedly to send the bytes in the response
void
peerimp::onwriteresponse (error_code ec, std::size_t bytes_transferred)
{
    canceltimer();
    if(! socket_.is_open())
        return;
    if(ec == boost::asio::error::operation_aborted)
        return;
    if(ec)
        return fail("onwriteresponse", ec);
    if(journal_.trace)
    {
        if (bytes_transferred > 0) journal_.trace <<
            "onwriteresponse: " << bytes_transferred << " bytes";
        else journal_.trace <<
            "onwriteresponse";
    }

    write_buffer_.consume (bytes_transferred);
    if (write_buffer_.size() == 0)
        return doprotocolstart(false);

    settimer();
    stream_.async_write_some (write_buffer_.data(),
        strand_.wrap (std::bind (&peerimp::onwriteresponse,
            shared_from_this(), beast::asio::placeholders::error,
                beast::asio::placeholders::bytes_transferred)));
}

//------------------------------------------------------------------------------

// protocol logic

// we have an encrypted connection to the peer.
// have it say who it is so we know to avoid redundant connections.
// establish that it really who we are talking to by having it sign a
// connection detail. also need to establish no man in the middle attack
// is in progress.
void
peerimp::doprotocolstart(bool legacy)
{
    if (legacy && !sendhello ())
    {
        journal_.error << "unable to send hello to " << remote_address_;
        return fail ("hello");
    }

    onreadmessage(error_code(), 0);
}

// called repeatedly with protocol message data
void
peerimp::onreadmessage (error_code ec, std::size_t bytes_transferred)
{
    if(! socket_.is_open())
        return;
    if(ec == boost::asio::error::operation_aborted)
        return;
    if(ec == boost::asio::error::eof)
    {
        if(journal_.info) journal_.info <<
            "eof";
        return gracefulclose();
    }
    if(ec)
        return fail("onreadmessage", ec);
    if(journal_.trace)
    {
        if (bytes_transferred > 0) journal_.trace <<
            "onreadmessage: " << bytes_transferred << " bytes";
        else journal_.trace <<
            "onreadmessage";
    }

    read_buffer_.commit (bytes_transferred);

    while (read_buffer_.size() > 0)
    {
        std::size_t bytes_consumed;
        std::tie(bytes_consumed, ec) = invokeprotocolmessage(
            read_buffer_.data(), *this);
        if (ec)
            return fail("onreadmessage", ec);
        if (! stream_.next_layer().is_open())
            return;
        if(gracefulclose_)
            return;
        if (bytes_consumed == 0)
            break;
        read_buffer_.consume (bytes_consumed);
    }
    // timeout on writes only
    stream_.async_read_some (read_buffer_.prepare (tuning::readbufferbytes),
        strand_.wrap (std::bind (&peerimp::onreadmessage,
            shared_from_this(), beast::asio::placeholders::error,
                beast::asio::placeholders::bytes_transferred)));
}

void
peerimp::onwritemessage (error_code ec, std::size_t bytes_transferred)
{
    canceltimer();
    if(! socket_.is_open())
        return;
    if(ec == boost::asio::error::operation_aborted)
        return;
    if(ec)
        return fail("onwritemessage", ec);
    if(journal_.trace)
    {
        if (bytes_transferred > 0) journal_.trace <<
            "onwritemessage: " << bytes_transferred << " bytes";
        else journal_.trace <<
            "onwritemessage";
    }

    assert(! send_queue_.empty());
    send_queue_.pop();
    if (! send_queue_.empty())
    {
        // timeout on writes only
        settimer();
        return boost::asio::async_write (stream_, boost::asio::buffer(
            send_queue_.front()->getbuffer()), strand_.wrap(std::bind(
                &peerimp::onwritemessage, shared_from_this(),
                    beast::asio::placeholders::error,
                        beast::asio::placeholders::bytes_transferred)));
    }

    if (gracefulclose_)
    {
        settimer();
        return stream_.async_shutdown(strand_.wrap(std::bind(
            &peerimp::onshutdown, shared_from_this(),
                beast::asio::placeholders::error)));
    }
}

//------------------------------------------------------------------------------
//
// protocolhandler
//
//------------------------------------------------------------------------------

peerimp::error_code
peerimp::onmessageunknown (std::uint16_t type)
{
    journal_.warning << "unknown message type " << type << " from " << remote_address_;
    error_code ec;
    // todo
    return ec;
}

peerimp::error_code
peerimp::onmessagebegin (std::uint16_t type,
    std::shared_ptr <::google::protobuf::message> const& m)
{
    error_code ec;

    if (type == protocol::mthello && state_ != state::connected)
    {
        journal_.warning <<
            "unexpected tmhello";
        ec = invalid_argument_error();
    }
    else if (type != protocol::mthello && state_ == state::connected)
    {
        journal_.warning <<
            "expected tmhello";
        ec = invalid_argument_error();
    }

    if (! ec)
    {
        load_event_ = getapp().getjobqueue ().getloadeventap (
            jtpeer, protocolmessagename(type));
    }

    return ec;
}

void
peerimp::onmessageend (std::uint16_t,
    std::shared_ptr <::google::protobuf::message> const&)
{
    load_event_.reset();
}

void
peerimp::onmessage (std::shared_ptr <protocol::tmhello> const& m)
{
    std::uint32_t const ourtime (getapp().getops ().getnetworktimenc ());
    std::uint32_t const mintime (ourtime - clocktolerancedeltaseconds);
    std::uint32_t const maxtime (ourtime + clocktolerancedeltaseconds);

#ifdef beast_debug
    if (m->has_nettime ())
    {
        std::int64_t to = ourtime;
        to -= m->nettime ();
        journal_.debug <<
            "time offset: " << to;
    }
#endif

    // vfalco todo report these failures in the http response

    auto const protocol = buildinfo::make_protocol(m->protoversion());
    if (m->has_nettime () &&
        ((m->nettime () < mintime) || (m->nettime () > maxtime)))
    {
        if (m->nettime () > maxtime)
        {
            journal_.info <<
                "hello: clock off by +" << m->nettime () - ourtime;
        }
        else if (m->nettime () < mintime)
        {
            journal_.info <<
                "hello: clock off by -" << ourtime - m->nettime ();
        }
    }
    else if (m->protoversionmin () > to_packed (buildinfo::getcurrentprotocol()))
    {
        journal_.info <<
            "hello: disconnect: protocol mismatch [" <<
            "peer expects " << to_string (protocol) <<
            " and we run " << to_string (buildinfo::getcurrentprotocol()) << "]";
    }
    else if (! publickey_.setnodepublic (m->nodepublic ()))
    {
        journal_.info <<
            "hello: disconnect: bad node public key.";
    }
    else if (! publickey_.verifynodepublic (
        sharedvalue_, m->nodeproof (), ecdsa::not_strict))
    {
        // unable to verify they have private key for claimed public key.
        journal_.info <<
            "hello: disconnect: failed to verify session.";
    }
    else
    {
        if(journal_.info) journal_.info <<
            "protocol: " << to_string(protocol);
        if(journal_.info) journal_.info <<
            "public key: " << publickey_.humannodepublic();
        bool const cluster = getapp().getunl().nodeincluster(publickey_, name_);
        if (cluster)
            if (journal_.info) journal_.info <<
                "cluster name: " << name_;

        assert (state_ == state::connected);
        // vfalco todo remove this needless state
        state_ = state::handshaked;
        hello_ = *m;

        auto const result = overlay_.peerfinder().activate (slot_,
            publickey_.topublickey(), cluster);

        if (result == peerfinder::result::success)
        {
            state_ = state::active;
            overlay_.activate(shared_from_this ());

            // xxx set timer: connection is in grace period to be useful.
            // xxx set timer: connection idle (idle may vary depending on connection type.)
            if ((hello_.has_ledgerclosed ()) && (
                hello_.ledgerclosed ().size () == (256 / 8)))
            {
                memcpy (closedledgerhash_.begin (),
                    hello_.ledgerclosed ().data (), 256 / 8);
                if ((hello_.has_ledgerprevious ()) &&
                    (hello_.ledgerprevious ().size () == (256 / 8)))
                {
                    memcpy (previousledgerhash_.begin (),
                        hello_.ledgerprevious ().data (), 256 / 8);
                    addledger (previousledgerhash_);
                }
                else
                {
                    previousledgerhash_.zero();
                }
            }

            return sendgetpeers();
        }

        if (result == peerfinder::result::full)
        {
            // todo provide correct http response
            auto const redirects = overlay_.peerfinder().redirect (slot_);
            sendendpoints (redirects.begin(), redirects.end());
            return gracefulclose();
        }
        else if (result == peerfinder::result::duplicate)
        {
            return fail("duplicate public key");
        }
    }

    fail("tmhello invalid");
}

void
peerimp::onmessage (std::shared_ptr <protocol::tmping> const& m)
{
    if (m->type () == protocol::tmping::ptping)
    {
        m->set_type (protocol::tmping::ptpong);
        send (std::make_shared<message> (*m, protocol::mtping));
    }
}

void
peerimp::onmessage (std::shared_ptr <protocol::tmcluster> const& m)
{
    // vfalco note i think we should drop the peer immediately
    if (! cluster())
        return charge (resource::feeunwanteddata);

    for (int i = 0; i < m->clusternodes().size(); ++i)
    {
        protocol::tmclusternode const& node = m->clusternodes(i);

        std::string name;
        if (node.has_nodename())
            name = node.nodename();
        clusternodestatus s(name, node.nodeload(), node.reporttime());

        rippleaddress nodepub;
        nodepub.setnodepublic(node.publickey());

        getapp().getunl().nodeupdate(nodepub, s);
    }

    int loadsources = m->loadsources().size();
    if (loadsources != 0)
    {
        resource::gossip gossip;
        gossip.items.reserve (loadsources);
        for (int i = 0; i < m->loadsources().size(); ++i)
        {
            protocol::tmloadsource const& node = m->loadsources (i);
            resource::gossip::item item;
            item.address = beast::ip::endpoint::from_string (node.name());
            item.balance = node.cost();
            if (item.address != beast::ip::endpoint())
                gossip.items.push_back(item);
        }
        overlay_.resourcemanager().importconsumers (name_, gossip);
    }

    getapp().getfeetrack().setclusterfee(getapp().getunl().getclusterfee());
}

void
peerimp::onmessage (std::shared_ptr <protocol::tmgetpeers> const& m)
{
    // vfalco todo this message is now obsolete due to peerfinder
}

void
peerimp::onmessage (std::shared_ptr <protocol::tmpeers> const& m)
{
    // vfalco todo this message is now obsolete due to peerfinder
    std::vector <beast::ip::endpoint> list;
    list.reserve (m->nodes().size());
    for (int i = 0; i < m->nodes ().size (); ++i)
    {
        in_addr addr;

        addr.s_addr = m->nodes (i).ipv4 ();

        {
            beast::ip::addressv4 v4 (ntohl (addr.s_addr));
            beast::ip::endpoint address (v4, m->nodes (i).ipv4port ());
            list.push_back (address);
        }
    }

    if (! list.empty())
        overlay_.peerfinder().on_legacy_endpoints (list);
}

void
peerimp::onmessage (std::shared_ptr <protocol::tmendpoints> const& m)
{
    std::vector <peerfinder::endpoint> endpoints;

    endpoints.reserve (m->endpoints().size());

    for (int i = 0; i < m->endpoints ().size (); ++i)
    {
        peerfinder::endpoint endpoint;
        protocol::tmendpoint const& tm (m->endpoints(i));

        // hops
        endpoint.hops = tm.hops();

        // ipv4
        if (endpoint.hops > 0)
        {
            in_addr addr;
            addr.s_addr = tm.ipv4().ipv4();
            beast::ip::addressv4 v4 (ntohl (addr.s_addr));
            endpoint.address = beast::ip::endpoint (v4, tm.ipv4().ipv4port ());
        }
        else
        {
            // this endpoint describes the peer we are connected to.
            // we will take the remote address seen on the socket and
            // store that in the ip::endpoint. if this is the first time,
            // then we'll verify that their listener can receive incoming
            // by performing a connectivity test.
            //
            endpoint.address = remote_address_.at_port (
                tm.ipv4().ipv4port ());
        }

        endpoints.push_back (endpoint);
    }

    if (! endpoints.empty())
        overlay_.peerfinder().on_endpoints (slot_, endpoints);
}

void
peerimp::onmessage (std::shared_ptr <protocol::tmtransaction> const& m)
{

    if (getapp().getops().isneednetworkledger ())
    {
        // if we've never been in synch, there's nothing we can do
        // with a transaction
        return;
    }

    serializer s (m->rawtransaction ());

    try
    {
        serializeriterator sit (s);
        sttx::pointer stx = std::make_shared <
            sttx> (std::ref (sit));
        uint256 txid = stx->gettransactionid ();

        int flags;

        if (! getapp().gethashrouter ().addsuppressionpeer (
            txid, id_, flags))
        {
            // we have seen this transaction recently
            if (flags & sf_bad)
            {
                charge (resource::feeinvalidsignature);
                return;
            }

            if (!(flags & sf_retry))
                return;
        }

        p_journal_.debug <<
            "got tx " << txid;

        if (cluster())
        {
            if (! m->has_deferred () || ! m->deferred ())
            {
                // skip local checks if a server we trust
                // put the transaction in its open ledger
                flags |= sf_trusted;
            }

            if (! getconfig().validation_priv.isset())
            {
                // for now, be paranoid and have each validator
                // check each transaction, regardless of source
                flags |= sf_siggood;
            }
        }

        if (getapp().getjobqueue().getjobcount(jttransaction) > 100)
            p_journal_.info << "transaction queue is full";
        else if (getapp().getledgermaster().getvalidatedledgerage() > 240)
            p_journal_.trace << "no new transactions until synchronized";
        else
            getapp().getjobqueue ().addjob (jttransaction,
                "recvtransaction->checktransaction",
                std::bind(beast::weak_fn(&peerimp::checktransaction,
                shared_from_this()), std::placeholders::_1, flags, stx));
    }
    catch (...)
    {
        p_journal_.warning << "transaction invalid: " <<
            s.gethex();
    }
}

void
peerimp::onmessage (std::shared_ptr <protocol::tmgetledger> const& m)
{
    getapp().getjobqueue().addjob (jtpack, "recvgetledger", std::bind(
        beast::weak_fn(&peerimp::getledger, shared_from_this()), m));
}

void
peerimp::onmessage (std::shared_ptr <protocol::tmledgerdata> const& m)
{
    protocol::tmledgerdata& packet = *m;

    if (m->nodes ().size () <= 0)
    {
        p_journal_.warning << "ledger/txset data with no nodes";
        return;
    }

    if (m->has_requestcookie ())
    {
        peer::ptr target = overlay_.findpeerbyshortid (m->requestcookie ());
        if (target)
        {
            m->clear_requestcookie ();
            target->send (std::make_shared<message> (
                packet, protocol::mtledger_data));
        }
        else
        {
            p_journal_.info << "unable to route tx/ledger data reply";
            charge (resource::feeunwanteddata);
        }
        return;
    }

    uint256 hash;

    if (m->ledgerhash ().size () != 32)
    {
        p_journal_.warning << "tx candidate reply with invalid hash size";
        charge (resource::feeinvalidrequest);
        return;
    }

    memcpy (hash.begin (), m->ledgerhash ().data (), 32);

    if (m->type () == protocol::lits_candidate)
    {
        // got data for a candidate transaction set
        getapp().getjobqueue().addjob(jttxn_data, "recvpeerdata", std::bind(
            beast::weak_fn(&peerimp::peertxdata, shared_from_this()),
            std::placeholders::_1, hash, m, p_journal_));
        return;
    }

    if (!getapp().getinboundledgers ().gotledgerdata (
        hash, shared_from_this(), m))
    {
        p_journal_.trace  << "got data for unwanted ledger";
        charge (resource::feeunwanteddata);
    }
}

void
peerimp::onmessage (std::shared_ptr <protocol::tmproposeset> const& m)
{
    protocol::tmproposeset& set = *m;

    // vfalco magic numbers are bad
    if ((set.closetime() + 180) < getapp().getops().getclosetimenc())
        return;

    // vfalco magic numbers are bad
    // roll this into a validation function
    if (
        (set.currenttxhash ().size () != 32) ||
        (set.nodepubkey ().size () < 28) ||
        (set.signature ().size () < 56) ||
        (set.nodepubkey ().size () > 128) ||
        (set.signature ().size () > 128)
    )
    {
        p_journal_.warning << "proposal: malformed";
        charge (resource::feeinvalidsignature);
        return;
    }

    if (set.has_previousledger () && (set.previousledger ().size () != 32))
    {
        p_journal_.warning << "proposal: malformed";
        charge (resource::feeinvalidrequest);
        return;
    }

    uint256 proposehash, prevledger;
    memcpy (proposehash.begin (), set.currenttxhash ().data (), 32);

    if (set.has_previousledger ())
        memcpy (prevledger.begin (), set.previousledger ().data (), 32);

    uint256 suppression = ledgerproposal::computesuppressionid (
        proposehash, prevledger, set.proposeseq(), set.closetime (),
            blob(set.nodepubkey ().begin (), set.nodepubkey ().end ()),
                blob(set.signature ().begin (), set.signature ().end ()));

    if (! getapp().gethashrouter ().addsuppressionpeer (
        suppression, id_))
    {
        p_journal_.trace << "proposal: duplicate";
        return;
    }

    rippleaddress signerpublic = rippleaddress::createnodepublic (
        strcopy (set.nodepubkey ()));

    if (signerpublic == getconfig ().validation_pub)
    {
        p_journal_.trace << "proposal: self";
        return;
    }

    bool istrusted = getapp().getunl ().nodeinunl (signerpublic);
    if (!istrusted && getapp().getfeetrack ().isloadedlocal ())
    {
        p_journal_.debug << "proposal: dropping untrusted (load)";
        return;
    }

    p_journal_.trace <<
        "proposal: " << (istrusted ? "trusted" : "untrusted");

    uint256 consensuslcl;

    {
        application::scopedlocktype lock (getapp ().getmasterlock ());
        consensuslcl = getapp().getops ().getconsensuslcl ();
    }

    ledgerproposal::pointer proposal = std::make_shared<ledgerproposal> (
        prevledger.isnonzero () ? prevledger : consensuslcl,
            set.proposeseq (), proposehash, set.closetime (),
                signerpublic, suppression);

    getapp().getjobqueue ().addjob (istrusted ? jtproposal_t : jtproposal_ut,
        "recvpropose->checkpropose", std::bind(beast::weak_fn(
            &peerimp::checkpropose, shared_from_this()), std::placeholders::_1,
            m, proposal, consensuslcl));
}

void
peerimp::onmessage (std::shared_ptr <protocol::tmstatuschange> const& m)
{
    p_journal_.trace << "status: change";

    if (!m->has_networktime ())
        m->set_networktime (getapp().getops ().getnetworktimenc ());

    if (!last_status_.has_newstatus () || m->has_newstatus ())
        last_status_ = *m;
    else
    {
        // preserve old status
        protocol::nodestatus status = last_status_.newstatus ();
        last_status_ = *m;
        m->set_newstatus (status);
    }

    if (m->newevent () == protocol::nelost_sync)
    {
        if (!closedledgerhash_.iszero ())
        {
            p_journal_.trace << "status: out of sync";
            closedledgerhash_.zero ();
        }

        previousledgerhash_.zero ();
        return;
    }

    if (m->has_ledgerhash () && (m->ledgerhash ().size () == (256 / 8)))
    {
        // a peer has changed ledgers
        memcpy (closedledgerhash_.begin (), m->ledgerhash ().data (), 256 / 8);
        addledger (closedledgerhash_);
        p_journal_.trace << "lcl is " << closedledgerhash_;
    }
    else
    {
        p_journal_.trace << "status: no ledger";
        closedledgerhash_.zero ();
    }

    if (m->has_ledgerhashprevious () &&
        m->ledgerhashprevious ().size () == (256 / 8))
    {
        memcpy (previousledgerhash_.begin (),
            m->ledgerhashprevious ().data (), 256 / 8);
        addledger (previousledgerhash_);
    }
    else previousledgerhash_.zero ();

    if (m->has_firstseq () && m->has_lastseq())
    {
        minledger_ = m->firstseq ();
        maxledger_ = m->lastseq ();

        // vfalco is this workaround still needed?
        // work around some servers that report sequences incorrectly
        if (minledger_ == 0)
            maxledger_ = 0;
        if (maxledger_ == 0)
            minledger_ = 0;
    }
}

void
peerimp::onmessage (std::shared_ptr <protocol::tmhavetransactionset> const& m)
{
    uint256 hashes;

    if (m->hash ().size () != (256 / 8))
    {
        charge (resource::feeinvalidrequest);
        return;
    }

    uint256 hash;

    // vfalco todo there should be no use of memcpy() throughout the program.
    //        todo clean up this magic number
    //
    memcpy (hash.begin (), m->hash ().data (), 32);

    if (m->status () == protocol::tshave)
        addtxset (hash);

    {
        application::scopedlocktype lock (getapp ().getmasterlock ());

        if (!getapp().getops ().hastxset (
                shared_from_this (), hash, m->status ()))
            charge (resource::feeunwanteddata);
    }
}

void
peerimp::onmessage (std::shared_ptr <protocol::tmvalidation> const& m)
{
    error_code ec;
    std::uint32_t closetime = getapp().getops().getclosetimenc();

    if (m->validation ().size () < 50)
    {
        p_journal_.warning << "validation: too small";
        charge (resource::feeinvalidrequest);
        return;
    }

    try
    {
        serializer s (m->validation ());
        serializeriterator sit (s);
        stvalidation::pointer val = std::make_shared <
            stvalidation> (std::ref (sit), false);

        if (closetime > (120 + val->getfieldu32(sfsigningtime)))
        {
            p_journal_.trace << "validation: too old";
            charge (resource::feeunwanteddata);
            return;
        }

        if (! getapp().gethashrouter ().addsuppressionpeer (
            s.getsha512half(), id_))
        {
            p_journal_.trace << "validation: duplicate";
            return;
        }

        bool istrusted = getapp().getunl ().nodeinunl (val->getsignerpublic ());
        if (istrusted || !getapp().getfeetrack ().isloadedlocal ())
        {
            getapp().getjobqueue ().addjob (istrusted ?
                jtvalidation_t : jtvalidation_ut, "recvvalidation->checkvalidation",
                    std::bind(beast::weak_fn(&peerimp::checkvalidation,
                        shared_from_this()), std::placeholders::_1, val,
                            istrusted, m));
        }
        else
        {
            p_journal_.debug <<
                "validation: dropping untrusted (load)";
        }
    }
    catch (std::exception const& e)
    {
        p_journal_.warning <<
            "validation: exception, " << e.what();
        charge (resource::feeinvalidrequest);
    }
    catch (...)
    {
        p_journal_.warning <<
            "validation: unknown exception";
        charge (resource::feeinvalidrequest);
    }
}

void
peerimp::onmessage (std::shared_ptr <protocol::tmgetobjectbyhash> const& m)
{
    protocol::tmgetobjectbyhash& packet = *m;

    if (packet.query ())
    {
        // this is a query
        if (packet.type () == protocol::tmgetobjectbyhash::otfetch_pack)
        {
            dofetchpack (m);
            return;
        }

        protocol::tmgetobjectbyhash reply;

        reply.set_query (false);

        if (packet.has_seq ())
            reply.set_seq (packet.seq ());

        reply.set_type (packet.type ());

        if (packet.has_ledgerhash ())
            reply.set_ledgerhash (packet.ledgerhash ());

        // this is a very minimal implementation
        for (int i = 0; i < packet.objects_size (); ++i)
        {
            uint256 hash;
            const protocol::tmindexedobject& obj = packet.objects (i);

            if (obj.has_hash () && (obj.hash ().size () == (256 / 8)))
            {
                memcpy (hash.begin (), obj.hash ().data (), 256 / 8);
                // vfalco todo move this someplace more sensible so we dont
                //             need to inject the nodestore interfaces.
                nodeobject::pointer hobj =
                    getapp().getnodestore ().fetch (hash);

                if (hobj)
                {
                    protocol::tmindexedobject& newobj = *reply.add_objects ();
                    newobj.set_hash (hash.begin (), hash.size ());
                    newobj.set_data (&hobj->getdata ().front (),
                        hobj->getdata ().size ());

                    if (obj.has_nodeid ())
                        newobj.set_index (obj.nodeid ());

                    // vfalco note "seq" in the message is obsolete
                }
            }
        }

        p_journal_.trace <<
            "getobj: " << reply.objects_size () <<
                " of " << packet.objects_size ();
        send (std::make_shared<message> (reply, protocol::mtget_objects));
    }
    else
    {
        // this is a reply
        std::uint32_t plseq = 0;
        bool pldo = true;
        bool progress = false;

        for (int i = 0; i < packet.objects_size (); ++i)
        {
            const protocol::tmindexedobject& obj = packet.objects (i);

            if (obj.has_hash () && (obj.hash ().size () == (256 / 8)))
            {
                if (obj.has_ledgerseq ())
                {
                    if (obj.ledgerseq () != plseq)
                    {
                        if ((pldo && (plseq != 0)) &&
                            p_journal_.active(beast::journal::severity::kdebug))
                            p_journal_.debug <<
                                "getobj: full fetch pack for " << plseq;

                        plseq = obj.ledgerseq ();
                        pldo = !getapp().getops ().haveledger (plseq);

                        if (!pldo)
                                p_journal_.debug <<
                                    "getobj: late fetch pack for " << plseq;
                        else
                            progress = true;
                    }
                }

                if (pldo)
                {
                    uint256 hash;
                    memcpy (hash.begin (), obj.hash ().data (), 256 / 8);

                    std::shared_ptr< blob > data (
                        std::make_shared< blob > (
                            obj.data ().begin (), obj.data ().end ()));

                    getapp().getops ().addfetchpack (hash, data);
                }
            }
        }

        if ((pldo && (plseq != 0)) &&
               p_journal_.active(beast::journal::severity::kdebug))
            p_journal_.debug << "getobj: partial fetch pack for " << plseq;

        if (packet.type () == protocol::tmgetobjectbyhash::otfetch_pack)
            getapp().getops ().gotfetchpack (progress, plseq);
    }
}

//--------------------------------------------------------------------------

void
peerimp::sendgetpeers ()
{
    // ask peer for known other peers.
    protocol::tmgetpeers msg;

    msg.set_doweneedthis (1);

    message::pointer packet = std::make_shared<message> (
        msg, protocol::mtget_peers);

    send (packet);
}

bool
peerimp::sendhello()
{
    bool success;
    std::tie(sharedvalue_, success) = makesharedvalue(
        stream_.native_handle(), journal_);
    if (! success)
        return false;

    auto const hello = buildhello (sharedvalue_, getapp());
    auto const m = std::make_shared<message> (
        std::move(hello), protocol::mthello);
    send (m);
    return true;
}

void
peerimp::addledger (uint256 const& hash)
{
    std::lock_guard<std::mutex> sl(recentlock_);

    if (std::find (recentledgers_.begin(),
        recentledgers_.end(), hash) != recentledgers_.end())
        return;

    // vfalco todo see if a sorted vector would be better.

    if (recentledgers_.size () == 128)
        recentledgers_.pop_front ();

    recentledgers_.push_back (hash);
}

void
peerimp::addtxset (uint256 const& hash)
{
    std::lock_guard<std::mutex> sl(recentlock_);

    if (std::find (recenttxsets_.begin (),
        recenttxsets_.end (), hash) != recenttxsets_.end ())
        return;

    if (recenttxsets_.size () == 128)
        recenttxsets_.pop_front ();

    recenttxsets_.push_back (hash);
}

void
peerimp::dofetchpack (const std::shared_ptr<protocol::tmgetobjectbyhash>& packet)
{
    // vfalco todo invert this dependency using an observer and shared state object.
    // don't queue fetch pack jobs if we're under load or we already have
    // some queued.
    if (getapp().getfeetrack ().isloadedlocal () ||
        (getapp().getledgermaster().getvalidatedledgerage() > 40) ||
        (getapp().getjobqueue().getjobcount(jtpack) > 10))
    {
        p_journal_.info << "too busy to make fetch pack";
        return;
    }

    if (packet->ledgerhash ().size () != 32)
    {
        p_journal_.warning << "fetchpack hash size malformed";
        charge (resource::feeinvalidrequest);
        return;
    }

    uint256 hash;
    memcpy (hash.begin (), packet->ledgerhash ().data (), 32);

    getapp().getjobqueue ().addjob (jtpack, "makefetchpack",
        std::bind (&networkops::makefetchpack, &getapp().getops (),
            std::placeholders::_1, std::weak_ptr<peerimp> (shared_from_this ()),
                packet, hash, uptimetimer::getinstance ().getelapsedseconds ()));
}

void
peerimp::checktransaction (job&, int flags,
    sttx::pointer stx)
{
    // vfalco todo rewrite to not use exceptions
    try
    {
        // expired?
        if (stx->isfieldpresent(sflastledgersequence) &&
            (stx->getfieldu32 (sflastledgersequence) <
            getapp().getledgermaster().getvalidledgerindex()))
        {
            getapp().gethashrouter().setflag(stx->gettransactionid(), sf_bad);
            return charge (resource::feeunwanteddata);
        }

        auto validate = (flags & sf_siggood) ? validate::no : validate::yes;
        auto tx = std::make_shared<transaction> (stx, validate);

        if (tx->getstatus () == invalid)
        {
            getapp().gethashrouter ().setflag (stx->gettransactionid (), sf_bad);
            return charge (resource::feeinvalidsignature);
        }
        else
        {
            getapp().gethashrouter ().setflag (stx->gettransactionid (), sf_siggood);
        }

        bool const trusted (flags & sf_trusted);
        getapp().getops ().processtransaction (tx, trusted, false, false);
    }
    catch (...)
    {
        getapp().gethashrouter ().setflag (stx->gettransactionid (), sf_bad);
        charge (resource::feeinvalidrequest);
    }
}

// called from our jobqueue
void
peerimp::checkpropose (job& job,
    std::shared_ptr <protocol::tmproposeset> const& packet,
        ledgerproposal::pointer proposal, uint256 consensuslcl)
{
    bool siggood = false;
    bool istrusted = (job.gettype () == jtproposal_t);

    p_journal_.trace <<
        "checking " << (istrusted ? "trusted" : "untrusted") << " proposal";

    assert (packet);
    protocol::tmproposeset& set = *packet;

    uint256 prevledger;

    if (set.has_previousledger ())
    {
        // proposal includes a previous ledger
        p_journal_.trace <<
            "proposal with previous ledger";
        memcpy (prevledger.begin (), set.previousledger ().data (), 256 / 8);

        if (! cluster() && !proposal->checksign (set.signature ()))
        {
            p_journal_.warning <<
                "proposal with previous ledger fails sig check";
            charge (resource::feeinvalidsignature);
            return;
        }
        else
            siggood = true;
    }
    else
    {
        if (consensuslcl.isnonzero () && proposal->checksign (set.signature ()))
        {
            prevledger = consensuslcl;
            siggood = true;
        }
        else
        {
            // could be mismatched prev ledger
            p_journal_.warning <<
                "ledger proposal fails signature check";
            proposal->setsignature (set.signature ());
        }
    }

    if (istrusted)
    {
        getapp().getops ().processtrustedproposal (
            proposal, packet, publickey_, prevledger, siggood);
    }
    else if (siggood && (prevledger == consensuslcl))
    {
        // relay untrusted proposal
        p_journal_.trace <<
            "relaying untrusted proposal";
        std::set<peer::id_t> peers;

        if (getapp().gethashrouter ().swapset (
            proposal->getsuppressionid (), peers, sf_relayed))
        {
            overlay_.foreach (send_if_not (
                std::make_shared<message> (set, protocol::mtpropose_ledger),
                peer_in_set(peers)));
        }
    }
    else
    {
        p_journal_.debug <<
            "not relaying untrusted proposal";
    }
}

void
peerimp::checkvalidation (job&, stvalidation::pointer val,
    bool istrusted, std::shared_ptr<protocol::tmvalidation> const& packet)
{
    try
    {
        // vfalco which functions throw?
        uint256 signinghash = val->getsigninghash();
        if (! cluster() && !val->isvalid (signinghash))
        {
            p_journal_.warning <<
                "validation is invalid";
            charge (resource::feeinvalidrequest);
            return;
        }

    #if ripple_hook_validators
        validatorsconnection_->onvalidation(*val);
    #endif

        std::set<peer::id_t> peers;
        if (getapp().getops ().recvvalidation (val, std::to_string(id())) &&
                getapp().gethashrouter ().swapset (
                    signinghash, peers, sf_relayed))
        {
            overlay_.foreach (send_if_not (
                std::make_shared<message> (*packet, protocol::mtvalidation),
                peer_in_set(peers)));
        }
    }
    catch (...)
    {
        p_journal_.trace <<
            "exception processing validation";
        charge (resource::feeinvalidrequest);
    }
}

// vfalco note this function is way too big and cumbersome.
void
peerimp::getledger (std::shared_ptr<protocol::tmgetledger> const& m)
{
    protocol::tmgetledger& packet = *m;
    shamap::pointer map;
    protocol::tmledgerdata reply;
    bool fatleaves = true, fatroot = false;

    if (packet.has_requestcookie ())
        reply.set_requestcookie (packet.requestcookie ());

    std::string logme;

    if (packet.itype () == protocol::lits_candidate)
    {
        // request is for a transaction candidate set
        p_journal_.trace <<
            "getledger: tx candidate set";

        if ((!packet.has_ledgerhash () || packet.ledgerhash ().size () != 32))
        {
            charge (resource::feeinvalidrequest);
            p_journal_.warning << "getledger: tx candidate set invalid";
            return;
        }

        uint256 txhash;
        memcpy (txhash.begin (), packet.ledgerhash ().data (), 32);

        {
            application::scopedlocktype lock (getapp ().getmasterlock ());
            map = getapp().getops ().gettxmap (txhash);
        }

        if (!map)
        {
            if (packet.has_querytype () && !packet.has_requestcookie ())
            {
                p_journal_.debug <<
                    "getledger: routing tx set request";

                struct get_usable_peers
                {
                    typedef overlay::peersequence return_type;

                    overlay::peersequence usablepeers;
                    uint256 const& txhash;
                    peer const* skip;

                    get_usable_peers(uint256 const& hash, peer const* s)
                        : txhash(hash), skip(s)
                    { }

                    void operator() (peer::ptr const& peer)
                    {
                        if (peer->hastxset (txhash) && (peer.get () != skip))
                            usablepeers.push_back (peer);
                    }

                    return_type operator() ()
                    {
                        return usablepeers;
                    }
                };

                overlay::peersequence usablepeers (overlay_.foreach (
                    get_usable_peers (txhash, this)));

                if (usablepeers.empty ())
                {
                    p_journal_.info <<
                        "getledger: route tx set failed";
                    return;
                }

                peer::ptr const& selectedpeer = usablepeers [
                    rand () % usablepeers.size ()];
                packet.set_requestcookie (id ());
                selectedpeer->send (std::make_shared<message> (
                    packet, protocol::mtget_ledger));
                return;
            }

            p_journal_.error <<
                "getledger: can't provide map ";
            charge (resource::feeinvalidrequest);
            return;
        }

        reply.set_ledgerseq (0);
        reply.set_ledgerhash (txhash.begin (), txhash.size ());
        reply.set_type (protocol::lits_candidate);
        fatleaves = false; // we'll already have most transactions
        fatroot = true; // save a pass
    }
    else
    {
        if (getapp().getfeetrack().isloadedlocal() && ! cluster())
        {
            p_journal_.debug <<
                "getledger: too busy";
            return;
        }

        // figure out what ledger they want
        p_journal_.trace <<
            "getledger: received";
        ledger::pointer ledger;

        if (packet.has_ledgerhash ())
        {
            uint256 ledgerhash;

            if (packet.ledgerhash ().size () != 32)
            {
                charge (resource::feeinvalidrequest);
                p_journal_.warning <<
                    "getledger: invalid request";
                return;
            }

            memcpy (ledgerhash.begin (), packet.ledgerhash ().data (), 32);
            logme += "ledgerhash:";
            logme += to_string (ledgerhash);
            ledger = getapp().getledgermaster ().getledgerbyhash (ledgerhash);

            if (!ledger && p_journal_.trace)
                p_journal_.trace <<
                    "getledger: don't have " << ledgerhash;

            if (!ledger && (packet.has_querytype () &&
                !packet.has_requestcookie ()))
            {
                std::uint32_t seq = 0;

                if (packet.has_ledgerseq ())
                    seq = packet.ledgerseq ();

                overlay::peersequence peerlist = overlay_.getactivepeers ();
                overlay::peersequence usablepeers;
                for (auto const& peer : peerlist)
                {
                    if (peer->hasledger (ledgerhash, seq) && (peer.get () != this))
                        usablepeers.push_back (peer);
                }

                if (usablepeers.empty ())
                {
                    p_journal_.trace <<
                        "getledger: cannot route";
                    return;
                }

                peer::ptr const& selectedpeer = usablepeers [
                    rand () % usablepeers.size ()];
                packet.set_requestcookie (id ());
                selectedpeer->send (
                    std::make_shared<message> (packet, protocol::mtget_ledger));
                p_journal_.debug <<
                    "getledger: request routed";
                return;
            }
        }
        else if (packet.has_ledgerseq ())
        {
            if (packet.ledgerseq() <
                    getapp().getledgermaster().getearliestfetch())
            {
                p_journal_.debug <<
                    "getledger: early ledger request";
                return;
            }
            ledger = getapp().getledgermaster ().getledgerbyseq (
                packet.ledgerseq ());
            if (!ledger && p_journal_.debug)
                p_journal_.debug <<
                    "getledger: don't have " << packet.ledgerseq ();
        }
        else if (packet.has_ltype () && (packet.ltype () == protocol::ltcurrent))
        {
            ledger = getapp().getledgermaster ().getcurrentledger ();
        }
        else if (packet.has_ltype () && (packet.ltype () == protocol::ltclosed) )
        {
            ledger = getapp().getledgermaster ().getclosedledger ();

            if (ledger && !ledger->isclosed ())
                ledger = getapp().getledgermaster ().getledgerbyseq (
                    ledger->getledgerseq () - 1);
        }
        else
        {
            charge (resource::feeinvalidrequest);
            p_journal_.warning <<
                "getledger: unknown request";
            return;
        }

        if ((!ledger) || (packet.has_ledgerseq () && (
            packet.ledgerseq () != ledger->getledgerseq ())))
        {
            charge (resource::feeinvalidrequest);

            if (p_journal_.warning && ledger)
                p_journal_.warning <<
                    "getledger: invalid sequence";

            return;
        }

            if (!packet.has_ledgerseq() && (ledger->getledgerseq() <
                getapp().getledgermaster().getearliestfetch()))
            {
                p_journal_.debug <<
                    "getledger: early ledger request";
                return;
            }

        // fill out the reply
        uint256 lhash = ledger->gethash ();
        reply.set_ledgerhash (lhash.begin (), lhash.size ());
        reply.set_ledgerseq (ledger->getledgerseq ());
        reply.set_type (packet.itype ());

        if (packet.itype () == protocol::libase)
        {
            // they want the ledger base data
            p_journal_.trace <<
                "getledger: base data";
            serializer ndata (128);
            ledger->addraw (ndata);
            reply.add_nodes ()->set_nodedata (
                ndata.getdataptr (), ndata.getlength ());

            shamap::pointer map = ledger->peekaccountstatemap ();

            if (map && map->gethash ().isnonzero ())
            {
                // return account state root node if possible
                serializer rootnode (768);

                if (map->getrootnode (rootnode, snfwire))
                {
                    reply.add_nodes ()->set_nodedata (
                        rootnode.getdataptr (), rootnode.getlength ());

                    if (ledger->gettranshash ().isnonzero ())
                    {
                        map = ledger->peektransactionmap ();

                        if (map && map->gethash ().isnonzero ())
                        {
                            rootnode.erase ();

                            if (map->getrootnode (rootnode, snfwire))
                                reply.add_nodes ()->set_nodedata (
                                    rootnode.getdataptr (),
                                        rootnode.getlength ());
                        }
                    }
                }
            }

            message::pointer opacket = std::make_shared<message> (
                reply, protocol::mtledger_data);
            send (opacket);
            return;
        }

        if (packet.itype () == protocol::litx_node)
        {
            map = ledger->peektransactionmap ();
            logme += " tx:";
            logme += to_string (map->gethash ());
        }
        else if (packet.itype () == protocol::lias_node)
        {
            map = ledger->peekaccountstatemap ();
            logme += " as:";
            logme += to_string (map->gethash ());
        }
    }

    if (!map || (packet.nodeids_size () == 0))
    {
        p_journal_.warning <<
            "getledger: can't find map or empty request";
        charge (resource::feeinvalidrequest);
        return;
    }

    p_journal_.trace <<
        "getleder: " << logme;

    for (int i = 0; i < packet.nodeids ().size (); ++i)
    {
        shamapnodeid mn (packet.nodeids (i).data (), packet.nodeids (i).size ());

        if (!mn.isvalid ())
        {
            p_journal_.warning <<
                "getledger: invalid node " << logme;
            charge (resource::feeinvalidrequest);
            return;
        }

        std::vector<shamapnodeid> nodeids;
        std::list< blob > rawnodes;

        try
        {
            if (map->getnodefat (mn, nodeids, rawnodes, fatroot, fatleaves))
            {
                assert (nodeids.size () == rawnodes.size ());
                p_journal_.trace <<
                    "getledger: getnodefat got " << rawnodes.size () << " nodes";
                std::vector<shamapnodeid>::iterator nodeiditerator;
                std::list< blob >::iterator rawnodeiterator;

                for (nodeiditerator = nodeids.begin (),
                        rawnodeiterator = rawnodes.begin ();
                            nodeiditerator != nodeids.end ();
                                ++nodeiditerator, ++rawnodeiterator)
                {
                    serializer nid (33);
                    nodeiditerator->addidraw (nid);
                    protocol::tmledgernode* node = reply.add_nodes ();
                    node->set_nodeid (nid.getdataptr (), nid.getlength ());
                    node->set_nodedata (&rawnodeiterator->front (),
                        rawnodeiterator->size ());
                }
            }
            else
                p_journal_.warning <<
                    "getledger: getnodefat returns false";
        }
        catch (std::exception&)
        {
            std::string info;

            if (packet.itype () == protocol::lits_candidate)
                info = "ts candidate";
            else if (packet.itype () == protocol::libase)
                info = "ledger base";
            else if (packet.itype () == protocol::litx_node)
                info = "tx node";
            else if (packet.itype () == protocol::lias_node)
                info = "as node";

            if (!packet.has_ledgerhash ())
                info += ", no hash specified";

            p_journal_.warning <<
                "getnodefat( " << mn << ") throws exception: " << info;
        }
    }

    message::pointer opacket = std::make_shared<message> (
        reply, protocol::mtledger_data);
    send (opacket);
}

// vfalco todo make this non-static
void
peerimp::peertxdata (job&, uint256 const& hash,
    std::shared_ptr <protocol::tmledgerdata> const& ppacket,
        beast::journal journal)
{
    protocol::tmledgerdata& packet = *ppacket;

    std::list<shamapnodeid> nodeids;
    std::list< blob > nodedata;
    for (int i = 0; i < packet.nodes ().size (); ++i)
    {
        const protocol::tmledgernode& node = packet.nodes (i);

        if (!node.has_nodeid () || !node.has_nodedata () || (
            node.nodeid ().size () != 33))
        {
            journal.warning << "ledgerdata request with invalid node id";
            charge (resource::feeinvalidrequest);
            return;
        }

        nodeids.push_back (shamapnodeid {node.nodeid ().data (),
                           static_cast<int>(node.nodeid ().size ())});
        nodedata.push_back (blob (node.nodedata ().begin (),
            node.nodedata ().end ()));
    }

    shamapaddnode san;
    {
        application::scopedlocktype lock (getapp ().getmasterlock ());

        san =  getapp().getops().gottxdata (shared_from_this(),
            hash, nodeids, nodedata);
    }

    if (san.isinvalid ())
        charge (resource::feeunwanteddata);
}

} // ripple
