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
#include <beast/unit_test/suite.h>

namespace beast {
namespace asio {

class streambuf_test : public unit_test::suite
{
public:
    // convert a buffer sequence to a string
    template <class buffers>
    static
    std::string
    to_str (buffers const& b)
    {
        std::string s;
        auto const n = boost::asio::buffer_size(b);
        s.resize(n);
        boost::asio::buffer_copy(
            boost::asio::buffer(&s[0], n), b);
        return s;
    }

    // fill a buffer sequence with predictable data
    template <class buffers>
    static
    void
    fill (buffers const& b)
    {
        char c = 0;
        auto first = boost::asio::buffers_begin(b);
        auto last = boost::asio::buffers_end(b);
        while (first != last)
            *first++ = c++;
    }

    // check that a buffer sequence has predictable data
    template <class buffers>
    void
    check (buffers const& b, char c = 0)
    {
        auto first = boost::asio::buffers_begin(b);
        auto last = boost::asio::buffers_end(b);
        while (first != last)
            expect (*first++ == c++);
    }

    void
    test_prepare()
    {
        testcase << "prepare";
        beast::asio::streambuf b(11);
        for (std::size_t n = 0; n < 97; ++n)
        {
            fill(b.prepare(n));
            b.commit(n);
            check(b.data());
            b.consume(n);
        }
    }

    void
    test_commit()
    {
        testcase << "commit";
        beast::asio::streambuf b(11);
        for (std::size_t n = 0; n < 97; ++n)
        {
            fill(b.prepare(n));
            char c = 0;
            for (int i = 1;; ++i)
            {
                b.commit(i);
                check(b.data(), c);
                b.consume(i);
                if (b.size() < 1)
                    break;
                c += i;
            }
        }
    }

    void
    test_consume()
    {
        testcase << "consume";
        beast::asio::streambuf b(11);
        for (std::size_t n = 0; n < 97; ++n)
        {
            fill(b.prepare(n));
            b.commit(n);
            char c = 0;
            for (int i = 1; b.size() > 0; ++i)
            {
                check(b.data(), c);
                b.consume(i);
                c += i;
            }
        }
    }

    void run()
    {
        {
            beast::asio::streambuf b(10);
            std::string const s = "1234567890";
            b << s;
            expect (to_str(b.data()) == s);
            b.prepare(5);
        }

        {
            beast::asio::streambuf b(10);
            b.prepare(10);
            b.commit(10);
            b.consume(10);
        }

        {
            beast::asio::streambuf b(5);
            boost::asio::buffer_copy(b.prepare(14),
                boost::asio::buffer(std::string("1234567890abcd")));
            b.commit(4);
            expect(to_str(b.data()) == "1234");
            b.consume(4);
            b.commit(10);
            expect(to_str(b.data()) == "567890abcd");
        }
        
        test_prepare();
        test_commit();
        test_consume();
    }
};

beast_define_testsuite(streambuf,asio,beast);

}
}
