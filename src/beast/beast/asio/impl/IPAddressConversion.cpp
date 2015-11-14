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

#include <beast/asio/ipaddressconversion.h>

namespace beast {
namespace ip {

endpoint from_asio (boost::asio::ip::address const& address)
{
    if (address.is_v4 ())
    {
        boost::asio::ip::address_v4::bytes_type const bytes (
            address.to_v4().to_bytes());
        return endpoint (addressv4 (
            bytes [0], bytes [1], bytes [2], bytes [3]));
    }

    // vfalco todo ipv6 support
    assert(false);
    return endpoint();
}

endpoint from_asio (boost::asio::ip::tcp::endpoint const& endpoint)
{
    return from_asio (endpoint.address()).at_port (endpoint.port());
}

boost::asio::ip::address to_asio_address (endpoint const& endpoint)
{
    if (endpoint.address().is_v4())
    {
        return boost::asio::ip::address (
            boost::asio::ip::address_v4 (
                endpoint.address().to_v4().value));
    }

    // vfalco todo ipv6 support
    assert(false);
    return boost::asio::ip::address (
        boost::asio::ip::address_v6 ());
}

boost::asio::ip::tcp::endpoint to_asio_endpoint (endpoint const& endpoint)
{
    return boost::asio::ip::tcp::endpoint (
        to_asio_address (endpoint), endpoint.port());
}

}
}
