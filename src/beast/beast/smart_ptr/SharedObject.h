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

#ifndef beast_sharedobject_h_included
#define beast_sharedobject_h_included

#include <atomic>

#include <beast/config.h>

namespace beast {

//==============================================================================
/**
    adds reference-counting to an object.

    to add reference-counting to a class, derive it from this class, and
    use the sharedptr class to point to it.

    e.g. @code
    class myclass : public sharedobject
    {
        void foo();

        // this is a neat way of declaring a typedef for a pointer class,
        // rather than typing out the full templated name each time..
        typedef sharedptr<myclass> ptr;
    };

    myclass::ptr p = new myclass();
    myclass::ptr p2 = p;
    p = nullptr;
    p2->foo();
    @endcode

    once a new sharedobject has been assigned to a pointer, be
    careful not to delete the object manually.

    this class uses an std::atomic<int> value to hold the reference count, so
    that the pointers can be passed between threads safely.

    @see sharedptr, sharedobjectarray
*/
class sharedobject
{
public:
    //==============================================================================
    /** increments the object's reference count.

        this is done automatically by the smart pointer, but is public just
        in case it's needed for nefarious purposes.
    */
    void increferencecount() const noexcept
    {
        ++refcount;
    }

    /** decreases the object's reference count. */
    void decreferencecount () const
    {
        bassert (getreferencecount() > 0);
        if (--refcount == 0)
            destroy ();
    }

    /** returns the object's current reference count. */
    int getreferencecount() const noexcept
    {
        return refcount.load();
    }

protected:
    //==============================================================================
    /** creates the reference-counted object (with an initial ref count of zero). */
    sharedobject()
        : refcount (0)
    {
    }

    sharedobject (sharedobject const&) = delete;
    sharedobject& operator= (sharedobject const&) = delete;

    /** destructor. */
    virtual ~sharedobject()
    {
        // it's dangerous to delete an object that's still referenced by something else!
        bassert (getreferencecount() == 0);
    }

    /** destroy the object.
        derived classes can override this for different behaviors.
    */
    virtual void destroy () const
    {
        delete this;
    }

    /** resets the reference count to zero without deleting the object.
        you should probably never need to use this!
    */
    void resetreferencecount() noexcept
    {
        refcount.store (0);
    }

private:
    //==============================================================================
    std::atomic <int> mutable refcount;
};

}

#endif
