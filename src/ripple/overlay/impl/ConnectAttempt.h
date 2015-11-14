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

#ifndef ripple_overlay_connectattempt_h_included
#define ripple_overlay_connectattempt_h_included

#include "ripple.pb.h"
#include <ripple/overlay/impl/overlayimpl.h>
#include <ripple/overlay/impl/protocolmessage.h>
#include <ripple/overlay/impl/tmhello.h>
#include <ripple/overlay/impl/tuning.h>
#include <ripple/overlay/message.h>
#include <ripple/app/peers/uniquenodelist.h> // move to .cpp
#include <ripple/protocol/buildinfo.h>
#include <ripple/protocol/uinttypes.h>
#include <beast/asio/placeholders.h>
#include <beast/asio/ssl_bundle.h>
#include <beast/asio/streambuf.h>
#include <beast/http/message.h>
#include <beast/http/parser.h>
#include <beast/asio/ipaddressconversion.h>
#include <beast/utility/wrappedsink.h>
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/buffers_iterator.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <beast/cxx14/memory.h> // <memory>
#include <chrono>
#include <functional>

namespace ripple {

/** manages an outbound connection attempt. */
class connectattempt
    : public overlayimpl::child
    , public std::enable_shared_from_this<connectattempt>
{
private:
    using error_code = boost::system::error_code;
    using endpoint_type = boost::asio::ip::tcp::endpoint;

    std::uint32_t const id_;
    beast::wrappedsink sink_;
    beast::journal journal_;
    endpoint_type remote_endpoint_;
    resource::consumer usage_;
    boost::asio::io_service::strand strand_;
    boost::asio::basic_waitable_timer<std::chrono::steady_clock> timer_;
    std::unique_ptr<beast::asio::ssl_bundle> ssl_bundle_;
    beast::asio::ssl_bundle::socket_type& socket_;
    beast::asio::ssl_bundle::stream_type& stream_;
    beast::asio::streambuf read_buf_;
    beast::asio::streambuf write_buf_;
    beast::http::message response_;
    beast::asio::streambuf body_;
    beast::http::parser parser_;
    peerfinder::slot::ptr slot_;

public:
    connectattempt (boost::asio::io_service& io_service,
        endpoint_type const& remote_endpoint, resource::consumer usage,
            beast::asio::ssl_bundle::shared_context const& context,
                std::uint32_t id, peerfinder::slot::ptr const& slot,
                    beast::journal journal, overlayimpl& overlay);

    ~connectattempt();

    void
    stop() override;

    void
    run();

private:
    void close();
    void fail (std::string const& reason);
    void fail (std::string const& name, error_code ec);
    void settimer();
    void canceltimer();
    void ontimer (error_code ec);
    void onconnect (error_code ec);
    void onhandshake (error_code ec);
    void onwrite (error_code ec, std::size_t bytes_transferred);
    void onread (error_code ec, std::size_t bytes_transferred);
    void onshutdown (error_code ec);

    void dolegacy();
    void onwritehello (error_code ec, std::size_t bytes_transferred);
    void onreadheader (error_code ec, std::size_t bytes_transferred);
    void onreadbody (error_code ec, std::size_t bytes_transferred);

    static
    beast::http::message
    makerequest (bool crawl,
        boost::asio::ip::address const& remote_address);

    template <class streambuf>
    void processresponse (beast::http::message const& m,
        streambuf const& body);

    template <class = void>
    static
    boost::asio::ip::tcp::endpoint
    parse_endpoint (std::string const& s, boost::system::error_code& ec)
    {
        beast::ip::endpoint bep;
        std::istringstream is(s);
        is >> bep;
        if (is.fail())
        {
            ec = boost::system::errc::make_error_code(
                boost::system::errc::invalid_argument);
            return boost::asio::ip::tcp::endpoint{};
        }

        return beast::ipaddressconversion::to_asio_endpoint(bep);
    }
};

}

#endif
