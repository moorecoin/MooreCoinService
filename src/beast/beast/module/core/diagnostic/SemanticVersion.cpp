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
#include <beast/module/core/text/lexicalcast.h>

#include <algorithm>

namespace beast {

std::string print_identifiers (semanticversion::identifier_list const& list)
{
    std::string ret;

    for (auto const& x : list)
    {
        if (!ret.empty ())
            ret += ".";
        ret += x;
    }

    return ret;
}

bool isnumeric (std::string const& s)
{
    int n;

    // must be convertible to an integer
    if (!lexicalcastchecked (n, s))
        return false;

    // must not have leading zeroes
    return std::to_string (n) == s;
}

bool chop (std::string const& what, std::string& input)
{
    auto ret = input.find (what);

    if (ret != 0)
        return false;

    input.erase (0, what.size ());
    return true;
}

bool chopuint (int& value, int limit, std::string& input)
{
    // must not be empty
    if (input.empty ())
        return false;

    auto left_iter = std::find_if_not (input.begin (), input.end (),
        [](std::string::value_type c)
        {
            return std::isdigit (c, std::locale::classic()); 
        });

    std::string item (input.begin (), left_iter);

    // must not be empty
    if (item.empty ())
        return false;

    int n;

    // must be convertible to an integer
    if (!lexicalcastchecked (n, item))
        return false;

    // must not have leading zeroes
    if (std::to_string (n) != item)
        return false;

    // must not be out of range
    if (n < 0 || n > limit)
        return false;

    input.erase (input.begin (), left_iter);
    value = n;

    return true;
}

bool extract_identifier (
    std::string& value, bool allowleadingzeroes, std::string& input)
{
    // must not be empty
    if (input.empty ())
        return false;

    // must not have a leading 0
    if (!allowleadingzeroes && input [0] == '0')
        return false;

    auto last = input.find_first_not_of (
        "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz0123456789-");

    // must not be empty
    if (last == 0)
        return false;

    value = input.substr (0, last);
    input.erase (0, last);
    return true;
}

bool extract_identifiers (
    semanticversion::identifier_list& identifiers,
    bool allowleadingzeroes,
    std::string& input)
{
    if (input.empty ())
        return false;

    do {
        std::string s;

        if (!extract_identifier (s, allowleadingzeroes, input))
            return false;
        identifiers.push_back (s);
    } while (chop (".", input));

    return true;
}

//------------------------------------------------------------------------------

semanticversion::semanticversion ()
    : majorversion (0)
    , minorversion (0)
    , patchversion (0)
{
}

semanticversion::semanticversion (std::string const& version)
    : semanticversion ()
{
    if (!parse (version))
        throw std::invalid_argument ("invalid version string");
}

bool semanticversion::parse (std::string const& input, bool debug)
{
    // may not have leading or trailing whitespace
    auto left_iter = std::find_if_not (input.begin (), input.end (),
        [](std::string::value_type c)
        {
            return std::isspace (c, std::locale::classic()); 
        });

    auto right_iter = std::find_if_not (input.rbegin (), input.rend (),
        [](std::string::value_type c)
        {
            return std::isspace (c, std::locale::classic());
        }).base ();

    // must not be empty!
    if (left_iter >= right_iter)
        return false;

    std::string version (left_iter, right_iter);

    // may not have leading or trailing whitespace
    if (version != input)
        return false;

    // must have major version number
    if (! chopuint (majorversion, std::numeric_limits <int>::max (), version))
        return false;
    if (! chop (".", version))
        return false;

    // must have minor version number
    if (! chopuint (minorversion, std::numeric_limits <int>::max (), version))
        return false;
    if (! chop (".", version))
        return false;

    // must have patch version number
    if (! chopuint (patchversion, std::numeric_limits <int>::max (), version))
        return false;

    // may have pre-release identifier list
    if (chop ("-", version))
    {
        if (!extract_identifiers (prereleaseidentifiers, false, version))
            return false;

        // must not be empty
        if (prereleaseidentifiers.empty ())
            return false;
    }

    // may have metadata identifier list
    if (chop ("+", version))
    {
        if (!extract_identifiers (metadata, true, version))
            return false;

        // must not be empty
        if (metadata.empty ())
            return false;
    }

    return version.empty ();
}

std::string semanticversion::print () const
{
    std::string s;

    s = std::to_string (majorversion) + "." +
        std::to_string (minorversion) + "." +
        std::to_string (patchversion);

    if (!prereleaseidentifiers.empty ())
    {
        s += "-";
        s += print_identifiers (prereleaseidentifiers);
    }

    if (!metadata.empty ())
    {
        s += "+";
        s += print_identifiers (metadata);
    }

    return s;
}

int compare (semanticversion const& lhs, semanticversion const& rhs)
{
    if (lhs.majorversion > rhs.majorversion)
        return 1;
    else if (lhs.majorversion < rhs.majorversion)
        return -1;

    if (lhs.minorversion > rhs.minorversion)
        return 1;
    else if (lhs.minorversion < rhs.minorversion)
        return -1;

    if (lhs.patchversion > rhs.patchversion)
        return 1;
    else if (lhs.patchversion < rhs.patchversion)
        return -1;

    if (lhs.isprerelease () || rhs.isprerelease ())
    {
        // pre-releases have a lower precedence
        if (lhs.isrelease () && rhs.isprerelease ())
            return 1;
        else if (lhs.isprerelease () && rhs.isrelease ())
            return -1;

        // compare pre-release identifiers
        for (int i = 0; i < std::max (lhs.prereleaseidentifiers.size (), rhs.prereleaseidentifiers.size ()); ++i)
        {
            // a larger list of identifiers has a higher precedence
            if (i >= rhs.prereleaseidentifiers.size ())
                return 1;
            else if (i >= lhs.prereleaseidentifiers.size ())
                return -1;

            std::string const& left (lhs.prereleaseidentifiers [i]);
            std::string const& right (rhs.prereleaseidentifiers [i]);

            // numeric identifiers have lower precedence
            if (! isnumeric (left) && isnumeric (right))
                return 1;
            else if (isnumeric (left) && ! isnumeric (right))
                return -1;

            if (isnumeric (left))
            {
                bassert (isnumeric (right));

                int const ileft (lexicalcastthrow <int> (left));
                int const iright (lexicalcastthrow <int> (right));

                if (ileft > iright)
                    return 1;
                else if (ileft < iright)
                    return -1;
            }
            else
            {
                bassert (! isnumeric (right));

                int result = left.compare (right);

                if (result != 0)
                    return result;
            }
        }
    }

    // metadata is ignored

    return 0;
}

//------------------------------------------------------------------------------

class semanticversion_test: public unit_test::suite
{
    typedef semanticversion::identifier_list identifier_list;

public:
    void checkpass (std::string const& input, bool shouldpass = true)
    {
        semanticversion v;

        if (shouldpass )
        {
            expect (v.parse (input));
            expect (v.print () == input);
        }
        else
        {
            expect (! v.parse (input));
        }
    }

    void checkfail (std::string const& input)
    {
        checkpass (input, false);
    }

    // check input and input with appended metadata
    void checkmeta (std::string const& input, bool shouldpass)
    {
        checkpass (input, shouldpass);

        checkpass (input + "+a", shouldpass);
        checkpass (input + "+1", shouldpass);
        checkpass (input + "+a.b", shouldpass);
        checkpass (input + "+ab.cd", shouldpass);

        checkfail (input + "!");
        checkfail (input + "+");
        checkfail (input + "++");
        checkfail (input + "+!");
        checkfail (input + "+.");
        checkfail (input + "+a.!");
    }

    void checkmetafail (std::string const& input)
    {
        checkmeta (input, false);
    }

    // check input, input with appended release data,
    // input with appended metadata, and input with both
    // appended release data and appended metadata
    //
    void checkrelease (std::string const& input, bool shouldpass = true)
    {
        checkmeta (input, shouldpass);

        checkmeta (input + "-1", shouldpass);
        checkmeta (input + "-a", shouldpass);
        checkmeta (input + "-a1", shouldpass);
        checkmeta (input + "-a1.b1", shouldpass);
        checkmeta (input + "-ab.cd", shouldpass);
        checkmeta (input + "--", shouldpass);

        checkmetafail (input + "+");
        checkmetafail (input + "!");
        checkmetafail (input + "-");
        checkmetafail (input + "-!");
        checkmetafail (input + "-.");
        checkmetafail (input + "-a.!");
        checkmetafail (input + "-0.a");
    }

    // checks the major.minor.version string alone and with all
    // possible combinations of release identifiers and metadata.
    //
    void check (std::string const& input, bool shouldpass = true)
    {
        checkrelease (input, shouldpass);
    }

    void negcheck (std::string const& input)
    {
        check (input, false);
    }

    void testparse ()
    {
        testcase ("parsing");

        check ("0.0.0");
        check ("1.2.3");
        check ("2147483647.2147483647.2147483647"); // max int

        // negative values
        negcheck ("-1.2.3");
        negcheck ("1.-2.3");
        negcheck ("1.2.-3");

        // missing parts
        negcheck ("");
        negcheck ("1");
        negcheck ("1.");
        negcheck ("1.2");
        negcheck ("1.2.");
        negcheck (".2.3");

        // whitespace
        negcheck (" 1.2.3");
        negcheck ("1 .2.3");
        negcheck ("1.2 .3");
        negcheck ("1.2.3 ");

        // leading zeroes
        negcheck ("01.2.3");
        negcheck ("1.02.3");
        negcheck ("1.2.03");
    }

    static identifier_list ids ()
    {
        return identifier_list ();
    }

    static identifier_list ids (
        std::string const& s1)
    {
        identifier_list v;
        v.push_back (s1);
        return v;
    }

    static identifier_list ids (
        std::string const& s1, std::string const& s2)
    {
        identifier_list v;
        v.push_back (s1);
        v.push_back (s2);
        return v;
    }

    static identifier_list ids (
        std::string const& s1, std::string const& s2, std::string const& s3)
    {
        identifier_list v;
        v.push_back (s1);
        v.push_back (s2);
        v.push_back (s3);
        return v;
    }

    // checks the decomposition of the input into appropriate values
    void checkvalues (std::string const& input,
        int majorversion,
        int minorversion,
        int patchversion,
        identifier_list const& prereleaseidentifiers = identifier_list (),
        identifier_list const& metadata = identifier_list ())
    {
        semanticversion v;

        expect (v.parse (input));

        expect (v.majorversion == majorversion);
        expect (v.minorversion == minorversion);
        expect (v.patchversion == patchversion);

        expect (v.prereleaseidentifiers == prereleaseidentifiers);
        expect (v.metadata == metadata);
    }

    void testvalues ()
    {
        testcase ("values");

        checkvalues ("0.1.2", 0, 1, 2);
        checkvalues ("1.2.3", 1, 2, 3);
        checkvalues ("1.2.3-rc1", 1, 2, 3, ids ("rc1"));
        checkvalues ("1.2.3-rc1.debug", 1, 2, 3, ids ("rc1", "debug"));
        checkvalues ("1.2.3-rc1.debug.asm", 1, 2, 3, ids ("rc1", "debug", "asm"));
        checkvalues ("1.2.3+full", 1, 2, 3, ids (), ids ("full"));
        checkvalues ("1.2.3+full.prod", 1, 2, 3, ids (), ids ("full", "prod"));
        checkvalues ("1.2.3+full.prod.x86", 1, 2, 3, ids (), ids ("full", "prod", "x86"));
        checkvalues ("1.2.3-rc1.debug.asm+full.prod.x86", 1, 2, 3,
            ids ("rc1", "debug", "asm"), ids ("full", "prod", "x86"));
    }

    // makes sure the left version is less than the right
    void checklessinternal (std::string const& lhs, std::string const& rhs)
    {
        semanticversion left;
        semanticversion right;

        expect (left.parse (lhs));
        expect (right.parse (rhs));

        expect (compare (left, left) == 0);
        expect (compare (right, right) == 0);
        expect (compare (left, right) < 0);
        expect (compare (right, left) > 0);

        expect (left < right);
        expect (right > left);
        expect (left == left);
        expect (right == right);
    }

    void checkless (std::string const& lhs, std::string const& rhs)
    {
        checklessinternal (lhs, rhs);
        checklessinternal (lhs + "+meta", rhs);
        checklessinternal (lhs, rhs + "+meta");
        checklessinternal (lhs + "+meta", rhs + "+meta");
    }

    void testcompare ()
    {
        testcase ("comparisons");

        checkless ("1.0.0-alpha", "1.0.0-alpha.1");
        checkless ("1.0.0-alpha.1", "1.0.0-alpha.beta");
        checkless ("1.0.0-alpha.beta", "1.0.0-beta");
        checkless ("1.0.0-beta", "1.0.0-beta.2");
        checkless ("1.0.0-beta.2", "1.0.0-beta.11");
        checkless ("1.0.0-beta.11", "1.0.0-rc.1");
        checkless ("1.0.0-rc.1", "1.0.0");
        checkless ("0.9.9", "1.0.0");
    }

    void run ()
    {
        testparse ();
        testvalues ();
        testcompare ();
    }
};

beast_define_testsuite(semanticversion,beast_core,beast);

} // beast
