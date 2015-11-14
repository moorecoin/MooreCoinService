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

#ifndef ripple_websocket_autosocket_h_included
#define ripple_websocket_autosocket_h_included

#include <ripple/basics/log.h>
#include <beast/asio/ipaddressconversion.h>
#include <beast/asio/bind_handler.h>
#include <beast/asio/placeholders.h>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>

// socket wrapper that supports both ssl and non-ssl connections.
// generally, handle it as you would an ssl connection.
// to force a non-ssl connection, just don't call async_handshake.
// to force ssl only inbound, call setsslonly.

class autosocket
{
public:
    typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket>   ssl_socket;
    typedef boost::asio::ip::tcp::socket::endpoint_type endpoint_type;
    typedef std::shared_ptr<ssl_socket>           socket_ptr;
    typedef ssl_socket::next_layer_type             plain_socket;
    typedef ssl_socket::lowest_layer_type           lowest_layer_type;
    typedef ssl_socket::handshake_type              handshake_type;
    typedef boost::system::error_code               error_code;
    typedef std::function <void (error_code)>       callback;

public:
    autosocket (boost::asio::io_service& s, boost::asio::ssl::context& c)
        : msecure (false)
        , mbuffer (4)
    {
        msocket = std::make_shared<ssl_socket> (s, c);
    }

    autosocket (boost::asio::io_service& s, boost::asio::ssl::context& c, bool secureonly, bool plainonly)
        : msecure (secureonly)
        , mbuffer ((plainonly || secureonly) ? 0 : 4)
    {
        msocket = std::make_shared<ssl_socket> (s, c);
    }

    boost::asio::io_service& get_io_service () noexcept
    {
        return msocket->get_io_service ();
    }

    bool            issecure ()
    {
        return msecure;
    }
    ssl_socket&     sslsocket ()
    {
        return *msocket;
    }
    plain_socket&   plainsocket ()
    {
        return msocket->next_layer ();
    }
    void setsslonly ()
    {
        msecure = true;
    }
    void setplainonly ()
    {
        mbuffer.clear ();
    }

    beast::ip::endpoint
    local_endpoint()
    {
        return beast::ip::from_asio(
            lowest_layer().local_endpoint());
    }

    beast::ip::endpoint
    remote_endpoint()
    {
        return beast::ip::from_asio(
            lowest_layer().remote_endpoint());
    }

    lowest_layer_type& lowest_layer ()
    {
        return msocket->lowest_layer ();
    }

    void swap (autosocket& s)
    {
        mbuffer.swap (s.mbuffer);
        msocket.swap (s.msocket);
        std::swap (msecure, s.msecure);
    }

    boost::system::error_code cancel (boost::system::error_code& ec)
    {
        return lowest_layer ().cancel (ec);
    }


    static bool rfc2818_verify (std::string const& domain, bool preverified, boost::asio::ssl::verify_context& ctx)
    {
        using namespace ripple;

        if (boost::asio::ssl::rfc2818_verification (domain) (preverified, ctx))
            return true;

        writelog (lswarning, autosocket) <<
            "outbound ssl connection to " << domain <<
            " fails certificate verification";
        return false;
    }

    boost::system::error_code verify (std::string const& strdomain)
    {
        boost::system::error_code ec;

        msocket->set_verify_mode (boost::asio::ssl::verify_peer);

        // xxx verify semantics of rfc 2818 are what we want.
        msocket->set_verify_callback (std::bind (&rfc2818_verify, strdomain, std::placeholders::_1, std::placeholders::_2), ec);

        return ec;
    }

/*
    template <typename handshakehandler>
    boost_asio_initfn_result_type(handshakehandler, void (boost::system::error_code))
    async_handshake (handshake_type role, boost_asio_move_arg(handshakehandler) handler)
    {
        return async_handshake_cb (role, handler);
    }
*/

    void async_handshake (handshake_type type, callback cbfunc)
    {
        if ((type == ssl_socket::client) || (msecure))
        {
            // must be ssl
            msecure = true;
            msocket->async_handshake (type, cbfunc);
        }
        else if (mbuffer.empty ())
        {
            // must be plain
            msecure = false;
            msocket->get_io_service ().post (
                beast::asio::bind_handler (cbfunc, error_code()));
        }
        else
        {
            // autodetect
            msocket->next_layer ().async_receive (boost::asio::buffer (mbuffer), boost::asio::socket_base::message_peek,
                                                  std::bind (&autosocket::handle_autodetect, this, cbfunc,
                                                          beast::asio::placeholders::error, beast::asio::placeholders::bytes_transferred));
        }
    }

    template <typename shutdownhandler>
    void async_shutdown (shutdownhandler handler)
    {
        if (issecure ())
            msocket->async_shutdown (handler);
        else
        {
            error_code ec;
            try
            {
                lowest_layer ().shutdown (plain_socket::shutdown_both);
            }
            catch (boost::system::system_error& e)
            {
                ec = e.code();
            }
            msocket->get_io_service ().post (
                beast::asio::bind_handler (handler, ec));
        }
    }

    template <typename seq, typename handler>
    void async_read_some (const seq& buffers, handler handler)
    {
        if (issecure ())
            msocket->async_read_some (buffers, handler);
        else
            plainsocket ().async_read_some (buffers, handler);
    }

    template <typename seq, typename condition, typename handler>
    void async_read_until (const seq& buffers, condition condition, handler handler)
    {
        if (issecure ())
            boost::asio::async_read_until (*msocket, buffers, condition, handler);
        else
            boost::asio::async_read_until (plainsocket (), buffers, condition, handler);
    }

    template <typename allocator, typename handler>
    void async_read_until (boost::asio::basic_streambuf<allocator>& buffers, std::string const& delim, handler handler)
    {
        if (issecure ())
            boost::asio::async_read_until (*msocket, buffers, delim, handler);
        else
            boost::asio::async_read_until (plainsocket (), buffers, delim, handler);
    }

    template <typename allocator, typename matchcondition, typename handler>
    void async_read_until (boost::asio::basic_streambuf<allocator>& buffers, matchcondition cond, handler handler)
    {
        if (issecure ())
            boost::asio::async_read_until (*msocket, buffers, cond, handler);
        else
            boost::asio::async_read_until (plainsocket (), buffers, cond, handler);
    }

    template <typename buf, typename handler>
    void async_write (const buf& buffers, handler handler)
    {
        if (issecure ())
            boost::asio::async_write (*msocket, buffers, handler);
        else
            boost::asio::async_write (plainsocket (), buffers, handler);
    }

    template <typename allocator, typename handler>
    void async_write (boost::asio::basic_streambuf<allocator>& buffers, handler handler)
    {
        if (issecure ())
            boost::asio::async_write (*msocket, buffers, handler);
        else
            boost::asio::async_write (plainsocket (), buffers, handler);
    }

    template <typename buf, typename condition, typename handler>
    void async_read (const buf& buffers, condition cond, handler handler)
    {
        if (issecure ())
            boost::asio::async_read (*msocket, buffers, cond, handler);
        else
            boost::asio::async_read (plainsocket (), buffers, cond, handler);
    }

    template <typename allocator, typename condition, typename handler>
    void async_read (boost::asio::basic_streambuf<allocator>& buffers, condition cond, handler handler)
    {
        if (issecure ())
            boost::asio::async_read (*msocket, buffers, cond, handler);
        else
            boost::asio::async_read (plainsocket (), buffers, cond, handler);
    }

    template <typename buf, typename handler>
    void async_read (const buf& buffers, handler handler)
    {
        if (issecure ())
            boost::asio::async_read (*msocket, buffers, handler);
        else
            boost::asio::async_read (plainsocket (), buffers, handler);
    }

    template <typename seq, typename handler>
    void async_write_some (const seq& buffers, handler handler)
    {
        if (issecure ())
            msocket->async_write_some (buffers, handler);
        else
            plainsocket ().async_write_some (buffers, handler);
    }

protected:
    void handle_autodetect (callback cbfunc, const error_code& ec, size_t bytestransferred)
    {
        using namespace ripple;

        if (ec)
        {
            writelog (lswarning, autosocket) <<
                "handle autodetect error: " << ec;
            cbfunc (ec);
        }
        else if ((mbuffer[0] < 127) && (mbuffer[0] > 31) &&
                 ((bytestransferred < 2) || ((mbuffer[1] < 127) && (mbuffer[1] > 31))) &&
                 ((bytestransferred < 3) || ((mbuffer[2] < 127) && (mbuffer[2] > 31))) &&
                 ((bytestransferred < 4) || ((mbuffer[3] < 127) && (mbuffer[3] > 31))))
        {
            // not ssl
            writelog (lstrace, autosocket) << "non-ssl";
            msecure = false;
            cbfunc (ec);
        }
        else
        {
            // ssl
            writelog (lstrace, autosocket) << "ssl";
            msecure = true;
            msocket->async_handshake (ssl_socket::server, cbfunc);
        }
    }

private:
    socket_ptr          msocket;
    bool                msecure;
    std::vector<char>   mbuffer;
};

#endif
