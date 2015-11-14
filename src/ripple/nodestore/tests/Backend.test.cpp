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
#include <ripple/nodestore/tests/base.test.h>
#include <ripple/nodestore/dummyscheduler.h>
#include <ripple/nodestore/manager.h>
#include <beast/module/core/diagnostic/unittestutilities.h>

namespace ripple {
namespace nodestore {

// tests the backend interface
//
class backend_test : public testbase
{
public:
    void testbackend (std::string const& type, std::int64_t const seedvalue,
                      int numobjectstotest = 2000)
    {
        dummyscheduler scheduler;

        testcase ("backend type=" + type);

        beast::stringpairarray params;
        beast::unittestutilities::tempdirectory path ("node_db");
        params.set ("type", type);
        params.set ("path", path.getfullpathname ());

        // create a batch
        batch batch;
        createpredictablebatch (batch, numobjectstotest, seedvalue);

        beast::journal j;

        {
            // open the backend
            std::unique_ptr <backend> backend =
                manager::instance().make_backend (params, scheduler, j);

            // write the batch
            storebatch (*backend, batch);

            {
                // read it back in
                batch copy;
                fetchcopyofbatch (*backend, &copy, batch);
                expect (arebatchesequal (batch, copy), "should be equal");
            }

            {
                // reorder and read the copy again
                batch copy;
                beast::unittestutilities::repeatableshuffle (batch.size (), batch, seedvalue);
                fetchcopyofbatch (*backend, &copy, batch);
                expect (arebatchesequal (batch, copy), "should be equal");
            }
        }

        {
            // re-open the backend
            std::unique_ptr <backend> backend = manager::instance().make_backend (
                params, scheduler, j);

            // read it back in
            batch copy;
            fetchcopyofbatch (*backend, &copy, batch);
            // canonicalize the source and destination batches
            std::sort (batch.begin (), batch.end (), nodeobject::lessthan ());
            std::sort (copy.begin (), copy.end (), nodeobject::lessthan ());
            expect (arebatchesequal (batch, copy), "should be equal");
        }
    }

    //--------------------------------------------------------------------------

    void run ()
    {
        int const seedvalue = 50;

        testbackend ("nudb", seedvalue);

        testbackend ("leveldb", seedvalue);

    #if ripple_hyperleveldb_available
        testbackend ("hyperleveldb", seedvalue);
    #endif

    #if ripple_rocksdb_available
        testbackend ("rocksdb", seedvalue);
    #endif

    #ifdef ripple_enable_sqlite_backend_tests
        testbackend ("sqlite", seedvalue);
    #endif
    }
};

beast_define_testsuite(backend,ripple_core,ripple);

}
}
