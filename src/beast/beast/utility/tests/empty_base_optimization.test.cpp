//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2014, howard hinnant <howard.hinnant@gmail.com>

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

#include <beast/utility/empty_base_optimization.h>

#include <beast/unit_test/suite.h>

namespace beast {

class empty_base_optimization_test
    : public unit_test::suite
{
public:
    template <class t>
    class test1
        : private empty_base_optimization<t>
    {
        typedef empty_base_optimization<t> base;
        void* m_p;
    public:
        explicit test1 (t const& t)
            : base (t)
        {}

        t&       member()       {return base::member();}
        t const& member() const {return base::member();}
    };

    template <class t>
    class test2
    {
        void* m_p;
        t m_t;
    public:
        explicit test2 (t const& t)
            : m_t (t)
        {}

        t&       member()       {return m_t;}
        t const& member() const {return m_t;}
    };

    struct empty
    {
        operator bool() {return true;}
    };

    static
    bool
    test_one ()
    {
        test1<int> t1(1);
        test2<int> t2(2);
        static_assert(sizeof(t1) == sizeof(t2), "don't optimize for int");
        if (t1.member() != 1)
            return false;
        if (t2.member() != 2)
            return false;
        return true;
    }

    static
    bool
    test_two ()
    {
        test1<empty> t1((empty()));
        test2<empty> t2((empty()));
        static_assert(sizeof(t1) < sizeof(t2), "do optimize for empty");
        if (t1.member() != true)
            return false;
        if (t2.member() != true)
            return false;
        return true;
    }

    void
    run ()
    {
        expect (test_one());
        expect (test_two());
        pass ();
    }
};

beast_define_testsuite(empty_base_optimization,utility,beast);

} // beast
