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

#if beast_include_beastconfig
#include "../../beastconfig.h"
#endif

#include <beast/net/ipendpoint.h>
#include <beast/net/detail/parse.h>

namespace beast {
namespace ip {

endpoint::endpoint ()
    : m_port (0)
{
}

endpoint::endpoint (address const& addr, port port)
    : m_addr (addr)
    , m_port (port)
{
}

std::pair <endpoint, bool> endpoint::from_string_checked (std::string const& s)
{
    std::stringstream is (s);
    endpoint endpoint;
    is >> endpoint;
    if (! is.fail() && is.rdbuf()->in_avail() == 0)
        return std::make_pair (endpoint, true);
    return std::make_pair (endpoint (), false);
}

endpoint endpoint::from_string (std::string const& s)
{
    std::pair <endpoint, bool> const result (
        from_string_checked (s));
    if (result.second)
        return result.first;
    return endpoint();
}

// vfalco note this is a hack to support legacy data format
//
endpoint endpoint::from_string_altform (std::string const& s)
{
    // accept the regular form if it parses
    {
        endpoint ep (endpoint::from_string (s));
        if (! is_unspecified (ep))
            return ep;
    }

    // now try the alt form
    std::stringstream is (s);

    addressv4 v4;
    is >> v4;
    if (! is.fail())
    {
        endpoint ep (v4);

        if (is.rdbuf()->in_avail()>0)
        {
            if (! ip::detail::expect_whitespace (is))
                return endpoint();

            while (is.rdbuf()->in_avail()>0)
            {
                char c;
                is.get(c);
                if (!isspace (c))
                {
                    is.unget();
                    break;
                }
            }

            port port;
            is >> port;
            if (is.fail())
                return endpoint();

            return ep.at_port (port);
        }
        else
        {
            // just an address with no port
            return ep;
        }
    }

    // could be v6 here...

    return endpoint();
}

std::string endpoint::to_string () const
{
    std::string s (address ().to_string ());
    if (port() != 0)
        s = s + ":" + std::to_string (port());
    return s;
}

bool operator== (endpoint const& lhs, endpoint const& rhs)
{
    return lhs.address() == rhs.address() &&
           lhs.port() == rhs.port();
}

bool operator<  (endpoint const& lhs, endpoint const& rhs)
{
    if (lhs.address() < rhs.address())
        return true;
    if (lhs.address() > rhs.address())
        return false;
    return lhs.port() < rhs.port();
}

//------------------------------------------------------------------------------

std::istream& operator>> (std::istream& is, endpoint& endpoint)
{
    // vfalco todo support ipv6!

    address addr;
    is >> addr;
    if (is.fail())
        return is;

    if (is.rdbuf()->in_avail()>0)
    {
        char c;
        is.get(c);
        if (c != ':')
        {
            is.unget();
            endpoint = endpoint (addr);
            return is;
        }

        port port;
        is >> port;
        if (is.fail())
            return is;

        endpoint = endpoint (addr, port);
        return is;
    }

    endpoint = endpoint (addr);
    return is;
}

}
}
