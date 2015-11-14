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
#include <ripple/basics/taggedcache.h>
#include <beast/unit_test/suite.h>
#include <beast/chrono/manual_clock.h>

namespace ripple {

/*
i guess you can put some items in, make sure they're still there. let some
time pass, make sure they're gone. keep a strong pointer to one of them, make
sure you can still find it even after time passes. create two objects with
the same key, canonicalize them both and make sure you get the same object.
put an object in but keep a strong pointer to it, advance the clock a lot,
then canonicalize a new object with the same key, make sure you get the
original object.
*/

class taggedcache_test : public beast::unit_test::suite
{
public:
    void run ()
    {
        beast::journal const j;

        beast::manual_clock <std::chrono::steady_clock> clock;
        clock.set (0);

        typedef int key;
        typedef std::string value;
        typedef taggedcache <key, value> cache;

        cache c ("test", 1, 1, clock, j);

        // insert an item, retrieve it, and age it so it gets purged.
        {
            expect (c.getcachesize() == 0);
            expect (c.gettracksize() == 0);
            expect (! c.insert (1, "one"));
            expect (c.getcachesize() == 1);
            expect (c.gettracksize() == 1);

            {
                std::string s;
                expect (c.retrieve (1, s));
                expect (s == "one");
            }

            ++clock;
            c.sweep ();
            expect (c.getcachesize () == 0);
            expect (c.gettracksize () == 0);
        }

        // insert an item, maintain a strong pointer, age it, and
        // verify that the entry still exists.
        {
            expect (! c.insert (2, "two"));
            expect (c.getcachesize() == 1);
            expect (c.gettracksize() == 1);

            {
                cache::mapped_ptr p (c.fetch (2));
                expect (p != nullptr);
                ++clock;
                c.sweep ();
                expect (c.getcachesize() == 0);
                expect (c.gettracksize() == 1);
            }

            // make sure its gone now that our reference is gone
            ++clock;
            c.sweep ();
            expect (c.getcachesize() == 0);
            expect (c.gettracksize() == 0);
        }

        // insert the same key/value pair and make sure we get the same result
        {
            expect (! c.insert (3, "three"));

            {
                cache::mapped_ptr const p1 (c.fetch (3));
                cache::mapped_ptr p2 (std::make_shared <value> ("three"));
                c.canonicalize (3, p2);
                expect (p1.get() == p2.get());
            }
            ++clock;
            c.sweep ();
            expect (c.getcachesize() == 0);
            expect (c.gettracksize() == 0);
        }

        // put an object in but keep a strong pointer to it, advance the clock a lot,
        // then canonicalize a new object with the same key, make sure you get the
        // original object.
        {
            // put an object in
            expect (! c.insert (4, "four"));
            expect (c.getcachesize() == 1);
            expect (c.gettracksize() == 1);

            {
                // keep a strong pointer to it
                cache::mapped_ptr p1 (c.fetch (4));
                expect (p1 != nullptr);
                expect (c.getcachesize() == 1);
                expect (c.gettracksize() == 1);
                // advance the clock a lot
                ++clock;
                c.sweep ();
                expect (c.getcachesize() == 0);
                expect (c.gettracksize() == 1);
                // canonicalize a new object with the same key
                cache::mapped_ptr p2 (std::make_shared <std::string> ("four"));
                expect (c.canonicalize (4, p2, false));
                expect (c.getcachesize() == 1);
                expect (c.gettracksize() == 1);
                // make sure we get the original object
                expect (p1.get() == p2.get());
            }

            ++clock;
            c.sweep ();
            expect (c.getcachesize() == 0);
            expect (c.gettracksize() == 0);
        }
    }
};

beast_define_testsuite(taggedcache,common,ripple);

}
