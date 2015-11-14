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

#include <beastconfig.h>
#include <ripple/basics/log.h>
#include <ripple/basics/rangeset.h>
#include <beast/unit_test/suite.h>
#include <beast/module/core/text/lexicalcast.h>
#include <boost/foreach.hpp>
#include <cstdint>

namespace ripple {

// vfalco note std::min and std::max not good enough?
//        note why isn't this written as a template?
//        todo replace this with std calls.
//
inline std::uint32_t min (std::uint32_t x, std::uint32_t y)
{
    return (x < y) ? x : y;
}
inline std::uint32_t max (std::uint32_t x, std::uint32_t y)
{
    return (x > y) ? x : y;
}

bool rangeset::hasvalue (std::uint32_t v) const
{
    boost_foreach (const value_type & it, mranges)
    {
        if (contains (it, v))
            return true;
    }
    return false;
}

std::uint32_t rangeset::getfirst () const
{
    const_iterator it = mranges.begin ();

    if (it == mranges.end ())
        return absent;

    return it->first;
}

std::uint32_t rangeset::getnext (std::uint32_t v) const
{
    boost_foreach (const value_type & it, mranges)
    {
        if (it.first > v)
            return it.first;

        if (contains (it, v + 1))
            return v + 1;
    }
    return absent;
}

std::uint32_t rangeset::getlast () const
{
    const_reverse_iterator it = mranges.rbegin ();

    if (it == mranges.rend ())
        return absent;

    return it->second;
}

std::uint32_t rangeset::getprev (std::uint32_t v) const
{
    boost_reverse_foreach (const value_type & it, mranges)
    {
        if (it.second < v)
            return it.second;

        if (contains (it, v + 1))
            return v - 1;
    }
    return absent;
}

// return the largest number not in the set that is less than the given number
//
std::uint32_t rangeset::prevmissing (std::uint32_t v) const
{
    std::uint32_t result = absent;

    if (v != 0)
    {
        checkinternalconsistency ();

        // handle the case where the loop reaches the terminating condition
        //
        result = v - 1;

        for (const_reverse_iterator cur = mranges.rbegin (); cur != mranges.rend (); ++cur)
        {
            // see if v-1 is in the range
            if (contains (*cur, result))
            {
                result = cur->first - 1;
                break;
            }
        }
    }

    bassert (result == absent || !hasvalue (result));

    return result;
}

void rangeset::setvalue (std::uint32_t v)
{
    if (!hasvalue (v))
    {
        mranges[v] = v;

        simplify ();
    }
}

void rangeset::setrange (std::uint32_t minv, std::uint32_t maxv)
{
    while (hasvalue (minv))
    {
        ++minv;

        if (minv >= maxv)
            return;
    }

    mranges[minv] = maxv;

    simplify ();
}

void rangeset::clearvalue (std::uint32_t v)
{
    for (iterator it = mranges.begin (); it != mranges.end (); ++it)
    {
        if (contains (*it, v))
        {
            if (it->first == v)
            {
                if (it->second == v)
                {
                    mranges.erase (it);
                }
                else
                {
                    std::uint32_t oldend = it->second;
                    mranges.erase(it);
                    mranges[v + 1] = oldend;
                }
            }
            else if (it->second == v)
            {
                -- (it->second);
            }
            else
            {
                std::uint32_t oldend = it->second;
                it->second = v - 1;
                mranges[v + 1] = oldend;
            }

            checkinternalconsistency();
            return;
        }
    }
}

std::string rangeset::tostring () const
{
    std::string ret;
    boost_foreach (value_type const & it, mranges)
    {
        if (!ret.empty ())
            ret += ",";

        if (it.first == it.second)
            ret += beast::lexicalcastthrow <std::string> ((it.first));
        else
            ret += beast::lexicalcastthrow <std::string> (it.first) + "-"
                   + beast::lexicalcastthrow <std::string> (it.second);
    }

    if (ret.empty ())
        return "empty";

    return ret;
}

void rangeset::simplify ()
{
    iterator it = mranges.begin ();

    while (1)
    {
        iterator nit = it;

        if (++nit == mranges.end ())
        {
            checkinternalconsistency();
            return;
        }

        if (it->second >= (nit->first - 1))
        {
            // ranges overlap
            it->second = std::max(it->second, nit->second);
            mranges.erase (nit);
        }
        else
        {
            it = nit;
        }
    }
}

void rangeset::checkinternalconsistency () const noexcept
{
#ifndef ndebug
    if (mranges.size () > 1)
    {
        const_iterator const last = std::prev (mranges.end ());

        for (const_iterator cur = mranges.begin (); cur != last; ++cur)
        {
            const_iterator const next = std::next (cur);
            assert (cur->first <= cur->second);
            assert (next->first <= next->second);
            assert (cur->second + 1 < next->first);
        }
    }
    else if (mranges.size () == 1)
    {
        const_iterator const iter = mranges.begin ();
        assert (iter->first <= iter->second);
    }
#endif
}

} // ripple
