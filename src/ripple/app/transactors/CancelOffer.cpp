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
#include <ripple/protocol/indexes.h>
#include <ripple/protocol/txflags.h>

namespace ripple {

class canceloffer
    : public transactor
{
public:
    canceloffer (
        sttx const& txn,
        transactionengineparams params,
        transactionengine* engine)
        : transactor (
            txn,
            params,
            engine,
            deprecatedlogs().journal("canceloffer"))
    {

    }

    ter doapply () override
    {
        std::uint32_t const uoffersequence = mtxn.getfieldu32 (sfoffersequence);
        std::uint32_t const uaccountsequencenext = mtxnaccount->getfieldu32 (sfsequence);

        m_journal.debug <<
            "uaccountsequencenext=" << uaccountsequencenext <<
            " uoffersequence=" << uoffersequence;

        std::uint32_t const utxflags (mtxn.getflags ());

        if (utxflags & tfuniversalmask)
        {
            m_journal.trace <<
                "malformed transaction: invalid flags set.";
            return teminvalid_flag;
        }

        if (!uoffersequence || uaccountsequencenext - 1 <= uoffersequence)
        {
            m_journal.trace <<
                "uaccountsequencenext=" << uaccountsequencenext <<
                " uoffersequence=" << uoffersequence;
            return tembad_sequence;
        }

        uint256 const offerindex (getofferindex (mtxnaccountid, uoffersequence));

        sle::pointer sleoffer (mengine->entrycache (ltoffer, offerindex));

        if (sleoffer)
        {
            m_journal.debug <<
                "offercancel: uoffersequence=" << uoffersequence;

            return mengine->view ().offerdelete (sleoffer);
        }

        m_journal.warning <<
            "offercancel: offer not found: " <<
            to_string (mtxnaccountid) <<
            " : " << uoffersequence <<
            " : " << to_string (offerindex);

        return tessuccess;
    }
};

ter
transact_canceloffer (
    sttx const& txn,
    transactionengineparams params,
    transactionengine* engine)
{
    return canceloffer (txn, params, engine).apply ();
}

}
