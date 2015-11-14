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
#include <ripple/app/ledger/orderbookdb.h>
#include <ripple/app/ledger/ledgermaster.h>
#include <ripple/app/main/application.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/basics/log.h>
#include <ripple/core/config.h>
#include <ripple/core/jobqueue.h>
#include <ripple/protocol/indexes.h>
#include <ripple/protocol/jsonfields.h>

namespace ripple {

orderbookdb::orderbookdb (stoppable& parent)
    : stoppable ("orderbookdb", parent)
    , mseq (0)
{
}

void orderbookdb::invalidate ()
{
    scopedlocktype sl (mlock);
    mseq = 0;
}

void orderbookdb::setup (ledger::ref ledger)
{
    {
        scopedlocktype sl (mlock);
        auto seq = ledger->getledgerseq ();

        // do a full update every 256 ledgers
        if (mseq != 0)
        {
            if (seq == mseq)
                return;
            if ((seq > mseq) && ((seq - mseq) < 256))
                return;
            if ((seq < mseq) && ((mseq - seq) < 16))
                return;
        }

        writelog (lsdebug, orderbookdb)
            << "advancing from " << mseq << " to " << seq;

        mseq = seq;
    }

    if (getconfig().run_standalone)
        update(ledger);
    else
        getapp().getjobqueue().addjob(jtupdate_pf, "orderbookdb::update",
            std::bind(&orderbookdb::update, this, ledger));
}

static void updatehelper (sle::ref entry,
    hash_set< uint256 >& seen,
    orderbookdb::issuetoorderbook& destmap,
    orderbookdb::issuetoorderbook& sourcemap,
    hash_set< issue >& xrpbooks,
    int& books)
{
    if (entry->gettype () == ltdir_node &&
        entry->isfieldpresent (sfexchangerate) &&
        entry->getfieldh256 (sfrootindex) == entry->getindex())
    {
        book book;
        book.in.currency.copyfrom (entry->getfieldh160 (sftakerpayscurrency));
        book.in.account.copyfrom (entry->getfieldh160 (sftakerpaysissuer));
        book.out.account.copyfrom (entry->getfieldh160 (sftakergetsissuer));
        book.out.currency.copyfrom (entry->getfieldh160 (sftakergetscurrency));

        uint256 index = getbookbase (book);
        if (seen.insert (index).second)
        {
            auto orderbook = std::make_shared<orderbook> (index, book);
            sourcemap[book.in].push_back (orderbook);
            destmap[book.out].push_back (orderbook);
            if (isxrp(book.out))
                xrpbooks.insert(book.in);
			//if (isvbc(book.out))
			//	vbcbooks.insert(book.in);
            ++books;
        }
    }
}

void orderbookdb::update (ledger::pointer ledger)
{
    hash_set< uint256 > seen;
    orderbookdb::issuetoorderbook destmap;
    orderbookdb::issuetoorderbook sourcemap;
    hash_set< issue > xrpbooks;
	hash_set< issue > vbcbooks;

    writelog (lsdebug, orderbookdb) << "orderbookdb::update>";

    // walk through the entire ledger looking for orderbook entries
    int books = 0;

    try
    {
        ledger->visitstateitems(std::bind(&updatehelper, std::placeholders::_1,
                                          std::ref(seen), std::ref(destmap),
            std::ref(sourcemap), std::ref(xrpbooks), std::ref(books)));
    }
    catch (const shamapmissingnode&)
    {
        writelog (lsinfo, orderbookdb)
            << "orderbookdb::update encountered a missing node";
        scopedlocktype sl (mlock);
        mseq = 0;
        return;
    }

    writelog (lsdebug, orderbookdb)
        << "orderbookdb::update< " << books << " books found";
    {
        scopedlocktype sl (mlock);

        mxrpbooks.swap(xrpbooks);
        msourcemap.swap(sourcemap);
        mdestmap.swap(destmap);
    }
    getapp().getledgermaster().neworderbookdb();
}

void orderbookdb::addorderbook(book const& book)
{
    bool toxrp = isxrp (book.out);
	bool tovbc = isvbc (book.out);
    scopedlocktype sl (mlock);

    if (toxrp)
    {
        // we don't want to search through all the to-xrp or from-xrp order
        // books!
        for (auto ob: msourcemap[book.in])
        {
            if (isxrp (ob->getcurrencyout ())) // also to xrp
                return;
        }
    }
	else if (tovbc)
	{
		for (auto ob : msourcemap[book.in])
		{
			if (isvbc (ob->getcurrencyout())) // also to vbc
				return;
		}
	}
    else
    {
        for (auto ob: mdestmap[book.out])
        {
            if (ob->getcurrencyin() == book.in.currency &&
                ob->getissuerin() == book.in.account)
            {
                return;
            }
        }
    }
    uint256 index = getbookbase(book);
    auto orderbook = std::make_shared<orderbook> (index, book);

    msourcemap[book.in].push_back (orderbook);
    mdestmap[book.out].push_back (orderbook);
    if (toxrp)
        mxrpbooks.insert(book.in);
	if (tovbc)
		mvbcbooks.insert(book.in);
}

// return list of all orderbooks that want this issuerid and currencyid
orderbook::list orderbookdb::getbooksbytakerpays (issue const& issue)
{
    scopedlocktype sl (mlock);
    auto it = msourcemap.find (issue);
    return it == msourcemap.end () ? orderbook::list() : it->second;
}

int orderbookdb::getbooksize(issue const& issue) {
    scopedlocktype sl (mlock);
    auto it = msourcemap.find (issue);
    return it == msourcemap.end () ? 0 : it->second.size();
}

bool orderbookdb::isbooktoxrp(issue const& issue)
{
    scopedlocktype sl (mlock);
    return mxrpbooks.count(issue) > 0;
}

bool orderbookdb::isbooktovbc(issue const& issue)
{
	scopedlocktype sl(mlock);
	return mvbcbooks.count(issue) > 0;
}

booklisteners::pointer orderbookdb::makebooklisteners (book const& book)
{
    scopedlocktype sl (mlock);
    auto ret = getbooklisteners (book);

    if (!ret)
    {
        ret = std::make_shared<booklisteners> ();

        mlisteners [book] = ret;
        assert (getbooklisteners (book) == ret);
    }

    return ret;
}

booklisteners::pointer orderbookdb::getbooklisteners (book const& book)
{
    booklisteners::pointer ret;
    scopedlocktype sl (mlock);

    auto it0 = mlisteners.find (book);
    if (it0 != mlisteners.end ())
        ret = it0->second;

    return ret;
}

// based on the meta, send the meta to the streams that are listening.
// we need to determine which streams a given meta effects.
void orderbookdb::processtxn (
    ledger::ref ledger, const acceptedledgertx& altx)
{
    scopedlocktype sl (mlock);
    
    json::value jvobj;
    bool bjvobjinitialized = false;

    if (altx.getresult () == tessuccess)
    {
        // check if this is an offer or an offer cancel or a payment that
        // consumes an offer.
        // check to see what the meta looks like.
        for (auto& node : altx.getmeta ()->getnodes ())
        {
            try
            {
                if (node.getfieldu16 (sfledgerentrytype) == ltoffer)
                {
                    sfield const* field = nullptr;

                    // we need a field that contains the takergets and takerpays
                    // parameters.
                    if (node.getfname () == sfmodifiednode)
                        field = &sfpreviousfields;
                    else if (node.getfname () == sfcreatednode)
                        field = &sfnewfields;
                    else if (node.getfname () == sfdeletednode)
                        field = &sffinalfields;

                    if (field)
                    {
                        auto data = dynamic_cast<const stobject*> (
                            node.peekatpfield (*field));

                        if (data)
                        {
                            // determine the orderbook
                            auto listeners = getbooklisteners (
                                {data->getfieldamount (sftakergets).issue(),
                                 data->getfieldamount (sftakerpays).issue()});

                            if (listeners)
                            {
                                if (!bjvobjinitialized)
                                {
                                    jvobj = networkops_transjson (*altx.gettxn (), altx.getresult (), true, ledger);
                                    jvobj[jss::meta] = altx.getmeta ()->getjson (0);
                                    bjvobjinitialized = true;
                                }
                                listeners->publish (jvobj);
                            }
                        }
                    }
                }
            }
            catch (...)
            {
                writelog (lsinfo, orderbookdb)
                    << "fields not found in orderbookdb::processtxn";
            }
        }
    }
}

} // ripple
