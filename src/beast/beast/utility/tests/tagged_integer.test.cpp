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

#if beast_include_beastconfig
#include <beastconfig.h>
#endif

#include <type_traits>

#include <beast/utility/tagged_integer.h>
#include <beast/unit_test/suite.h>

namespace beast {

class tagged_integer_test 
    : public unit_test::suite
{
private:
    struct tag1 { };
    struct tag2 { };

    typedef tagged_integer <std::uint32_t, tag1> tagint1;
    typedef tagged_integer <std::uint32_t, tag2> tagint2;
    typedef tagged_integer <std::uint64_t, tag1> tagint3;

    // check construction of tagged_integers
    static_assert (std::is_constructible<tagint1, std::uint32_t>::value,
        "tagint1 should be constructible using a std::uint32_t");

    static_assert (!std::is_constructible<tagint1, std::uint64_t>::value,
        "tagint1 should not be constructible using a std::uint64_t");

    static_assert (std::is_constructible<tagint3, std::uint32_t>::value,
        "tagint3 should be constructible using a std::uint32_t");

    static_assert (std::is_constructible<tagint3, std::uint64_t>::value,
        "tagint3 should be constructible using a std::uint64_t");

    // check assignment of tagged_integers
    static_assert (!std::is_assignable<tagint1, std::uint32_t>::value,
        "tagint1 should not be assignable with a std::uint32_t");

    static_assert (!std::is_assignable<tagint1, std::uint64_t>::value,
        "tagint1 should not be assignable with a std::uint64_t");

    static_assert (!std::is_assignable<tagint3, std::uint32_t>::value,
        "tagint3 should not be assignable with a std::uint32_t");

    static_assert (!std::is_assignable<tagint3, std::uint64_t>::value,
        "tagint3 should not be assignable with a std::uint64_t");

    static_assert (std::is_assignable<tagint1, tagint1>::value,
        "tagint1 should be assignable with a tagint1");

    static_assert (!std::is_assignable<tagint1, tagint2>::value,
        "tagint1 should not be assignable with a tagint2");

    static_assert (std::is_assignable<tagint3, tagint3>::value,
        "tagint3 should be assignable with a tagint1");

    static_assert (!std::is_assignable<tagint1, tagint3>::value,
        "tagint1 should not be assignable with a tagint3");

    static_assert (!std::is_assignable<tagint3, tagint1>::value,
        "tagint3 should not be assignable with a tagint1");

    // check convertibility of tagged_integers
    static_assert (!std::is_convertible<std::uint32_t, tagint1>::value,
        "std::uint32_t should not be convertible to a tagint1");

    static_assert (!std::is_convertible<std::uint32_t, tagint3>::value,
        "std::uint32_t should not be convertible to a tagint3");

    static_assert (!std::is_convertible<std::uint64_t, tagint3>::value,
        "std::uint64_t should not be convertible to a tagint3");

    static_assert (!std::is_convertible<std::uint64_t, tagint2>::value,
        "std::uint64_t should not be convertible to a tagint2");

    static_assert (!std::is_convertible<tagint1, tagint2>::value,
        "tagint1 should not be convertible to tagint2");

    static_assert (!std::is_convertible<tagint1, tagint3>::value,
        "tagint1 should not be convertible to tagint3");

    static_assert (!std::is_convertible<tagint2, tagint3>::value,
        "tagint2 should not be convertible to a tagint3");

public:
    void run ()
    {
        tagint1 const zero (0);
        tagint1 const one (1);

        testcase ("comparison operators");

        expect (zero >= zero, "should be greater than or equal");
        expect (zero == zero, "should be equal");
        
        expect (one > zero, "should be greater");
        expect (one >= zero, "should be greater than or equal");
        expect (one != zero, "should not be equal");

        unexpected (one < zero, "should be greater");
        unexpected (one <= zero, "should not be greater than or equal");
        unexpected (one == zero, "should not be equal");

        testcase ("arithmetic operators");

        tagint1 tmp;

        tmp = zero + 0u;
        expect (tmp == zero, "should be equal");

        tmp = 1u + zero;
        expect (tmp == one, "should be equal");

        expect(--tmp == zero, "should be equal");
        expect(tmp++ == zero, "should be equal");
        expect(tmp == one, "should be equal");

        expect(tmp-- == one, "should be equal");
        expect(tmp == zero, "should be equal");
        expect(++tmp == one, "should be equal");

        tmp = zero;
        
        tmp += 1u;
        expect(tmp == one, "should be equal");

        tmp -= 1u;
        expect(tmp == zero, "should be equal");
    }
};

beast_define_testsuite(tagged_integer,utility,beast);

} // beast
