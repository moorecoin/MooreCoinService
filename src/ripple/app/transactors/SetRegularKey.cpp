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
#include <ripple/app/transactors/transactor.h>
#include <ripple/basics/log.h>
#include <ripple/protocol/txflags.h>

namespace ripple {

class setregularkey
    : public transactor
{
    std::uint64_t calculatebasefee () override
    {
        if ( mtxnaccount
                && (! (mtxnaccount->getflags () & lsfpasswordspent))
                && (msigningpubkey.getaccountid () == mtxnaccountid))
        {
            // flag is armed and they signed with the right account
            return 0;
        }

        return transactor::calculatebasefee ();
    }

public:
    setregularkey (
        sttx const& txn,
        transactionengineparams params,
        transactionengine* engine)
        : transactor (
            txn,
            params,
            engine,
            deprecatedlogs().journal("setregularkey"))
    {

    }

    ter doapply () override
    {
        std::uint32_t const utxflags = mtxn.getflags ();

        if (utxflags & tfuniversalmask)
        {
            m_journal.trace <<
                "malformed transaction: invalid flags set.";

            return teminvalid_flag;
        }

        if (mfeedue == zero)
        {
            mtxnaccount->setflag (lsfpasswordspent);
        }

        if (mtxn.isfieldpresent (sfregularkey))
        {
            account uauthkeyid = mtxn.getfieldaccount160 (sfregularkey);
            mtxnaccount->setfieldaccount (sfregularkey, uauthkeyid);
        }
        else
        {
            if (mtxnaccount->isflag (lsfdisablemaster))
                return tecmaster_disabled;
            mtxnaccount->makefieldabsent (sfregularkey);
        }

        return tessuccess;
    }
};

ter
transact_setregularkey (
    sttx const& txn,
    transactionengineparams params,
    transactionengine* engine)
{
    return setregularkey(txn, params, engine).apply ();
}

}
