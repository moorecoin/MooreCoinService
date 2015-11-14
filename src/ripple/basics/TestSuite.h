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
    any  special,  direct, indirect, or consequential damages or any damages
    whatsoever  resulting  from  loss  of use, data or profits, whether in an
    action  of  contract, negligence or other tortious action, arising out of
    or in connection with the use or performance of this software.
*/
//==============================================================================

#ifndef rippled_ripple_basics_testsuite_h
#define rippled_ripple_basics_testsuite_h

#include <beast/unit_test/suite.h>
#include <string>

namespace ripple {

class testsuite : public beast::unit_test::suite
{
public:
    template <class s, class t>
    bool expectequals (s actual, t expected, std::string const& message = "")
    {
        if (actual != expected)
        {
            std::stringstream ss;
            if (!message.empty())
                ss << message << "\n";
            ss << "actual: " << actual << "\n"
               << "expected: " << expected;
            fail (ss.str());
            return false;
        }
        pass ();
        return true;

    }

    template <class collection>
    bool expectcollectionequals (
        collection const& actual, collection const& expected,
        std::string const& message = "")
    {
        auto msg = addprefix (message);
        bool success = expectequals (actual.size(), expected.size(),
                                     msg + "sizes are different");
        using std::begin;
        using std::end;

        auto i = begin (actual);
        auto j = begin (expected);
        auto k = 0;

        for (; i != end (actual) && j != end (expected); ++i, ++j, ++k)
        {
            if (!expectequals (*i, *j, msg + "elements at " +
                               std::to_string(k) + " are different."))
                return false;
        }

        return success;
    }

    template <class exception, class functor>
    bool expectexception (functor f, std::string const& message = "")
    {
        bool success = false;
        try
        {
            f();
        } catch (exception const&)
        {
            success = true;
        }
        return expect (success, addprefix (message) + "no exception thrown");
    }

    template <class functor>
    bool expectexception (functor f, std::string const& message = "")
    {
        bool success = false;
        try
        {
            f();
        } catch (...)
        {
            success = true;
        }
        return expect (success, addprefix (message) + "no exception thrown");
    }

private:
    static std::string addprefix (std::string const& message)
    {
        std::string msg = message;
        if (!msg.empty())
            msg = ": " + msg;
        return msg;
    }
};

} // ripple

#endif
