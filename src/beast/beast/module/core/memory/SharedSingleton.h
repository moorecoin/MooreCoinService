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

#ifndef beast_sharedsingleton_h_included
#define beast_sharedsingleton_h_included

#include <beast/threads/spinlock.h>
#include <beast/smart_ptr/sharedptr.h>
#include <beast/module/core/time/atexithook.h>

#include <atomic>

namespace beast
{

/** thread-safe singleton which comes into existence on first use. use this
    instead of creating objects with static storage duration. these singletons
    are automatically reference counted, so if you hold a pointer to it in every
    object that depends on it, the order of destruction of objects is assured
    to be correct.

    object requirements:
        defaultconstructible
        triviallydestructible (when lifetime == neverdestroyed)
        destructible

    @class sharedsingleton
    @ingroup beast_core
*/
/** @{ */
class singletonlifetime
{
public:
    // it would be nice if we didn't have to qualify the enumeration but
    // argument dependent lookup is inapplicable here because:
    //
    // "base classes dependent on a template parameter aren't part of lookup."
    //  - ville
    //

    /** construction options for sharedsingleton

        @ingroup beast_core
    */
    enum lifetime
    {
        /** created on first use, destroyed when the last reference is removed.
        */
        createondemand,

        /** the singleton is created on first use and persists until program exit.
        */
        persistaftercreation,

        /** the singleton is created when needed and never destroyed.

            this is useful for applications which do not have a clean exit.
        */
        neverdestroyed
    };
};

//------------------------------------------------------------------------------

/** wraps object to produce a reference counted singleton. */
template <class object>
class sharedsingleton
    : public object
    , private sharedobject
{
public:
    typedef sharedptr <sharedsingleton <object> > ptr;

    static ptr get (singletonlifetime::lifetime lifetime
        = singletonlifetime::persistaftercreation)
    {
        staticdata& staticdata (getstaticdata ());
        sharedsingleton* instance = staticdata.instance;
        if (instance == nullptr)
        {
            std::lock_guard <locktype> lock (staticdata.mutex);
            instance = staticdata.instance;
            if (instance == nullptr)
            {
                bassert (lifetime == singletonlifetime::createondemand || ! staticdata.destructorcalled);
                staticdata.instance = &staticdata.object;
                new (staticdata.instance) sharedsingleton (lifetime);
                std::atomic_thread_fence (std::memory_order_seq_cst);
                instance = staticdata.instance;
            }
        }
        return instance;
    }

    // deprecated legacy function name
    static ptr getinstance (singletonlifetime::lifetime lifetime
        = singletonlifetime::persistaftercreation)
    {
        return get (lifetime);
    }

private:
    explicit sharedsingleton (singletonlifetime::lifetime lifetime)
        : m_lifetime (lifetime)
        , m_exithook (this)
    {
        if (m_lifetime == singletonlifetime::persistaftercreation ||
            m_lifetime == singletonlifetime::neverdestroyed)
            this->increferencecount ();
    }

    ~sharedsingleton ()
    {
    }

    void onexit ()
    {
        if (m_lifetime == singletonlifetime::persistaftercreation)
            this->decreferencecount ();
    }

    void destroy () const
    {
        bool calldestructor;

        // handle the condition where one thread is releasing the last
        // reference just as another thread is trying to acquire it.
        //
        {
            staticdata& staticdata (getstaticdata ());
            std::lock_guard <locktype> lock (staticdata.mutex);

            if (this->getreferencecount() != 0)
            {
                calldestructor = false;
            }
            else
            {
                calldestructor = true;
                staticdata.instance = nullptr;
                staticdata.destructorcalled = true;
            }
        }

        if (calldestructor)
        {
            bassert (m_lifetime != singletonlifetime::neverdestroyed);

            this->~sharedsingleton();
        }
    }

    typedef spinlock locktype;

    // this structure gets zero-filled at static initialization time.
    // no constructors are called.
    //
    class staticdata
    {
    public:
        locktype mutex;
        sharedsingleton* instance;
        sharedsingleton  object;
        bool destructorcalled;

        staticdata() = delete;
        staticdata(staticdata const&) = delete;
        staticdata& operator= (staticdata const&) = delete;
        ~staticdata() = delete;
    };

    static staticdata& getstaticdata ()
    {
        static std::uint8_t storage [sizeof (staticdata)];
        return *(reinterpret_cast <staticdata*> (&storage [0]));
    }

    friend class sharedptr <sharedsingleton>;
    friend class atexitmemberhook <sharedsingleton>;

    singletonlifetime::lifetime m_lifetime;
    atexitmemberhook <sharedsingleton> m_exithook;
};

//------------------------------------------------------------------------------

} // beast

#endif
