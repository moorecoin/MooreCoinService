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
#include <ripple/json/json_value.h>
#include <ripple/json/json_reader.h>
#include <beast/unit_test/suite.h>
#include <beast/utility/type_name.h>

namespace ripple {

class jsoncpp_test : public beast::unit_test::suite
{
public:
    void test_bad_json ()
    {
        char const* s (
            "{\"method\":\"ledger\",\"params\":[{\"ledger_index\":1e300}]}"
            );

        json::value j;
        json::reader r;

        r.parse (s, j);
        pass ();
    }

    void test_edge_cases ()
    {
        std::string json;

        std::uint32_t max_uint = std::numeric_limits<std::uint32_t>::max ();
        std::int32_t max_int = std::numeric_limits<std::int32_t>::max ();
        std::int32_t min_int = std::numeric_limits<std::int32_t>::min ();

        std::uint32_t a_uint = max_uint - 1978;
        std::int32_t a_large_int = max_int - 1978;
        std::int32_t a_small_int = min_int + 1978;

        json  = "{\"max_uint\":"    + std::to_string (max_uint);
        json += ",\"max_int\":"     + std::to_string (max_int);
        json += ",\"min_int\":"     + std::to_string (min_int);
        json += ",\"a_uint\":"      + std::to_string (a_uint);
        json += ",\"a_large_int\":" + std::to_string (a_large_int);
        json += ",\"a_small_int\":" + std::to_string (a_small_int);
        json += "}";

        json::value j1;
        json::reader r1;

        expect (r1.parse (json, j1), "parsing integer edge cases");
        expect (j1["max_uint"].asuint() == max_uint, "max_uint");
        expect (j1["max_int"].asint() == max_int, "min_int");
        expect (j1["min_int"].asint() == min_int, "max_int");
        expect (j1["a_uint"].asuint() == a_uint, "a_uint");
        expect (j1["a_large_int"].asint() == a_large_int, "a_large_int");
        expect (j1["a_small_int"].asint() == a_small_int, "a_large_int");

        json  = "{\"overflow\":";
        json += std::to_string(std::uint64_t(max_uint) + 1);
        json += "}";

        json::value j2;
        json::reader r2;

        expect (!r2.parse (json, j2), "parsing unsigned integer that overflows");

        json  = "{\"underflow\":";
        json += std::to_string(std::int64_t(min_int) - 1);
        json += "}";

        json::value j3;
        json::reader r3;

        expect (!r3.parse (json, j3), "parsing signed integer that underflows");

        pass ();
    }

    void
    test_copy ()
    {
        json::value v1{2.5};
        expect (v1.isdouble ());
        expect (v1.asdouble () == 2.5);

        json::value v2 = v1;
        expect (v1.isdouble ());
        expect (v1.asdouble () == 2.5);
        expect (v2.isdouble ());
        expect (v2.asdouble () == 2.5);
        expect (v1 == v2);

        v1 = v2;
        expect (v1.isdouble ());
        expect (v1.asdouble () == 2.5);
        expect (v2.isdouble ());
        expect (v2.asdouble () == 2.5);
        expect (v1 == v2);

        pass ();
    }

    void
    test_move ()
    {
        json::value v1{2.5};
        expect (v1.isdouble ());
        expect (v1.asdouble () == 2.5);

        json::value v2 = std::move(v1);
        expect (v1.isnull ());
        expect (v2.isdouble ());
        expect (v2.asdouble () == 2.5);
        expect (v1 != v2);

        v1 = std::move(v2);
        expect (v1.isdouble ());
        expect (v1.asdouble () == 2.5);
        expect (v2.isnull ());
        expect (v1 != v2);

        pass ();
    }

    void run ()
    {
        test_bad_json ();
        test_edge_cases ();
        test_copy ();
        test_move ();
    }
};

beast_define_testsuite(jsoncpp,json,ripple);

} // ripple
