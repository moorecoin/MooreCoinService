//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012, 2013 ripple labs inc.

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

#ifndef ripple_basics_countedobject_h_included
#define ripple_basics_countedobject_h_included

#include <beast/utility/leakchecked.h>
#include <atomic>
#include <string>
#include <utility>
#include <vector>

namespace ripple {

/** manages all counted object types. */
class countedobjects
{
public:
    static countedobjects& getinstance ();

    typedef std::pair <std::string, int> entry;
    typedef std::vector <entry> list;

    list getcounts (int minimumthreshold) const;

public:
    /** implementation for @ref countedobject.

        @internal
    */
    class counterbase
    {
    public:
        counterbase ();

        virtual ~counterbase ();

        int increment () noexcept
        {
            return ++m_count;
        }

        int decrement () noexcept
        {
            return --m_count;
        }

        int getcount () const noexcept
        {
            return m_count.load ();
        }

        counterbase* getnext () const noexcept
        {
            return m_next;
        }

        virtual char const* getname () const = 0;

    private:
        virtual void checkpurevirtual () const = 0;

    protected:
        std::atomic <int> m_count;
        counterbase* m_next;
    };

private:
    countedobjects ();
    ~countedobjects ();

private:
    std::atomic <int> m_count;
    std::atomic <counterbase*> m_head;
};

//------------------------------------------------------------------------------

/** tracks the number of instances of an object.

    derived classes have their instances counted automatically. this is used
    for reporting purposes.

    @ingroup ripple_basics
*/
template <class object>
class countedobject : beast::leakchecked <countedobject <object> >
{
public:
    countedobject ()
    {
        getcounter ().increment ();
    }

    countedobject (countedobject const&)
    {
        getcounter ().increment ();
    }

    ~countedobject ()
    {
        getcounter ().decrement ();
    }

private:
    class counter : public countedobjects::counterbase
    {
    public:
        counter () { }

        char const* getname () const
        {
            return object::getcountedobjectname ();
        }

        void checkpurevirtual () const { }
    };

private:
    static counter& getcounter ()
    {
        return beast::staticobject <counter>::get();
    }
};

} // ripple

#endif
