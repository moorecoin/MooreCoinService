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

#include <beast/chrono/manual_clock.h>
#include <beast/unit_test/suite.h>

#include <beast/container/aged_set.h>
#include <beast/container/aged_map.h>
#include <beast/container/aged_multiset.h>
#include <beast/container/aged_multimap.h>
#include <beast/container/aged_unordered_set.h>
#include <beast/container/aged_unordered_map.h>
#include <beast/container/aged_unordered_multiset.h>
#include <beast/container/aged_unordered_multimap.h>

#include <vector>
#include <list>

#ifndef beast_aged_unordered_no_alloc_defaultctor
# ifdef _msc_ver
#  define beast_aged_unordered_no_alloc_defaultctor 0
# else
#  define beast_aged_unordered_no_alloc_defaultctor 1
# endif
#endif

#ifndef beast_container_extract_noref
# ifdef _msc_ver
#  define beast_container_extract_noref 1
# else
#  define beast_container_extract_noref 1
# endif
#endif

namespace beast {

class aged_associative_container_test_base : public unit_test::suite
{
public:
    template <class t>
    struct compt
    {
        explicit compt (int)
        {
        }

        compt (compt const&)
        {
        }

        bool operator() (t const& lhs, t const& rhs) const
        {
            return m_less (lhs, rhs);
        }

    private:
        compt () = delete;
        std::less <t> m_less;
    };

    template <class t>
    class hasht
    {
    public:
        explicit hasht (int)
        {
        }

        std::size_t operator() (t const& t) const
        {
            return m_hash (t);
        }

    private:
        hasht() = delete;
        std::hash <t> m_hash;
    };

    template <class t>
    struct equalt
    {
    public:
        explicit equalt (int)
        {
        }

        bool operator() (t const& lhs, t const& rhs) const
        {
            return m_eq (lhs, rhs);
        }

    private:
        equalt() = delete;
        std::equal_to <t> m_eq;
    };

    template <class t>
    struct alloct
    {
        typedef t value_type;

        //typedef propagate_on_container_swap : std::true_type::type;

        template <class u>
        struct rebind
        {
            typedef alloct <u> other;
        };

        explicit alloct (int)
        {
        }

        alloct (alloct const&)
        {
        }

        template <class u>
        alloct (alloct <u> const&)
        {
        }

        template <class u>
        bool operator== (alloct <u> const&) const
        {
            return true;
        }

        t* allocate (std::size_t n, t const* = 0)
        {
            return static_cast <t*> (
                ::operator new (n * sizeof(t)));
        }

        void deallocate (t* p, std::size_t)
        {
            ::operator delete (p);
        }

#if ! beast_aged_unordered_no_alloc_defaultctor
        alloct ()
        {
        }
#else
    private:
        alloct() = delete;
#endif
    };

    //--------------------------------------------------------------------------

    // ordered
    template <class base, bool isunordered>
    class maybeunordered : public base
    {
    public:
        typedef std::less <typename base::key> comp;
        typedef compt <typename base::key> mycomp;

    protected:
        static std::string name_ordered_part()
        {
            return "";
        }
    };

    // unordered
    template <class base>
    class maybeunordered <base, true> : public base
    {
    public:
        typedef std::hash <typename base::key> hash;
        typedef std::equal_to <typename base::key> equal;
        typedef hasht <typename base::key> myhash;
        typedef equalt <typename base::key> myequal;

    protected:
        static std::string name_ordered_part()
        {
            return "unordered_";
        }
    };

    // unique
    template <class base, bool ismulti>
    class maybemulti : public base
    {
    public:
    protected:
        static std::string name_multi_part()
        {
            return "";
        }
    };

    // multi
    template <class base>
    class maybemulti <base, true> : public base
    {
    public:
    protected:
        static std::string name_multi_part()
        {
            return "multi";
        }
    };

    // set
    template <class base, bool ismap>
    class maybemap : public base
    {
    public:
        typedef void t;
        typedef typename base::key value;
        typedef std::vector <value> values;

        static typename base::key const& extract (value const& value)
        {
            return value;
        }

        static values values()
        {
            values v {
                "apple",
                "banana",
                "cherry",
                "grape",
                "orange",
            };
            return v;
        };

    protected:
        static std::string name_map_part()
        {
            return "set";
        }
    };

    // map
    template <class base>
    class maybemap <base, true> : public base
    {
    public:
        typedef int t;
        typedef std::pair <typename base::key, t> value;
        typedef std::vector <value> values;

        static typename base::key const& extract (value const& value)
        {
            return value.first;
        }

        static values values()
        {
            values v {
                std::make_pair ("apple",  1),
                std::make_pair ("banana", 2),
                std::make_pair ("cherry", 3),
                std::make_pair ("grape",  4),
                std::make_pair ("orange", 5)
            };
            return v;
        };

    protected:
        static std::string name_map_part()
        {
            return "map";
        }
    };

    //--------------------------------------------------------------------------

    // ordered
    template <
        class base,
        bool isunordered = base::is_unordered::value
    >
    struct conttype
    {
        template <
            class compare = std::less <typename base::key>,
            class allocator = std::allocator <typename base::value>
        >
        using cont = detail::aged_ordered_container <
            base::is_multi::value, base::is_map::value, typename base::key,
                typename base::t, typename base::clock, compare, allocator>;
    };

    // unordered
    template <
        class base
    >
    struct conttype <base, true>
    {
        template <
            class hash = std::hash <typename base::key>,
            class keyequal = std::equal_to <typename base::key>,
            class allocator = std::allocator <typename base::value>
        >
        using cont = detail::aged_unordered_container <
            base::is_multi::value, base::is_map::value,
                typename base::key, typename base::t, typename base::clock,
                    hash, keyequal, allocator>;
    };

    //--------------------------------------------------------------------------

    struct testtraitsbase
    {
        typedef std::string key;
        typedef std::chrono::steady_clock clock;
        typedef manual_clock<clock> manualclock;
    };

    template <bool isunordered, bool ismulti, bool ismap>
    struct testtraitshelper
        : maybeunordered <maybemulti <maybemap <
            testtraitsbase, ismap>, ismulti>, isunordered>
    {
    private:
        typedef maybeunordered <maybemulti <maybemap <
            testtraitsbase, ismap>, ismulti>, isunordered> base;

    public:
        using typename base::key;

        typedef std::integral_constant <bool, isunordered> is_unordered;
        typedef std::integral_constant <bool, ismulti> is_multi;
        typedef std::integral_constant <bool, ismap> is_map;

        typedef std::allocator <typename base::value> alloc;
        typedef alloct <typename base::value> myalloc;

        static std::string name()
        {
            return std::string ("aged_") +
                base::name_ordered_part() +
                    base::name_multi_part() +
                        base::name_map_part();
        }
    };

    template <bool isunordered, bool ismulti, bool ismap>
    struct testtraits
        : testtraitshelper <isunordered, ismulti, ismap>
        , conttype <testtraitshelper <isunordered, ismulti, ismap>>
    {
    };

    template <class cont>
    static std::string name (cont const&)
    {
        return testtraits <
            cont::is_unordered,
            cont::is_multi,
            cont::is_map>::name();
    }

    template <class traits>
    struct equal_value
    {
        bool operator() (typename traits::value const& lhs,
            typename traits::value const& rhs)
        {
            return traits::extract (lhs) == traits::extract (rhs);
        }
    };

    template <class cont>
    static
    std::list <typename cont::value_type>
    make_list (cont const& c)
    {
        return std::list <typename cont::value_type> (
            c.begin(), c.end());
    }

    //--------------------------------------------------------------------------

    template <
        class container,
        class values
    >
    typename std::enable_if <
        container::is_map::value && ! container::is_multi::value>::type
    checkmapcontents (container& c, values const& v);

    template <
        class container,
        class values
    >
    typename std::enable_if <!
        (container::is_map::value && ! container::is_multi::value)>::type
    checkmapcontents (container, values const&)
    {
    }

    // unordered
    template <
        class c,
        class values
    >
    typename std::enable_if <
        std::remove_reference <c>::type::is_unordered::value>::type
    checkunorderedcontentsrefref (c&& c, values const& v);

    template <
        class c,
        class values
    >
    typename std::enable_if <!
        std::remove_reference <c>::type::is_unordered::value>::type
    checkunorderedcontentsrefref (c&&, values const&)
    {
    }

    template <class c, class values>
    void checkcontentsrefref (c&& c, values const& v);

    template <class cont, class values>
    void checkcontents (cont& c, values const& v);

    template <class cont>
    void checkcontents (cont& c);

    //--------------------------------------------------------------------------

    // ordered
    template <bool isunordered, bool ismulti, bool ismap>
    typename std::enable_if <! isunordered>::type
    testconstructempty ();

    // unordered
    template <bool isunordered, bool ismulti, bool ismap>
    typename std::enable_if <isunordered>::type
    testconstructempty ();

    // ordered
    template <bool isunordered, bool ismulti, bool ismap>
    typename std::enable_if <! isunordered>::type
    testconstructrange ();

    // unordered
    template <bool isunordered, bool ismulti, bool ismap>
    typename std::enable_if <isunordered>::type
    testconstructrange ();

    // ordered
    template <bool isunordered, bool ismulti, bool ismap>
    typename std::enable_if <! isunordered>::type
    testconstructinitlist ();

    // unordered
    template <bool isunordered, bool ismulti, bool ismap>
    typename std::enable_if <isunordered>::type
    testconstructinitlist ();

    //--------------------------------------------------------------------------

    template <bool isunordered, bool ismulti, bool ismap>
    void
    testcopymove ();

    //--------------------------------------------------------------------------

    template <bool isunordered, bool ismulti, bool ismap>
    void
    testiterator ();

    // unordered containers don't have reverse iterators
    template <bool isunordered, bool ismulti, bool ismap>
    typename std::enable_if <! isunordered>::type
    testreverseiterator();

    template <bool isunordered, bool ismulti, bool ismap>
    typename std::enable_if <isunordered>::type
    testreverseiterator()
    {
    }

    //--------------------------------------------------------------------------

    template <class container, class values>
    void checkinsertcopy (container& c, values const& v);

    template <class container, class values>
    void checkinsertmove (container& c, values const& v);

    template <class container, class values>
    void checkinserthintcopy (container& c, values const& v);

    template <class container, class values>
    void checkinserthintmove (container& c, values const& v);

    template <class container, class values>
    void checkemplace (container& c, values const& v);

    template <class container, class values>
    void checkemplacehint (container& c, values const& v);

    template <bool isunordered, bool ismulti, bool ismap>
    void testmodifiers();

    //--------------------------------------------------------------------------

    template <bool isunordered, bool ismulti, bool ismap>
    void
    testchronological ();

    //--------------------------------------------------------------------------

    // map, unordered_map
    template <bool isunordered, bool ismulti, bool ismap>
    typename std::enable_if <ismap && ! ismulti>::type
    testarraycreate();

    template <bool isunordered, bool ismulti, bool ismap>
    typename std::enable_if <! (ismap && ! ismulti)>::type
    testarraycreate()
    {
    }

    //--------------------------------------------------------------------------

    // helpers for erase tests
    template <class container, class values>
    void reversefillagedcontainer(container& c, values const& v);

    template <class iter>
    iter nexttoenditer (iter const beginiter, iter const enditr);

   //--------------------------------------------------------------------------

    template <class container, class iter>
    bool doelementerase (container& c, iter const beginitr, iter const enditr);

    template <bool isunordered, bool ismulti, bool ismap>
    void testelementerase();

    //--------------------------------------------------------------------------

    template <class container, class beginendsrc>
    void dorangeerase (container& c, beginendsrc const& beginendsrc);

    template <bool isunordered, bool ismulti, bool ismap>
    void testrangeerase();

    //--------------------------------------------------------------------------

    // ordered
    template <bool isunordered, bool ismulti, bool ismap>
    typename std::enable_if <! isunordered>::type
    testcompare ();

    template <bool isunordered, bool ismulti, bool ismap>
    typename std::enable_if <isunordered>::type
    testcompare ()
    {
    }

    //--------------------------------------------------------------------------

    // ordered
    template <bool isunordered, bool ismulti, bool ismap>
    typename std::enable_if <! isunordered>::type
    testobservers();

    // unordered
    template <bool isunordered, bool ismulti, bool ismap>
    typename std::enable_if <isunordered>::type
    testobservers();

    //--------------------------------------------------------------------------

    template <bool isunordered, bool ismulti, bool ismap>
    void testmaybeunorderedmultimap ();

    template <bool isunordered, bool ismulti>
    void testmaybeunorderedmulti();

    template <bool isunordered>
    void testmaybeunordered();
};

//------------------------------------------------------------------------------

// check contents via at() and operator[]
// map, unordered_map
template <
    class container,
    class values
>
typename std::enable_if <
    container::is_map::value && ! container::is_multi::value>::type
aged_associative_container_test_base::
checkmapcontents (container& c, values const& v)
{
    if (v.empty())
    {
        expect (c.empty());
        expect (c.size() == 0);
        return;
    }

    try
    {
        // make sure no exception is thrown
        for (auto const& e : v)
            c.at (e.first);
        for (auto const& e : v)
            expect (c.operator[](e.first) == e.second);
    }
    catch (std::out_of_range const&)
    {
        fail ("caught exception");
    }
}

// unordered
template <
    class c,
    class values
>
typename std::enable_if <
    std::remove_reference <c>::type::is_unordered::value>::type
aged_associative_container_test_base::
checkunorderedcontentsrefref (c&& c, values const& v)
{
    typedef typename std::remove_reference <c>::type cont;
    typedef testtraits <
        cont::is_unordered::value,
            cont::is_multi::value,
                cont::is_map::value
                > traits;
    typedef typename cont::size_type size_type;
    auto const hash (c.hash_function());
    auto const key_eq (c.key_eq());
    for (size_type i (0); i < c.bucket_count(); ++i)
    {
        auto const last (c.end(i));
        for (auto iter (c.begin (i)); iter != last; ++iter)
        {
            auto const match (std::find_if (v.begin(), v.end(),
                [iter](typename values::value_type const& e)
                {
                    return traits::extract (*iter) ==
                        traits::extract (e);
                }));
            expect (match != v.end());
            expect (key_eq (traits::extract (*iter),
                traits::extract (*match)));
            expect (hash (traits::extract (*iter)) ==
                hash (traits::extract (*match)));
        }
    }
}

template <class c, class values>
void
aged_associative_container_test_base::
checkcontentsrefref (c&& c, values const& v)
{
    typedef typename std::remove_reference <c>::type cont;
    typedef testtraits <
        cont::is_unordered::value,
            cont::is_multi::value,
                cont::is_map::value
                > traits;
    typedef typename cont::size_type size_type;

    expect (c.size() == v.size());
    expect (size_type (std::distance (
        c.begin(), c.end())) == v.size());
    expect (size_type (std::distance (
        c.cbegin(), c.cend())) == v.size());
    expect (size_type (std::distance (
        c.chronological.begin(), c.chronological.end())) == v.size());
    expect (size_type (std::distance (
        c.chronological.cbegin(), c.chronological.cend())) == v.size());
    expect (size_type (std::distance (
        c.chronological.rbegin(), c.chronological.rend())) == v.size());
    expect (size_type (std::distance (
        c.chronological.crbegin(), c.chronological.crend())) == v.size());

    checkunorderedcontentsrefref (c, v);
}

template <class cont, class values>
void
aged_associative_container_test_base::
checkcontents (cont& c, values const& v)
{
    checkcontentsrefref (c, v);
    checkcontentsrefref (const_cast <cont const&> (c), v);
    checkmapcontents (c, v);
}

template <class cont>
void
aged_associative_container_test_base::
checkcontents (cont& c)
{
    typedef testtraits <
        cont::is_unordered::value,
            cont::is_multi::value,
                cont::is_map::value
                > traits;
    typedef typename traits::values values;
    checkcontents (c, values());
}

//------------------------------------------------------------------------------
//
// construction
//
//------------------------------------------------------------------------------

// ordered
template <bool isunordered, bool ismulti, bool ismap>
typename std::enable_if <! isunordered>::type
aged_associative_container_test_base::
testconstructempty ()
{
    typedef testtraits <isunordered, ismulti, ismap> traits;
    typedef typename traits::value value;
    typedef typename traits::key key;
    typedef typename traits::t t;
    typedef typename traits::clock clock;
    typedef typename traits::comp comp;
    typedef typename traits::alloc alloc;
    typedef typename traits::mycomp mycomp;
    typedef typename traits::myalloc myalloc;
    typename traits::manualclock clock;

    //testcase (traits::name() + " empty");
    testcase ("empty");

    {
        typename traits::template cont <comp, alloc> c (
            clock);
        checkcontents (c);
    }

    {
        typename traits::template cont <mycomp, alloc> c (
            clock, mycomp(1));
        checkcontents (c);
    }

    {
        typename traits::template cont <comp, myalloc> c (
            clock, myalloc(1));
        checkcontents (c);
    }

    {
        typename traits::template cont <mycomp, myalloc> c (
            clock, mycomp(1), myalloc(1));
        checkcontents (c);
    }
}

// unordered
template <bool isunordered, bool ismulti, bool ismap>
typename std::enable_if <isunordered>::type
aged_associative_container_test_base::
testconstructempty ()
{
    typedef testtraits <isunordered, ismulti, ismap> traits;
    typedef typename traits::value value;
    typedef typename traits::key key;
    typedef typename traits::t t;
    typedef typename traits::clock clock;
    typedef typename traits::hash hash;
    typedef typename traits::equal equal;
    typedef typename traits::alloc alloc;
    typedef typename traits::myhash myhash;
    typedef typename traits::myequal myequal;
    typedef typename traits::myalloc myalloc;
    typename traits::manualclock clock;

    //testcase (traits::name() + " empty");
    testcase ("empty");
    {
        typename traits::template cont <hash, equal, alloc> c (
            clock);
        checkcontents (c);
    }

    {
        typename traits::template cont <myhash, equal, alloc> c (
            clock, myhash(1));
        checkcontents (c);
    }

    {
        typename traits::template cont <hash, myequal, alloc> c (
            clock, myequal (1));
        checkcontents (c);
    }

    {
        typename traits::template cont <hash, equal, myalloc> c (
            clock, myalloc (1));
        checkcontents (c);
    }

    {
        typename traits::template cont <myhash, myequal, alloc> c (
            clock, myhash(1), myequal(1));
        checkcontents (c);
    }

    {
        typename traits::template cont <myhash, equal, myalloc> c (
            clock, myhash(1), myalloc(1));
        checkcontents (c);
    }

    {
        typename traits::template cont <hash, myequal, myalloc> c (
            clock, myequal(1), myalloc(1));
        checkcontents (c);
    }

    {
        typename traits::template cont <myhash, myequal, myalloc> c (
            clock, myhash(1), myequal(1), myalloc(1));
        checkcontents (c);
    }
}

// ordered
template <bool isunordered, bool ismulti, bool ismap>
typename std::enable_if <! isunordered>::type
aged_associative_container_test_base::
testconstructrange ()
{
    typedef testtraits <isunordered, ismulti, ismap> traits;
    typedef typename traits::value value;
    typedef typename traits::key key;
    typedef typename traits::t t;
    typedef typename traits::clock clock;
    typedef typename traits::comp comp;
    typedef typename traits::alloc alloc;
    typedef typename traits::mycomp mycomp;
    typedef typename traits::myalloc myalloc;
    typename traits::manualclock clock;
    auto const v (traits::values());

    //testcase (traits::name() + " range");
    testcase ("range");

    {
        typename traits::template cont <comp, alloc> c (
            v.begin(), v.end(),
            clock);
        checkcontents (c, v);
    }

    {
        typename traits::template cont <mycomp, alloc> c (
            v.begin(), v.end(),
            clock, mycomp(1));
        checkcontents (c, v);
    }

    {
        typename traits::template cont <comp, myalloc> c (
            v.begin(), v.end(),
            clock, myalloc(1));
        checkcontents (c, v);
    }

    {
        typename traits::template cont <mycomp, myalloc> c (
            v.begin(), v.end(),
            clock, mycomp(1), myalloc(1));
        checkcontents (c, v);

    }

    // swap

    {
        typename traits::template cont <comp, alloc> c1 (
            v.begin(), v.end(),
            clock);
        typename traits::template cont <comp, alloc> c2 (
            clock);
        std::swap (c1, c2);
        checkcontents (c2, v);
    }
}

// unordered
template <bool isunordered, bool ismulti, bool ismap>
typename std::enable_if <isunordered>::type
aged_associative_container_test_base::
testconstructrange ()
{
    typedef testtraits <isunordered, ismulti, ismap> traits;
    typedef typename traits::value value;
    typedef typename traits::key key;
    typedef typename traits::t t;
    typedef typename traits::clock clock;
    typedef typename traits::hash hash;
    typedef typename traits::equal equal;
    typedef typename traits::alloc alloc;
    typedef typename traits::myhash myhash;
    typedef typename traits::myequal myequal;
    typedef typename traits::myalloc myalloc;
    typename traits::manualclock clock;
    auto const v (traits::values());

    //testcase (traits::name() + " range");
    testcase ("range");

    {
        typename traits::template cont <hash, equal, alloc> c (
            v.begin(), v.end(),
            clock);
        checkcontents (c, v);
    }

    {
        typename traits::template cont <myhash, equal, alloc> c (
            v.begin(), v.end(),
            clock, myhash(1));
        checkcontents (c, v);
    }

    {
        typename traits::template cont <hash, myequal, alloc> c (
            v.begin(), v.end(),
            clock, myequal (1));
        checkcontents (c, v);
    }

    {
        typename traits::template cont <hash, equal, myalloc> c (
            v.begin(), v.end(),
            clock, myalloc (1));
        checkcontents (c, v);
    }

    {
        typename traits::template cont <myhash, myequal, alloc> c (
            v.begin(), v.end(),
            clock, myhash(1), myequal(1));
        checkcontents (c, v);
    }

    {
        typename traits::template cont <myhash, equal, myalloc> c (
            v.begin(), v.end(),
            clock, myhash(1), myalloc(1));
        checkcontents (c, v);
    }

    {
        typename traits::template cont <hash, myequal, myalloc> c (
            v.begin(), v.end(),
            clock, myequal(1), myalloc(1));
        checkcontents (c, v);
    }

    {
        typename traits::template cont <myhash, myequal, myalloc> c (
            v.begin(), v.end(),
            clock, myhash(1), myequal(1), myalloc(1));
        checkcontents (c, v);
    }
}

// ordered
template <bool isunordered, bool ismulti, bool ismap>
typename std::enable_if <! isunordered>::type
aged_associative_container_test_base::
testconstructinitlist ()
{
    typedef testtraits <isunordered, ismulti, ismap> traits;
    typedef typename traits::value value;
    typedef typename traits::key key;
    typedef typename traits::t t;
    typedef typename traits::clock clock;
    typedef typename traits::comp comp;
    typedef typename traits::alloc alloc;
    typedef typename traits::mycomp mycomp;
    typedef typename traits::myalloc myalloc;
    typename traits::manualclock clock;

    //testcase (traits::name() + " init-list");
    testcase ("init-list");

    // vfalco todo

    pass();
}

// unordered
template <bool isunordered, bool ismulti, bool ismap>
typename std::enable_if <isunordered>::type
aged_associative_container_test_base::
testconstructinitlist ()
{
    typedef testtraits <isunordered, ismulti, ismap> traits;
    typedef typename traits::value value;
    typedef typename traits::key key;
    typedef typename traits::t t;
    typedef typename traits::clock clock;
    typedef typename traits::hash hash;
    typedef typename traits::equal equal;
    typedef typename traits::alloc alloc;
    typedef typename traits::myhash myhash;
    typedef typename traits::myequal myequal;
    typedef typename traits::myalloc myalloc;
    typename traits::manualclock clock;

    //testcase (traits::name() + " init-list");
    testcase ("init-list");

    // vfalco todo
    pass();
}

//------------------------------------------------------------------------------
//
// copy/move construction and assign
//
//------------------------------------------------------------------------------

template <bool isunordered, bool ismulti, bool ismap>
void
aged_associative_container_test_base::
testcopymove ()
{
    typedef testtraits <isunordered, ismulti, ismap> traits;
    typedef typename traits::value value;
    typedef typename traits::alloc alloc;
    typename traits::manualclock clock;
    auto const v (traits::values());

    //testcase (traits::name() + " copy/move");
    testcase ("copy/move");

    // copy

    {
        typename traits::template cont <> c (
            v.begin(), v.end(), clock);
        typename traits::template cont <> c2 (c);
        checkcontents (c, v);
        checkcontents (c2, v);
        expect (c == c2);
        unexpected (c != c2);
    }

    {
        typename traits::template cont <> c (
            v.begin(), v.end(), clock);
        typename traits::template cont <> c2 (c, alloc());
        checkcontents (c, v);
        checkcontents (c2, v);
        expect (c == c2);
        unexpected (c != c2);
    }

    {
        typename traits::template cont <> c (
            v.begin(), v.end(), clock);
        typename traits::template cont <> c2 (
            clock);
        c2 = c;
        checkcontents (c, v);
        checkcontents (c2, v);
        expect (c == c2);
        unexpected (c != c2);
    }

    // move

    {
        typename traits::template cont <> c (
            v.begin(), v.end(), clock);
        typename traits::template cont <> c2 (
            std::move (c));
        checkcontents (c2, v);
    }

    {
        typename traits::template cont <> c (
            v.begin(), v.end(), clock);
        typename traits::template cont <> c2 (
            std::move (c), alloc());
        checkcontents (c2, v);
    }

    {
        typename traits::template cont <> c (
            v.begin(), v.end(), clock);
        typename traits::template cont <> c2 (
            clock);
        c2 = std::move (c);
        checkcontents (c2, v);
    }
}

//------------------------------------------------------------------------------
//
// iterator construction and assignment
//
//------------------------------------------------------------------------------

template <bool isunordered, bool ismulti, bool ismap>
void
aged_associative_container_test_base::
testiterator()
{
    typedef testtraits <isunordered, ismulti, ismap> traits;
    typedef typename traits::value value;
    typedef typename traits::alloc alloc;
    typename traits::manualclock clock;
    auto const v (traits::values());

    //testcase (traits::name() + " iterators");
    testcase ("iterator");

    typename traits::template cont <> c {clock};

    using iterator = decltype (c.begin());
    using const_iterator = decltype (c.cbegin());

    // should be able to construct or assign an iterator from an iterator.
    iterator nnit_0 {c.begin()};
    iterator nnit_1 {nnit_0};
    expect (nnit_0 == nnit_1, "iterator constructor failed");
    iterator nnit_2;
    nnit_2 = nnit_1;
    expect (nnit_1 == nnit_2, "iterator assignment failed");

    // should be able to construct or assign a const_iterator from a
    // const_iterator.
    const_iterator ccit_0 {c.cbegin()};
    const_iterator ccit_1 {ccit_0};
    expect (ccit_0 == ccit_1, "const_iterator constructor failed");
    const_iterator ccit_2;
    ccit_2 = ccit_1;
    expect (ccit_1 == ccit_2, "const_iterator assignment failed");

    // comparison between iterator and const_iterator is okay
    expect (nnit_0 == ccit_0,
        "comparing an iterator to a const_iterator failed");
    expect (ccit_1 == nnit_1,
        "comparing a const_iterator to an iterator failed");

    // should be able to construct a const_iterator from an iterator.
    const_iterator ncit_3 {c.begin()};
    const_iterator ncit_4 {nnit_0};
    expect (ncit_3 == ncit_4,
        "const_iterator construction from iterator failed");
    const_iterator ncit_5;
    ncit_5 = nnit_2;
    expect (ncit_5 == ncit_4,
        "const_iterator assignment from iterator failed");

    // none of these should compile because they construct or assign to a
    // non-const iterator with a const_iterator.

//  iterator cnit_0 {c.cbegin()};

//  iterator cnit_1 {ccit_0};

//  iterator cnit_2;
//  cnit_2 = ccit_2;
}

template <bool isunordered, bool ismulti, bool ismap>
typename std::enable_if <! isunordered>::type
aged_associative_container_test_base::
testreverseiterator()
{
    typedef testtraits <isunordered, ismulti, ismap> traits;
    typedef typename traits::value value;
    typedef typename traits::alloc alloc;
    typename traits::manualclock clock;
    auto const v (traits::values());

    //testcase (traits::name() + " reverse_iterators");
    testcase ("reverse_iterator");

    typename traits::template cont <> c {clock};

    using iterator = decltype (c.begin());
    using const_iterator = decltype (c.cbegin());
    using reverse_iterator = decltype (c.rbegin());
    using const_reverse_iterator = decltype (c.crbegin());

    // naming decoder ring
    //       constructed from ------+ +----- constructed type
    //                              /\/\  -- character pairs
    //                              xaybit
    //  r (reverse) or f (forward)--^-^
    //                               ^-^------ c (const) or n (non-const)

    // should be able to construct or assign a reverse_iterator from a
    // reverse_iterator.
    reverse_iterator rnrnit_0 {c.rbegin()};
    reverse_iterator rnrnit_1 {rnrnit_0};
    expect (rnrnit_0 == rnrnit_1, "reverse_iterator constructor failed");
    reverse_iterator xxrnit_2;
    xxrnit_2 = rnrnit_1;
    expect (rnrnit_1 == xxrnit_2, "reverse_iterator assignment failed");

    // should be able to construct or assign a const_reverse_iterator from a
    // const_reverse_iterator
    const_reverse_iterator rcrcit_0 {c.crbegin()};
    const_reverse_iterator rcrcit_1 {rcrcit_0};
    expect (rcrcit_0 == rcrcit_1, "reverse_iterator constructor failed");
    const_reverse_iterator xxrcit_2;
    xxrcit_2 = rcrcit_1;
    expect (rcrcit_1 == xxrcit_2, "reverse_iterator assignment failed");

    // comparison between reverse_iterator and const_reverse_iterator is okay
    expect (rnrnit_0 == rcrcit_0,
        "comparing an iterator to a const_iterator failed");
    expect (rcrcit_1 == rnrnit_1,
        "comparing a const_iterator to an iterator failed");

    // should be able to construct or assign a const_reverse_iterator from a
    // reverse_iterator
    const_reverse_iterator rnrcit_0 {c.rbegin()};
    const_reverse_iterator rnrcit_1 {rnrnit_0};
    expect (rnrcit_0 == rnrcit_1,
        "const_reverse_iterator construction from reverse_iterator failed");
    xxrcit_2 = rnrnit_1;
    expect (rnrcit_1 == xxrcit_2,
        "const_reverse_iterator assignment from reverse_iterator failed");

    // the standard allows these conversions:
    //  o reverse_iterator is explicitly constructible from iterator.
    //  o const_reverse_iterator is explicitly constructible from const_iterator.
    // should be able to construct or assign reverse_iterators from
    // non-reverse iterators.
    reverse_iterator fnrnit_0 {c.begin()};
    const_reverse_iterator fnrcit_0 {c.begin()};
    expect (fnrnit_0 == fnrcit_0,
        "reverse_iterator construction from iterator failed");
    const_reverse_iterator fcrcit_0 {c.cbegin()};
    expect (fnrcit_0 == fcrcit_0,
        "const_reverse_iterator construction from const_iterator failed");

    // none of these should compile because they construct a non-reverse
    // iterator from a reverse_iterator.
//  iterator rnfnit_0 {c.rbegin()};
//  const_iterator rnfcit_0 {c.rbegin()};
//  const_iterator rcfcit_0 {c.crbegin()};

    // you should not be able to assign an iterator to a reverse_iterator or
    // vise-versa.  so the following lines should not compile.
    iterator xxfnit_0;
//  xxfnit_0 = xxrnit_2;
//  xxrnit_2 = xxfnit_0;
}

//------------------------------------------------------------------------------
//
// modifiers
//
//------------------------------------------------------------------------------


template <class container, class values>
void
aged_associative_container_test_base::
checkinsertcopy (container& c, values const& v)
{
    for (auto const& e : v)
        c.insert (e);
    checkcontents (c, v);
}

template <class container, class values>
void
aged_associative_container_test_base::
checkinsertmove (container& c, values const& v)
{
    values v2 (v);
    for (auto& e : v2)
        c.insert (std::move (e));
    checkcontents (c, v);
}

template <class container, class values>
void
aged_associative_container_test_base::
checkinserthintcopy (container& c, values const& v)
{
    for (auto const& e : v)
        c.insert (c.cend(), e);
    checkcontents (c, v);
}

template <class container, class values>
void
aged_associative_container_test_base::
checkinserthintmove (container& c, values const& v)
{
    values v2 (v);
    for (auto& e : v2)
        c.insert (c.cend(), std::move (e));
    checkcontents (c, v);
}

template <class container, class values>
void
aged_associative_container_test_base::
checkemplace (container& c, values const& v)
{
    for (auto const& e : v)
        c.emplace (e);
    checkcontents (c, v);
}

template <class container, class values>
void
aged_associative_container_test_base::
checkemplacehint (container& c, values const& v)
{
    for (auto const& e : v)
        c.emplace_hint (c.cend(), e);
    checkcontents (c, v);
}

template <bool isunordered, bool ismulti, bool ismap>
void
aged_associative_container_test_base::
testmodifiers()
{
    typedef testtraits <isunordered, ismulti, ismap> traits;
    typename traits::manualclock clock;
    auto const v (traits::values());
    auto const l (make_list (v));

    //testcase (traits::name() + " modify");
    testcase ("modify");

    {
        typename traits::template cont <> c (clock);
        checkinsertcopy (c, v);
    }

    {
        typename traits::template cont <> c (clock);
        checkinsertcopy (c, l);
    }

    {
        typename traits::template cont <> c (clock);
        checkinsertmove (c, v);
    }

    {
        typename traits::template cont <> c (clock);
        checkinsertmove (c, l);
    }

    {
        typename traits::template cont <> c (clock);
        checkinserthintcopy (c, v);
    }

    {
        typename traits::template cont <> c (clock);
        checkinserthintcopy (c, l);
    }

    {
        typename traits::template cont <> c (clock);
        checkinserthintmove (c, v);
    }

    {
        typename traits::template cont <> c (clock);
        checkinserthintmove (c, l);
    }
}

//------------------------------------------------------------------------------
//
// chronological ordering
//
//------------------------------------------------------------------------------

template <bool isunordered, bool ismulti, bool ismap>
void
aged_associative_container_test_base::
testchronological ()
{
    typedef testtraits <isunordered, ismulti, ismap> traits;
    typedef typename traits::value value;
    typename traits::manualclock clock;
    auto const v (traits::values());

    //testcase (traits::name() + " chronological");
    testcase ("chronological");

    typename traits::template cont <> c (
        v.begin(), v.end(), clock);

    expect (std::equal (
        c.chronological.cbegin(), c.chronological.cend(),
            v.begin(), v.end(), equal_value <traits> ()));

    // test touch() with a non-const iterator.
    for (auto iter (v.crbegin()); iter != v.crend(); ++iter)
    {
        using iterator = typename decltype (c)::iterator;
        iterator found (c.find (traits::extract (*iter)));

        expect (found != c.cend());
        if (found == c.cend())
            return;
        c.touch (found);
    }

    expect (std::equal (
        c.chronological.cbegin(), c.chronological.cend(),
            v.crbegin(), v.crend(), equal_value <traits> ()));

    // test touch() with a const_iterator
    for (auto iter (v.cbegin()); iter != v.cend(); ++iter)
    {
        using const_iterator = typename decltype (c)::const_iterator;
        const_iterator found (c.find (traits::extract (*iter)));

        expect (found != c.cend());
        if (found == c.cend())
            return;
        c.touch (found);
    }

    expect (std::equal (
        c.chronological.cbegin(), c.chronological.cend(),
            v.cbegin(), v.cend(), equal_value <traits> ()));

    {
        // because touch (reverse_iterator pos) is not allowed, the following
        // lines should not compile for any aged_container type.
//      c.touch (c.rbegin());
//      c.touch (c.crbegin());
    }
}

//------------------------------------------------------------------------------
//
// element creation via operator[]
//
//------------------------------------------------------------------------------

// map, unordered_map
template <bool isunordered, bool ismulti, bool ismap>
typename std::enable_if <ismap && ! ismulti>::type
aged_associative_container_test_base::
testarraycreate()
{
    typedef testtraits <isunordered, ismulti, ismap> traits;
    typename traits::manualclock clock;
    auto v (traits::values());

    //testcase (traits::name() + " array create");
    testcase ("array create");

    {
        // copy construct key
        typename traits::template cont <> c (clock);
        for (auto e : v)
            c [e.first] = e.second;
        checkcontents (c, v);
    }

    {
        // move construct key
        typename traits::template cont <> c (clock);
        for (auto e : v)
            c [std::move (e.first)] = e.second;
        checkcontents (c, v);
    }
}

//------------------------------------------------------------------------------
//
// helpers for erase tests
//
//------------------------------------------------------------------------------

template <class container, class values>
void
aged_associative_container_test_base::
reversefillagedcontainer (container& c, values const& values)
{
    // just in case the passed in container was not empty.
    c.clear();

    // c.clock() returns an abstract_clock, so dynamic_cast to manual_clock.
    // vfalco note this is sketchy
    typedef testtraitsbase::manualclock manualclock;
    manualclock& clk (dynamic_cast <manualclock&> (c.clock()));
    clk.set (0);

    values rev (values);
    std::sort (rev.begin (), rev.end ());
    std::reverse (rev.begin (), rev.end ());
    for (auto& v : rev)
    {
        // add values in reverse order so they are reversed chronologically.
        ++clk;
        c.insert (v);
    }
}

// get one iterator before enditer.  we have to use operator++ because you
// cannot use operator-- with unordered container iterators.
template <class iter>
iter
aged_associative_container_test_base::
nexttoenditer (iter beginiter, iter const enditer)
{
    if (beginiter == enditer)
    {
        fail ("internal test failure. cannot advance beginiter");
        return beginiter;
    }

    //
    iter nexttoend = beginiter;
    do
    {
        nexttoend = beginiter++;
    } while (beginiter != enditer);
    return nexttoend;
}

// implementation for the element erase tests
//
// this test accepts:
//  o the container from which we will erase elements
//  o iterators into that container defining the range of the erase
//
// this implementation does not declare a pass, since it wants to allow
// the caller to examine the size of the container and the returned iterator
//
// note that this test works on the aged_associative containers because an
// erase only invalidates references and iterators to the erased element
// (see 23.2.4/13).  therefore the passed-in end iterator stays valid through
// the whole test.
template <class container, class iter>
bool aged_associative_container_test_base::
doelementerase (container& c, iter const beginitr, iter const enditr)
{
    auto it (beginitr);
    size_t count = c.size();
    while (it != enditr)
    {
        auto expectit = it;
        ++expectit;
        it = c.erase (it);

        if (it != expectit)
        {
            fail ("unexpected returned iterator from element erase");
            return false;
        }

        --count;
        if (count != c.size())
        {
            fail ("failed to erase element");
            return false;
        }

        if (c.empty ())
        {
            if (it != enditr)
            {
                fail ("erase of last element didn't produce end");
                return false;
            }
        }
    }
   return true;
}

//------------------------------------------------------------------------------
//
// erase of individual elements
//
//------------------------------------------------------------------------------

template <bool isunordered, bool ismulti, bool ismap>
void
aged_associative_container_test_base::
testelementerase ()
{
    typedef testtraits <isunordered, ismulti, ismap> traits;

    //testcase (traits::name() + " element erase"
    testcase ("element erase");

    // make and fill the container
    typename traits::manualclock clock;
    typename traits::template cont <> c {clock};
    reversefillagedcontainer (c, traits::values());

    {
        // test standard iterators
        auto tempcontainer (c);
        if (! doelementerase (tempcontainer,
            tempcontainer.cbegin(), tempcontainer.cend()))
            return; // test failed

        expect (tempcontainer.empty(), "failed to erase all elements");
        pass();
    }
    {
        // test chronological iterators
        auto tempcontainer (c);
        auto& chron (tempcontainer.chronological);
        if (! doelementerase (tempcontainer, chron.begin(), chron.end()))
            return; // test failed

        expect (tempcontainer.empty(),
            "failed to chronologically erase all elements");
        pass();
    }
    {
        // test standard iterator partial erase
        auto tempcontainer (c);
        expect (tempcontainer.size() > 2,
            "internal failure.  container too small.");
        if (! doelementerase (tempcontainer, ++tempcontainer.begin(),
            nexttoenditer (tempcontainer.begin(), tempcontainer.end())))
            return; // test failed

        expect (tempcontainer.size() == 2,
            "failed to erase expected number of elements");
        pass();
    }
    {
        // test chronological iterator partial erase
        auto tempcontainer (c);
        expect (tempcontainer.size() > 2,
            "internal failure.  container too small.");
        auto& chron (tempcontainer.chronological);
        if (! doelementerase (tempcontainer, ++chron.begin(),
            nexttoenditer (chron.begin(), chron.end())))
            return; // test failed

        expect (tempcontainer.size() == 2,
            "failed to chronologically erase expected number of elements");
        pass();
    }
    {
        auto tempcontainer (c);
        expect (tempcontainer.size() > 4,
            "internal failure.  container too small.");
        // erase(reverse_iterator) is not allowed.  none of the following
        // should compile for any aged_container type.
//      c.erase (c.rbegin());
//      c.erase (c.crbegin());
//      c.erase(c.rbegin(), ++c.rbegin());
//      c.erase(c.crbegin(), ++c.crbegin());
    }
}

// implementation for the range erase tests
//
// this test accepts:
//
//  o a container with more than 2 elements and
//  o an object to ask for begin() and end() iterators in the passed container
//
// this peculiar interface allows either the container itself to be passed as
// the second argument or the container's "chronological" element.  both
// sources of iterators need to be tested on the container.
//
// the test locates iterators such that a range-based delete leaves the first
// and last elements in the container.  it then validates that the container
// ended up with the expected contents.
//
template <class container, class beginendsrc>
void
aged_associative_container_test_base::
dorangeerase (container& c, beginendsrc const& beginendsrc)
{
    expect (c.size () > 2,
        "internal test failure. container must have more than 2 elements");
    auto itbeginplusone (beginendsrc.begin ());
    auto const valuefront = *itbeginplusone;
    ++itbeginplusone;

    // get one iterator before end()
    auto itback (nexttoenditer (itbeginplusone, beginendsrc.end ()));
    auto const valueback = *itback;

    // erase all elements but first and last
    auto const retiter = c.erase (itbeginplusone, itback);

    expect (c.size() == 2,
        "unexpected size for range-erased container");

    expect (valuefront == *(beginendsrc.begin()),
        "unexpected first element in range-erased container");

    expect (valueback == *(++beginendsrc.begin()),
        "unexpected last element in range-erased container");

    expect (retiter == (++beginendsrc.begin()),
        "unexpected return iterator from erase");

    pass ();
}

//------------------------------------------------------------------------------
//
// erase range of elements
//
//------------------------------------------------------------------------------

template <bool isunordered, bool ismulti, bool ismap>
void
aged_associative_container_test_base::
testrangeerase ()
{
    typedef testtraits <isunordered, ismulti, ismap> traits;

    //testcase (traits::name() + " element erase"
    testcase ("range erase");

    // make and fill the container
    typename traits::manualclock clock;
    typename traits::template cont <> c {clock};
    reversefillagedcontainer (c, traits::values());

    // not bothering to test range erase with reverse iterators.
    {
        auto tempcontainer (c);
        dorangeerase (tempcontainer, tempcontainer);
    }
    {
        auto tempcontainer (c);
        dorangeerase (tempcontainer, tempcontainer.chronological);
    }
}

//------------------------------------------------------------------------------
//
// container-wide comparison
//
//------------------------------------------------------------------------------

// ordered
template <bool isunordered, bool ismulti, bool ismap>
typename std::enable_if <! isunordered>::type
aged_associative_container_test_base::
testcompare ()
{
    typedef testtraits <isunordered, ismulti, ismap> traits;
    typedef typename traits::value value;
    typename traits::manualclock clock;
    auto const v (traits::values());

    //testcase (traits::name() + " array create");
    testcase ("array create");

    typename traits::template cont <> c1 (
        v.begin(), v.end(), clock);

    typename traits::template cont <> c2 (
        v.begin(), v.end(), clock);
    c2.erase (c2.cbegin());

    expect      (c1 != c2);
    unexpected  (c1 == c2);
    expect      (c1 <  c2);
    expect      (c1 <= c2);
    unexpected  (c1 >  c2);
    unexpected  (c1 >= c2);
}

//------------------------------------------------------------------------------
//
// observers
//
//------------------------------------------------------------------------------

// ordered
template <bool isunordered, bool ismulti, bool ismap>
typename std::enable_if <! isunordered>::type
aged_associative_container_test_base::
testobservers()
{
    typedef testtraits <isunordered, ismulti, ismap> traits;
    typename traits::manualclock clock;

    //testcase (traits::name() + " observers");
    testcase ("observers");

    typename traits::template cont <> c (clock);
    c.key_comp();
    c.value_comp();

    pass();
}

// unordered
template <bool isunordered, bool ismulti, bool ismap>
typename std::enable_if <isunordered>::type
aged_associative_container_test_base::
testobservers()
{
    typedef testtraits <isunordered, ismulti, ismap> traits;
    typename traits::manualclock clock;

    //testcase (traits::name() + " observers");
    testcase ("observers");

    typename traits::template cont <> c (clock);
    c.hash_function();
    c.key_eq();

    pass();
}

//------------------------------------------------------------------------------
//
// matrix
//
//------------------------------------------------------------------------------

template <bool isunordered, bool ismulti, bool ismap>
void
aged_associative_container_test_base::
testmaybeunorderedmultimap ()
{
    typedef testtraits <isunordered, ismulti, ismap> traits;

    testconstructempty      <isunordered, ismulti, ismap> ();
    testconstructrange      <isunordered, ismulti, ismap> ();
    testconstructinitlist   <isunordered, ismulti, ismap> ();
    testcopymove            <isunordered, ismulti, ismap> ();
    testiterator            <isunordered, ismulti, ismap> ();
    testreverseiterator     <isunordered, ismulti, ismap> ();
    testmodifiers           <isunordered, ismulti, ismap> ();
    testchronological       <isunordered, ismulti, ismap> ();
    testarraycreate         <isunordered, ismulti, ismap> ();
    testelementerase        <isunordered, ismulti, ismap> ();
    testrangeerase          <isunordered, ismulti, ismap> ();
    testcompare             <isunordered, ismulti, ismap> ();
    testobservers           <isunordered, ismulti, ismap> ();
}

//------------------------------------------------------------------------------

class aged_set_test : public aged_associative_container_test_base
{
public:
    // compile time checks

    typedef std::string key;
    typedef int t;

    static_assert (std::is_same <
        aged_set <key>,
        detail::aged_ordered_container <false, false, key, void>>::value,
            "bad alias: aged_set");

    static_assert (std::is_same <
        aged_multiset <key>,
        detail::aged_ordered_container <true, false, key, void>>::value,
            "bad alias: aged_multiset");

    static_assert (std::is_same <
        aged_map <key, t>,
        detail::aged_ordered_container <false, true, key, t>>::value,
            "bad alias: aged_map");

    static_assert (std::is_same <
        aged_multimap <key, t>,
        detail::aged_ordered_container <true, true, key, t>>::value,
            "bad alias: aged_multimap");

    static_assert (std::is_same <
        aged_unordered_set <key>,
        detail::aged_unordered_container <false, false, key, void>>::value,
            "bad alias: aged_unordered_set");

    static_assert (std::is_same <
        aged_unordered_multiset <key>,
        detail::aged_unordered_container <true, false, key, void>>::value,
            "bad alias: aged_unordered_multiset");

    static_assert (std::is_same <
        aged_unordered_map <key, t>,
        detail::aged_unordered_container <false, true, key, t>>::value,
            "bad alias: aged_unordered_map");

    static_assert (std::is_same <
        aged_unordered_multimap <key, t>,
        detail::aged_unordered_container <true, true, key, t>>::value,
            "bad alias: aged_unordered_multimap");

    void run ()
    {
        testmaybeunorderedmultimap <false, false, false>();
    }
};

class aged_map_test : public aged_associative_container_test_base
{
public:
    void run ()
    {
        testmaybeunorderedmultimap <false, false, true>();
    }
};

class aged_multiset_test : public aged_associative_container_test_base
{
public:
    void run ()
    {
        testmaybeunorderedmultimap <false, true, false>();
    }
};

class aged_multimap_test : public aged_associative_container_test_base
{
public:
    void run ()
    {
        testmaybeunorderedmultimap <false, true, true>();
    }
};


class aged_unordered_set_test : public aged_associative_container_test_base
{
public:
    void run ()
    {
        testmaybeunorderedmultimap <true, false, false>();
    }
};

class aged_unordered_map_test : public aged_associative_container_test_base
{
public:
    void run ()
    {
        testmaybeunorderedmultimap <true, false, true>();
    }
};

class aged_unordered_multiset_test : public aged_associative_container_test_base
{
public:
    void run ()
    {
        testmaybeunorderedmultimap <true, true, false>();
    }
};

class aged_unordered_multimap_test : public aged_associative_container_test_base
{
public:
    void run ()
    {
        testmaybeunorderedmultimap <true, true, true>();
    }
};

beast_define_testsuite(aged_set,container,beast);
beast_define_testsuite(aged_map,container,beast);
beast_define_testsuite(aged_multiset,container,beast);
beast_define_testsuite(aged_multimap,container,beast);
beast_define_testsuite(aged_unordered_set,container,beast);
beast_define_testsuite(aged_unordered_map,container,beast);
beast_define_testsuite(aged_unordered_multiset,container,beast);
beast_define_testsuite(aged_unordered_multimap,container,beast);

} // namespace beast
