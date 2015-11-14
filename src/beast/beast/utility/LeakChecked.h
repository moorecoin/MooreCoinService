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

#ifndef beast_utility_leakchecked_h_included
#define beast_utility_leakchecked_h_included

#include <beast/config.h>
#include <beast/intrusive/lockfreestack.h>
#include <beast/utility/staticobject.h>

#include <atomic>

namespace beast {

namespace detail {

class leakcheckedbase
{
public:
    static void checkforleaks ();

protected:
    class leakcounterbase : public lockfreestack <leakcounterbase>::node
    {
    public:
        leakcounterbase ();

        virtual ~leakcounterbase ()
        {
        }

        inline int increment ()
        {
            return ++m_count;
        }

        inline int decrement ()
        {
            return --m_count;
        }

        virtual char const* getclassname () const = 0;

    private:
        void checkforleaks ();
        virtual void checkpurevirtual () const = 0;

        class singleton;
        friend class leakcheckedbase;

        std::atomic <int> m_count;
    };

    static void reportdanglingpointer (char const* objectname);
};

//------------------------------------------------------------------------------

/** detects leaks at program exit.

    to use this, derive your class from this template using crtp (curiously
    recurring template pattern).
*/
template <class object>
class leakchecked : private leakcheckedbase
{
protected:
    leakchecked () noexcept
    {
        getcounter ().increment ();
    }

    leakchecked (leakchecked const&) noexcept
    {
        getcounter ().increment ();
    }

    ~leakchecked ()
    {
        if (getcounter ().decrement () < 0)
        {
            reportdanglingpointer (getleakcheckedname ());
        }
    }

private:
    // singleton that maintains the count of this object
    //
    class leakcounter : public leakcounterbase
    {
    public:
        leakcounter () noexcept
        {
        }

        char const* getclassname () const
        {
            return getleakcheckedname ();
        }

        void checkpurevirtual () const { }
    };

private:
    /* due to a bug in visual studio 10 and earlier, the string returned by
       typeid().name() will appear to leak on exit. therefore, we should
       only call this function when there's an actual leak, or else there
       will be spurious leak notices at exit.
    */
    static const char* getleakcheckedname ()
    {
        return typeid (object).name ();
    }

    // retrieve the singleton for this object
    //
    static leakcounter& getcounter () noexcept
    {
        return staticobject <leakcounter>::get();
    }
};

}

//------------------------------------------------------------------------------

namespace detail
{

namespace disabled
{

class leakcheckedbase
{
public:
    static void checkforleaks ()
    {
    }
};

template <class object>
class leakchecked : public leakcheckedbase
{
public:
};

}

}

//------------------------------------------------------------------------------

// lift the appropriate implementation into our namespace
//
#if beast_check_memory_leaks
using detail::leakchecked;
using detail::leakcheckedbase;
#else
using detail::disabled::leakchecked;
using detail::disabled::leakcheckedbase;
#endif

}

#endif
