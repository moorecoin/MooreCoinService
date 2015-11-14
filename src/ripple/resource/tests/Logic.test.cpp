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
#include <beast/unit_test/suite.h>
#include <beast/chrono/chrono_io.h>
#include <beast/chrono/manual_clock.h>
#include <beast/module/core/maths/random.h>
#include <boost/utility/base_from_member.hpp>

namespace ripple {
namespace resource {

class manager_test : public beast::unit_test::suite
{
public:
    class testlogic
        : private boost::base_from_member <
            beast::manual_clock <std::chrono::steady_clock>>
        , public logic

    {
    private:
        typedef boost::base_from_member <
            beast::manual_clock <std::chrono::steady_clock>> clock_type;

    public:
        explicit testlogic (beast::journal journal)
            : logic (beast::insight::nullcollector::new(), member, journal)
        {
        }

        void advance ()
        {
            ++member;
        }

        beast::manual_clock <std::chrono::steady_clock>& clock ()
        {
            return member;
        }
    };

    //--------------------------------------------------------------------------

    void creategossip (gossip& gossip)
    {
        beast::random r;
        int const v (10 + r.nextint (10));
        int const n (10 + r.nextint (10));
        gossip.items.reserve (n);
        for (int i = 0; i < n; ++i)
        {
            gossip::item item;
            item.balance = 100 + r.nextint (500);
            item.address = beast::ip::endpoint (
                beast::ip::addressv4 (207, 127, 82, v + i));
            gossip.items.push_back (item);
        }
    }

    //--------------------------------------------------------------------------

    void testdrop (beast::journal j)
    {
        testcase ("warn/drop");

        testlogic logic (j);

        charge const fee (dropthreshold + 1);
        beast::ip::endpoint const addr (
            beast::ip::endpoint::from_string ("207.127.82.2"));

        {
            consumer c (logic.newinboundendpoint (addr));

            // create load until we get a warning
            int n = 10000;

            while (--n >= 0)
            {
                if (n == 0)
                {
                    fail ("loop count exceeded without warning");
                    return;
                }

                if (c.charge (fee) == warn)
                {
                    pass ();
                    break;
                }
                ++logic.clock ();
            }

            // create load until we get dropped
            while (--n >= 0)
            {
                if (n == 0)
                {
                    fail ("loop count exceeded without dropping");
                    return;
                }

                if (c.charge (fee) == drop)
                {
                    // disconnect abusive consumer
                    expect (c.disconnect ());
                    break;
                }
                ++logic.clock ();
            }
        }

        // make sure the consumer is on the blacklist for a while.
        {
            consumer c (logic.newinboundendpoint (addr));
            logic.periodicactivity();
            if (c.disposition () != drop)
            {
                fail ("dropped consumer not put on blacklist");
                return;
            }
        }

        // makes sure the consumer is eventually removed from blacklist
        bool readmitted = false;
        {
            // give consumer time to become readmitted.  should never
            // exceed expiration time.
            std::size_t n (secondsuntilexpiration + 1);
            while (--n > 0)
            {
                ++logic.clock ();
                logic.periodicactivity();
                consumer c (logic.newinboundendpoint (addr));
                if (c.disposition () != drop)
                {
                    readmitted = true;
                    break;
                }
            }
        }
        if (readmitted == false)
        {
            fail ("dropped consumer left on blacklist too long");
            return;
        }
        pass();
    }

    void testimports (beast::journal j)
    {
        testcase ("imports");

        testlogic logic (j);

        gossip g[5];

        for (int i = 0; i < 5; ++i)
            creategossip (g[i]);

        for (int i = 0; i < 5; ++i)
            logic.importconsumers (std::to_string (i), g[i]);

        pass();
    }

    void testimport (beast::journal j)
    {
        testcase ("import");

        testlogic logic (j);

        gossip g;
        gossip::item item;
        item.balance = 100;
        item.address = beast::ip::endpoint (
            beast::ip::addressv4 (207, 127, 82, 1));
        g.items.push_back (item);

        logic.importconsumers ("g", g);

        pass();
    }

    void testcharges (beast::journal j)
    {
        testcase ("charge");

        testlogic logic (j);

        {
            beast::ip::endpoint address (beast::ip::endpoint::from_string ("207.127.82.1"));
            consumer c (logic.newinboundendpoint (address));
            charge fee (1000);
            j.info <<
                "charging " << c.to_string() << " " << fee << " per second";
            c.charge (fee);
            for (int i = 0; i < 128; ++i)
            {
                j.info <<
                    "time= " << logic.clock().now().time_since_epoch() <<
                    ", balance = " << c.balance();
                logic.advance();
            }
        }

        {
            beast::ip::endpoint address (beast::ip::endpoint::from_string ("207.127.82.2"));
            consumer c (logic.newinboundendpoint (address));
            charge fee (1000);
            j.info <<
                "charging " << c.to_string() << " " << fee << " per second";
            for (int i = 0; i < 128; ++i)
            {
                c.charge (fee);
                j.info <<
                    "time= " << logic.clock().now().time_since_epoch() <<
                    ", balance = " << c.balance();
                logic.advance();
            }
        }

        pass();
    }

    void run()
    {
        beast::journal j;

        testdrop (j);
        testcharges (j);
        testimports (j);
        testimport (j);
    }
};

beast_define_testsuite(manager,resource,ripple);

}
}
