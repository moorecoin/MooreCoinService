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

#ifndef beast_utility_static_initializer_h_included
#define beast_utility_static_initializer_h_included

#include <beast/utility/noexcept.h>
#include <utility>

#ifdef _msc_ver
#include <cassert>
#include <new>
#include <thread>
#include <type_traits>
#include <intrin.h>
#endif

namespace beast {

/** returns an object with static storage duration.
    this is a workaround for visual studio 2013 and earlier non-thread
    safe initialization of function local objects with static storage duration.

    usage:
    @code
    my_class& foo()
    {
        static static_initializer <my_class> instance;
        return *instance;
    }
    @endcode
*/
#ifdef _msc_ver
template <
    class t,
    class tag = void
>
class static_initializer
{
private:
    struct data_t
    {
        //  0 = unconstructed
        //  1 = constructing
        //  2 = constructed
        long volatile state;

        typename std::aligned_storage <sizeof(t),
            std::alignment_of <t>::value>::type storage;
    };

    struct destroyer
    {
        t* t_;
        explicit destroyer (t* t) : t_(t) { }
        ~destroyer() { t_->~t(); }
    };

    static
    data_t&
    data() noexcept;

public:
    template <class... args>
    explicit static_initializer (args&&... args);

    t&
    get() noexcept;

    t const&
    get() const noexcept
    {
        return const_cast<static_initializer&>(*this).get();
    }

    t&
    operator*() noexcept
    {
        return get();
    }

    t const&
    operator*() const noexcept
    {
        return get();
    }

    t*
    operator->() noexcept
    {
        return &get();
    }

    t const*
    operator->() const noexcept
    {
        return &get();
    }
};

//------------------------------------------------------------------------------

template <class t, class tag>
auto
static_initializer <t, tag>::data() noexcept ->
    data_t&
{
    static data_t _; // zero-initialized
    return _;
}

template <class t, class tag>
template <class... args>
static_initializer <t, tag>::static_initializer (args&&... args)
{
    data_t& _(data());

    // double checked locking pattern

    if (_.state != 2)
    {
        t* const t (reinterpret_cast<t*>(&_.storage));

        for(;;)
        {
            long prev;
            prev = _interlockedcompareexchange(&_.state, 1, 0);
            if (prev == 0)
            {
                try
                {
                    ::new(t) t (std::forward<args>(args)...);                   
                    static destroyer on_exit (t);
                    _interlockedincrement(&_.state);
                }
                catch(...)
                {
                    // constructors that throw exceptions are unsupported
                    std::terminate();
                }
            }
            else if (prev == 1)
            {
                std::this_thread::yield();
            }
            else
            {
                assert(prev == 2);
                break;
            }
        }
    }
}

template <class t, class tag>
t&
static_initializer <t, tag>::get() noexcept
{
    data_t& _(data());
    for(;;)
    {
        if (_.state == 2)
            break;
        std::this_thread::yield();
    }
    return *reinterpret_cast<t*>(&_.storage);
}

#else
template <
    class t,
    class tag = void
>
class static_initializer
{
private:
    t* instance_;

public:
    template <class... args>
    explicit
    static_initializer (args&&... args);

    static_initializer ();
    
    t&
    get() noexcept
    {
        return *instance_;
    }

    t const&
    get() const noexcept
    {
        return const_cast<static_initializer&>(*this).get();
    }

    t&
    operator*() noexcept
    {
        return get();
    }

    t const&
    operator*() const noexcept
    {
        return get();
    }

    t*
    operator->() noexcept
    {
        return &get();
    }

    t const*
    operator->() const noexcept
    {
        return &get();
    }
};

template <class t, class tag>
static_initializer <t, tag>::static_initializer ()
{
    static t t;
    instance_ = &t;
}

template <class t, class tag>
template <class... args>
static_initializer <t, tag>::static_initializer (args&&... args)
{
    static t t (std::forward<args>(args)...);
    instance_ = &t;
}

#endif

}

#endif
