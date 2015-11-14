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
#include <ripple/overlay/impl/connectattempt.h>
#include <ripple/overlay/impl/peerimp.h>
#include <ripple/overlay/impl/tuning.h>
#include <ripple/json/json_reader.h>
    
namespace ripple {

connectattempt::connectattempt (boost::asio::io_service& io_service,
    endpoint_type const& remote_endpoint, resource::consumer usage,
        beast::asio::ssl_bundle::shared_context const& context,
            std::uint32_t id, peerfinder::slot::ptr const& slot,
                beast::journal journal, overlayimpl& overlay)
    : child (overlay)
    , id_ (id)
    , sink_ (journal, overlayimpl::makeprefix(id))
    , journal_ (sink_)
    , remote_endpoint_ (remote_endpoint)
    , usage_ (usage)
    , strand_ (io_service)
    , timer_ (io_service)
    , ssl_bundle_ (std::make_unique<beast::asio::ssl_bundle>(
        context, io_service))
    , socket_ (ssl_bundle_->socket)
    , stream_ (ssl_bundle_->stream)
    , parser_ (
        [&](void const* data, std::size_t size)
        {
            body_.commit(boost::asio::buffer_copy(body_.prepare(size),
                boost::asio::buffer(data, size)));
        }
        , response_, false)
    , slot_ (slot)
{
    if (journal_.debug) journal_.debug <<
        "connect " << remote_endpoint;
}

connectattempt::~connectattempt()
{
    if (slot_ != nullptr)
        overlay_.peerfinder().on_closed(slot_);
    if (journal_.trace) journal_.trace <<
        "~connectattempt";
}

void
connectattempt::stop()
{
    if (! strand_.running_in_this_thread())
        return strand_.post(std::bind(
            &connectattempt::stop, shared_from_this()));
    if (stream_.next_layer().is_open())
    {
        if (journal_.debug) journal_.debug <<
            "stop";
    }
    close();
}

void
connectattempt::run()
{
    error_code ec;
    stream_.next_layer().async_connect (remote_endpoint_,
        strand_.wrap (std::bind (&connectattempt::onconnect,
            shared_from_this(), beast::asio::placeholders::error)));
}

//------------------------------------------------------------------------------

void
connectattempt::close()
{
    assert(strand_.running_in_this_thread());
    if (stream_.next_layer().is_open())
    {
        error_code ec;
        timer_.cancel(ec);
        socket_.close(ec);
        if (journal_.debug) journal_.debug <<
            "closed";
    }
}

void
connectattempt::fail (std::string const& reason)
{
    assert(strand_.running_in_this_thread());
    if (stream_.next_layer().is_open())
        if (journal_.debug) journal_.debug <<
            reason;
    close();
}

void
connectattempt::fail (std::string const& name, error_code ec)
{
    assert(strand_.running_in_this_thread());
    if (stream_.next_layer().is_open())
        if (journal_.debug) journal_.debug <<
            name << ": " << ec.message();
    close();
}

void
connectattempt::settimer()
{
    error_code ec;
    timer_.expires_from_now(std::chrono::seconds(15), ec);
    if (ec)
    {
        if (journal_.error) journal_.error <<
            "settimer: " << ec.message();
        return;
    }

    timer_.async_wait(strand_.wrap(std::bind(
        &connectattempt::ontimer, shared_from_this(),
            beast::asio::placeholders::error)));
}

void
connectattempt::canceltimer()
{
    error_code ec;
    timer_.cancel(ec);
}

void
connectattempt::ontimer (error_code ec)
{
    if (! stream_.next_layer().is_open())
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
connectattempt::onconnect (error_code ec)
{
    canceltimer();

    if(ec == boost::asio::error::operation_aborted)
        return;
    endpoint_type local_endpoint;
    if(! ec)
        local_endpoint = stream_.next_layer().local_endpoint(ec);
    if(ec)
        return fail("onconnect", ec);
    if(! stream_.next_layer().is_open())
        return;
    if(journal_.trace) journal_.trace <<
        "onconnect";

    settimer();
    stream_.set_verify_mode (boost::asio::ssl::verify_none);
    stream_.async_handshake (boost::asio::ssl::stream_base::client,
        strand_.wrap (std::bind (&connectattempt::onhandshake,
            shared_from_this(), beast::asio::placeholders::error)));
}

void
connectattempt::onhandshake (error_code ec)
{
    canceltimer();
    if(! stream_.next_layer().is_open())
        return;
    if(ec == boost::asio::error::operation_aborted)
        return;
    endpoint_type local_endpoint;
    if (! ec)
        local_endpoint = stream_.next_layer().local_endpoint(ec);
    if(ec)
        return fail("onhandshake", ec);
    if(journal_.trace) journal_.trace <<
        "onhandshake";

    if (! overlay_.peerfinder().onconnected (slot_,
            beast::ipaddressconversion::from_asio (local_endpoint)))
        return fail("duplicate connection");

    bool success;
    uint256 sharedvalue;
    std::tie(sharedvalue, success) = makesharedvalue(
        stream_.native_handle(), journal_);
    if (! success)
        return close(); // makesharedvalue logs

    beast::http::message req = makerequest(
        ! overlay_.peerfinder().config().peerprivate,
            remote_endpoint_.address());
    auto const hello = buildhello (sharedvalue, getapp());
    appendhello (req, hello);

    using beast::http::write;
    write (write_buf_, req);

    settimer();
    stream_.async_write_some (write_buf_.data(),
        strand_.wrap (std::bind (&connectattempt::onwrite,
            shared_from_this(), beast::asio::placeholders::error,
                beast::asio::placeholders::bytes_transferred)));
}

void
connectattempt::onwrite (error_code ec, std::size_t bytes_transferred)
{
    canceltimer();

    if(! stream_.next_layer().is_open())
        return;
    if(ec == boost::asio::error::operation_aborted)
        return;
    if(ec)
        return fail("onwrite", ec);
    if(journal_.trace) journal_.trace <<
        "onwrite: " << bytes_transferred << " bytes";

    write_buf_.consume (bytes_transferred);
    if (write_buf_.size() == 0)
        return onread (error_code(), 0);

    settimer();
    stream_.async_write_some (write_buf_.data(),
        strand_.wrap (std::bind (&connectattempt::onwrite,
            shared_from_this(), beast::asio::placeholders::error,
                beast::asio::placeholders::bytes_transferred)));
}

void
connectattempt::onread (error_code ec, std::size_t bytes_transferred)
{
    canceltimer();

    if(! stream_.next_layer().is_open())
        return;
    if(ec == boost::asio::error::operation_aborted)
        return;
    if(ec == boost::asio::error::eof)
    {
        if(journal_.info) journal_.info <<
            "eof";
        settimer();
        return stream_.async_shutdown(strand_.wrap(std::bind(
            &connectattempt::onshutdown, shared_from_this(),
                beast::asio::placeholders::error)));
    }
    if(ec)
        return fail("onread", ec);
    if(journal_.trace)
    {
        if(bytes_transferred > 0) journal_.trace <<
            "onread: " << bytes_transferred << " bytes";
        else journal_.trace <<
            "onread";
    }

    if (! ec)
    {
        write_buf_.commit (bytes_transferred);
        std::size_t bytes_consumed;
        std::tie (ec, bytes_consumed) = parser_.write(
            write_buf_.data());
        if (! ec)
        {
            write_buf_.consume (bytes_consumed);
            if (parser_.complete())
                return processresponse(response_, body_);
        }
    }

    if (ec)
        return fail("onread", ec);

    settimer();
    stream_.async_read_some (write_buf_.prepare (tuning::readbufferbytes),
        strand_.wrap (std::bind (&connectattempt::onread,
            shared_from_this(), beast::asio::placeholders::error,
                beast::asio::placeholders::bytes_transferred)));
}

void
connectattempt::onshutdown (error_code ec)
{
    canceltimer();
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

//--------------------------------------------------------------------------

// perform a legacy outgoing connection
void
connectattempt::dolegacy()
{
    if(journal_.trace) journal_.trace <<
        "dolegacy";

    bool success;
    uint256 sharedvalue;
    std::tie(sharedvalue, success) = makesharedvalue(
        stream_.native_handle(), journal_);
    if (! success)
        return fail("hello");

    auto const hello = buildhello(sharedvalue, getapp());
    write (write_buf_, hello, protocol::mthello,
        tuning::readbufferbytes);

    stream_.async_write_some (write_buf_.data(),
        strand_.wrap (std::bind (&connectattempt::onwritehello,
            shared_from_this(), beast::asio::placeholders::error,
                beast::asio::placeholders::bytes_transferred)));

    // timer gets reset after header and body received        
    settimer();
    boost::asio::async_read (stream_, read_buf_.prepare (
        message::kheaderbytes), boost::asio::transfer_exactly (
            message::kheaderbytes), strand_.wrap (std::bind (
                &connectattempt::onreadheader, shared_from_this(),
                    beast::asio::placeholders::error,
                        beast::asio::placeholders::bytes_transferred)));
}

void
connectattempt::onwritehello (error_code ec, std::size_t bytes_transferred)
{
    if(! stream_.next_layer().is_open())
        return;
    if(ec == boost::asio::error::operation_aborted)
        return;
    if(ec)
        return fail("onwritehello", ec);
    if(journal_.trace)
    {
        if(bytes_transferred > 0) journal_.trace <<
            "onwritehello: " << bytes_transferred << " bytes";
        else journal_.trace <<
            "onwritehello";
    }

    write_buf_.consume (bytes_transferred);
    if (write_buf_.size() > 0)
        return stream_.async_write_some (write_buf_.data(),
            strand_.wrap (std::bind (&connectattempt::onwritehello,
                shared_from_this(), beast::asio::placeholders::error,
                    beast::asio::placeholders::bytes_transferred)));
}

void
connectattempt::onreadheader (error_code ec,
    std::size_t bytes_transferred)
{
    if(! stream_.next_layer().is_open())
        return;
    if(ec == boost::asio::error::operation_aborted)
        return;
    if(ec == boost::asio::error::eof)
    {
        if(journal_.info) journal_.info <<
            "eof";
        settimer();
        return stream_.async_shutdown(strand_.wrap(std::bind(
            &connectattempt::onshutdown, shared_from_this(),
                beast::asio::placeholders::error)));
    }
    if(ec)
        return fail("onreadheader", ec);
    if(journal_.trace)
    {
        if(bytes_transferred > 0) journal_.trace <<
            "onreadheader: " << bytes_transferred << " bytes";
        else journal_.trace <<
            "onreadheader";
    }

    assert(bytes_transferred == message::kheaderbytes);
    read_buf_.commit(bytes_transferred);

    int const type = message::type(read_buf_.data());
    if (type != protocol::mthello)
        return fail("expected tmhello");

    std::size_t const bytes_needed =
        message::size(read_buf_.data());

    read_buf_.consume (message::kheaderbytes);

    boost::asio::async_read (stream_, read_buf_.prepare(bytes_needed),
        boost::asio::transfer_exactly(bytes_needed), strand_.wrap (
            std::bind (&connectattempt::onreadbody, shared_from_this(),
                beast::asio::placeholders::error,
                    beast::asio::placeholders::bytes_transferred)));
}

void
connectattempt::onreadbody (error_code ec,
    std::size_t bytes_transferred)
{
    canceltimer();

    if(! stream_.next_layer().is_open())
        return;
    if(ec == boost::asio::error::operation_aborted)
        return;
    if(ec == boost::asio::error::eof)
    {
        if(journal_.info) journal_.info <<
            "eof";
        settimer();
        return stream_.async_shutdown(strand_.wrap(std::bind(
            &connectattempt::onshutdown, shared_from_this(),
                beast::asio::placeholders::error)));
    }
    if(ec)
        return fail("onreadbody", ec);
    if(journal_.trace)
    {
        if(bytes_transferred > 0) journal_.trace <<
            "onreadbody: " << bytes_transferred << " bytes";
        else journal_.trace <<
            "onreadbody";
    }

    read_buf_.commit (bytes_transferred);

    protocol::tmhello hello;
    zerocopyinputstream<
        beast::asio::streambuf::const_buffers_type> stream (
            read_buf_.data());
    if (! hello.parsefromzerocopystream (&stream))
        return fail("onreadbody: parse");
    read_buf_.consume (stream.bytecount());

    bool success;
    uint256 sharedvalue;
    std::tie(sharedvalue, success) = makesharedvalue(
        ssl_bundle_->stream.native_handle(), journal_);
    if(! success)
        return close(); // makesharedvalue logs

    rippleaddress publickey;
    std::tie(publickey, success) = verifyhello (hello,
        sharedvalue, journal_, getapp());
    if(! success)
        return close(); // verifyhello logs

    auto protocol = buildinfo::make_protocol(hello.protoversion());
    if(journal_.info) journal_.info <<
        "protocol: " << to_string(protocol);
    if(journal_.info) journal_.info <<
        "public key: " << publickey.humannodepublic();
    std::string name;
    bool const cluster = getapp().getunl().nodeincluster(publickey, name);
    if (cluster)
        if (journal_.info) journal_.info <<
            "cluster name: " << name;

    auto const result = overlay_.peerfinder().activate (
        slot_, publickey.topublickey(), cluster);
    if (result != peerfinder::result::success)
        return fail("outbound slots full");

    auto const peer = std::make_shared<peerimp>(
        std::move(ssl_bundle_), read_buf_.data(),
            std::move(slot_), usage_, std::move(hello),
                publickey, id_, overlay_);

    overlay_.add_active (peer);
}

//--------------------------------------------------------------------------

beast::http::message
connectattempt::makerequest (bool crawl,
    boost::asio::ip::address const& remote_address)
{
    beast::http::message m;
    m.method (beast::http::method_t::http_get);
    m.url ("/");
    m.version (1, 1);
    m.headers.append ("user-agent", buildinfo::getfullversionstring());
    m.headers.append ("upgrade", "rtxp/1.2");
        //std::string("rtxp/") + to_string (buildinfo::getcurrentprotocol()));
    m.headers.append ("connection", "upgrade");
    m.headers.append ("connect-as", "peer");
    m.headers.append ("crawl", crawl ? "public" : "private");
    return m;
}

template <class streambuf>
void
connectattempt::processresponse (beast::http::message const& m,
    streambuf const& body)
{
    if (response_.status() == 503)
    {
        json::value json;
        json::reader r;
        auto const success = r.parse(to_string(body), json);
        if (success)
        {
            if (json.isobject() && json.ismember("peer-ips"))
            {
                json::value const& ips = json["peer-ips"];
                if (ips.isarray())
                {
                    std::vector<boost::asio::ip::tcp::endpoint> eps;
                    eps.reserve(ips.size());
                    for (auto const& v : ips)
                    {
                        if (v.isstring())
                        {
                            error_code ec;
                            auto const ep = parse_endpoint(v.asstring(), ec);
                            if (!ec)
                                eps.push_back(ep);
                        }
                    }
                    overlay_.peerfinder().onredirects(
                        remote_endpoint_, eps);
                }
            }
        }
    }

    if (! overlayimpl::ispeerupgrade(m))
    {
        if (journal_.info) journal_.info <<
            "http response: " << m.status() << " " << m.reason();
        return close();
    }

    bool success;
    protocol::tmhello hello;
    std::tie(hello, success) = parsehello (response_, journal_);
    if(! success)
        return fail("processresponse: bad tmhello");

    uint256 sharedvalue;
    std::tie(sharedvalue, success) = makesharedvalue(
        ssl_bundle_->stream.native_handle(), journal_);
    if(! success)
        return close(); // makesharedvalue logs

    rippleaddress publickey;
    std::tie(publickey, success) = verifyhello (hello,
        sharedvalue, journal_, getapp());
    if(! success)
        return close(); // verifyhello logs
    if(journal_.info) journal_.info <<
        "public key: " << publickey.humannodepublic();

    auto const protocol =
        buildinfo::make_protocol(hello.protoversion());
    if(journal_.info) journal_.info <<
        "protocol: " << to_string(protocol);

    std::string name;
    bool const clusternode =
        getapp().getunl().nodeincluster(publickey, name);
    if (clusternode)
        if (journal_.info) journal_.info <<
            "cluster name: " << name;

    auto const result = overlay_.peerfinder().activate (slot_,
        publickey.topublickey(), clusternode);
    if (result != peerfinder::result::success)
        return fail("outbound slots full");

    auto const peer = std::make_shared<peerimp>(
        std::move(ssl_bundle_), read_buf_.data(),
            std::move(slot_), usage_, std::move(hello),
                publickey, id_, overlay_);

    overlay_.add_active (peer);
}

} // ripple
