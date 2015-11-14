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

#ifndef ripple_server_port_h_included
#define ripple_server_port_h_included

#include <beast/utility/ci_char_traits.h>
#include <boost/asio/ip/address.hpp>
#include <cstdint>
#include <memory>
#include <set>
#include <string>

namespace boost { namespace asio { namespace ssl { class context; } } }

namespace ripple {
namespace http {

/** configuration information for a server listening port. */
struct port
{
    std::string name;
    boost::asio::ip::address ip;
    std::uint16_t port = 0;
    std::set<std::string, beast::ci_less> protocol;
    bool allow_admin = false;
    std::string user;
    std::string password;
    std::string admin_user;
    std::string admin_password;
    std::string ssl_key;
    std::string ssl_cert;
    std::string ssl_chain;
    std::shared_ptr<boost::asio::ssl::context> context;

    // returns `true` if any websocket protocols are specified
    template <class = void>
    bool
    websockets() const;

    // returns `true` if any secure protocols are specified
    template <class = void>
    bool
    secure() const;

    // returns a string containing the list of protocols
    template <class = void>
    std::string
    protocols() const;
};

//------------------------------------------------------------------------------

template <class>
bool
port::websockets() const
{
    return protocol.count("ws") > 0 || protocol.count("wss") > 0;
}

template <class>
bool
port::secure() const
{
    return protocol.count("peer") > 0 ||
        protocol.count("https") > 0 || protocol.count("wss") > 0;
}

template <class>
std::string
port::protocols() const
{
    std::string s;
    for (auto iter = protocol.cbegin();
            iter != protocol.cend(); ++iter)
        s += (iter != protocol.cbegin() ? "," : "") + *iter;
    return s;
}

inline
std::ostream&
operator<< (std::ostream& os, port const& p)
{
    os <<
        "'" << p.name <<
        "' (ip=" << p.ip << ":" << p.port <<
        (p.allow_admin ? ", admin, " : ", ") <<
        p.protocols() << ")";
    return os;
}

} // http
} // ripple

#endif
