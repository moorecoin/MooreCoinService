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

#ifndef ripple_resource_key_h_included
#define ripple_resource_key_h_included

#include <ripple/resource/impl/kind.h>
#include <beast/net/ipendpoint.h>
#include <cassert>

namespace ripple {
namespace resource {

// the consumer key
struct key
{
    kind kind;
    beast::ip::endpoint address;
    std::string name;

    key () = delete;

    // constructor for inbound and outbound (non-admin) keys
    key (kind k, beast::ip::endpoint const& addr)
        : kind(k)
        , address(addr)
        , name()
    {
        assert(kind != kindadmin);
    }

    // constructor for admin keys
    key (kind k, std::string const& n)
        : kind(k)
        , address()
        , name(n)
    {
        assert(kind == kindadmin);
    }

    struct hasher
    {
        std::size_t operator() (key const& v) const
        {
            switch (v.kind)
            {
            case kindinbound:
            case kindoutbound:
                return m_addr_hash (v.address);

            case kindadmin:
                return m_name_hash (v.name);

            default:
                assert(false);
            };

            return 0;
        }

    private:
        beast::uhash <> m_addr_hash;
        beast::uhash <> m_name_hash;
    };

    struct key_equal
    {
        bool operator() (key const& lhs, key const& rhs) const
        {
            if (lhs.kind != rhs.kind)
                return false;

            switch (lhs.kind)
            {
            case kindinbound:
            case kindoutbound:
                return lhs.address == rhs.address;

            case kindadmin:
                return lhs.name == rhs.name;

            default:
                assert(false);
            };

            return false;
        }

    private:
    };
};

}
}

#endif
