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

#include <beast/asio/streambuf.h>
#include <beast/http/chunk_encode.h>
#include <beast/unit_test/suite.h>

namespace beast {
namespace http {

class chunk_encode_test : public unit_test::suite
{
public:
    // convert cr lf to printables for display
    static
    std::string
    encode (std::string const& s)
    {
        std::string result;
        for(auto const c : s)
        {
            if (c == '\r')
                result += "\\r";
            else if (c== '\n')
                result += "\\n";
            else
                result += c;
        }
        return result;
    }

    // print the contents of a constbuffersequence to the log
    template <class constbuffersequence, class log>
    static
    void
    print (constbuffersequence const& buffers, log log)
    {
        for(auto const& buf : buffers)
            log << encode (std::string(
                boost::asio::buffer_cast<char const*>(buf),
                    boost::asio::buffer_size(buf)));
    } 

    // convert a constbuffersequence to a string
    template <class constbuffersequence>
    static
    std::string
    buffer_to_string (constbuffersequence const& b)
    {
        std::string s;
        auto const n = boost::asio::buffer_size(b);
        s.resize(n);
        boost::asio::buffer_copy(
            boost::asio::buffer(&s[0], n), b);
        return s;
    }

    // append a constbuffersequence to an existing string
    template <class constbuffersequence>
    static
    void
    buffer_append (std::string& s, constbuffersequence const& b)
    {
        s += buffer_to_string(b);
    }

    // convert the input sequence of the stream to a
    // chunked-encoded string. the input sequence is consumed.
    template <class streambuf>
    static
    std::string
    streambuf_to_string (streambuf& sb,
        bool final_chunk = false)
    {
        std::string s;
        buffer_append(s, chunk_encode(sb.data(), final_chunk));
        return s;
    }

    // check an input against the correct chunk encoded version
    void
    check (std::string const& in, std::string const& answer,
        bool final_chunk = true)
    {
        asio::streambuf sb(3);
        sb << in;
        auto const out = streambuf_to_string (sb, final_chunk);
        if (! expect (out == answer))
            log << "expected\n" << encode(answer) <<
                "\ngot\n" << encode(out);
    }

    void teststreambuf()
    {
        asio::streambuf sb(3);
        std::string const s =
            "0123456789012345678901234567890123456789012345678901234567890123456789"
            "0123456789012345678901234567890123456789012345678901234567890123456789"
            "0123456789012345678901234567890123456789012345678901234567890123456789";
        sb << s;
        expect(buffer_to_string(sb.data()) == s);
    }

    void
    testencoder()
    {
        check("", "0\r\n\r\n");
        check("x", "1\r\nx\r\n0\r\n\r\n");
        check("abcd", "4\r\nabcd\r\n0\r\n\r\n");
        check("x", "1\r\nx\r\n", false);
        check(
            "0123456789012345678901234567890123456789012345678901234567890123456789"
            "0123456789012345678901234567890123456789012345678901234567890123456789"
            "0123456789012345678901234567890123456789012345678901234567890123456789"
            ,
            "d2\r\n"
            "0123456789012345678901234567890123456789012345678901234567890123456789"
            "0123456789012345678901234567890123456789012345678901234567890123456789"
            "0123456789012345678901234567890123456789012345678901234567890123456789"
            "\r\n"
            "0\r\n\r\n");
    }

    void
    run()
    {
        teststreambuf();
        testencoder();
    }
};

beast_define_testsuite(chunk_encode,http,beast);

}
}
