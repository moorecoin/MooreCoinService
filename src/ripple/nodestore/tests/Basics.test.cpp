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
#include <ripple/nodestore/impl/decodedblob.h>
#include <ripple/nodestore/impl/encodedblob.h>

namespace ripple {
namespace nodestore {

// tests predictable batches, and nodeobject blob encoding
//
class nodestorebasic_test : public testbase
{
public:
    // make sure predictable object generation works!
    void testbatches (std::int64_t const seedvalue)
    {
        testcase ("batch");

        batch batch1;
        createpredictablebatch (batch1, numobjectstotest, seedvalue);

        batch batch2;
        createpredictablebatch (batch2, numobjectstotest, seedvalue);

        expect (arebatchesequal (batch1, batch2), "should be equal");

        batch batch3;
        createpredictablebatch (batch3, numobjectstotest, seedvalue+1);

        expect (! arebatchesequal (batch1, batch3), "should not be equal");
    }

    // checks encoding/decoding blobs
    void testblobs (std::int64_t const seedvalue)
    {
        testcase ("encoding");

        batch batch;
        createpredictablebatch (batch, numobjectstotest, seedvalue);

        encodedblob encoded;
        for (int i = 0; i < batch.size (); ++i)
        {
            encoded.prepare (batch [i]);

            decodedblob decoded (encoded.getkey (), encoded.getdata (), encoded.getsize ());

            expect (decoded.wasok (), "should be ok");

            if (decoded.wasok ())
            {
                nodeobject::ptr const object (decoded.createobject ());

                expect (batch [i]->iscloneof (object), "should be clones");
            }
        }
    }

    void run ()
    {
        std::int64_t const seedvalue = 50;

        testbatches (seedvalue);

        testblobs (seedvalue);
    }
};

beast_define_testsuite(nodestorebasic,ripple_core,ripple);

}
}
