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
#include <ripple/app/book/quality.h>
#include <beast/unit_test/suite.h>
#include <beast/cxx14/type_traits.h>

namespace ripple {
namespace core {

class quality_test : public beast::unit_test::suite
{
public:
    // create a raw, non-integral amount from mantissa and exponent
    amount
    static raw (std::uint64_t mantissa, int exponent)
    {
        return amount ({currency(3), account(3)}, mantissa, exponent);
    }

    template <class integer>
    static
    amount
    amount (integer integer,
        std::enable_if_t <std::is_signed <integer>::value>* = 0)
    {
        static_assert (std::is_integral <integer>::value, "");
        return amount (integer, false);
    }

    template <class integer>
    static
    amount
    amount (integer integer,
        std::enable_if_t <! std::is_signed <integer>::value>* = 0)
    {
        static_assert (std::is_integral <integer>::value, "");
        if (integer < 0)
            return amount (-integer, true);
        return amount (integer, false);
    }

    template <class in, class out>
    static
    amounts
    amounts (in in, out out)
    {
        return amounts (amount(in), amount(out));
    }

    template <class in1, class out1, class int, class in2, class out2>
    void
    ceil_in (quality const& q,
        in1 in, out1 out, int limit, in2 in_expected, out2 out_expected)
    {
        auto expect_result (amounts (in_expected, out_expected));
        auto actual_result (q.ceil_in (amounts(in, out), amount(limit)));

        expect (actual_result == expect_result);
    }

    template <class in1, class out1, class int, class in2, class out2>
    void
    ceil_out (quality const& q,
        in1 in, out1 out, int limit, in2 in_expected, out2 out_expected)
    {
        auto const expect_result (amounts (in_expected, out_expected));
        auto const actual_result (q.ceil_out (amounts(in, out), amount(limit)));

        expect (actual_result == expect_result);
    }

    void
    test_ceil_in ()
    {
        testcase ("ceil_in");

        {
            // 1 in, 1 out:
            quality q (amounts (amount(1), amount(1)));

            ceil_in (q,
                1,  1,   // 1 in, 1 out
                1,       // limit: 1
                1,  1);  // 1 in, 1 out

            ceil_in (q,
                10, 10, // 10 in, 10 out
                5,      // limit: 5
                5, 5);  // 5 in, 5 out

            ceil_in (q,
                5, 5,   // 5 in, 5 out
                10,     // limit: 10
                5, 5);  // 5 in, 5 out
        }

        {
            // 1 in, 2 out:
            quality q (amounts (amount(1), amount(2)));

            ceil_in (q,
                40, 80,   // 40 in, 80 out
                40,       // limit: 40
                40, 80);  // 40 in, 20 out

            ceil_in (q,
                40, 80,   // 40 in, 80 out
                20,       // limit: 20
                20, 40);  // 20 in, 40 out

            ceil_in (q,
                40, 80,   // 40 in, 80 out
                60,       // limit: 60
                40, 80);  // 40 in, 80 out
        }

        {
            // 2 in, 1 out:
            quality q (amounts (amount(2), amount(1)));

            ceil_in (q,
                40, 20,   // 40 in, 20 out
                20,       // limit: 20
                20, 10);  // 20 in, 10 out

            ceil_in (q,
                40, 20,   // 40 in, 20 out
                40,       // limit: 40
                40, 20);  // 40 in, 20 out

            ceil_in (q,
                40, 20,   // 40 in, 20 out
                50,       // limit: 40
                40, 20);  // 40 in, 20 out
        }
    }

    void
    test_ceil_out ()
    {
        testcase ("ceil_out");

        {
            // 1 in, 1 out:
            quality q (amounts (amount(1),amount(1)));

            ceil_out (q,
                1,  1,    // 1 in, 1 out
                1,        // limit 1
                1,  1);   // 1 in, 1 out

            ceil_out (q,
                10, 10,   // 10 in, 10 out
                5,        // limit 5
                5, 5);    // 5 in, 5 out

            ceil_out (q,
                10, 10,   // 10 in, 10 out
                20,       // limit 20
                10, 10);  // 10 in, 10 out
        }

        {
            // 1 in, 2 out:
            quality q (amounts (amount(1),amount(2)));

            ceil_out (q,
                40, 80,    // 40 in, 80 out
                40,        // limit 40
                20, 40);   // 20 in, 40 out

            ceil_out (q,
                40, 80,    // 40 in, 80 out
                80,        // limit 80
                40, 80);   // 40 in, 80 out

            ceil_out (q,
                40, 80,    // 40 in, 80 out
                100,       // limit 100
                40, 80);   // 40 in, 80 out
        }

        {
            // 2 in, 1 out:
            quality q (amounts (amount(2),amount(1)));

            ceil_out (q,
                40, 20,    // 40 in, 20 out
                20,        // limit 20
                40, 20);   // 40 in, 20 out

            ceil_out (q,
                40, 20,    // 40 in, 20 out
                40,        // limit 40
                40, 20);   // 40 in, 20 out

            ceil_out (q,
                40, 20,    // 40 in, 20 out
                10,        // limit 10
                20, 10);   // 20 in, 10 out
        }
    }

    void
    test_raw()
    {
        testcase ("raw");

        {
            quality q (0x5d048191fb9130daull);      // 126836389.7680090
            amounts const value (
                amount(349469768),                  // 349.469768 xrp
                raw (2755280000000000ull, -15));    // 2.75528
            amount const limit (
                raw (4131113916555555, -16));       // .4131113916555555
            amounts const result (q.ceil_out (value, limit));
            expect (result.in != zero);
        }
    }

    void
    test_comparisons()
    {
        testcase ("comparisons");

        amount const amount1 (noissue(), 231);
        amount const amount2 (noissue(), 462);
        amount const amount3 (noissue(), 924);

        quality const q11 (core::amounts (amount1, amount1));
        quality const q12 (core::amounts (amount1, amount2));
        quality const q13 (core::amounts (amount1, amount3));
        quality const q21 (core::amounts (amount2, amount1));
        quality const q31 (core::amounts (amount3, amount1));

        expect (q11 == q11);
        expect (q11 < q12);
        expect (q12 < q13);
        expect (q31 < q21);
        expect (q21 < q11);
        expect (q31 != q21);
    }

    void
    test_composition ()
    {
        testcase ("composition");

        amount const amount1 (noissue(), 231);
        amount const amount2 (noissue(), 462);
        amount const amount3 (noissue(), 924);

        quality const q11 (core::amounts (amount1, amount1));
        quality const q12 (core::amounts (amount1, amount2));
        quality const q13 (core::amounts (amount1, amount3));
        quality const q21 (core::amounts (amount2, amount1));
        quality const q31 (core::amounts (amount3, amount1));

        expect (composed_quality (q12, q21) == q11);

        quality const q13_31 (composed_quality (q13, q31));
        quality const q31_13 (composed_quality (q31, q13));

        expect (q13_31 == q31_13);
        expect (q13_31 == q11);
    }

    void
    test_operations ()
    {
        testcase ("operations");

        quality const q11 (core::amounts (
            amount (noissue(), 731),
            amount (noissue(), 731)));

        quality qa (q11);
        quality qb (q11);

        expect (qa == qb);
        expect (++qa != q11);
        expect (qa != qb);
        expect (--qb != q11);
        expect (qa != qb);
        expect (qb < qa);
        expect (qb++ < qa);
        expect (qb++ < qa);
        expect (qb++ == qa);
        expect (qa < qb);
    }
    void
    run()
    {
        test_comparisons ();
        test_composition ();
        test_operations ();
        test_ceil_in ();
        test_ceil_out ();
        test_raw ();
    }
};

beast_define_testsuite(quality,core,ripple);

}
}
