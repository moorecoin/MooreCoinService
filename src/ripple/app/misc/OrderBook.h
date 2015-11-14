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

#ifndef ripple_orderbook_h
#define ripple_orderbook_h

namespace ripple {

/** describes a serialized ledger entry for an order book. */
class orderbook : beast::leakchecked <orderbook>
{
public:
    typedef std::shared_ptr <orderbook> pointer;
    typedef std::shared_ptr <orderbook> const& ref;
    typedef std::vector<pointer> list;

    /** construct from a currency specification.

        @param index ???
        @param book in and out currency/issuer pairs.
    */
    // vfalco note what is the meaning of the index parameter?
    orderbook (uint256 const& base, book const& book)
            : mbookbase(base), mbook(book)
    {
    }

    uint256 const& getbookbase () const
    {
        return mbookbase;
    }

    book const& book() const
    {
        return mbook;
    }

    currency const& getcurrencyin () const
    {
        return mbook.in.currency;
    }

    currency const& getcurrencyout () const
    {
        return mbook.out.currency;
    }

    account const& getissuerin () const
    {
        return mbook.in.account;
    }

    account const& getissuerout () const
    {
        return mbook.out.account;
    }

private:
    uint256 const mbookbase;
    book const mbook;
};

} // ripple

#endif
