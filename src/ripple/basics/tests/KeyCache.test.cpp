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
#include <ripple/basics/keycache.h>
#include <beast/unit_test/suite.h>
#include <beast/chrono/manual_clock.h>

namespace ripple {

class keycache_test : public beast::unit_test::suite
{
public:
    void run ()
    {
        beast::manual_clock <std::chrono::steady_clock> clock;
        clock.set (0);

        typedef std::string key;
        typedef keycache <key> cache;

        // insert an item, retrieve it, and age it so it gets purged.
        {
            cache c ("test", clock, 1, 2);

            expect (c.size () == 0);
            expect (c.insert ("one"));
            expect (! c.insert ("one"));
            expect (c.size () == 1);
            expect (c.exists ("one"));
            expect (c.touch_if_exists ("one"));
            ++clock;
            c.sweep ();
            expect (c.size () == 1);
            expect (c.exists ("one"));
            ++clock;
            c.sweep ();
            expect (c.size () == 0);
            expect (! c.exists ("one"));
            expect (! c.touch_if_exists ("one"));
        }

        // insert two items, have one expire
        {
            cache c ("test", clock, 2, 2);

            expect (c.insert ("one"));
            expect (c.size  () == 1);
            expect (c.insert ("two"));
            expect (c.size  () == 2);
            ++clock;
            c.sweep ();
            expect (c.size () == 2);
            expect (c.touch_if_exists ("two"));
            ++clock;
            c.sweep ();
            expect (c.size () == 1);
            expect (c.exists ("two"));
        }

        // insert three items (1 over limit), sweep
        {
            cache c ("test", clock, 2, 3);

            expect (c.insert ("one"));
            ++clock;
            expect (c.insert ("two"));
            ++clock;
            expect (c.insert ("three"));
            ++clock;
            expect (c.size () == 3);
            c.sweep ();
            expect (c.size () < 3);
        }
    }
};

beast_define_testsuite(keycache,common,ripple);

}
