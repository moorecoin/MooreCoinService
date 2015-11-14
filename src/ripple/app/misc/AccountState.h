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

#ifndef ripple_accountstate_h
#define ripple_accountstate_h

#include <ripple/basics/blob.h>
#include <ripple/protocol/rippleaddress.h>
#include <ripple/protocol/stamount.h>
#include <ripple/protocol/stledgerentry.h>

namespace ripple {

//
// provide abstract access to an account's state, such that access to the serialized format is hidden.
//

class accountstate
{
public:
    typedef std::shared_ptr<accountstate> pointer;

public:
    // for new accounts
    explicit accountstate (rippleaddress const& naaccountid);

    // for accounts in a ledger
    accountstate (sle::ref ledgerentry, rippleaddress const& naaccounti, sle::pointer slerefer);

    bool haveauthorizedkey ()
    {
        return mledgerentry->isfieldpresent (sfregularkey);
    }

    rippleaddress getauthorizedkey ()
    {
        return mledgerentry->getfieldaccount (sfregularkey);
    }

    stamount getbalance () const
    {
        return mledgerentry->getfieldamount (sfbalance);
    }

	stamount getbalancevbc() const
	{
		return mledgerentry->getfieldamount(sfbalancevbc);
	}

    std::uint32_t getseq () const
    {
        return mledgerentry->getfieldu32 (sfsequence);
    }

    stledgerentry::pointer getsle ()
    {
        return mledgerentry;
    }

    stledgerentry const& peeksle () const
    {
        return *mledgerentry;
    }

    stledgerentry& peeksle ()
    {
        return *mledgerentry;
    }

    blob getraw () const;

    void addjson (json::value& value);

    void dump ();

    static std::string creategravatarurl (uint128 uemailhash);

private:
    rippleaddress const maccountid;
    rippleaddress                  mauthorizedkey;
    stledgerentry::pointer mledgerentry;

    bool                           mvalid;
    sle::pointer mslerefer;
};

} // ripple

#endif
