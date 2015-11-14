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

#ifndef ripple_orderbookiterator_h_included
#define ripple_orderbookiterator_h_included

#include <ripple/app/ledger/directoryentryiterator.h>
#include <ripple/protocol/indexes.h>

// vfalco todo split these to separate files and de-inline the definitions

namespace ripple {

/** an iterator that walks the directories in a book */
class bookdiriterator
{

public:

    bookdiriterator ()
    {
    }

    bookdiriterator (
        currency const& currencyin, account const& issuerin,
        currency const& currencyout, account const& issuerout);

    uint256 const& getbookbase () const
    {
        return mbase;
    }

    uint256 const& getbookend () const
    {
        return mend;
    }

    uint256 const& getcurrentindex() const
    {
        return mindex;
    }

    void setcurrentindex(uint256 const& index)
    {
        mindex = index;
    }

    /** get the current exchange rate
    */
    stamount getcurrentrate () const
    {
        return amountfromquality (getcurrentquality());
    }

    /** get the current quality
    */
    std::uint64_t getcurrentquality () const
    {
        return getquality(mindex);
    }

    /** make this iterator refer to the next book
    */
    bool nextdirectory (ledgerentryset&);

    /** make this iterator refer to the first book
    */
    bool firstdirectory (ledgerentryset&);

    /** the les may have changed
        resync the iterator
    */
    bool resync (ledgerentryset&);

    /** get an iterator to the offers in this directory
    */
    directoryentryiterator getofferiterator () const;

    std::uint64_t getrate () const;

    bool addjson (json::value&) const;
    bool setjson (json::value const&);

    // does this iterator currently point to a valid directory
    explicit
    operator bool () const
    {
        return mofferdir &&  (mofferdir->getindex() == mindex);
    }

    bool operator== (bookdiriterator const& other) const
    {
        assert (! mindex.iszero() && ! other.mindex.iszero());
        return mindex == other.mindex;
    }

    bool operator!= (bookdiriterator const& other) const
    {
        return ! (*this == other);
    }

private:
    uint256      mbase;     // the first index a directory in the book can have
    uint256      mend;      // the first index a directory in the book cannot have
    uint256      mindex;    // the index we are currently on
    sle::pointer mofferdir; // the directory page we are currently on
};

//------------------------------------------------------------------------------

/** an iterator that walks the offers in a book
    caution: the ledgerentryset must remain valid for the life of the iterator
*/
class orderbookiterator
{
public:
    orderbookiterator (
        ledgerentryset& set,
        currency const& currencyin,
        account const& issuerin,
        currency const& currencyout,
        account const& issuerout) :
            mentryset (set),
            mdirectoryiterator (currencyin, issuerin, currencyout, issuerout)
    {
    }

    orderbookiterator&
    operator= (orderbookiterator const&) = default;

    bool addjson (json::value&) const;

    bool setjson (json::value const&);

    stamount getcurrentrate () const;

    std::uint64_t getcurrentquality () const;

    uint256 getcurrentindex () const;

    uint256 getcurrentdirectory () const;

    sle::pointer getcurrentoffer ();

    /** position the iterator at the first offer in the first directory.
        returns whether there is an offer to point to.
    */
    bool firstoffer ();

    /** position the iterator at the next offer, going to the next directory if needed
        returns whether there is a next offer.
    */
    bool nextoffer ();

    /** position the iterator at the first offer in the next directory.
        returns whether there is a next directory to point to.
    */
    bool nextdir ();

    /** position the iterator at the first offer in the current directory.
        returns whether there is an offer in the directory.
    */
    bool firstofferindir ();

    /** position the iterator at the next offer in the current directory.
        returns whether there is a next offer in the directory.
    */
    bool nextofferindir ();

    /** position the iterator at the first offer at the current quality.
        if none, position the iterator at the first offer at the next quality.
        this rather odd semantic is required by the payment engine.
    */
    bool rewind ();

    ledgerentryset& peekentryset ()
    {
        return mentryset;
    }

    bookdiriterator const& peekdiriterator () const
    {
        return mdirectoryiterator;
    }

    directoryentryiterator const& peekdirectoryentryiterator () const
    {
        return mofferiterator;
    }

    bookdiriterator& peekdiriterator ()
    {
        return mdirectoryiterator;
    }

    directoryentryiterator& peekdirectoryentryiterator ()
    {
        return mofferiterator;
    }

    bool
    operator== (orderbookiterator const& other) const
    {
        return
            std::addressof(mentryset) == std::addressof(other.mentryset) &&
            mdirectoryiterator == other.mdirectoryiterator &&
            mofferiterator == other.mofferiterator;
    }

    bool
    operator!= (orderbookiterator const& other) const
    {
        return ! (*this == other);
    }

private:
    std::reference_wrapper <ledgerentryset> mentryset;
    bookdiriterator         mdirectoryiterator;
    directoryentryiterator  mofferiterator;
};

} // ripple

#endif
