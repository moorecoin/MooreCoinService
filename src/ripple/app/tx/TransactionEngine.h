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

#ifndef ripple_app_transactionengine_h_included
#define ripple_app_transactionengine_h_included

#include <ripple/app/ledger/ledger.h>
#include <ripple/app/ledger/ledgerentryset.h>

namespace ripple {

// a transactionengine applies serialized transactions to a ledger
// it can also, verify signatures, verify fees, and give rejection reasons

// one instance per ledger.
// only one transaction applied at a time.
class transactionengine
    : public countedobject <transactionengine>
{
public:
    static char const* getcountedobjectname () { return "transactionengine"; }

private:
    ledgerentryset      mnodes;

    ter setauthorized (const sttx & txn, bool bmustsetgenerator);
    ter checksig (const sttx & txn);

protected:
    ledger::pointer     mledger;
    int                 mtxnseq;

    account             mtxnaccountid;
    sle::pointer        mtxnaccount;

    void                txnwrite ();

public:
    typedef std::shared_ptr<transactionengine> pointer;

    transactionengine () : mtxnseq (0)
    {
        ;
    }
    transactionengine (ledger::ref ledger) : mledger (ledger), mtxnseq (0)
    {
        assert (mledger);
    }

    ledgerentryset& view ()
    {
        return mnodes;
    }
    ledger::ref getledger ()
    {
        return mledger;
    }
    void setledger (ledger::ref ledger)
    {
        assert (ledger);
        mledger = ledger;
    }

    // vfalco todo remove these pointless wrappers
    sle::pointer entrycreate (ledgerentrytype type, uint256 const& index)
    {
        return mnodes.entrycreate (type, index);
    }

    sle::pointer entrycache (ledgerentrytype type, uint256 const& index)
    {
        return mnodes.entrycache (type, index);
    }

    void entrydelete (sle::ref sleentry)
    {
        mnodes.entrydelete (sleentry);
    }

    void entrymodify (sle::ref sleentry)
    {
        mnodes.entrymodify (sleentry);
    }

    ter applytransaction (const sttx&, transactionengineparams, bool & didapply);
    bool checkinvariants (ter result, const sttx & txn, transactionengineparams params);
};

inline transactionengineparams operator| (const transactionengineparams& l1, const transactionengineparams& l2)
{
    return static_cast<transactionengineparams> (static_cast<int> (l1) | static_cast<int> (l2));
}

inline transactionengineparams operator& (const transactionengineparams& l1, const transactionengineparams& l2)
{
    return static_cast<transactionengineparams> (static_cast<int> (l1) & static_cast<int> (l2));
}

} // ripple

#endif
