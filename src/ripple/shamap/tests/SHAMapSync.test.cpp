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
#include <ripple/shamap/shamap.h>
#include <ripple/shamap/shamapitem.h>
#include <ripple/basics/stringutilities.h>
#include <ripple/nodestore/database.h>
#include <ripple/nodestore/dummyscheduler.h>
#include <ripple/nodestore/manager.h>
#include <ripple/protocol/uint160.h>
#include <beast/chrono/manual_clock.h>
#include <beast/unit_test/suite.h>
#include <openssl/rand.h> // deprecated

namespace ripple {

#ifdef beast_debug
//#define sms_debug
#endif

class shamapsync_test : public beast::unit_test::suite
{
public:
    struct handler
    {
        void operator()(std::uint32_t refnum) const
        {
            throw std::runtime_error("missing node");
        }
    };

    static shamapitem::pointer makerandomas ()
    {
        serializer s;

        for (int d = 0; d < 3; ++d) s.add32 (rand ());

        return std::make_shared<shamapitem> (to256 (s.getripemd160 ()), s.peekdata ());
    }

    bool confusemap (shamap& map, int count)
    {
        // add a bunch of random states to a map, then remove them
        // map should be the same
        uint256 beforehash = map.gethash ();

        std::list<uint256> items;

        for (int i = 0; i < count; ++i)
        {
            shamapitem::pointer item = makerandomas ();
            items.push_back (item->gettag ());

            if (!map.additem (*item, false, false))
            {
                log <<
                    "unable to add item to map";
                assert (false);
                return false;
            }
        }

        for (std::list<uint256>::iterator it = items.begin (); it != items.end (); ++it)
        {
            if (!map.delitem (*it))
            {
                log <<
                    "unable to remove item from map";
                assert (false);
                return false;
            }
        }

        if (beforehash != map.gethash ())
        {
            log <<
                "hashes do not match " << beforehash << " " << map.gethash ();
            assert (false);
            return false;
        }

        return true;
    }

    void run ()
    {
        unsigned int seed;

        // vfalco deprecated should use c++11
        rand_pseudo_bytes (reinterpret_cast<unsigned char*> (&seed), sizeof (seed));
        srand (seed);

        beast::manual_clock <std::chrono::steady_clock> clock;  // manual advance clock
        beast::journal const j;                            // debug journal

        fullbelowcache fullbelowcache ("test.full_below", clock);
        treenodecache treenodecache ("test.tree_node_cache", 65536, 60, clock, j);
        nodestore::dummyscheduler scheduler;
        auto db = nodestore::manager::instance().make_database (
            "test", scheduler, j, 1, parsedelimitedkeyvaluestring("type=memory|path=shamapsync_test"));

        shamap source (smtfree, fullbelowcache, treenodecache,
            *db, handler(), beast::journal());
        shamap destination (smtfree, fullbelowcache, treenodecache,
            *db, handler(), beast::journal());

        int items = 10000;
        for (int i = 0; i < items; ++i)
            source.additem (*makerandomas (), false, false);

        unexpected (!confusemap (source, 500), "confusemap");

        source.setimmutable ();

        std::vector<shamapnodeid> nodeids, gotnodeids;
        std::list< blob > gotnodes;
        std::vector<uint256> hashes;

        std::vector<shamapnodeid>::iterator nodeiditerator;
        std::list< blob >::iterator rawnodeiterator;

        int passes = 0;
        int nodes = 0;

        destination.setsynching ();

        unexpected (!source.getnodefat (shamapnodeid (), nodeids, gotnodes, (rand () % 2) == 0, (rand () % 2) == 0),
            "getnodefat");

        unexpected (gotnodes.size () < 1, "nodesize");

        unexpected (!destination.addrootnode (*gotnodes.begin (), snfwire, nullptr).isgood(), "addrootnode");

        nodeids.clear ();
        gotnodes.clear ();

#ifdef sms_debug
        int bytes = 0;
#endif

        do
        {
            ++clock;
            ++passes;
            hashes.clear ();

            // get the list of nodes we know we need
            destination.getmissingnodes (nodeids, hashes, 2048, nullptr);

            if (nodeids.empty ()) break;

            // get as many nodes as possible based on this information
            for (nodeiditerator = nodeids.begin (); nodeiditerator != nodeids.end (); ++nodeiditerator)
            {
                if (!source.getnodefat (*nodeiditerator, gotnodeids, gotnodes, (rand () % 2) == 0, (rand () % 2) == 0))
                {
                    fail ("getnodefat");
                }
                else
                {
                    pass ();
                }
            }

            assert (gotnodeids.size () == gotnodes.size ());
            nodeids.clear ();
            hashes.clear ();

            if (gotnodeids.empty ())
            {
                fail ("got node id");
            }
            else
            {
                pass ();
            }

            for (nodeiditerator = gotnodeids.begin (), rawnodeiterator = gotnodes.begin ();
                    nodeiditerator != gotnodeids.end (); ++nodeiditerator, ++rawnodeiterator)
            {
                ++nodes;
#ifdef sms_debug
                bytes += rawnodeiterator->size ();
#endif

                if (!destination.addknownnode (*nodeiditerator, *rawnodeiterator, nullptr).isgood ())
                {
                    fail ("addknownnode");
                }
                else
                {
                    pass ();
                }
            }

            gotnodeids.clear ();
            gotnodes.clear ();
        }
        while (true);

        destination.clearsynching ();

#ifdef sms_debug
        log << "synching complete " << items << " items, " << nodes << " nodes, " <<
                                  bytes / 1024 << " kb";
#endif

        if (!source.deepcompare (destination))
        {
            fail ("deep compare");
        }
        else
        {
            pass ();
        }

#ifdef sms_debug
        log << "shamapsync test passed: " << items << " items, " <<
            passes << " passes, " << nodes << " nodes";
#endif
    }
};

beast_define_testsuite(shamapsync,ripple_app,ripple);

} // ripple
