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
#include <ripple/rpc/status.h>
#include <beast/unit_test/suite.h>

namespace ripple {
namespace rpc {

class codestring_test : public beast::unit_test::suite
{
private:
    template <typename type>
    std::string codestring (type t)
    {
        return status (t).codestring();
    }

    void test_ok ()
    {
        testcase ("ok");
        {
            auto s = codestring (status ());
            expect (s.empty(), "string for ok status");
        }

        {
            auto s = codestring (status::ok);
            expect (s.empty(), "string for ok status");
        }

        {
            auto s = codestring (0);
            expect (s.empty(), "string for 0 status");
        }

        {
            auto s = codestring (tessuccess);
            expect (s.empty(), "string for tessuccess");
        }

        {
            auto s = codestring (rpcsuccess);
            expect (s.empty(), "string for rpcsuccess");
        }
    }

    void test_error ()
    {
        testcase ("error");
        {
            auto s = codestring (23);
            expect (s == "23", s);
        }

        {
            auto s = codestring (tembad_amount);
            expect (s == "tembad_amount: can only send positive amounts.", s);
        }

        {
            auto s = codestring (rpcbad_syntax);
            expect (s == "badsyntax: syntax error.", s);
        }
    }

public:
    void run()
    {
        test_ok ();
        test_error ();
    }
};

beast_define_testsuite (codestring, status, rpc);

class filljson_test : public beast::unit_test::suite
{
private:
    json::value value_;

    template <typename type>
    void filljson (type t)
    {
        value_.clear ();
        status (t).filljson (value_);
    }

    void test_ok ()
    {
        testcase ("ok");
        filljson (status ());
        expect (value_.empty(), "value for empty status");

        filljson (0);
        expect (value_.empty(), "value for 0 status");

        filljson (status::ok);
        expect (value_.empty(), "value for ok status");

        filljson (tessuccess);
        expect (value_.empty(), "value for tessuccess");

        filljson (rpcsuccess);
        expect (value_.empty(), "value for rpcsuccess");
    }

    template <typename type>
    void expectfill (
        std::string const& label,
        type status,
        status::strings messages,
        std::string const& message)
    {
        value_.clear ();
        filljson (status (status, messages));

        auto prefix = label + ": ";
        expect (!value_.empty(), prefix + "no value");

        auto error = value_[jss::error];
        expect (!error.empty(), prefix + "no error.");

        auto code = error[jss::code].asint();
        expect (status == code, prefix + "wrong status " +
                std::to_string (code) + " != " + std::to_string (status));

        auto m = error[jss::message].asstring ();
        expect (m == message, m + " != " + message);

        auto d = error[jss::data];
        size_t s1 = d.size(), s2 = messages.size();
        expect (s1 == s2, prefix + "data sizes differ " +
                std::to_string (s1) + " != " + std::to_string (s2));
        for (auto i = 0; i < std::min (s1, s2); ++i)
        {
            auto ds = d[i].asstring();
            expect (ds == messages[i], prefix + ds + " != " +  messages[i]);
        }
    }

    void test_error ()
    {
        testcase ("error");
        expectfill (
            "tembad_amount",
            tembad_amount,
            {},
            "tembad_amount: can only send positive amounts.");

        expectfill (
            "rpcbad_syntax",
            rpcbad_syntax,
            {"an error.", "another error."},
            "badsyntax: syntax error.");

        expectfill (
            "integer message",
            23,
            {"stuff."},
            "23");
    }

    void test_throw ()
    {
        testcase ("throw");
        try
        {
            throw status (tembad_path, {"path=sdcdfd"});
        }
        catch (status const& s)
        {
            expect (s.toter () == tembad_path, "tembad_path wasn't thrown");
            auto msgs = s.messages ();
            expect (msgs.size () == 1, "wrong number of messages");
            expect (msgs[0] == "path=sdcdfd", msgs[0]);
        }
        catch (...)
        {
            expect (false, "didn't catch a status");
        }
    }

public:
    void run()
    {
        test_ok ();
        test_error ();
        test_throw ();
    }
};

beast_define_testsuite (filljson, status, rpc);

} // namespace rpc
} // ripple
