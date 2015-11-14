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

#ifndef beast_asio_ipaddressconversion_h_included
#define beast_asio_ipaddressconversion_h_included

#include <beast/net/ipendpoint.h>

#include <sstream>

#include <boost/asio.hpp>

namespace beast {
namespace ip {

/** convert to endpoint.
    the port is set to zero.
*/
endpoint from_asio (boost::asio::ip::address const& address);

/** convert to endpoint. */
endpoint from_asio (boost::asio::ip::tcp::endpoint const& endpoint);

/** convert to asio::ip::address.
    the port is ignored.
*/
boost::asio::ip::address to_asio_address (endpoint const& endpoint);

/** convert to asio::ip::tcp::endpoint. */
boost::asio::ip::tcp::endpoint to_asio_endpoint (endpoint const& endpoint);

}
}

namespace beast {

// deprecated
struct ipaddressconversion
{
    static ip::endpoint from_asio (boost::asio::ip::address const& address)
        { return ip::from_asio (address); }
    static ip::endpoint from_asio (boost::asio::ip::tcp::endpoint const& endpoint)
        { return ip::from_asio (endpoint); }
    static boost::asio::ip::address to_asio_address (ip::endpoint const& address)
        { return ip::to_asio_address (address); }
    static boost::asio::ip::tcp::endpoint to_asio_endpoint (ip::endpoint const& address)
        { return ip::to_asio_endpoint (address); }
};

}

#endif
