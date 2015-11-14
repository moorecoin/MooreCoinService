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

#include <beast/container/buffer_view.h>

#include <beast/cxx14/algorithm.h> // <algorithm>

namespace beast {

class buffer_view_test : public unit_test::suite
{
public:
    // returns `true` if the iterator distance matches the size
    template <class fwdit, class size>
    static bool eq_dist (fwdit first, fwdit last, size size)
    {
        auto const dist (std::distance (first, last));
        
        static_assert (std::is_signed <decltype (dist)>::value,
            "dist must be signed");
        
        if (dist < 0)
            return false;

        return static_cast <size> (dist) == size;
    }

    // check the contents of a buffer_view against the container
    template <class c, class t>
    void check (c const& c, buffer_view <t> v)
    {
        expect (! v.empty() || c.empty());
        expect (v.size() == c.size());
        expect (v.max_size() == v.size());
        expect (v.capacity() == v.size());

        expect (eq_dist (v.begin(), v.end(), v.size()));
        expect (eq_dist (v.cbegin(), v.cend(), v.size()));
        expect (eq_dist (v.rbegin(), v.rend(), v.size()));
        expect (eq_dist (v.crbegin(), v.crend(), v.size()));

        expect (std::equal (
            c.cbegin(), c.cend(), v.cbegin(), v.cend()));

        expect (std::equal (
            c.crbegin(), c.crend(), v.crbegin(), v.crend()));

        if (v.size() == c.size())
        {
            if (! v.empty())
            {
                expect (v.front() == c.front());
                expect (v.back() == c.back());
            }

            for (std::size_t i (0); i < v.size(); ++i)
                expect (v[i] == c[i]);
        }
    }

    //--------------------------------------------------------------------------

    // call at() with an invalid index
    template <class v>
    void checkbadindex (v& v,
        std::enable_if_t <
            std::is_const <typename v::value_type>::value>* = 0)
    {
        try
        {
            v.at(0);
            fail();
        }
        catch (std::out_of_range e)
        {
            pass();
        }
        catch (...)
        {
            fail();
        }
    }

    // call at() with an invalid index
    template <class v>
    void checkbadindex (v& v,
        std::enable_if_t <
            ! std::is_const <typename v::value_type>::value>* = 0)
    {
        try
        {
            v.at(0);
            fail();
        }
        catch (std::out_of_range e)
        {
            pass();
        }
        catch (...)
        {
            fail();
        }

        try
        {
            v.at(0) = 1;
            fail();
        }
        catch (std::out_of_range e)
        {
            pass();
        }
        catch (...)
        {
            fail();
        }
    }

    // checks invariants for an empty buffer_view
    template <class v>
    void checkempty (v& v)
    {
        expect (v.empty());
        expect (v.size() == 0);
        expect (v.max_size() == v.size());
        expect (v.capacity() == v.size());
        expect (v.begin() == v.end());
        expect (v.cbegin() == v.cend());
        expect (v.begin() == v.cend());
        expect (v.rbegin() == v.rend());
        expect (v.crbegin() == v.rend());

        checkbadindex (v);
    }

    // test empty containers
    void testempty()
    {
        testcase ("empty");

        buffer_view <char> v1;
        checkempty (v1);

        buffer_view <char> v2;
        swap (v1, v2);
        checkempty (v1);
        checkempty (v2);

        buffer_view <char const> v3 (v2);
        checkempty (v3);
    }

    //--------------------------------------------------------------------------

    // construct const views from a container
    template <class c>
    void testconstructconst (c const& c)
    {
        typedef buffer_view <std::add_const_t <
            typename c::value_type>> v;

        {
            // construct from container
            v v (c);
            check (c, v);

            // construct from buffer_view
            v v2 (v);
            check (c, v2);
        }

        if (! c.empty())
        {
            {
                // construct from const pointer range
                v v (&c.front(), &c.back()+1);
                check (c, v);

                // construct from pointer and size
                v v2 (&c.front(), c.size());
                check (v, v2);
            }

            {
                // construct from non const pointer range
                c cp (c);
                v v (&cp.front(), &cp.back()+1);
                check (cp, v);

                // construct from pointer and size
                v v2 (&cp.front(), cp.size());
                check (v, v2);

                // construct from data and size
                v v3 (v2.data(), v2.size());
                check (c, v3);
            }
        }
    }

    // construct view from a container
    template <class c>
    void testconstruct (c const& c)
    {
        static_assert (! std::is_const <typename c::value_type>::value,
            "container value_type cannot be const");

        testconstructconst (c);

        typedef buffer_view <typename c::value_type> v;

        c cp (c);
        v v (cp);
        check (cp, v);

        std::reverse (v.begin(), v.end());
        check (cp, v);

        expect (std::equal (v.rbegin(), v.rend(),
            c.begin(), c.end()));
    }

    void testconstruct()
    {
        testcase ("std::vector <char>");
        testconstruct (
            std::vector <char> ({'h', 'e', 'l', 'l', 'o'}));

        testcase ("std::string <char>");
        testconstruct (
            std::basic_string <char> ("hello"));
    }

    //--------------------------------------------------------------------------

    void testcoerce()
    {
        testcase ("coerce");

        std::string const s ("hello");
        const_buffer_view <unsigned char> v (s);

        pass();
    }

    //--------------------------------------------------------------------------

    void testassign()
    {
        testcase ("testassign");
        std::vector <int> v1({1, 2, 3});
        buffer_view<int> r1(v1);
        std::vector <int> v2({4, 5, 6, 7});
        buffer_view<int> r2(v2);
        r1 = r2;
        expect (std::equal (r1.begin(), r1.end(), v2.begin(), v2.end()));
    }

    //--------------------------------------------------------------------------

    static_assert (std::is_constructible <buffer_view <int>,
        std::vector <int>&>::value, "");

    static_assert (!std::is_constructible <buffer_view <int>,
        std::vector <int> const&>::value, "");

    static_assert (std::is_constructible <buffer_view <int const>,
        std::vector <int>&>::value, "");

    static_assert (std::is_constructible <buffer_view <int const>,
        std::vector <int> const&>::value, "");

    static_assert (std::is_nothrow_default_constructible <
        buffer_view <int>>::value, "");

    static_assert (std::is_nothrow_destructible <
        buffer_view <int>>::value, "");

    static_assert (std::is_nothrow_copy_constructible <
        buffer_view <int>>::value, "");

    static_assert (std::is_nothrow_copy_assignable <
        buffer_view<int>>::value, "");

    static_assert (std::is_nothrow_move_constructible <
        buffer_view <int>>::value, "");

    static_assert (std::is_nothrow_move_assignable <
        buffer_view <int>>::value, "");

    void run()
    {
        testempty();
        testconstruct();
        testcoerce();
        testassign();
    }
};

beast_define_testsuite(buffer_view,container,beast);

}
