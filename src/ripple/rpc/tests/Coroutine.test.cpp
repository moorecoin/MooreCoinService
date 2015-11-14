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
#include <ripple/rpc/coroutine.h>
#include <ripple/rpc/yield.h>
#include <ripple/rpc/tests/testoutputsuite.test.h>

namespace ripple {
namespace rpc {

class coroutine_test : public testoutputsuite
{
public:
    using strings = std::vector <std::string>;

    void test (std::string const& name, int chunksize, strings const& expected)
    {
        setup (name);

        std::string buffer;
        output output = stringoutput (buffer);

        auto coroutine = coroutine ([=] (yield yield)
        {
            auto out = chunkedyieldingoutput (output, yield, chunksize);
            out ("hello ");
            out ("there ");
            out ("world.");
        });

        strings result;
        while (coroutine)
        {
            coroutine();
            result.push_back (buffer);
        }

        expectcollectionequals (result, expected);
    }

    void run() override
    {
        test ("zero", 0, {"hello ", "hello there ", "hello there world."});
        test ("three", 3, {"hello ", "hello there ", "hello there world."});
        test ("five", 5, {"hello ", "hello there ", "hello there world."});
        test ("seven", 7, {"hello there ", "hello there world."});
        test ("ten", 10, {"hello there ", "hello there world."});
        test ("thirteen", 13, {"hello there world."});
        test ("fifteen", 15, {"hello there world."});
    }
};

beast_define_testsuite(coroutine, rpc, ripple);

} // rpc
} // ripple
