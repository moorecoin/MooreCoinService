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
#include <ripple/shamap/fullbelowcache.h>
#include <ripple/shamap/shamap.h>
#include <ripple/basics/blob.h>
#include <ripple/basics/stringutilities.h>
#include <ripple/nodestore/dummyscheduler.h>
#include <ripple/nodestore/manager.h>
#include <beast/unit_test/suite.h>
#include <beast/utility/journal.h>
#include <beast/chrono/manual_clock.h>

namespace ripple {

class shamap_test : public beast::unit_test::suite
{
public:
    struct handler
    {
        void operator()(std::uint32_t refnum) const
        {
            throw std::runtime_error("missing node");
        }
    };

    static blob inttovuc (int v)
    {
        blob vuc;

        for (int i = 0; i < 32; ++i)
            vuc.push_back (static_cast<unsigned char> (v));

        return vuc;
    }

    void run ()
    {
        testcase ("add/traverse");

        beast::manual_clock <std::chrono::steady_clock> clock;  // manual advance clock
        beast::journal const j;                            // debug journal
        
        fullbelowcache fullbelowcache ("test.full_below", clock);
        treenodecache treenodecache ("test.tree_node_cache", 65536, 60, clock, j);
        nodestore::dummyscheduler scheduler;
        auto db = nodestore::manager::instance().make_database (
            "test", scheduler, j, 0, parsedelimitedkeyvaluestring("type=memory|path=shamap_test"));

        // h3 and h4 differ only in the leaf, same terminal node (level 19)
        uint256 h1, h2, h3, h4, h5;
        h1.sethex ("092891fe4ef6cee585fdc6fda0e09eb4d386363158ec3321b8123e5a772c6ca7");
        h2.sethex ("436ccbac3347baa1f1e53baeef1f43334da88f1f6d70d963b833afd6dfa289fe");
        h3.sethex ("b92891fe4ef6cee585fdc6fda1e09eb4d386363158ec3321b8123e5a772c6ca8");
        h4.sethex ("b92891fe4ef6cee585fdc6fda2e09eb4d386363158ec3321b8123e5a772c6ca8");
        h5.sethex ("a92891fe4ef6cee585fdc6fda0e09eb4d386363158ec3321b8123e5a772c6ca7");

        shamap smap (smtfree, fullbelowcache, treenodecache,
            *db, handler(), beast::journal());
        shamapitem i1 (h1, inttovuc (1)), i2 (h2, inttovuc (2)), i3 (h3, inttovuc (3)), i4 (h4, inttovuc (4)), i5 (h5, inttovuc (5));
        unexpected (!smap.additem (i2, true, false), "no add");
        unexpected (!smap.additem (i1, true, false), "no add");

        shamapitem::pointer i;
        i = smap.peekfirstitem ();
        unexpected (!i || (*i != i1), "bad traverse");
        i = smap.peeknextitem (i->gettag ());
        unexpected (!i || (*i != i2), "bad traverse");
        i = smap.peeknextitem (i->gettag ());
        unexpected (i, "bad traverse");
        smap.additem (i4, true, false);
        smap.delitem (i2.gettag ());
        smap.additem (i3, true, false);
        i = smap.peekfirstitem ();
        unexpected (!i || (*i != i1), "bad traverse");
        i = smap.peeknextitem (i->gettag ());
        unexpected (!i || (*i != i3), "bad traverse");
        i = smap.peeknextitem (i->gettag ());
        unexpected (!i || (*i != i4), "bad traverse");
        i = smap.peeknextitem (i->gettag ());
        unexpected (i, "bad traverse");

        testcase ("snapshot");
        uint256 maphash = smap.gethash ();
        shamap::pointer map2 = smap.snapshot (false);
        unexpected (smap.gethash () != maphash, "bad snapshot");
        unexpected (map2->gethash () != maphash, "bad snapshot");
        unexpected (!smap.delitem (smap.peekfirstitem ()->gettag ()), "bad mod");
        unexpected (smap.gethash () == maphash, "bad snapshot");
        unexpected (map2->gethash () != maphash, "bad snapshot");
    }
};

beast_define_testsuite(shamap,ripple_app,ripple);

} // ripple
