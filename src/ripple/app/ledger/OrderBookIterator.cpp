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
#include <ripple/app/ledger/orderbookiterator.h>
#include <ripple/basics/log.h>

namespace ripple {

/** iterate through the directories in an order book */
bookdiriterator::bookdiriterator(
    currency const& currencyin, account const& issuerin,
    currency const& currencyout, account const& issuerout)
{
    mbase = ripple::getbookbase({{currencyin, issuerin},
            {currencyout, issuerout}});
    mend = getqualitynext(mbase);
    mindex = mbase;
}

bool bookdiriterator::nextdirectory (ledgerentryset& les)
{
    writelog (lstrace, ledger) << "bookdirectoryiterator:: nextdirectory";

    // are we already at the end?
    if (mindex.iszero ())
        return false;

    // get the ledger index of the next directory
    mindex = les.getnextledgerindex (mindex, mend);

    if (mindex.iszero ())
    {
        // we ran off the end of the book
        writelog (lstrace, ledger) <<
            "bookdirectoryiterator:: no next ledger index";
        return false;
    }
    assert (mindex < mend);

    writelog (lstrace, ledger) <<
        "bookdirectoryiterator:: index " << to_string (mindex);

    // retrieve the sle from the les
    mofferdir = les.entrycache (ltdir_node, mindex);
    assert (mofferdir);

    return bool(mofferdir);
}

bool bookdiriterator::firstdirectory (ledgerentryset& les)
{
    writelog (lstrace, ledger) <<
        "bookdiriterator(" << to_string (mbase) << ") firstdirectory";

    /** jump to the beginning
    */
    mindex = mbase;

    return nextdirectory (les);
}

/** the les may have changed. repoint to the current directory if it still exists,
    otherwise, go to the next one.
*/
bool bookdiriterator::resync (ledgerentryset& les)
{
    if (mindex.iszero ())
        mindex = mbase;
    else if (mindex != mbase)
        --mindex;

    return nextdirectory (les);
}

directoryentryiterator bookdiriterator::getofferiterator () const
{
    writelog (lstrace, ledger) << "bookdiriterator(" <<
        to_string (mbase) << ") get offer iterator";
    return directoryentryiterator (mofferdir);
}

std::uint64_t bookdiriterator::getrate () const
{
    return getquality(mindex);
}

bool bookdiriterator::addjson (json::value& jv) const
{
    if (! (*this))
        return false;

    jv["book_index"] = to_string (mindex);
    return true;
}

bool bookdiriterator::setjson(json::value const& jv)
{
    if (!jv.ismember("book_index"))
        return false;
    json::value const& bi = jv["book_index"];
    if (!bi.isstring ())
        return false;
    mindex.sethexexact(bi.asstring());
    return true;
}

bool orderbookiterator::addjson (json::value& jv) const
{
    return mofferiterator.addjson(jv) && mdirectoryiterator.addjson(jv);
}

bool orderbookiterator::setjson (json::value const& jv)
{
    return mdirectoryiterator.setjson (jv) && mofferiterator.setjson (jv, mentryset);
}

stamount orderbookiterator::getcurrentrate () const
{
    return mdirectoryiterator.getcurrentrate();
}

std::uint64_t orderbookiterator::getcurrentquality () const
{
    return mdirectoryiterator.getcurrentquality();
}

uint256 orderbookiterator::getcurrentdirectory () const
{
    return mofferiterator.getdirectory ();
}

uint256 orderbookiterator::getcurrentindex () const
{
    return mofferiterator.getentryledgerindex();
}

/** retrieve the offer the iterator points to
*/
sle::pointer orderbookiterator::getcurrentoffer ()
{
    return mofferiterator.getentry (mentryset, ltoffer);
}

/** go to the first offer in the first directory
*/
bool orderbookiterator::firstoffer ()
{
    writelog (lstrace, ledger) << "orderbookiterator: first offer";
    // go to first directory in order book
    if (!mdirectoryiterator.firstdirectory (mentryset))
    {
        writelog (lstrace, ledger) << "orderbookiterator: no first directory";
        return false;
    }
    mofferiterator = mdirectoryiterator.getofferiterator ();

    // take the next offer
    return nextoffer();
}

/** go to the next offer, possibly changing directories
*/
bool orderbookiterator::nextoffer ()
{
    writelog (lstrace, ledger) << "orderbookiterator: next offer";
    do
    {

         // is there a next offer in the current directory
         if (mofferiterator.nextentry (mentryset))
         {
             writelog (lstrace, ledger) << "orderbookiterator: there is a next offer in this directory";
             return true;
         }

         // is there a next directory
         if (!mdirectoryiterator.nextdirectory (mentryset))
         {
             writelog (lstrace, ledger) << "orderbookiterator: there is no next directory";
             return false;
         }
         writelog (lstrace, ledger) << "orderbookiterator: going to next directory";

         // set to before its first offer
         mofferiterator = mdirectoryiterator.getofferiterator ();
     }
     while (1);
}

/** rewind to the beginning of this directory, then take the next offer
*/
bool orderbookiterator::rewind ()
{
    if (!mdirectoryiterator.resync (mentryset))
        return false;

    mofferiterator = mdirectoryiterator.getofferiterator ();
    return nextoffer ();
}

/** go to before the first offer in the next directory
*/
bool orderbookiterator::nextdir ()
{
    if (!mdirectoryiterator.nextdirectory (mentryset))
        return false;

    mofferiterator = mdirectoryiterator.getofferiterator ();

    return true;
}

/** advance to the next offer in this directory
*/
bool orderbookiterator::nextofferindir ()
{
    return mofferiterator.nextentry (mentryset);
}

} // ripple
