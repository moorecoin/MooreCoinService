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

#include <beast/unit_test/suite.h>

namespace beast {

class lexicalcast_test : public unit_test::suite
{
public:
    template <class inttype>
    static inttype nextrandomint (random& r)
    {
        return static_cast <inttype> (r.nextint64 ());
    }

    template <class inttype>
    void testinteger (inttype in)
    {
        std::string s;
        inttype out (in+1);

        expect (lexicalcastchecked (s, in));
        expect (lexicalcastchecked (out, s));
        expect (out == in);
    }

    template <class inttype>
    void testintegers (random& r)
    {
        {
            std::stringstream ss;
            ss <<
                "random " << typeid (inttype).name ();
            testcase (ss.str());

            for (int i = 0; i < 1000; ++i)
            {
                inttype const value (nextrandomint <inttype> (r));
                testinteger (value);
            }
        }

        {
            std::stringstream ss;
            ss <<
                "numeric_limits <" << typeid (inttype).name () << ">";
            testcase (ss.str());

            testinteger (std::numeric_limits <inttype>::min ());
            testinteger (std::numeric_limits <inttype>::max ());
        }
    }

    void testpathologies()
    {
        testcase("pathologies");
        try
        {
            lexicalcastthrow<int>("\xef\xbc\x91\xef\xbc\x90"); // utf-8 encoded
        }
        catch(badlexicalcast const&)
        {
            pass();
        }
    }

    template <class t>
    void trybadconvert (std::string const& s)
    {
        t out;
        expect (!lexicalcastchecked (out, s), s);
    }

    void testconversionoverflows()
    {
        testcase ("conversion overflows");

        trybadconvert <std::uint64_t> ("99999999999999999999");
        trybadconvert <std::uint32_t> ("4294967300");
        trybadconvert <std::uint16_t> ("75821");
    }

    void testconversionunderflows ()
    {
        testcase ("conversion underflows");

        trybadconvert <std::uint32_t> ("-1");

        trybadconvert <std::int64_t> ("-99999999999999999999");
        trybadconvert <std::int32_t> ("-4294967300");
        trybadconvert <std::int16_t> ("-75821");
    }

    template <class t>
    bool tryedgecase (std::string const& s)
    {
        t ret;

        bool const result = lexicalcastchecked (ret, s);

        if (!result)
            return false;

        return s == std::to_string (ret);
    }

    void testedgecases ()
    {
        testcase ("conversion edge cases");

        expect(tryedgecase <std::uint64_t> ("18446744073709551614"));
        expect(tryedgecase <std::uint64_t> ("18446744073709551615"));
        expect(!tryedgecase <std::uint64_t> ("18446744073709551616"));

        expect(tryedgecase <std::int64_t> ("9223372036854775806"));
        expect(tryedgecase <std::int64_t> ("9223372036854775807"));
        expect(!tryedgecase <std::int64_t> ("9223372036854775808"));

        expect(tryedgecase <std::int64_t> ("-9223372036854775807"));
        expect(tryedgecase <std::int64_t> ("-9223372036854775808"));
        expect(!tryedgecase <std::int64_t> ("-9223372036854775809"));

        expect(tryedgecase <std::uint32_t> ("4294967294"));
        expect(tryedgecase <std::uint32_t> ("4294967295"));
        expect(!tryedgecase <std::uint32_t> ("4294967296"));

        expect(tryedgecase <std::int32_t> ("2147483646"));
        expect(tryedgecase <std::int32_t> ("2147483647"));
        expect(!tryedgecase <std::int32_t> ("2147483648"));

        expect(tryedgecase <std::int32_t> ("-2147483647"));
        expect(tryedgecase <std::int32_t> ("-2147483648"));
        expect(!tryedgecase <std::int32_t> ("-2147483649"));

        expect(tryedgecase <std::uint16_t> ("65534"));
        expect(tryedgecase <std::uint16_t> ("65535"));
        expect(!tryedgecase <std::uint16_t> ("65536"));

        expect(tryedgecase <std::int16_t> ("32766"));
        expect(tryedgecase <std::int16_t> ("32767"));
        expect(!tryedgecase <std::int16_t> ("32768"));

        expect(tryedgecase <std::int16_t> ("-32767"));
        expect(tryedgecase <std::int16_t> ("-32768"));
        expect(!tryedgecase <std::int16_t> ("-32769"));
    }

    template <class t>
    void testthrowconvert(std::string const& s, bool success)
    {
        bool result = !success;
        t out;

        try
        {
            out = lexicalcastthrow <t> (s);
            result = true;
        }
        catch(badlexicalcast const&)
        {
            result = false;
        }

        expect (result == success, s);
    }

    void testthrowingconversions ()
    {
        testcase ("throwing conversion");

        testthrowconvert <std::uint64_t> ("99999999999999999999", false);
        testthrowconvert <std::uint64_t> ("9223372036854775806", true);

        testthrowconvert <std::uint32_t> ("4294967290", true);
        testthrowconvert <std::uint32_t> ("42949672900", false);
        testthrowconvert <std::uint32_t> ("429496729000", false);
        testthrowconvert <std::uint32_t> ("4294967290000", false);

        testthrowconvert <std::int32_t> ("5294967295", false);
        testthrowconvert <std::int32_t> ("-2147483644", true);

        testthrowconvert <std::int16_t> ("66666", false);
        testthrowconvert <std::int16_t> ("-5711", true);
    }

    void testzero ()
    {
        testcase ("zero conversion");

        {
            std::int32_t out;

            expect (lexicalcastchecked (out, "-0"), "0");
            expect (lexicalcastchecked (out, "0"), "0");
            expect (lexicalcastchecked (out, "+0"), "0");
        }

        {
            std::uint32_t out;

            expect (!lexicalcastchecked (out, "-0"), "0");
            expect (lexicalcastchecked (out, "0"), "0");
            expect (lexicalcastchecked (out, "+0"), "0");
        }
    }

    void testentirerange ()
    {
        testcase ("entire range");

        std::int32_t i = std::numeric_limits<std::int16_t>::min();
        std::string const empty("");

        while (i <= std::numeric_limits<std::int16_t>::max())
        {
            std::int16_t j = static_cast<std::int16_t>(i);

            auto actual = std::to_string (j);

            auto result = lexicalcast (j, empty);

            expect (result == actual, actual + " (string to integer)");

            if (result == actual)
            {
                auto number = lexicalcast <std::int16_t> (result);

                if (number != j)
                    expect (false, actual + " (integer to string)");
            }

            i++;
        }
    }

    void run()
    {
        std::int64_t const seedvalue = 50;

        random r (seedvalue);

        testintegers <int> (r);
        testintegers <unsigned int> (r);
        testintegers <short> (r);
        testintegers <unsigned short> (r);
        testintegers <std::int32_t> (r);
        testintegers <std::uint32_t> (r);
        testintegers <std::int64_t> (r);
        testintegers <std::uint64_t> (r);

        testpathologies();
        testconversionoverflows ();
        testconversionunderflows ();
        testthrowingconversions ();
        testzero ();
        testedgecases ();
        testentirerange ();
    }
};

beast_define_testsuite(lexicalcast,beast_core,beast);

} // beast
