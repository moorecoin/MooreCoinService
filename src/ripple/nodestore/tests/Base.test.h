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

#ifndef ripple_nodestore_testbase_h_included
#define ripple_nodestore_testbase_h_included

#include <ripple/nodestore/database.h>
#include <ripple/basics/stringutilities.h>
#include <beast/unit_test/suite.h>
#include <beast/module/core/maths/random.h>
#include <boost/algorithm/string.hpp>
#include <iomanip>

namespace ripple {
namespace nodestore {

// some common code for the unit tests
//
class testbase : public beast::unit_test::suite
{
public:
    // tunable parameters
    //
    enum
    {
        maxpayloadbytes = 2000,
        numobjectstotest = 2000
    };

    // creates predictable objects
    class predictableobjectfactory
    {
    public:
        explicit predictableobjectfactory (std::int64_t seedvalue)
            : r (seedvalue)
        {
        }

        nodeobject::ptr createobject ()
        {
            nodeobjecttype type;
            switch (r.nextint (4))
            {
            case 0: type = hotledger; break;
            case 1: type = hottransaction; break;
            case 2: type = hotaccount_node; break;
            case 3: type = hottransaction_node; break;
            default:
                type = hotunknown;
                break;
            };

            uint256 hash;
            r.fillbitsrandomly (hash.begin (), hash.size ());

            int const payloadbytes = 1 + r.nextint (maxpayloadbytes);

            blob data (payloadbytes);

            r.fillbitsrandomly (data.data (), payloadbytes);

            return nodeobject::createobject(type, std::move(data), hash);
        }

    private:
        beast::random r;
    };

public:
 // create a predictable batch of objects
 static void createpredictablebatch(batch& batch, int numobjects,
                                    std::int64_t seedvalue) {
        batch.reserve (numobjects);

        predictableobjectfactory factory (seedvalue);

        for (int i = 0; i < numobjects; ++i)
            batch.push_back (factory.createobject ());
    }

    // compare two batches for equality
    static bool arebatchesequal (batch const& lhs, batch const& rhs)
    {
        bool result = true;

        if (lhs.size () == rhs.size ())
        {
            for (int i = 0; i < lhs.size (); ++i)
            {
                if (! lhs [i]->iscloneof (rhs [i]))
                {
                    result = false;
                    break;
                }
            }
        }
        else
        {
            result = false;
        }

        return result;
    }

    // store a batch in a backend
    void storebatch (backend& backend, batch const& batch)
    {
        for (int i = 0; i < batch.size (); ++i)
        {
            backend.store (batch [i]);
        }
    }

    // get a copy of a batch in a backend
    void fetchcopyofbatch (backend& backend, batch* pcopy, batch const& batch)
    {
        pcopy->clear ();
        pcopy->reserve (batch.size ());

        for (int i = 0; i < batch.size (); ++i)
        {
            nodeobject::ptr object;

            status const status = backend.fetch (
                batch [i]->gethash ().cbegin (), &object);

            expect (status == ok, "should be ok");

            if (status == ok)
            {
                expect (object != nullptr, "should not be null");

                pcopy->push_back (object);
            }
        }
    }

    void fetchmissing(backend& backend, batch const& batch)
    {
        for (int i = 0; i < batch.size (); ++i)
        {
            nodeobject::ptr object;

            status const status = backend.fetch (
                batch [i]->gethash ().cbegin (), &object);

            expect (status == notfound, "should be notfound");
        }
    }

    // store all objects in a batch
    static void storebatch (database& db, batch const& batch)
    {
        for (int i = 0; i < batch.size (); ++i)
        {
            nodeobject::ptr const object (batch [i]);

            blob data (object->getdata ());

            db.store (object->gettype (),
                      std::move (data),
                      object->gethash ());
        }
    }

    // fetch all the hashes in one batch, into another batch.
    static void fetchcopyofbatch (database& db,
                                  batch* pcopy,
                                  batch const& batch)
    {
        pcopy->clear ();
        pcopy->reserve (batch.size ());

        for (int i = 0; i < batch.size (); ++i)
        {
            nodeobject::ptr object = db.fetch (batch [i]->gethash ());

            if (object != nullptr)
                pcopy->push_back (object);
        }
    }
};

}
}

#endif
