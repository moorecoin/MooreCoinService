//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2014, nikolaos d. bougalis <nikb@bougalis.net>

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

#include <beast/utility/zero.h>

#include <beast/unit_test/suite.h>

namespace beast {

struct adl_tester {};

int signum (adl_tester)
{
    return 0;
}


namespace detail {

struct adl_tester2 {};

int signum (adl_tester2)
{
    return 0;
}

}  // detail

class zero_test : public beast::unit_test::suite
{
private:
    struct integerwrapper
    {
        int value;

        integerwrapper (int v)
            : value (v)
        {
        }

        int signum() const
        {
            return value;
        }
    };

public:
    void expect_same(bool result, bool correct, char const* message)
    {
        expect(result == correct, message);
    }

    void
    test_lhs_zero (integerwrapper x)
    {
        expect_same (x >= zero, x.signum () >= 0,
            "lhs greater-than-or-equal-to");
        expect_same (x > zero, x.signum () > 0,
            "lhs greater than");
        expect_same (x == zero, x.signum () == 0,
            "lhs equal to");
        expect_same (x != zero, x.signum () != 0,
            "lhs not equal to");
        expect_same (x < zero, x.signum () < 0,
            "lhs less than");
        expect_same (x <= zero, x.signum () <= 0,
            "lhs less-than-or-equal-to");
    }

    void
    test_lhs_zero ()
    {
        testcase ("lhs zero");

        test_lhs_zero(-7);
        test_lhs_zero(0);
        test_lhs_zero(32);
    }

    void
    test_rhs_zero (integerwrapper x)
    {
        expect_same (zero >= x, 0 >= x.signum (),
            "rhs greater-than-or-equal-to");
        expect_same (zero > x, 0 > x.signum (),
            "rhs greater than");
        expect_same (zero == x, 0 == x.signum (),
            "rhs equal to");
        expect_same (zero != x, 0 != x.signum (),
            "rhs not equal to");
        expect_same (zero < x, 0 < x.signum (),
            "rhs less than");
        expect_same (zero <= x, 0 <= x.signum (),
            "rhs less-than-or-equal-to");
    }

    void
    test_rhs_zero ()
    {
        testcase ("rhs zero");

        test_rhs_zero(-4);
        test_rhs_zero(0);
        test_rhs_zero(64);
    }

    void
    test_adl ()
    {
      expect (adl_tester{} == zero, "adl failure!");
      expect (detail::adl_tester2{} == zero, "adl failure!");
    }

    void
    run()
    {
        test_lhs_zero ();
        test_rhs_zero ();
        test_adl ();
    }

};

beast_define_testsuite(zero, types, beast);

}
