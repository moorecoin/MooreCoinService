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

#ifndef beast_threads_shareddata_h_included
#define beast_threads_shareddata_h_included

#include <beast/threads/recursivemutex.h>
#include <beast/threads/sharedmutexadapter.h>

namespace beast {

/** structured, multi-threaded access to a shared state.

    this container combines locking semantics with data access semantics to
    create an alternative to the typical synchronization method of first
    acquiring a lock and then accessing data members.

    with this container, access to the underlying data is only possible after
    first acquiring a lock. the steps of acquiring the lock and obtaining
    a const or non-const reference to the data are combined into one
    raii style operation.

    there are three types of access:

    - access
        provides access to the shared state via a non-const reference or pointer.
        access acquires a unique lock on the mutex associated with the
        container.

    - constaccess
        provides access to the shared state via a const reference or pointer.
        constaccess acquires a shared lock on the mutex associated with the
        container.

    - constunlockedaccess
        provides access to the shared state via a const reference or pointer.
        no locking is performed. it is the callers responsibility to ensure that
        the operation is synchronized. this can be useful for diagnostics or
        assertions, or for when it is known that no other threads can access
        the data.

    - unlockedaccess
        provides access to the shared state via a reference or pointer.
        no locking is performed. it is the callers responsibility to ensure that
        the operation is synchronized. this can be useful for diagnostics or
        assertions, or for when it is known that no other threads can access
        the data.

    usage example:

    @code

    struct state
    {
        int value1;
        string value2;
    };

    typedef shareddata <state> sharedstate;

    sharedstate m_state;

    void readexample ()
    {
        sharedstate::constaccess state (m_state);

        print (state->value1);   // read access
        print (state->value2);   // read access

        state->value1 = 42;      // write disallowed: compile error
    }

    void writeexample ()
    {
        sharedstate::access state (m_state);

        state->value2 = "label"; // write access, allowed
    }

    @endcode

    requirements for value:
        constructible
        destructible

    requirements for sharedmutextype:
        x is sharedmutextype, a is an instance of x:
        x a;                    defaultconstructible
        x::lockguardtype        names a type that implements the lockguard concept.
        x::sharedlockguardtype  names a type that implements the sharedlockguard concept.

    @tparam value the type which the container holds.
    @tparam sharedmutextype the type of shared mutex to use.
*/
template <typename value, class sharedmutextype =
    sharedmutexadapter <recursivemutex> >
class shareddata
{
private:
    typedef typename sharedmutextype::lockguardtype lockguardtype;
    typedef typename sharedmutextype::sharedlockguardtype sharedlockguardtype;

public:
    typedef value valuetype;

    class access;
    class constaccess;
    class unlockedaccess;
    class constunlockedaccess;

    /** create a shared data container.
        up to 8 parameters can be specified in the constructor. these parameters
        are forwarded to the corresponding constructor in data. if no
        constructor in data matches the parameter list, a compile error is
        generated.
    */
    /** @{ */
    shareddata () = default;

    template <class t1>
    explicit shareddata (t1 t1)
        : m_value (t1) { }

    template <class t1, class t2>
    shareddata (t1 t1, t2 t2)
        : m_value (t1, t2) { }

    template <class t1, class t2, class t3>
    shareddata (t1 t1, t2 t2, t3 t3)
        : m_value (t1, t2, t3) { }

    template <class t1, class t2, class t3, class t4>
    shareddata (t1 t1, t2 t2, t3 t3, t4 t4)
        : m_value (t1, t2, t3, t4) { }

    template <class t1, class t2, class t3, class t4, class t5>
    shareddata (t1 t1, t2 t2, t3 t3, t4 t4, t5 t5)
        : m_value (t1, t2, t3, t4, t5) { }

    template <class t1, class t2, class t3, class t4, class t5, class t6>
    shareddata (t1 t1, t2 t2, t3 t3, t4 t4, t5 t5, t6 t6)
        : m_value (t1, t2, t3, t4, t5, t6) { }

    template <class t1, class t2, class t3, class t4, class t5, class t6, class t7>
    shareddata (t1 t1, t2 t2, t3 t3, t4 t4, t5 t5, t6 t6, t7 t7) : m_value (t1, t2, t3, t4, t5, t6, t7) { }

    template <class t1, class t2, class t3, class t4, class t5, class t6, class t7, class t8>
    shareddata (t1 t1, t2 t2, t3 t3, t4 t4, t5 t5, t6 t6, t7 t7, t8 t8)
        : m_value (t1, t2, t3, t4, t5, t6, t7, t8) { }
    /** @} */

    shareddata (shareddata const&) = delete;
    shareddata& operator= (shareddata const&) = delete;

private:
    value m_value;
    sharedmutextype m_mutex;
};

//------------------------------------------------------------------------------

/** provides non-const access to the contents of a shareddata.
    this acquires a unique lock on the underlying mutex.
*/
template <class data, class sharedmutextype>
class shareddata <data, sharedmutextype>::access
{
public:
    explicit access (shareddata& state)
        : m_state (state)
        , m_lock (m_state.m_mutex)
        { }

    access (access const&) = delete;
    access& operator= (access const&) = delete;

    data const& get () const { return m_state.m_value; }
    data const& operator* () const { return get (); }
    data const* operator-> () const { return &get (); }
    data& get () { return m_state.m_value; }
    data& operator* () { return get (); }
    data* operator-> () { return &get (); }

private:
    shareddata& m_state;
    typename shareddata::lockguardtype m_lock;
};

//------------------------------------------------------------------------------

/** provides const access to the contents of a shareddata.
    this acquires a shared lock on the underlying mutex.
*/
template <class data, class sharedmutextype>
class shareddata <data, sharedmutextype>::constaccess
{
public:
    /** create a constaccess from the specified shareddata */
    explicit constaccess (shareddata const volatile& state)
        : m_state (const_cast <shareddata const&> (state))
        , m_lock (m_state.m_mutex)
        { }

    constaccess (constaccess const&) = delete;
    constaccess& operator= (constaccess const&) = delete;

    data const& get () const { return m_state.m_value; }
    data const& operator* () const { return get (); }
    data const* operator-> () const { return &get (); }

private:
    shareddata const& m_state;
    typename shareddata::sharedlockguardtype m_lock;
};

//------------------------------------------------------------------------------

/** provides const access to the contents of a shareddata.
    this acquires a shared lock on the underlying mutex.
*/
template <class data, class sharedmutextype>
class shareddata <data, sharedmutextype>::constunlockedaccess
{
public:
    /** create an unlockedaccess from the specified shareddata */
    explicit constunlockedaccess (shareddata const volatile& state)
        : m_state (const_cast <shareddata const&> (state))
        { }

    constunlockedaccess (constunlockedaccess const&) = delete;
    constunlockedaccess& operator= (constunlockedaccess const&) = delete;

    data const& get () const { return m_state.m_value; }
    data const& operator* () const { return get (); }
    data const* operator-> () const { return &get (); }

private:
    shareddata const& m_state;
};

//------------------------------------------------------------------------------

/** provides access to the contents of a shareddata.
    this acquires a shared lock on the underlying mutex.
*/
template <class data, class sharedmutextype>
class shareddata <data, sharedmutextype>::unlockedaccess
{
public:
    /** create an unlockedaccess from the specified shareddata */
    explicit unlockedaccess (shareddata& state)
        : m_state (state)
        { }

    unlockedaccess (unlockedaccess const&) = delete;
    unlockedaccess& operator= (unlockedaccess const&) = delete;

    data const& get () const { return m_state.m_value; }
    data const& operator* () const { return get (); }
    data const* operator-> () const { return &get (); }
    data& get () { return m_state.m_value; }
    data& operator* () { return get (); }
    data* operator-> () { return &get (); }

private:
    shareddata& m_state;
};

}

#endif
