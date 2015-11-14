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
#include <ripple/rpc/impl/writejson.h>
#include <ripple/rpc/tests/testoutputsuite.test.h>

namespace ripple {
namespace rpc {

struct writejson_test : testoutputsuite
{
    void runtest (std::string const& name, std::string const& valuedesc)
    {
        setup (name);
        json::value value;
        expect (json::reader().parse (valuedesc, value));
        auto out = stringoutput (output_);
        writejson (value, out);

        // compare with the original version.
        auto expected = json::fastwriter().write (value);
        expected.resize (expected.size() - 1);
        // for some reason, the fastwriter puts a carriage return on the end of
        // every piece of json it outputs.

        expectresult (expected);
        expectresult (valuedesc);
        expectresult (jsonasstring (value));
    }

    void runtest (std::string const& name)
    {
        runtest (name, name);
    }

    void run () override
    {
        runtest ("null");
        runtest ("true");
        runtest ("0");
        runtest ("23.5");
        runtest ("string", "\"a string\"");
        runtest ("empty dict", "{}");
        runtest ("empty array", "[]");
        runtest ("array", "[23,4.25,true,null,\"string\"]");
        runtest ("dict", "{\"hello\":\"world\"}");
        runtest ("array dict", "[{}]");
        runtest ("array array", "[[]]");
        runtest ("more complex",
                 "{\"array\":[{\"12\":23},{},null,false,0.5]}");
    }
};

beast_define_testsuite(writejson, ripple_basics, ripple);

} // rpc
} // ripple
