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

#include <beast/http/message.h>
#include <beast/http/parser.h>
#include <beast/unit_test/suite.h>
#include <utility>

namespace beast {
namespace http {

class message_test : public beast::unit_test::suite
{
public:
    std::pair <message, bool>
    request (std::string const& text)
    {
        message m;
        body b;
        parser p (m, b, true);
        auto result (p.write (boost::asio::buffer(text)));
        p.write_eof();
        return std::make_pair (std::move(m), result.first);
    }

    void
    dump()
    {
        auto const result = request (
            "get / http/1.1\r\n"
            //"connection: upgrade\r\n"
            //"upgrade: ripple\r\n"
            "field: \t value \t \r\n"
            "blib: continu\r\n"
            "  ation\r\n"
            "field: hey\r\n"
            "content-length: 1\r\n"
            "\r\n"
            "x"
            );
        log << result.first.headers;
        log << "|" << result.first.headers["field"] << "|";
    }

    void
    test_headers()
    {
        headers h;
        h.append("field", "value");
        expect (h.erase("field") == 1);
    }

    void
    run()
    {
        test_headers();

        {
            std::string const text =
                "get / http/1.1\r\n"
                "\r\n"
                ;
            message m;
            body b;
            parser p (m, b, true);
            auto result (p.write (boost::asio::buffer(text)));
            expect (! result.first);
            auto result2 (p.write_eof());
            expect (! result2);
            expect (p.complete());
        }

        {
            // malformed
            std::string const text =
                "get\r\n"
                "\r\n"
                ;
            message m;
            body b;
            parser p (m, b, true);
            auto result = p.write (boost::asio::buffer(text));
            if (expect (result.first))
                expect (result.first.message() == "invalid http method");
        }
    }
};

beast_define_testsuite(message,http,beast);

} // http
} // beast
