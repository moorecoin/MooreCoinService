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

class database_test : public testbase
{
public:
    void testimport (std::string const& destbackendtype,
        std::string const& srcbackendtype, std::int64_t seedvalue)
    {
        dummyscheduler scheduler;

        beast::unittestutilities::tempdirectory node_db ("node_db");
        beast::stringpairarray srcparams;
        srcparams.set ("type", srcbackendtype);
        srcparams.set ("path", node_db.getfullpathname ());

        // create a batch
        batch batch;
        createpredictablebatch (batch, numobjectstotest, seedvalue);

        beast::journal j;

        // write to source db
        {
            std::unique_ptr <database> src = manager::instance().make_database (
                "test", scheduler, j, 2, srcparams);
            storebatch (*src, batch);
        }

        batch copy;

        {
            // re-open the db
            std::unique_ptr <database> src = manager::instance().make_database (
                "test", scheduler, j, 2, srcparams);

            // set up the destination database
            beast::unittestutilities::tempdirectory dest_db ("dest_db");
            beast::stringpairarray destparams;
            destparams.set ("type", destbackendtype);
            destparams.set ("path", dest_db.getfullpathname ());

            std::unique_ptr <database> dest = manager::instance().make_database (
                "test", scheduler, j, 2, destparams);

            testcase ("import into '" + destbackendtype +
                "' from '" + srcbackendtype + "'");

            // do the import
            dest->import (*src);

            // get the results of the import
            fetchcopyofbatch (*dest, &copy, batch);
        }

        // canonicalize the source and destination batches
        std::sort (batch.begin (), batch.end (), nodeobject::lessthan ());
        std::sort (copy.begin (), copy.end (), nodeobject::lessthan ());
        expect (arebatchesequal (batch, copy), "should be equal");
    }

    //--------------------------------------------------------------------------

    void testnodestore (std::string const& type,
                        bool const useephemeraldatabase,
                        bool const testpersistence,
                        std::int64_t const seedvalue,
                        int numobjectstotest = 2000)
    {
        dummyscheduler scheduler;

        std::string s = "nodestore backend '" + type + "'";
        if (useephemeraldatabase)
            s += " (with ephemeral database)";

        testcase (s);

        beast::unittestutilities::tempdirectory node_db ("node_db");
        beast::stringpairarray nodeparams;
        nodeparams.set ("type", type);
        nodeparams.set ("path", node_db.getfullpathname ());

        beast::unittestutilities::tempdirectory temp_db ("temp_db");
        beast::stringpairarray tempparams;
        if (useephemeraldatabase)
        {
            tempparams.set ("type", type);
            tempparams.set ("path", temp_db.getfullpathname ());
        }

        // create a batch
        batch batch;
        createpredictablebatch (batch, numobjectstotest, seedvalue);

        beast::journal j;

        {
            // open the database
            std::unique_ptr <database> db = manager::instance().make_database (
                "test", scheduler, j, 2, nodeparams, tempparams);

            // write the batch
            storebatch (*db, batch);

            {
                // read it back in
                batch copy;
                fetchcopyofbatch (*db, &copy, batch);
                expect (arebatchesequal (batch, copy), "should be equal");
            }

            {
                // reorder and read the copy again
                batch copy;
                beast::unittestutilities::repeatableshuffle (batch.size (), batch, seedvalue);
                fetchcopyofbatch (*db, &copy, batch);
                expect (arebatchesequal (batch, copy), "should be equal");
            }
        }

        if (testpersistence)
        {
            {
                // re-open the database without the ephemeral db
                std::unique_ptr <database> db = manager::instance().make_database (
                    "test", scheduler, j, 2, nodeparams);

                // read it back in
                batch copy;
                fetchcopyofbatch (*db, &copy, batch);

                // canonicalize the source and destination batches
                std::sort (batch.begin (), batch.end (), nodeobject::lessthan ());
                std::sort (copy.begin (), copy.end (), nodeobject::lessthan ());
                expect (arebatchesequal (batch, copy), "should be equal");
            }

            if (useephemeraldatabase)
            {
                // verify the ephemeral db
                std::unique_ptr <database> db = manager::instance().make_database ("test",
                    scheduler, j, 2, tempparams, beast::stringpairarray ());

                // read it back in
                batch copy;
                fetchcopyofbatch (*db, &copy, batch);

                // canonicalize the source and destination batches
                std::sort (batch.begin (), batch.end (), nodeobject::lessthan ());
                std::sort (copy.begin (), copy.end (), nodeobject::lessthan ());
                expect (arebatchesequal (batch, copy), "should be equal");
            }
        }
    }

    //--------------------------------------------------------------------------

    void runbackendtests (bool useephemeraldatabase, std::int64_t const seedvalue)
    {
        testnodestore ("nudb", useephemeraldatabase, true, seedvalue);

        testnodestore ("leveldb", useephemeraldatabase, true, seedvalue);

    #if ripple_hyperleveldb_available
        testnodestore ("hyperleveldb", useephemeraldatabase, true, seedvalue);
    #endif

    #if ripple_rocksdb_available
        testnodestore ("rocksdb", useephemeraldatabase, true, seedvalue);
    #endif

    #if ripple_enable_sqlite_backend_tests
        testnodestore ("sqlite", useephemeraldatabase, true, seedvalue);
    #endif
    }

    //--------------------------------------------------------------------------

    void runimporttests (std::int64_t const seedvalue)
    {
        testimport ("nudb", "nudb", seedvalue);

        testimport ("leveldb", "leveldb", seedvalue);
        
    #if ripple_hyperleveldb_available
        testimport ("hyperleveldb", "hyperleveldb", seedvalue);
    #endif

    #if ripple_rocksdb_available
        testimport ("rocksdb", "rocksdb", seedvalue);
    #endif

    #if ripple_enable_sqlite_backend_tests
        testimport ("sqlite", "sqlite", seedvalue);
    #endif
    }

    //--------------------------------------------------------------------------

    void run ()
    {
        std::int64_t const seedvalue = 50;

        testnodestore ("memory", false, false, seedvalue);

        runbackendtests (false, seedvalue);

        runbackendtests (true, seedvalue);

        runimporttests (seedvalue);
    }
};

beast_define_testsuite(database,nodestore,ripple);

}
}
