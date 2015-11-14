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
#include <ripple/rpc/yield.h>
#include <ripple/rpc/tests/testoutputsuite.test.h>

namespace ripple {
namespace rpc {

struct yield_test : testoutputsuite
{
    void chunkedyieldingtest ()
    {
        setup ("chunkedyieldingtest");
        std::string lastyield;

        auto yield = [&]() { lastyield = output_; };
        auto output = chunkedyieldingoutput (stringoutput (output_), yield, 5);
        output ("hello");
        expectresult ("hello");
        expectequals (lastyield, "");

        output (", th");  // goes over the boundary.
        expectresult ("hello, th");
        expectequals (lastyield, "");

        output ("ere!");  // forces a yield.
        expectresult ("hello, there!");
        expectequals (lastyield, "hello, th");

        output ("!!");
        expectresult ("hello, there!!!");
        expectequals (lastyield, "hello, th");

        output ("");    // forces a yield.
        expectresult ("hello, there!!!");
        expectequals (lastyield, "hello, there!!!");
    }

    void trivialcountedyieldtest()
    {
        setup ("trivialcountedyield");

        auto didyield = false;
        auto yield = [&]() { didyield = true; };

        countedyield cy (0, yield);

        for (auto i = 0; i < 4; ++i)
        {
            cy.yield();
            expect (!didyield, "we yielded when we shouldn't have.");
        }
    }

    void countedyieldtest()
    {
        setup ("countedyield");

        auto didyield = false;
        auto yield = [&]() { didyield = true; };

        countedyield cy (5, yield);

        for (auto j = 0; j < 3; ++j)
        {
            for (auto i = 0; i < 4; ++i)
            {
                cy.yield();
                expect (!didyield, "we yielded when we shouldn't have.");
            }
            cy.yield();
            expect (didyield, "we didn't yield");
            didyield = false;
        }
    }

    void run () override
    {
        chunkedyieldingtest();
        trivialcountedyieldtest();
        countedyieldtest();
    }
};

beast_define_testsuite(yield, ripple_basics, ripple);

} // rpc
} // ripple
