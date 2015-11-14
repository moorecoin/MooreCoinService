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

#ifndef ripple_acceptedledgertx_h
#define ripple_acceptedledgertx_h

#include <ripple/app/ledger/ledger.h>

namespace ripple {

/**
    a transaction that is in a closed ledger.

    description

    an accepted ledger transaction contains additional information that the
    server needs to tell clients about the transaction. for example,
        - the transaction in json form
        - which accounts are affected
          * this is used by infosub to report to clients
        - cached stuff

    @code
    @endcode

    @see {uri}

    @ingroup ripple_ledger
*/
class acceptedledgertx
{
public:
    typedef std::shared_ptr <acceptedledgertx> pointer;
    typedef const pointer& ref;

public:
    acceptedledgertx (ledger::ref ledger, serializeriterator& sit);
    acceptedledgertx (ledger::ref ledger, sttx::ref,
        transactionmetaset::ref);
    acceptedledgertx (ledger::ref ledger, sttx::ref, ter result);

    sttx::ref gettxn () const
    {
        return mtxn;
    }
    transactionmetaset::ref getmeta () const
    {
        return mmeta;
    }
    std::vector <rippleaddress> const& getaffected () const
    {
        return maffected;
    }

    txid gettransactionid () const
    {
        return mtxn->gettransactionid ();
    }
    txtype gettxntype () const
    {
        return mtxn->gettxntype ();
    }
    ter getresult () const
    {
        return mresult;
    }
    std::uint32_t gettxnseq () const
    {
        return mmeta->getindex ();
    }

    bool isapplied () const
    {
        return bool(mmeta);
    }
    int getindex () const
    {
        return mmeta ? mmeta->getindex () : 0;
    }
    std::string getescmeta () const;
    json::value getjson ()
    {
        if (mjson == json::nullvalue)
            buildjson ();
        return mjson;
    }

private:
    ledger::pointer                 mledger;
    sttx::pointer  mtxn;
    transactionmetaset::pointer     mmeta;
    ter                             mresult;
    std::vector <rippleaddress>     maffected;
    blob        mrawmeta;
    json::value                     mjson;

    void buildjson ();
};

} // ripple

#endif
