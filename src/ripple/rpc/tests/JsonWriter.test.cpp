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
#include <ripple/json/json_writer.h>
#include <ripple/rpc/impl/jsonwriter.h>
#include <ripple/rpc/tests/testoutputsuite.test.h>
#include <beast/unit_test/suite.h>

namespace ripple {
namespace rpc {

class jsonwriter_test : public testoutputsuite
{
public:
    void testtrivial ()
    {
        setup ("trivial");
        expect (output_.empty ());
        expectresult("");
    }

    void testneartrivial ()
    {
        setup ("near trivial");
        expect (output_.empty ());
        writer_->output (0);
        expectresult("0");
    }

    void testprimitives ()
    {
        setup ("true");
        writer_->output (true);
        expectresult ("true");

        setup ("false");
        writer_->output (false);
        expectresult ("false");

        setup ("23");
        writer_->output (23);
        expectresult ("23");

        setup ("23.0");
        writer_->output (23.0);
        expectresult ("23.0");

        setup ("23.5");
        writer_->output (23.5);
        expectresult ("23.5");

        setup ("a string");
        writer_->output ("a string");
        expectresult ("\"a string\"");

        setup ("nullptr");
        writer_->output (nullptr);
        expectresult ("null");
    }

    void testempty ()
    {
        setup ("empty array");
        writer_->startroot (writer::array);
        writer_->finish ();
        expectresult ("[]");

        setup ("empty object");
        writer_->startroot (writer::object);
        writer_->finish ();
        expectresult ("{}");
    }

    void testescaping ()
    {
        setup ("backslash");
        writer_->output ("\\");
        expectresult ("\"\\\\\"");

        setup ("quote");
        writer_->output ("\"");
        expectresult ("\"\\\"\"");

        setup ("backslash and quote");
        writer_->output ("\\\"");
        expectresult ("\"\\\\\\\"\"");

        setup ("escape embedded");
        writer_->output ("this contains a \\ in the middle of it.");
        expectresult ("\"this contains a \\\\ in the middle of it.\"");

        setup ("remaining escapes");
        writer_->output ("\b\f\n\r\t");
        expectresult ("\"\\b\\f\\n\\r\\t\"");
    }

    void testarray ()
    {
        setup ("empty array");
        writer_->startroot (writer::array);
        writer_->append (12);
        writer_->finish ();
        expectresult ("[12]");
    }

    void testlongarray ()
    {
        setup ("long array");
        writer_->startroot (writer::array);
        writer_->append (12);
        writer_->append (true);
        writer_->append ("hello");
        writer_->finish ();
        expectresult ("[12,true,\"hello\"]");
    }

    void testembeddedarraysimple ()
    {
        setup ("embedded array simple");
        writer_->startroot (writer::array);
        writer_->startappend (writer::array);
        writer_->finish ();
        writer_->finish ();
        expectresult ("[[]]");
    }

    void testobject ()
    {
        setup ("object");
        writer_->startroot (writer::object);
        writer_->set ("hello", "world");
        writer_->finish ();

        expectresult ("{\"hello\":\"world\"}");
    }

    void testcomplexobject ()
    {
        setup ("complex object");
        writer_->startroot (writer::object);

        writer_->set ("hello", "world");
        writer_->startset (writer::array, "array");

        writer_->append (true);
        writer_->append (12);
        writer_->startappend (writer::array);
        writer_->startappend (writer::object);
        writer_->set ("goodbye", "cruel world.");
        writer_->startset (writer::array, "subarray");
        writer_->append (23.5);
        writer_->finishall ();

        expectresult ("{\"hello\":\"world\",\"array\":[true,12,"
                      "[{\"goodbye\":\"cruel world.\","
                      "\"subarray\":[23.5]}]]}");
    }

    void testjson ()
    {
        setup ("object");
        json::value value (json::objectvalue);
        value["foo"] = 23;
        writer_->startroot (writer::object);
        writer_->set ("hello", value);
        writer_->finish ();

        expectresult ("{\"hello\":{\"foo\":23}}");
    }

    void run () override
    {
        testtrivial ();
        testneartrivial ();
        testprimitives ();
        testempty ();
        testescaping ();
        testarray ();
        testlongarray ();
        testembeddedarraysimple ();
        testobject ();
        testcomplexobject ();
        testjson();
    }
};

beast_define_testsuite(jsonwriter, ripple_basics, ripple);

} // rpc
} // ripple
