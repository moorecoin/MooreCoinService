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

#ifndef ripple_basics_rangeset_h_included
#define ripple_basics_rangeset_h_included

#include <beast/utility/noexcept.h>
#include <cstdint>
#include <map>
#include <string>

namespace ripple {

/** a sparse set of integers. */
// vfalco todo replace with juce::sparseset
class rangeset
{
public:
    static const std::uint32_t absent = static_cast <std::uint32_t> (-1);

public:
    rangeset () { }

    bool hasvalue (std::uint32_t) const;

    std::uint32_t getfirst () const;
    std::uint32_t getnext (std::uint32_t) const;
    std::uint32_t getlast () const;
    std::uint32_t getprev (std::uint32_t) const;

    // largest number not in the set that is less than the given number
    std::uint32_t prevmissing (std::uint32_t) const;

    // add an item to the set
    void setvalue (std::uint32_t);

    // add the closed interval to the set
    void setrange (std::uint32_t, std::uint32_t);

    void clearvalue (std::uint32_t);

    std::string tostring () const;

    /** check invariants of the data.

        this is for diagnostics, and does nothing in release builds.
    */
    void checkinternalconsistency () const noexcept;

private:
    void simplify ();

private:
    typedef std::map <std::uint32_t, std::uint32_t> map;

    typedef map::const_iterator            const_iterator;
    typedef map::const_reverse_iterator    const_reverse_iterator;
    typedef map::value_type                value_type;
    typedef map::iterator                  iterator;

    static bool contains (value_type const& it, std::uint32_t v)
    {
        return (it.first <= v) && (it.second >= v);
    }

    // first is lowest value in range, last is highest value in range
    map mranges;
};

} // ripple

#endif
