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
#include <ripple/basics/rangeset.h>
#include <beast/unit_test/suite.h>

namespace ripple {

class rangeset_test : public beast::unit_test::suite
{
public:
    rangeset createpredefinedset ()
    {
        rangeset set;

        // set will include:
        // [ 0, 5]
        // [10,15]
        // [20,25]
        // etc...

        for (int i = 0; i < 10; ++i)
            set.setrange (10 * i, 10 * i + 5);

        return set;
    }

    void testmembership ()
    {
        testcase ("membership");

        rangeset r1, r2;

        r1.setrange (1, 10);
        r1.clearvalue (5);
        r1.setrange (11, 20);

        r2.setrange (1, 4);
        r2.setrange (6, 10);
        r2.setrange (10, 20);

        expect (!r1.hasvalue (5));

        expect (r2.hasvalue (9));
    }

    void testprevmissing ()
    {
        testcase ("prevmissing");

        rangeset const set = createpredefinedset ();

        for (int i = 0; i < 100; ++i)
        {
            int const onebelowrange = (10*(i/10))-1;

            int const expectedprevmissing =
                ((i % 10) > 6) ? (i-1) : onebelowrange;

            expect (set.prevmissing (i) == expectedprevmissing);
        }
    }

    void run ()
    {
        testmembership ();

        testprevmissing ();

        // todo: traverse functions must be tested
    }
};

beast_define_testsuite(rangeset,ripple_basics,ripple);

} // ripple

