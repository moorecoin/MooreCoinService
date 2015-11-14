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

#ifndef ripple_directoryentryiterator_h_included
#define ripple_directoryentryiterator_h_included

#include <ripple/basics/base_uint.h>
#include <ripple/app/ledger/ledgerentryset.h>
#include <ripple/protocol/stledgerentry.h>

namespace ripple {

/** an iterator that walks the ledger entries in a single directory */
class directoryentryiterator
{

public:
    directoryentryiterator ()
        : mentry(0)
    {
    }

    directoryentryiterator (uint256 const& index)
        : mrootindex(index), mentry(0)
    {
    }

    /** construct from a reference to the root directory
    */
    directoryentryiterator (sle::ref directory) : mentry (0), mdirnode (directory)
    {
        if (mdirnode)
            mrootindex = mdirnode->getindex();
    }

    /** get the sle this iterator currently references */
    sle::pointer getentry (ledgerentryset& les, ledgerentrytype type);

    /** make this iterator point to the first offer */
    bool firstentry (ledgerentryset&);

    /** make this iterator point to the next offer */
    bool nextentry (ledgerentryset&);

    /** add this iterator's position to a json object */
    bool addjson (json::value&) const;

    /** set this iterator's position from a json object */
    bool setjson (json::value const&, ledgerentryset& les);

    uint256 const& getentryledgerindex () const
    {
        return mentryindex;
    }

    uint256 getdirectory () const
    {
        return mdirnode ? mdirnode->getindex () : uint256();
    }

    bool
    operator== (directoryentryiterator const& other) const
    {
        return mentry == other.mentry && mdirindex == other.mdirindex;
    }

    bool
    operator!= (directoryentryiterator const& other) const
    {
        return ! (*this == other);
    }

private:
    uint256      mrootindex;    // ledger index of the root directory
    uint256      mdirindex;     // ledger index of the current directory
    unsigned int mentry;        // entry index we are on (0 means first is next)
    uint256      mentryindex;   // ledger index of the current entry
    sle::pointer mdirnode;      // sle for the entry we are on
};

} // ripple

#endif
