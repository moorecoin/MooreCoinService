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

#include <beast/utility/static_initializer.h>
#include <beast/unit_test/suite.h>
#include <atomic>
#include <sstream>
#include <thread>
#include <utility>

namespace beast {

static_assert(__alignof(long) >= 4, "");

class static_initializer_test : public unit_test::suite
{
public:
    // used to create separate instances for each test
    struct cxx11_tag { };
    struct beast_tag { };
    template <std::size_t n, class tag>
    struct case
    {
        enum { count = n };
        typedef tag type;
    };

    struct counts
    {
        counts()
            : calls (0)
            , constructed (0)
            , access (0)
        {
        }

        // number of calls to the constructor
        std::atomic <long> calls;

        // incremented after construction completes
        std::atomic <long> constructed;

        // increment when class is accessed before construction
        std::atomic <long> access;
    };

    /*  this testing singleton detects two conditions:
        1. being accessed before getting fully constructed
        2. getting constructed twice
    */
    template <class tag>
    class test;

    template <class function>
    static
    void
    run_many (std::size_t n, function f);

    template <class tag>
    void
    test (cxx11_tag);

    template <class tag>
    void
    test (beast_tag);

    template <class tag>
    void
    test();

    void
    run ();
};

//------------------------------------------------------------------------------

template <class tag>
class static_initializer_test::test
{
public:
    explicit
    test (counts& counts);

    void
    operator() (counts& counts);
};

template <class tag>
static_initializer_test::test<tag>::test (counts& counts)
{
    ++counts.calls;
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
    ++counts.constructed;
}

template <class tag>
void
static_initializer_test::test<tag>::operator() (counts& counts)
{
    if (! counts.constructed)
        ++counts.access;
}

//------------------------------------------------------------------------------

template <class function>
void
static_initializer_test::run_many (std::size_t n, function f)
{
    std::atomic <bool> start (false);
    std::vector <std::thread> threads;

    threads.reserve (n);

    for (auto i (n); i-- ;)
    {
        threads.emplace_back([&]()
        {
            while (! start.load())
                std::this_thread::yield();
            f();
        });
    }
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
    std::this_thread::yield();
    start.store (true);
    for (auto& thread : threads)
        thread.join();
}

template <class tag>
void
static_initializer_test::test (cxx11_tag)
{
    testcase << "cxx11 " << tag::count << " threads";

    counts counts;

    run_many (tag::count, [&]()
    {
        static test <tag> t (counts);
        t(counts);
    });

#ifdef _msc_ver
    // visual studio 2013 and earlier can exhibit both double
    // construction, and access before construction when function
    // local statics are initialized concurrently.
    //
    expect (counts.constructed > 1 || counts.access > 0);

#else
    expect (counts.constructed == 1 && counts.access == 0);

#endif
}

template <class tag>
void
static_initializer_test::test (beast_tag)
{
    testcase << "beast " << tag::count << " threads";

    counts counts;

    run_many (tag::count, [&counts]()
    {
        static static_initializer <test <tag>> t (counts);
        (*t)(counts);
    });

    expect (counts.constructed == 1 && counts.access == 0);
}

template <class tag>
void
static_initializer_test::test()
{
    test <tag> (typename tag::type {});
}

void
static_initializer_test::run ()
{
    test <case<  4, cxx11_tag>> ();
    test <case< 16, cxx11_tag>> ();
    test <case< 64, cxx11_tag>> ();
    test <case<256, cxx11_tag>> ();

    test <case<  4, beast_tag>> ();
    test <case< 16, beast_tag>> ();
    test <case< 64, beast_tag>> ();
    test <case<256, beast_tag>> ();
}

//------------------------------------------------------------------------------

beast_define_testsuite(static_initializer,utility,beast);

}
