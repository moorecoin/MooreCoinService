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
#include <ripple/basics/stringutilities.h>
#include <ripple/basics/tostring.h>
#include <beast/unit_test/suite.h>

namespace ripple {

class stringutilities_test : public beast::unit_test::suite
{
public:
    void testunhexsuccess (std::string strin, std::string strexpected)
    {
        std::string strout;

        expect (strunhex (strout, strin) == strexpected.length (),
            "strunhex: parsing correct input failed");

        expect (strout == strexpected,
            "strunhex: parsing doesn't produce expected result");
    }

    void testunhexfailure (std::string strin)
    {
        std::string strout;

        expect (strunhex (strout, strin) == -1,
            "strunhex: parsing incorrect input succeeded");

        expect (strout.empty (),
            "strunhex: parsing incorrect input returned data");
    }

    void testunhex ()
    {
        testcase ("strunhex");

        testunhexsuccess ("526970706c6544", "rippled");
        testunhexsuccess ("a", "\n");
        testunhexsuccess ("0a", "\n");
        testunhexsuccess ("d0a", "\r\n");
        testunhexsuccess ("0d0a", "\r\n");
        testunhexsuccess ("200d0a", " \r\n");
        testunhexsuccess ("282a2b2c2d2e2f29", "(*+,-./)");

        // check for things which contain some or only invalid characters
        testunhexfailure ("123x");
        testunhexfailure ("v");
        testunhexfailure ("xrp");
    }

    void testparseurl ()
    {
        testcase ("parseurl");

        std::string strscheme;
        std::string strdomain;
        int         iport;
        std::string strpath;

        unexpected (!parseurl ("lower://domain", strscheme, strdomain, iport, strpath),
            "parseurl: lower://domain failed");

        unexpected (strscheme != "lower",
            "parseurl: lower://domain : scheme failed");

        unexpected (strdomain != "domain",
            "parseurl: lower://domain : domain failed");

        unexpected (iport != -1,
            "parseurl: lower://domain : port failed");

        unexpected (strpath != "",
            "parseurl: lower://domain : path failed");

        unexpected (!parseurl ("upper://domain:234/", strscheme, strdomain, iport, strpath),
            "parseurl: upper://domain:234/ failed");

        unexpected (strscheme != "upper",
            "parseurl: upper://domain:234/ : scheme failed");

        unexpected (iport != 234,
            boost::str (boost::format ("parseurl: upper://domain:234/ : port failed: %d") % iport));

        unexpected (strpath != "/",
            "parseurl: upper://domain:234/ : path failed");

        unexpected (!parseurl ("mixed://domain/path", strscheme, strdomain, iport, strpath),
            "parseurl: mixed://domain/path failed");

        unexpected (strscheme != "mixed",
            "parseurl: mixed://domain/path tolower failed");

        unexpected (strpath != "/path",
            "parseurl: mixed://domain/path path failed");
    }

    void testtostring ()
    {
        testcase ("tostring");
        auto result = to_string("hello");
        expect(result == "hello", result);
    }

    void run ()
    {
        testparseurl ();
        testunhex ();
        testtostring ();
    }
};

beast_define_testsuite(stringutilities, ripple_basics, ripple);

} // ripple
