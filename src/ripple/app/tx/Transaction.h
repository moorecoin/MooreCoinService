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

#ifndef ripple_transaction_h
#define ripple_transaction_h

#include <ripple/protocol/protocol.h>
#include <ripple/protocol/sttx.h>
#include <ripple/protocol/ter.h>

namespace ripple {

//
// transactions should be constructed in json with. use stobject::parsejson to
// obtain a binary version.
//

class database;

enum transstatus
{
    new         = 0, // just received / generated
    invalid     = 1, // no valid signature, insufficient funds
    included    = 2, // added to the current ledger
    conflicted  = 3, // losing to a conflicting transaction
    committed   = 4, // known to be in a ledger
    held        = 5, // not valid now, maybe later
    removed     = 6, // taken out of a ledger
    obsolete    = 7, // a compatible transaction has taken precedence
    incomplete  = 8  // needs more signatures
};

enum class validate {no, yes};

// this class is for constructing and examining transactions.
// transactions are static so manipulation functions are unnecessary.
class transaction
    : public std::enable_shared_from_this<transaction>
    , public countedobject <transaction>
{
public:
    static char const* getcountedobjectname () { return "transaction"; }

    typedef std::shared_ptr<transaction> pointer;
    typedef const pointer& ref;

public:
    transaction (sttx::ref, validate);

    static transaction::pointer sharedtransaction (blob const&, validate);
    static transaction::pointer transactionfromsql (database*, validate);

    bool checksign () const;

    sttx::ref getstransaction ()
    {
        return mtransaction;
    }

    uint256 const& getid () const
    {
        return mtransactionid;
    }

    ledgerindex getledger () const
    {
        return minledger;
    }

    transstatus getstatus () const
    {
        return mstatus;
    }

    ter getresult ()
    {
        return mresult;
    }

    void setresult (ter terresult)
    {
        mresult = terresult;
    }

    void setstatus (transstatus status, std::uint32_t ledgerseq);

    void setstatus (transstatus status)
    {
        mstatus = status;
    }

    void setledger (ledgerindex ledger)
    {
        minledger = ledger;
    }

    json::value getjson (int options, bool binary = false) const;

    static transaction::pointer load (uint256 const& id);

    static bool ishextxid (std::string const&);

protected:
    static transaction::pointer transactionfromsql (std::string const&);

private:
    uint256         mtransactionid;
    rippleaddress   maccountfrom;
    rippleaddress   mfrompubkey;    // sign transaction with this. msignpubkey
    rippleaddress   msourceprivate; // sign transaction with this.

    ledgerindex     minledger;
    transstatus     mstatus;
    ter             mresult;

    sttx::pointer mtransaction;
};

} // ripple

#endif
