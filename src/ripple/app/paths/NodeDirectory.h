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

#ifndef rippled_ripple_module_app_paths_nodedirectory_h
#define rippled_ripple_module_app_paths_nodedirectory_h

#include <ripple/app/ledger/ledgerentryset.h>
#include <ripple/protocol/indexes.h>

namespace ripple {

// vfalco todo de-inline these function definitions

class nodedirectory {
  public:
    // current directory - the last 64 bits of this are the quality.
    uint256 current;

    // start of the next order book - one past the worst quality possible
    // for the current order book.
    uint256 next;

    // todo(tom): directory.current and directory.next should be of type
    // directory.

    bool advanceneeded;  // need to advance directory.
    bool restartneeded;  // need to restart directory.

    sle::pointer ledgerentry;

    void restart (bool multiquality)
    {
        if (multiquality)
            current = 0;             // restart book searching.
        else
            restartneeded  = true;   // restart at same quality.
    }

    bool initialize (book const& book, ledgerentryset& les)
    {
        if (current != zero)
            return false;

        current.copyfrom (getbookbase (book));
        next.copyfrom (getqualitynext (current));

        // todo(tom): it seems impossible that any actual offers with
        // quality == 0 could occur - we should disallow them, and clear
        // directory.ledgerentry without the database call in the next line.
        ledgerentry = les.entrycache (ltdir_node, current);

        // advance, if didn't find it. normal not to be unable to lookup
        // firstdirectory. maybe even skip this lookup.
        advanceneeded  = !ledgerentry;
        restartneeded  = false;

        // associated vars are dirty, if found it.
        return bool(ledgerentry);
    }

    enum advance {no_advance, new_quality, end_advance};
    advance advance(ledgerentryset& les)
    {
        if (!(advanceneeded || restartneeded))
            return no_advance;

        // get next quality.
        // the merkel radix tree is ordered by key so we can go to the next
        // quality in o(1).
        if (advanceneeded)
            current = les.getnextledgerindex (current, next);

        advanceneeded  = false;
        restartneeded  = false;

        if (current == zero)
            return end_advance;

        ledgerentry = les.entrycache (ltdir_node, current);
        return new_quality;
    }
};

} // ripple

#endif
