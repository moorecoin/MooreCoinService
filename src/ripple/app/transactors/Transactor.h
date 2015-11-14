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

#ifndef ripple_tx_transactor_h_included
#define ripple_tx_transactor_h_included

#include <ripple/app/tx/transactionengine.h>

namespace ripple {

class transactor
{
public:
    static
    ter
    transact (
        sttx const& txn,
        transactionengineparams params,
        transactionengine* engine);

    ter
    apply ();

protected:
    sttx const&    mtxn;
    transactionengine*              mengine;
    transactionengineparams         mparams;

    account                         mtxnaccountid;
    stamount                        mfeedue;
    stamount                        mpriorbalance;  // balance before fees.
    stamount                        msourcebalance; // balance after fees.
    sle::pointer                    mtxnaccount;
    bool                            mhasauthkey;
    bool                            msigmaster;
    rippleaddress                   msigningpubkey;

    beast::journal m_journal;

    virtual ter precheck ();
    virtual ter checkseq ();
    virtual ter payfee ();

    virtual void calculatefee ();

    // returns the fee, not scaled for load (should be in fee units. fixme)
    virtual std::uint64_t calculatebasefee ();

    virtual ter checksig ();
    virtual ter doapply () = 0;

    transactor (
        const sttx& txn,
        transactionengineparams params,
        transactionengine* engine,
        beast::journal journal = beast::journal ());

    virtual bool musthavevalidaccount ()
    {
        return true;
    }
};

}

#endif
