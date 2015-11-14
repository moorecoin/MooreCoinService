//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

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

#include <beast/http/url.h>

#include <beast/unit_test/suite.h>

namespace beast {

class url_test : public unit_test::suite
{
public:
    void check_url_parsing (std::string const& url, bool expected)
    {
        auto result = parse_url (url);

        expect (result.first == expected,
            (expected ? "failed to parse " : "succeeded in parsing ") + url);
        expect (to_string (result.second) == url);
    }

    void test_url_parsing ()
    {
        char const* const urls[] = 
        {
            "http://en.wikipedia.org/wiki/uri#examples_of_uri_references",
            "ftp://ftp.funet.fi/pub/standards/rfc/rfc959.txt"
            "ftp://test:test@example.com:21/path/specifier/is/here"
            "http://www.boost.org/doc/libs/1_54_0/doc/html/boost_asio/reference.html",
            "foo://username:password@example.com:8042/over/there/index.dtb?type=animal&name=narwhal#nose",
        };

        testcase ("url parsing");

        for (auto url : urls)
            check_url_parsing (url, true);
    }

    void
    run ()
    {
        test_url_parsing ();
    }
};

beast_define_testsuite(url,http,beast);

}
