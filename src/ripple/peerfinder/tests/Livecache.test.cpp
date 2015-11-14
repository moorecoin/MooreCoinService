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
#include <ripple/peerfinder/impl/livecache.h>
#include <beast/unit_test/suite.h>
#include <beast/chrono/manual_clock.h>

namespace ripple {
namespace peerfinder {

class livecache_test : public beast::unit_test::suite
{
public:
    beast::manual_clock <std::chrono::steady_clock> m_clock;

    // add the address as an endpoint
    template <class c>
    void add (std::uint32_t index, std::uint16_t port, c& c)
    {
        endpoint ep;
        ep.hops = 0;
        ep.address = beast::ip::endpoint (
            beast::ip::addressv4 (index), port);
        c.insert (ep);
    }

    void testfetch ()
    {
        livecache <> c (m_clock, beast::journal());

        add (1, 1, c);
        add (2, 1, c);
        add (3, 1, c);
        add (4, 1, c);
        add (4, 2, c);
        add (4, 3, c);
        add (5, 1, c);
        add (6, 1, c);
        add (6, 2, c);
        add (7, 1, c);

        // vfalco todo!

        pass();
    }

    void run ()
    {
        testfetch ();
    }
};

beast_define_testsuite(livecache,peerfinder,ripple);

}
}
