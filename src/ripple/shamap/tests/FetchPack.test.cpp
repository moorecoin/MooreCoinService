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
#include <ripple/basics/stringutilities.h>
#include <ripple/basics/unorderedcontainers.h>
#include <ripple/nodestore/dummyscheduler.h>
#include <ripple/nodestore/manager.h>
#include <ripple/protocol/uint160.h>
#include <beast/chrono/manual_clock.h>
#include <beast/module/core/maths/random.h>
#include <beast/unit_test/suite.h>
#include <functional>
#include <stdexcept>

namespace ripple {

class fetchpack_test : public beast::unit_test::suite
{
public:
    enum
    {
        tableitems = 100,
        tableitemsextra = 20
    };

    using map = hash_map <uint256, blob> ;
    using table = shamap;
    using item = shamapitem;

    struct handler
    {
        void operator()(std::uint32_t refnum) const
        {
            throw std::runtime_error("missing node");
        }
    };

    struct testfilter : shamapsyncfilter
    {
        testfilter (map& map, beast::journal journal) : mmap (map), mjournal (journal)
        {
        }

        void gotnode (bool fromfilter,
            shamapnodeid const& id, uint256 const& nodehash,
                blob& nodedata, shamaptreenode::tntype type)
        {
        }

        bool havenode (shamapnodeid const& id,
            uint256 const& nodehash, blob& nodedata)
        {
            map::iterator it = mmap.find (nodehash);
            if (it == mmap.end ())
            {
                mjournal.fatal << "test filter missing node";
                return false;
            }
            nodedata = it->second;
            return true;
        }

        map& mmap;
        beast::journal mjournal;
    };

    std::shared_ptr <item>
    make_random_item (beast::random& r)
    {
        serializer s;
        for (int d = 0; d < 3; ++d)
            s.add32 (r.nextint ());
        return std::make_shared <item> (
            to256(s.getripemd160()), s.peekdata ());
    }

    void
    add_random_items (std::size_t n, table& t, beast::random& r)
    {
        while (n--)
        {
            std::shared_ptr <shamapitem> item (
                make_random_item (r));
            auto const result (t.additem (*item, false, false));
            assert (result);
            (void) result;
        }
    }

    void on_fetch (map& map, uint256 const& hash, blob const& blob)
    {
        serializer s (blob);
        expect (s.getsha512half() == hash, "hash mismatch");
        map.emplace (hash, blob);
    }

    void run ()
    {
        beast::manual_clock <std::chrono::steady_clock> clock;  // manual advance clock
        beast::journal const j;                            // debug journal

        fullbelowcache fullbelowcache ("test.full_below", clock);
        treenodecache treenodecache ("test.tree_node_cache", 65536, 60, clock, j);
        nodestore::dummyscheduler scheduler;
        auto db = nodestore::manager::instance().make_database (
            "test", scheduler, j, 0, parsedelimitedkeyvaluestring("type=memory|path=fetchpack"));

        std::shared_ptr <table> t1 (std::make_shared <table> (
            smtfree, fullbelowcache, treenodecache, *db, handler(), beast::journal()));

        pass ();

//         beast::random r;
//         add_random_items (tableitems, *t1, r);
//         std::shared_ptr <table> t2 (t1->snapshot (true));
//
//         add_random_items (tableitemsextra, *t1, r);
//         add_random_items (tableitemsextra, *t2, r);

        // turn t1 into t2
//         map map;
//         t2->getfetchpack (t1.get(), true, 1000000, std::bind (
//             &fetchpack_test::on_fetch, this, std::ref (map), std::placeholders::_1, std::placeholders::_2));
//         t1->getfetchpack (nullptr, true, 1000000, std::bind (
//             &fetchpack_test::on_fetch, this, std::ref (map), std::placeholders::_1, std::placeholders::_2));

        // try to rebuild t2 from the fetch pack
//         std::shared_ptr <table> t3;
//         try
//         {
//             testfilter filter (map, beast::journal());
//
//             t3 = std::make_shared <table> (smtfree, t2->gethash (),
//                 fullbelowcache);
//
//             expect (t3->fetchroot (t2->gethash (), &filter), "unable to get root");
//
//             // everything should be in the pack, no hashes should be needed
//             std::vector <uint256> hashes = t3->getneededhashes(1, &filter);
//             expect (hashes.empty(), "missing hashes");
//
//             expect (t3->gethash () == t2->gethash (), "root hashes do not match");
//             expect (t3->deepcompare (*t2), "failed compare");
//         }
//         catch (...)
//         {
//             fail ("unhandled exception");
//         }
    }
};

beast_define_testsuite(fetchpack,ripple_app,ripple);

}
