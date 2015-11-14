//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

    portions of this file are from juce.
    copyright (c) 2013 - raw material software ltd.
    please visit http://www.juce.com

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

#ifndef beast_smartptr_sharedptr_h_included
#define beast_smartptr_sharedptr_h_included

#include <beast/config.h>
#include <beast/smart_ptr/sharedobject.h>

namespace beast {

// visual studio doesn't seem to do very well when it comes
// to templated constructors and assignments so we provide
// non-templated versions when u and t are the same type.
//
#ifndef beast_sharedptr_provide_compiler_workarounds
#define beast_sharedptr_provide_compiler_workarounds 1
#endif

/** a smart-pointer container.

    requirement:
        t must support the sharedobject concept.

    the template parameter specifies the class of the object you want to point
    to - the easiest way to make a class reference-countable is to simply make
    it inherit from sharedobject, but if you need to, you could roll your own
    reference-countable class by implementing a pair of methods called
    increferencecount() and decreferencecount().

    when using this class, you'll probably want to create a typedef to
    abbreviate the full templated name - e.g.

    @code
    
    typedef sharedptr <myclass> myclassptr;
    
    @endcode

    @see sharedobject, sharedobjectarray
*/
template <class t>
class sharedptr
{
public:
    typedef t value_type;

    /** the class being referenced by this container. */
    typedef t referencedtype;

    /** construct a container pointing to nothing. */
    sharedptr () noexcept
        : m_p (nullptr)
    {
    }

    /** construct a container holding an object.
        this will increment the object's reference-count if it is non-null.
        requirement:
            u* must be convertible to t*
    */
    /** @{ */
    sharedptr (t* t) noexcept
        : m_p (acquire (t))
    {
    }

    template <class u>
    sharedptr (u* u) noexcept
        : m_p (acquire (u))
    {
    }
    /** @} */

    /** construct a container holding an object from another container.
        this will increment the object's reference-count (if it is non-null).
        requirement:
            u* must be convertible to t*
    */
    /** @{ */
#if beast_sharedptr_provide_compiler_workarounds
    sharedptr (sharedptr const& sp) noexcept
        : m_p (acquire (sp.get ()))
    {
    }
#endif

    template <class u>
    sharedptr (sharedptr <u> const& sp) noexcept
        : m_p (acquire (sp.get ()))
    {
    }
    /** @} */

    /** assign a different object to the container.
        the previous object beind held, if any, loses a reference count and
        will be destroyed if it is the last reference.
        requirement:
            u* must be convertible to t*
    */
    /** @{ */
#if beast_sharedptr_provide_compiler_workarounds
    sharedptr& operator= (t* t)
    {
        return assign (t);
    }
#endif

    template <class u>
    sharedptr& operator= (u* u)
    {
        return assign (u);
    }
    /** @} */

    /** assign an object from another container to this one. */
    /** @{ */
    sharedptr& operator= (sharedptr const& sp)
    {
        return assign (sp.get ());
    }

    /** assign an object from another container to this one. */
    template <class u>
    sharedptr& operator= (sharedptr <u> const& sp)
    {
        return assign (sp.get ());
    }
    /** @} */

#if beast_compiler_supports_move_semantics
    /** construct a container with an object transferred from another container.
        the originating container loses its reference to the object.
        requires:
            u* must be convertible to t*
    */
    /** @{ */
#if beast_sharedptr_provide_compiler_workarounds
    sharedptr (sharedptr && sp) noexcept
        : m_p (sp.swap <t> (nullptr))
    {
    }
#endif

    template <class u>
    sharedptr (sharedptr <u>&& sp) noexcept
        : m_p (sp.template swap <u> (nullptr))
    {
    }
    /** @} */

    /** transfer ownership of another object to this container.
        the originating container loses its reference to the object.
        requires:
            u* must be convertible to t*
    */
    /** @{ */
#if beast_sharedptr_provide_compiler_workarounds
    sharedptr& operator= (sharedptr && sp)
    {
        return assign (sp.swap <t> (nullptr));
    }
#endif

    template <class u>
    sharedptr& operator= (sharedptr <u>&& sp)
    {
        return assign (sp.template swap <u> (nullptr));
    }
    /** @} */
#endif

    /** destroy the container and release the held reference, if any.
    */
    ~sharedptr ()
    {
        release (m_p);
    }

    /** returns `true` if the container is not pointing to an object. */
    bool empty () const noexcept
    {
        return m_p == nullptr;
    }

    /** returns the object that this pointer references if any, else nullptr. */
    operator t* () const noexcept
    {
        return m_p;
    }

    /** returns the object that this pointer references if any, else nullptr. */
    t* operator-> () const noexcept
    {
        return m_p;
    }

    /** returns the object that this pointer references if any, else nullptr. */
    t* get () const noexcept
    {
        return m_p;
    }

private:
    // acquire a reference to u for the caller.
    //
    template <class u>
    static t* acquire (u* u) noexcept
    {
        if (u != nullptr)
            u->increferencecount ();
        return u;
    }

    static void release (t* t)
    {
        if (t != nullptr)
            t->decreferencecount ();
    }

    // swap ownership of the currently referenced object.
    // the caller receives a pointer to the original object,
    // and this container is left with the passed object. no
    // reference counts are changed.
    //
    template <class u>
    t* swap (u* u)
    {
        t* const t (m_p);
        m_p = u;
        return t;
    }

    // acquire ownership of u.
    // any previous reference is released.
    //
    template <class u>
    sharedptr& assign (u* u)
    {
        if (m_p != u)
            release (this->swap (acquire (u)));
        return *this;
    }

    t* m_p;
};

//------------------------------------------------------------------------------

// bind() helpers for pointer to member function

template <class t>
const t* get_pointer (sharedptr<t> const& ptr)
{
    return ptr.get();
}

template <class t>
t* get_pointer (sharedptr<t>& ptr)
{
    return ptr.get();
}

//------------------------------------------------------------------------------

/** sharedptr comparisons. */
/** @{ */
template <class t, class u>
bool operator== (sharedptr <t> const& lhs, u* const rhs) noexcept
{
    return lhs.get() == rhs;
}

template <class t, class u>
bool operator== (sharedptr <t> const& lhs, sharedptr <u> const& rhs) noexcept
{
    return lhs.get() == rhs.get();
}

template <class t, class u>
bool operator== (t const* lhs, sharedptr <u> const& rhs) noexcept
{
    return lhs == rhs.get();
}

template <class t, class u>
bool operator!= (sharedptr <t> const& lhs, u const* rhs) noexcept
{
    return lhs.get() != rhs;
}

template <class t, class u>
bool operator!= (sharedptr <t> const& lhs, sharedptr <u> const& rhs) noexcept
{
    return lhs.get() != rhs.get();
}

template <class t, class u>
bool operator!= (t const* lhs, sharedptr <u> const& rhs) noexcept
{
    return lhs != rhs.get();
}
/** @} */

}

#endif
