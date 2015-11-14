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
#include <ripple/rpc/impl/jsonobject.h>
#include <ripple/rpc/tests/testoutputsuite.test.h>
#include <beast/unit_test/suite.h>

namespace ripple {
namespace rpc {

class jsonobject_test : public testoutputsuite
{
    void setup (std::string const& testname)
    {
        testcase (testname);
        output_.clear ();
    }

    std::unique_ptr<writerobject> writerobject_;

    object& makeroot()
    {
        writerobject_ = std::make_unique<writerobject> (
            stringwriterobject (output_));
        return **writerobject_;
    }

    void expectresult (std::string const& expected)
    {
        writerobject_.reset();
        testoutputsuite::expectresult (expected);
    }

public:
    void testtrivial ()
    {
        setup ("trivial");

        {
            auto& root = makeroot();
            (void) root;
        }
        expectresult ("{}");
    }

    void testsimple ()
    {
        setup ("simple");
        {
            auto& root = makeroot();
            root["hello"] = "world";
            root["skidoo"] = 23;
            root["awake"] = false;
            root["temperature"] = 98.6;
        }

        expectresult (
            "{\"hello\":\"world\","
            "\"skidoo\":23,"
            "\"awake\":false,"
            "\"temperature\":98.6}");
    }

    void testsimpleshort ()
    {
        setup ("simpleshort");
        makeroot()
                .set ("hello", "world")
                .set ("skidoo", 23)
                .set ("awake", false)
                .set ("temperature", 98.6);

        expectresult (
            "{\"hello\":\"world\","
            "\"skidoo\":23,"
            "\"awake\":false,"
            "\"temperature\":98.6}");
    }

    void testonesub ()
    {
        setup ("onesub");
        {
            auto& root = makeroot();
            root.makearray ("ar");
        }
        expectresult ("{\"ar\":[]}");
    }

    void testsubs ()
    {
        setup ("subs");
        {
            auto& root = makeroot();

            {
                // add an array with three entries.
                auto array = root.makearray ("ar");
                array.append (23);
                array.append (false);
                array.append (23.5);
            }

            {
                // add an object with one entry.
                auto obj = root.makeobject ("obj");
                obj["hello"] = "world";
            }

            {
                // add another object with two entries.
                auto obj = root.makeobject ("obj2");
                obj["h"] = "w";
                obj["f"] = false;
            }
        }

        expectresult (
            "{\"ar\":[23,false,23.5],"
            "\"obj\":{\"hello\":\"world\"},"
            "\"obj2\":{\"h\":\"w\",\"f\":false}}");
    }

    void testsubsshort ()
    {
        setup ("subsshort");

        {
            auto& root = makeroot();

            // add an array with three entries.
            root.makearray ("ar")
                    .append (23)
                    .append (false)
                    .append (23.5);

            // add an object with one entry.
            root.makeobject ("obj")["hello"] = "world";

            // add another object with two entries.
            root.makeobject ("obj2")
                    .set("h", "w")
                    .set("f", false);
        }

        expectresult (
            "{\"ar\":[23,false,23.5],"
            "\"obj\":{\"hello\":\"world\"},"
            "\"obj2\":{\"h\":\"w\",\"f\":false}}");
    }

    void testfailureobject()
    {
        {
            setup ("object failure assign");
            auto& root = makeroot();
            auto obj = root.makeobject ("o1");
            expectexception ([&]() { root["fail"] = "complete"; });
        }
        {
            setup ("object failure object");
            auto& root = makeroot();
            auto obj = root.makeobject ("o1");
            expectexception ([&] () { root.makeobject ("o2"); });
        }
        {
            setup ("object failure array");
            auto& root = makeroot();
            auto obj = root.makearray ("o1");
            expectexception ([&] () { root.makearray ("o2"); });
        }
    }

    void testfailurearray()
    {
        {
            setup ("array failure append");
            auto& root = makeroot();
            auto array = root.makearray ("array");
            auto subarray = array.makearray ();
            auto fail = [&]() { array.append ("fail"); };
            expectexception (fail);
        }
        {
            setup ("array failure makearray");
            auto& root = makeroot();
            auto array = root.makearray ("array");
            auto subarray = array.makearray ();
            auto fail = [&]() { array.makearray (); };
            expectexception (fail);
        }
        {
            setup ("array failure makeobject");
            auto& root = makeroot();
            auto array = root.makearray ("array");
            auto subarray = array.makearray ();
            auto fail = [&]() { array.makeobject (); };
            expectexception (fail);
        }
    }

    void testkeyfailure ()
    {
#ifdef debug
        setup ("repeating keys");
        auto& root = makeroot();
        root.set ("foo", "bar")
            .set ("baz", 0);
        auto fail = [&]() { root.set ("foo", "bar"); };
        expectexception (fail);
#endif
    }

    void run () override
    {
        testtrivial ();
        testsimple ();
        testsimpleshort ();

        testonesub ();
        testsubs ();
        testsubsshort ();

        testfailureobject ();
        testfailurearray ();
        testkeyfailure ();
    }
};

beast_define_testsuite(jsonobject, ripple_basics, ripple);

} // rpc
} // ripple
