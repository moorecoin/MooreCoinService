//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

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

#ifndef beast_asio_ssl_bundle_h_included
#define beast_asio_ssl_bundle_h_included

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <memory>
#include <utility>

namespace beast {
namespace asio {

/** work-around for non-movable boost::ssl::stream.
    this allows ssl::stream to be movable and allows the stream to
    construct from an already-existing socket.
*/
struct ssl_bundle
{
    using socket_type = boost::asio::ip::tcp::socket;
    using stream_type = boost::asio::ssl::stream <socket_type&>;
    using shared_context = std::shared_ptr<boost::asio::ssl::context>;

    template <class... args>
    ssl_bundle (shared_context const& context_, args&&... args);

    // deprecated
    template <class... args>
    ssl_bundle (boost::asio::ssl::context& context_, args&&... args);

    shared_context context;
    socket_type socket;
    stream_type stream;
};

template <class... args>
ssl_bundle::ssl_bundle (shared_context const& context_, args&&... args)
    : socket(std::forward<args>(args)...)
    , stream (socket, *context_)
{
}

template <class... args>
ssl_bundle::ssl_bundle (boost::asio::ssl::context& context_, args&&... args)
    : socket(std::forward<args>(args)...)
    , stream (socket, context_)
{
}

} // asio
} // beast

#endif
