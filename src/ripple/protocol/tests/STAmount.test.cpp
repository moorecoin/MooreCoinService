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
#include <ripple/basics/log.h>
#include <ripple/crypto/cbignum.h>
#include <ripple/protocol/stamount.h>
#include <beast/unit_test/suite.h>

namespace ripple {

class stamount_test : public beast::unit_test::suite
{
public:
    static stamount serializeanddeserialize (stamount const& s)
    {
        serializer ser;
        s.add (ser);

        serializeriterator sit (ser);
        return stamount::deserialize (sit);
    }

    //--------------------------------------------------------------------------

    bool roundtest (int n, int d, int m)
    {
        // check stamount rounding
        stamount num (noissue(), n);
        stamount den (noissue(), d);
        stamount mul (noissue(), m);
        stamount quot = divide (n, d, noissue());
        stamount res = multiply (quot, mul, noissue());

        expect (! res.isnative (), "product should not be native");

        res.roundself ();

        stamount cmp (noissue(), (n * m) / d);

        expect (! cmp.isnative (), "comparison amount should not be native");

        if (res != cmp)
        {
            cmp.throwcomparable (res);

            writelog (lswarning, stamount) << "(" << num.gettext () << "/" << den.gettext () << ") x " << mul.gettext () << " = "
                                       << res.gettext () << " not " << cmp.gettext ();

            fail ("rounding");

            return false;
        }
        else
        {
            pass ();
        }

        return true;
    }

    void multest (int a, int b)
    {
        stamount aa (noissue(), a);
        stamount bb (noissue(), b);
        stamount prod1 (multiply (aa, bb, noissue()));

        expect (! prod1.isnative ());

        stamount prod2 (noissue(), static_cast<std::uint64_t> (a) * static_cast<std::uint64_t> (b));

        if (prod1 != prod2)
        {
            writelog (lswarning, stamount) << "nn(" << aa.getfulltext () << " * " << bb.getfulltext () << ") = " << prod1.getfulltext ()
                                           << " not " << prod2.getfulltext ();

            fail ("multiplication result is not exact");
        }
        else
        {
            pass ();
        }

        aa = a;
        prod1 = multiply (aa, bb, noissue());

        if (prod1 != prod2)
        {
            writelog (lswarning, stamount) << "n(" << aa.getfulltext () << " * " << bb.getfulltext () << ") = " << prod1.getfulltext ()
                                           << " not " << prod2.getfulltext ();
            fail ("multiplication result is not exact");
        }
        else
        {
            pass ();
        }
    }

    //--------------------------------------------------------------------------

    void testsetvalue (
        std::string const& value, issue const& issue, bool success = true)
    {
        stamount amount (issue);

        bool const result = amount.setvalue (value);

        expect (result == success, "parse " + value);

        if (success)
            expect (amount.gettext () == value, "format " + value);
    }

    void testsetvalue ()
    {
        {
            testcase ("set value (native)");

            issue const xrp (xrpissue ());

            // fractional xrp (i.e. drops)
            testsetvalue ("1", xrp);
            testsetvalue ("22", xrp);
            testsetvalue ("333", xrp);
            testsetvalue ("4444", xrp);
            testsetvalue ("55555", xrp);
            testsetvalue ("666666", xrp);

            // 1 xrp up to 100 billion, in powers of 10 (in drops)
            testsetvalue ("1000000", xrp);
            testsetvalue ("10000000", xrp);
            testsetvalue ("100000000", xrp);
            testsetvalue ("1000000000", xrp);
            testsetvalue ("10000000000", xrp);
            testsetvalue ("100000000000", xrp);
            testsetvalue ("1000000000000", xrp);
            testsetvalue ("10000000000000", xrp);
            testsetvalue ("100000000000000", xrp);
            testsetvalue ("1000000000000000", xrp);
            testsetvalue ("10000000000000000", xrp);
            testsetvalue ("100000000000000000", xrp);

            // invalid native values:
            testsetvalue ("1.1", xrp, false);
            testsetvalue ("100000000000000001", xrp, false);
            testsetvalue ("1000000000000000000", xrp, false);
        }

        {
            testcase ("set value (iou)");

            issue const usd (currency (0x5553440000000000), account (0x4985601));

            testsetvalue ("1", usd);
            testsetvalue ("10", usd);
            testsetvalue ("100", usd);
            testsetvalue ("1000", usd);
            testsetvalue ("10000", usd);
            testsetvalue ("100000", usd);
            testsetvalue ("1000000", usd);
            testsetvalue ("10000000", usd);
            testsetvalue ("100000000", usd);
            testsetvalue ("1000000000", usd);
            testsetvalue ("10000000000", usd);

            testsetvalue ("1234567.1", usd);
            testsetvalue ("1234567.12", usd);
            testsetvalue ("1234567.123", usd);
            testsetvalue ("1234567.1234", usd);
            testsetvalue ("1234567.12345", usd);
            testsetvalue ("1234567.123456", usd);
            testsetvalue ("1234567.1234567", usd);
            testsetvalue ("1234567.12345678", usd);
            testsetvalue ("1234567.123456789", usd);
        }
    }

    //--------------------------------------------------------------------------

    void testnativecurrency ()
    {
        testcase ("native currency");
        stamount zerost, one (1), hundred (100);
        // vfalco note why repeat "stamount fail" so many times??
        unexpected (serializeanddeserialize (zerost) != zerost, "stamount fail");
        unexpected (serializeanddeserialize (one) != one, "stamount fail");
        unexpected (serializeanddeserialize (hundred) != hundred, "stamount fail");
        unexpected (!zerost.isnative (), "stamount fail");
        unexpected (!hundred.isnative (), "stamount fail");
        unexpected (zerost != zero, "stamount fail");
        unexpected (one == zero, "stamount fail");
        unexpected (hundred == zero, "stamount fail");
        unexpected ((zerost < zerost), "stamount fail");
        unexpected (! (zerost < one), "stamount fail");
        unexpected (! (zerost < hundred), "stamount fail");
        unexpected ((one < zerost), "stamount fail");
        unexpected ((one < one), "stamount fail");
        unexpected (! (one < hundred), "stamount fail");
        unexpected ((hundred < zerost), "stamount fail");
        unexpected ((hundred < one), "stamount fail");
        unexpected ((hundred < hundred), "stamount fail");
        unexpected ((zerost > zerost), "stamount fail");
        unexpected ((zerost > one), "stamount fail");
        unexpected ((zerost > hundred), "stamount fail");
        unexpected (! (one > zerost), "stamount fail");
        unexpected ((one > one), "stamount fail");
        unexpected ((one > hundred), "stamount fail");
        unexpected (! (hundred > zerost), "stamount fail");
        unexpected (! (hundred > one), "stamount fail");
        unexpected ((hundred > hundred), "stamount fail");
        unexpected (! (zerost <= zerost), "stamount fail");
        unexpected (! (zerost <= one), "stamount fail");
        unexpected (! (zerost <= hundred), "stamount fail");
        unexpected ((one <= zerost), "stamount fail");
        unexpected (! (one <= one), "stamount fail");
        unexpected (! (one <= hundred), "stamount fail");
        unexpected ((hundred <= zerost), "stamount fail");
        unexpected ((hundred <= one), "stamount fail");
        unexpected (! (hundred <= hundred), "stamount fail");
        unexpected (! (zerost >= zerost), "stamount fail");
        unexpected ((zerost >= one), "stamount fail");
        unexpected ((zerost >= hundred), "stamount fail");
        unexpected (! (one >= zerost), "stamount fail");
        unexpected (! (one >= one), "stamount fail");
        unexpected ((one >= hundred), "stamount fail");
        unexpected (! (hundred >= zerost), "stamount fail");
        unexpected (! (hundred >= one), "stamount fail");
        unexpected (! (hundred >= hundred), "stamount fail");
        unexpected (! (zerost == zerost), "stamount fail");
        unexpected ((zerost == one), "stamount fail");
        unexpected ((zerost == hundred), "stamount fail");
        unexpected ((one == zerost), "stamount fail");
        unexpected (! (one == one), "stamount fail");
        unexpected ((one == hundred), "stamount fail");
        unexpected ((hundred == zerost), "stamount fail");
        unexpected ((hundred == one), "stamount fail");
        unexpected (! (hundred == hundred), "stamount fail");
        unexpected ((zerost != zerost), "stamount fail");
        unexpected (! (zerost != one), "stamount fail");
        unexpected (! (zerost != hundred), "stamount fail");
        unexpected (! (one != zerost), "stamount fail");
        unexpected ((one != one), "stamount fail");
        unexpected (! (one != hundred), "stamount fail");
        unexpected (! (hundred != zerost), "stamount fail");
        unexpected (! (hundred != one), "stamount fail");
        unexpected ((hundred != hundred), "stamount fail");
        unexpected (stamount ().gettext () != "0", "stamount fail");
        unexpected (stamount (31).gettext () != "31", "stamount fail");
        unexpected (stamount (310).gettext () != "310", "stamount fail");
        unexpected (to_string (currency ()) != "vrp", "chc(vrp)");
        currency c;
        unexpected (!to_currency (c, "usd"), "create usd currency");
        unexpected (to_string (c) != "usd", "check usd currency");

        const std::string cur = "015841551a748ad2c1f76ff6ecb0cccd00000000";
        unexpected (!to_currency (c, cur), "create custom currency");
        unexpected (to_string (c) != cur, "check custom currency");
        unexpected (c != currency (cur), "check custom currency");
        
        unexpected (to_string (vbccurrency ()) != "vbc", "chc(vbc)");
        unexpected (!to_currency (c, "vbc"), "create vbc currency");
        unexpected (to_string (c) != "vbc", "check vbc currency");
    }

    //--------------------------------------------------------------------------

    void testcustomcurrency ()
    {
        testcase ("custom currency");
        stamount zerost (noissue()), one (noissue(), 1), hundred (noissue(), 100);
        unexpected (serializeanddeserialize (zerost) != zerost, "stamount fail");
        unexpected (serializeanddeserialize (one) != one, "stamount fail");
        unexpected (serializeanddeserialize (hundred) != hundred, "stamount fail");
        unexpected (zerost.isnative (), "stamount fail");
        unexpected (hundred.isnative (), "stamount fail");
        unexpected (zerost != zero, "stamount fail");
        unexpected (one == zero, "stamount fail");
        unexpected (hundred == zero, "stamount fail");
        unexpected ((zerost < zerost), "stamount fail");
        unexpected (! (zerost < one), "stamount fail");
        unexpected (! (zerost < hundred), "stamount fail");
        unexpected ((one < zerost), "stamount fail");
        unexpected ((one < one), "stamount fail");
        unexpected (! (one < hundred), "stamount fail");
        unexpected ((hundred < zerost), "stamount fail");
        unexpected ((hundred < one), "stamount fail");
        unexpected ((hundred < hundred), "stamount fail");
        unexpected ((zerost > zerost), "stamount fail");
        unexpected ((zerost > one), "stamount fail");
        unexpected ((zerost > hundred), "stamount fail");
        unexpected (! (one > zerost), "stamount fail");
        unexpected ((one > one), "stamount fail");
        unexpected ((one > hundred), "stamount fail");
        unexpected (! (hundred > zerost), "stamount fail");
        unexpected (! (hundred > one), "stamount fail");
        unexpected ((hundred > hundred), "stamount fail");
        unexpected (! (zerost <= zerost), "stamount fail");
        unexpected (! (zerost <= one), "stamount fail");
        unexpected (! (zerost <= hundred), "stamount fail");
        unexpected ((one <= zerost), "stamount fail");
        unexpected (! (one <= one), "stamount fail");
        unexpected (! (one <= hundred), "stamount fail");
        unexpected ((hundred <= zerost), "stamount fail");
        unexpected ((hundred <= one), "stamount fail");
        unexpected (! (hundred <= hundred), "stamount fail");
        unexpected (! (zerost >= zerost), "stamount fail");
        unexpected ((zerost >= one), "stamount fail");
        unexpected ((zerost >= hundred), "stamount fail");
        unexpected (! (one >= zerost), "stamount fail");
        unexpected (! (one >= one), "stamount fail");
        unexpected ((one >= hundred), "stamount fail");
        unexpected (! (hundred >= zerost), "stamount fail");
        unexpected (! (hundred >= one), "stamount fail");
        unexpected (! (hundred >= hundred), "stamount fail");
        unexpected (! (zerost == zerost), "stamount fail");
        unexpected ((zerost == one), "stamount fail");
        unexpected ((zerost == hundred), "stamount fail");
        unexpected ((one == zerost), "stamount fail");
        unexpected (! (one == one), "stamount fail");
        unexpected ((one == hundred), "stamount fail");
        unexpected ((hundred == zerost), "stamount fail");
        unexpected ((hundred == one), "stamount fail");
        unexpected (! (hundred == hundred), "stamount fail");
        unexpected ((zerost != zerost), "stamount fail");
        unexpected (! (zerost != one), "stamount fail");
        unexpected (! (zerost != hundred), "stamount fail");
        unexpected (! (one != zerost), "stamount fail");
        unexpected ((one != one), "stamount fail");
        unexpected (! (one != hundred), "stamount fail");
        unexpected (! (hundred != zerost), "stamount fail");
        unexpected (! (hundred != one), "stamount fail");
        unexpected ((hundred != hundred), "stamount fail");
        unexpected (stamount (noissue()).gettext () != "0", "stamount fail");
        unexpected (stamount (noissue(), 31).gettext () != "31", "stamount fail");
        unexpected (stamount (noissue(), 31, 1).gettext () != "310", "stamount fail");
        unexpected (stamount (noissue(), 31, -1).gettext () != "3.1", "stamount fail");
        unexpected (stamount (noissue(), 31, -2).gettext () != "0.31", "stamount fail");
        unexpected (multiply (stamount (noissue(), 20), stamount (3), noissue()).gettext () != "60",
            "stamount multiply fail 1");
        unexpected (multiply (stamount (noissue(), 20), stamount (3), xrpissue ()).gettext () != "60",
            "stamount multiply fail 2");
        unexpected (multiply (stamount (20), stamount (3), noissue()).gettext () != "60",
            "stamount multiply fail 3");
        unexpected (multiply (stamount (20), stamount (3), xrpissue ()).gettext () != "60",
            "stamount multiply fail 4");

        if (divide (stamount (noissue(), 60), stamount (3), noissue()).gettext () != "20")
        {
            writelog (lsfatal, stamount) << "60/3 = " <<
                divide (stamount (noissue(), 60),
                    stamount (3), noissue()).gettext ();
            fail ("stamount divide fail");
        }
        else
        {
            pass ();
        }

        unexpected (divide (stamount (noissue(), 60), stamount (3), xrpissue ()).gettext () != "20",
            "stamount divide fail");

        unexpected (divide (stamount (noissue(), 60), stamount (noissue(), 3), noissue()).gettext () != "20",
            "stamount divide fail");

        unexpected (divide (stamount (noissue(), 60), stamount (noissue(), 3), xrpissue ()).gettext () != "20",
            "stamount divide fail");

        stamount a1 (noissue(), 60), a2 (noissue(), 10, -1);

        unexpected (divide (a2, a1, noissue()) != amountfromquality (getrate (a1, a2)),
            "stamount setrate(getrate) fail");

        unexpected (divide (a1, a2, noissue()) != amountfromquality (getrate (a2, a1)),
            "stamount setrate(getrate) fail");
    }

    //--------------------------------------------------------------------------

    void testarithmetic ()
    {
        testcase ("arithmetic");

        cbignum b;

        for (int i = 0; i < 16; ++i)
        {
            std::uint64_t r = rand ();
            r <<= 32;
            r |= rand ();
            b.setuint64 (r);

            if (b.getuint64 () != r)
            {
                writelog (lsfatal, stamount) << r << " != " << b.getuint64 () << " " << b.tostring (16);
                fail ("setull64/getull64 failure");
            }
            else
            {
                pass ();
            }
        }

        // test currency multiplication and division operations such as
        // converttodisplayamount, converttointernalamount, getrate, getclaimed, and getneeded

        unexpected (getrate (stamount (1), stamount (10)) != (((100ull - 14) << (64 - 8)) | 1000000000000000ull),
            "stamount getrate fail 1");

        unexpected (getrate (stamount (10), stamount (1)) != (((100ull - 16) << (64 - 8)) | 1000000000000000ull),
            "stamount getrate fail 2");

        unexpected (getrate (stamount (noissue(), 1), stamount (noissue(), 10)) != (((100ull - 14) << (64 - 8)) | 1000000000000000ull),
            "stamount getrate fail 3");

        unexpected (getrate (stamount (noissue(), 10), stamount (noissue(), 1)) != (((100ull - 16) << (64 - 8)) | 1000000000000000ull),
            "stamount getrate fail 4");

        unexpected (getrate (stamount (noissue(), 1), stamount (10)) != (((100ull - 14) << (64 - 8)) | 1000000000000000ull),
            "stamount getrate fail 5");

        unexpected (getrate (stamount (noissue(), 10), stamount (1)) != (((100ull - 16) << (64 - 8)) | 1000000000000000ull),
            "stamount getrate fail 6");

        unexpected (getrate (stamount (1), stamount (noissue(), 10)) != (((100ull - 14) << (64 - 8)) | 1000000000000000ull),
            "stamount getrate fail 7");

        unexpected (getrate (stamount (10), stamount (noissue(), 1)) != (((100ull - 16) << (64 - 8)) | 1000000000000000ull),
            "stamount getrate fail 8");

        roundtest (1, 3, 3);
        roundtest (2, 3, 9);
        roundtest (1, 7, 21);
        roundtest (1, 2, 4);
        roundtest (3, 9, 18);
        roundtest (7, 11, 44);

        for (int i = 0; i <= 100000; ++i)
            multest (rand () % 10000000, rand () % 10000000);
    }

    //--------------------------------------------------------------------------

    void testunderflow ()
    {
        testcase ("underflow");

        stamount bignative (stamount::cmaxnative / 2);
        stamount bigvalue (noissue(),
            (stamount::cminvalue + stamount::cmaxvalue) / 2,
            stamount::cmaxoffset - 1);
        stamount smallvalue (noissue(),
            (stamount::cminvalue + stamount::cmaxvalue) / 2,
            stamount::cminoffset + 1);
        stamount zerost (noissue(), 0);

        stamount smallxsmall = multiply (smallvalue, smallvalue, noissue());

        expect (smallxsmall == zero, "smallxsmall != 0");

        stamount bigdsmall = divide (smallvalue, bigvalue, noissue());

        expect (bigdsmall == zero, "small/big != 0: " + bigdsmall.gettext ());

#if 0
        // todo(tom): this test makes no sense - we should have no way to have
        // the currency not be xrp while the account is xrp.
        bigdsmall = divide (smallvalue, bignative, nocurrency(), xrpaccount ());
#endif

        expect (bigdsmall == zero,
            "small/bignative != 0: " + bigdsmall.gettext ());

        bigdsmall = divide (smallvalue, bigvalue, xrpissue ());

        expect (bigdsmall == zero,
            "(small/big)->n != 0: " + bigdsmall.gettext ());

        bigdsmall = divide (smallvalue, bignative, xrpissue ());

        expect (bigdsmall == zero,
            "(small/bignative)->n != 0: " + bigdsmall.gettext ());

        // very bad offer
        std::uint64_t r = getrate (smallvalue, bigvalue);

        expect (r == 0, "getrate(smallout/bigin) != 0");

        // very good offer
        r = getrate (bigvalue, smallvalue);

        expect (r == 0, "getrate(smallin/bigout) != 0");
    }

    //--------------------------------------------------------------------------

    void testrounding ()
    {
        // vfalco todo there are no actual tests here, just printed output?
        //             change this to actually do something.

#if 0
        begintestcase ("rounding ");

        std::uint64_t value = 25000000000000000ull;
        int offset = -14;
        canonicalizeround (false, value, offset, true);

        stamount one (noissue(), 1);
        stamount two (noissue(), 2);
        stamount three (noissue(), 3);

        stamount onethird1 = divround (one, three, noissue(), false);
        stamount onethird2 = divide (one, three, noissue());
        stamount onethird3 = divround (one, three, noissue(), true);
        writelog (lsinfo, stamount) << onethird1;
        writelog (lsinfo, stamount) << onethird2;
        writelog (lsinfo, stamount) << onethird3;

        stamount twothird1 = divround (two, three, noissue(), false);
        stamount twothird2 = divide (two, three, noissue());
        stamount twothird3 = divround (two, three, noissue(), true);
        writelog (lsinfo, stamount) << twothird1;
        writelog (lsinfo, stamount) << twothird2;
        writelog (lsinfo, stamount) << twothird3;

        stamount onea = mulround (onethird1, three, noissue(), false);
        stamount oneb = multiply (onethird2, three, noissue());
        stamount onec = mulround (onethird3, three, noissue(), true);
        writelog (lsinfo, stamount) << onea;
        writelog (lsinfo, stamount) << oneb;
        writelog (lsinfo, stamount) << onec;

        stamount fourthirdsa = addround (twothird2, twothird2, false);
        stamount fourthirdsb = twothird2 + twothird2;
        stamount fourthirdsc = addround (twothird2, twothird2, true);
        writelog (lsinfo, stamount) << fourthirdsa;
        writelog (lsinfo, stamount) << fourthirdsb;
        writelog (lsinfo, stamount) << fourthirdsc;

        stamount driptest1 = mulround (twothird2, two, xrpissue (), false);
        stamount driptest2 = multiply (twothird2, two, xrpissue ());
        stamount driptest3 = mulround (twothird2, two, xrpissue (), true);
        writelog (lsinfo, stamount) << driptest1;
        writelog (lsinfo, stamount) << driptest2;
        writelog (lsinfo, stamount) << driptest3;
#endif
    }

    //--------------------------------------------------------------------------

    void testfloor ()
    {
        testcase ("flooring ");

        stamount smallvalue (noissue(), (uint64_t)25011000000000000ull, -14);
        smallvalue.floor();
        expect (smallvalue == stamount(noissue(), (uint64_t)25000000000000000ull, -14), "floor to integer failed");
        
        smallvalue = stamount(noissue(), (uint64_t)25011000000000000ull, -14);
        smallvalue.floor(-1);
        expect (smallvalue == stamount(noissue(), (uint64_t)25010000000000000ull, -14), "floor to e-1 failed");
        
        smallvalue = stamount(noissue(), (uint64_t)25011980000000000ull, -14, true);
        smallvalue.floor(-2);
        expect (smallvalue == stamount(noissue(), (uint64_t)25011000000000000ull, -14, true), "floor negative to e-2 failed");
        
        smallvalue = stamount(noissue(), (uint64_t)25011980000000000ull, -14, true);
        smallvalue.floor(2);
        expect (smallvalue == stamount(noissue(), (uint64_t)20000000000000000ull, -14, true), "floor negative to e+2 failed");
        
        smallvalue = stamount(noissue(), (uint64_t)25011980000000000ull, -14);
        smallvalue.floor(3);
        expect (smallvalue == zero, "floor to e+3 failed");
    }

    //--------------------------------------------------------------------------

    void run ()
    {
        testsetvalue ();
        testnativecurrency ();
        testcustomcurrency ();
        testarithmetic ();
        testunderflow ();
        testrounding ();
        testfloor ();
    }
};

beast_define_testsuite(stamount,ripple_data,ripple);

} // ripple
