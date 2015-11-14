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

#ifndef ripple_protocol_starray_h_included
#define ripple_protocol_starray_h_included

#include <ripple/basics/countedobject.h>
#include <ripple/protocol/stobject.h>
#include <boost/ptr_container/ptr_vector.hpp>

namespace ripple {

class starray final
    : public stbase
    , public countedobject <starray>
{
public:
    static char const* getcountedobjectname () { return "starray"; }

    typedef boost::ptr_vector<stobject>    vector;

    typedef vector::iterator               iterator;
    typedef vector::const_iterator         const_iterator;
    typedef vector::reverse_iterator       reverse_iterator;
    typedef vector::const_reverse_iterator const_reverse_iterator;
    typedef vector::size_type              size_type;

public:
    starray ()
    {
        ;
    }
    explicit starray (int n)
    {
        value.reserve (n);
    }
    explicit starray (sfield::ref f) : stbase (f)
    {
        ;
    }
    starray (sfield::ref f, int n) : stbase (f)
    {
        value.reserve (n);
    }
    starray (sfield::ref f, const vector & v) : stbase (f), value (v)
    {
        ;
    }
    explicit starray (vector & v) : value (v)
    {
        ;
    }

    virtual ~starray () { }

    static std::unique_ptr<stbase>
    deserialize (serializeriterator & sit, sfield::ref name);

    const vector& getvalue () const
    {
        return value;
    }
    vector& getvalue ()
    {
        return value;
    }

    // vfalco note as long as we're married to boost why not use
    //             boost::iterator_facade?
    //
    // vector-like functions
    void push_back (const stobject & object)
    {
        value.push_back (object.oclone ().release ());
    }
    stobject& operator[] (int j)
    {
        return value[j];
    }
    const stobject& operator[] (int j) const
    {
        return value[j];
    }
    iterator begin ()
    {
        return value.begin ();
    }
    const_iterator begin () const
    {
        return value.begin ();
    }
    iterator end ()
    {
        return value.end ();
    }
    const_iterator end () const
    {
        return value.end ();
    }
    size_type size () const
    {
        return value.size ();
    }
    reverse_iterator rbegin ()
    {
        return value.rbegin ();
    }
    const_reverse_iterator rbegin () const
    {
        return value.rbegin ();
    }
    reverse_iterator rend ()
    {
        return value.rend ();
    }
    const_reverse_iterator rend () const
    {
        return value.rend ();
    }
    iterator erase (iterator pos)
    {
        return value.erase (pos);
    }
    stobject& front ()
    {
        return value.front ();
    }
    const stobject& front () const
    {
        return value.front ();
    }
    stobject& back ()
    {
        return value.back ();
    }
    const stobject& back () const
    {
        return value.back ();
    }
    void pop_back ()
    {
        value.pop_back ();
    }
    bool empty () const
    {
        return value.empty ();
    }
    void clear ()
    {
        value.clear ();
    }
    void swap (starray & a)
    {
        value.swap (a.value);
    }

    virtual std::string getfulltext () const override;
    virtual std::string gettext () const override;

    virtual json::value getjson (int index) const override;
    virtual void add (serializer & s) const override;

    void sort (bool (*compare) (const stobject & o1, const stobject & o2));

    bool operator== (const starray & s)
    {
        return value == s.value;
    }
    bool operator!= (const starray & s)
    {
        return value != s.value;
    }

    virtual serializedtypeid getstype () const override
    {
        return sti_array;
    }
    virtual bool isequivalent (const stbase & t) const override;
    virtual bool isdefault () const override
    {
        return value.empty ();
    }

private:
    virtual starray* duplicate () const override
    {
        return new starray (*this);
    }

private:
    vector value;
};

} // ripple

#endif
