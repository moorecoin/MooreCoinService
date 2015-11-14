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

#ifndef ripple_basics_syncunorderedmap_h_included
#define ripple_basics_syncunorderedmap_h_included

#include <ripple/basics/unorderedcontainers.h>
#include <mutex>

namespace ripple {

// common base
class syncunorderedmap
{
public:
    typedef std::recursive_mutex locktype;
    typedef std::lock_guard <locktype> scopedlocktype;
};

/** this is a synchronized unordered map.
    it is useful for cases where an unordered map contains all
    or a subset of an unchanging data set.
*/

template <typename c_key, typename c_data, typename c_hash = beast::uhash<>>
class syncunorderedmaptype : public syncunorderedmap
{
public:
    typedef c_key                                        key_type;
    typedef c_data                                       data_type;
    typedef hash_map<c_key, c_data, c_hash>              map_type;

    class iterator
    {
    public:
        bool operator== (const iterator& i)     { return it == i.it; }
        bool operator!= (const iterator& i)     { return it != i.it; }
        key_type const& key ()                  { return it.first; }
        data_type& data ()                      { return it.second; }

    protected:
        typename map_type::iterator it;
    };

public:
    typedef syncunorderedmap::locktype locktype;
    typedef syncunorderedmap::scopedlocktype scopedlocktype;

    syncunorderedmaptype (const syncunorderedmaptype& m)
    {
        scopedlocktype sl (m.mlock);
        mmap = m.mmap;
    }

    syncunorderedmaptype ()
    { ; }

    // operations that are not inherently synchronous safe
    // (usually because they can change the contents of the map or
    // invalidated its members.)

    void operator= (const syncunorderedmaptype& m)
    {
        scopedlocktype sl (m.mlock);
        mmap = m.mmap;
    }

    void clear ()
    {
        mmap.clear();
    }

    int erase (key_type const& key)
    {
        return mmap.erase (key);
    }

    void erase (iterator& iterator)
    {
        mmap.erase (iterator.it);
    }

    void replace (key_type const& key, data_type const& data)
    {
        mmap[key] = data;
    }

    void rehash (int s)
    {
        mmap.rehash (s);
    }

    map_type& peekmap ()
    {
        return mmap;
    }

    // operations that are inherently synchronous safe

    std::size_t size () const
    {
        scopedlocktype sl (mlock);
        return mmap.size ();
    }

    // if the value is already in the map, replace it with the old value
    // otherwise, store the value passed.
    // returns 'true' if the value was added to the map
    bool canonicalize (key_type const& key, data_type* value)
    {
        scopedlocktype sl (mlock);

        auto it = mmap.emplace (key, *value);

        if (!it.second) // value was not added, take existing value
            *value = it.first->second;

        return it.second;
    }

    // retrieve the existing value from the map.
    // if none, return an 'empty' value
    data_type retrieve (key_type const& key)
    {
        data_type ret;

        {
            scopedlocktype sl (mlock);

            auto it = mmap.find (key);

            if (it != mmap.end ())
               ret = it->second;
        }

        return ret;
    }

private:
    map_type mmap;
    mutable locktype mlock;
};

} // ripple

#endif
