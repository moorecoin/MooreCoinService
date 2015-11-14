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

#ifndef ripple_orderbookdb_h_included
#define ripple_orderbookdb_h_included

#include <ripple/app/ledger/acceptedledgertx.h>
#include <ripple/app/ledger/booklisteners.h>
#include <ripple/app/misc/orderbook.h>

namespace ripple {

class orderbookdb
    : public beast::stoppable
    , public beast::leakchecked <orderbookdb>
{
public:
    explicit orderbookdb (stoppable& parent);

    void setup (ledger::ref ledger);
    void update (ledger::pointer ledger);
    void invalidate ();

    void addorderbook(book const&);

    /** @return a list of all orderbooks that want this issuerid and currencyid.
     */
    orderbook::list getbooksbytakerpays (issue const&);

    /** @return a count of all orderbooks that want this issuerid and currencyid.
     */
    int getbooksize(issue const&);

    bool isbooktoxrp (issue const&);
	bool isbooktovbc (issue const&);

    booklisteners::pointer getbooklisteners (book const&);
    booklisteners::pointer makebooklisteners (book const&);

    // see if this txn effects any orderbook
    void processtxn (
        ledger::ref ledger, const acceptedledgertx& altx);

    typedef hash_map <issue, orderbook::list> issuetoorderbook;

private:
    void rawaddbook(book const&);

    // by ci/ii
    issuetoorderbook msourcemap;

    // by co/io
    issuetoorderbook mdestmap;

    // does an order book to xrp exist
    hash_set <issue> mxrpbooks;

	// does an order book to vbc exist
	hash_set <issue> mvbcbooks;

    typedef ripplerecursivemutex locktype;
    typedef std::lock_guard <locktype> scopedlocktype;
    locktype mlock;

    typedef hash_map <book, booklisteners::pointer>
    booktolistenersmap;

    booktolistenersmap mlisteners;

    std::uint32_t mseq;
};

} // ripple

#endif
