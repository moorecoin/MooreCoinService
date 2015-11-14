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
#include <ripple/app/book/quality.h>
#include <ripple/app/transactors/transactor.h>
#include <ripple/basics/log.h>
#include <ripple/core/config.h>
#include <ripple/protocol/indexes.h>
#include <ripple/protocol/txflags.h>

namespace ripple {

class setaccount
    : public transactor
{
    static std::size_t const domain_bytes_max = 256;
    static std::size_t const public_bytes_max = 33;

public:
    setaccount (
        sttx const& txn,
        transactionengineparams params,
        transactionengine* engine)
        : transactor (
            txn,
            params,
            engine,
            deprecatedlogs().journal("setaccount"))
    {

    }

    ter doapply () override
    {
        std::uint32_t const utxflags = mtxn.getflags ();

        std::uint32_t const uflagsin = mtxnaccount->getfieldu32 (sfflags);
        std::uint32_t uflagsout = uflagsin;

        std::uint32_t const usetflag = mtxn.getfieldu32 (sfsetflag);
        std::uint32_t const uclearflag = mtxn.getfieldu32 (sfclearflag);

        if ((usetflag != 0) && (usetflag == uclearflag))
        {
            m_journal.trace << "malformed transaction: set and clear same flag";
            return teminvalid_flag;
        }

        // legacy accountset flags
        bool bsetrequiredest   = (utxflags & txflag::requiredesttag) || (usetflag == asfrequiredest);
        bool bclearrequiredest = (utxflags & tfoptionaldesttag) || (uclearflag == asfrequiredest);
        bool bsetrequireauth   = (utxflags & tfrequireauth) || (usetflag == asfrequireauth);
        bool bclearrequireauth = (utxflags & tfoptionalauth) || (uclearflag == asfrequireauth);
        bool bsetdisallowxrp   = (utxflags & tfdisallowxrp) || (usetflag == asfdisallowxrp);
        bool bcleardisallowxrp = (utxflags & tfallowxrp) || (uclearflag == asfdisallowxrp);

        if (utxflags & tfaccountsetmask)
        {
            m_journal.trace << "malformed transaction: invalid flags set.";
            return teminvalid_flag;
        }

        //
        // requireauth
        //

        if (bsetrequireauth && bclearrequireauth)
        {
            m_journal.trace << "malformed transaction: contradictory flags set.";
            return teminvalid_flag;
        }

        if (bsetrequireauth && !(uflagsin & lsfrequireauth))
        {
            if (!mengine->view().dirisempty (getownerdirindex (mtxnaccountid)))
            {
                m_journal.trace << "retry: owner directory not empty.";
                return (mparams & tapretry) ? terowners : tecowners;
            }

            m_journal.trace << "set requireauth.";
            uflagsout |= lsfrequireauth;
        }

        if (bclearrequireauth && (uflagsin & lsfrequireauth))
        {
            m_journal.trace << "clear requireauth.";
            uflagsout &= ~lsfrequireauth;
        }

        //
        // requiredesttag
        //

        if (bsetrequiredest && bclearrequiredest)
        {
            m_journal.trace << "malformed transaction: contradictory flags set.";
            return teminvalid_flag;
        }

        if (bsetrequiredest && !(uflagsin & lsfrequiredesttag))
        {
            m_journal.trace << "set lsfrequiredesttag.";
            uflagsout |= lsfrequiredesttag;
        }

        if (bclearrequiredest && (uflagsin & lsfrequiredesttag))
        {
            m_journal.trace << "clear lsfrequiredesttag.";
            uflagsout &= ~lsfrequiredesttag;
        }

        //
        // disallowxrp
        //

        if (bsetdisallowxrp && bcleardisallowxrp)
        {
            m_journal.trace << "malformed transaction: contradictory flags set.";
            return teminvalid_flag;
        }

        if (bsetdisallowxrp && !(uflagsin & lsfdisallowxrp))
        {
            m_journal.trace << "set lsfdisallowxrp.";
            uflagsout |= lsfdisallowxrp;
        }

        if (bcleardisallowxrp && (uflagsin & lsfdisallowxrp))
        {
            m_journal.trace << "clear lsfdisallowxrp.";
            uflagsout &= ~lsfdisallowxrp;
        }

        //
        // disablemaster
        //

        if ((usetflag == asfdisablemaster) && !(uflagsin & lsfdisablemaster))
        {
            if (!mtxnaccount->isfieldpresent (sfregularkey))
                return tecno_regular_key;

            m_journal.trace << "set lsfdisablemaster.";
            uflagsout   |= lsfdisablemaster;
        }

        if ((uclearflag == asfdisablemaster) && (uflagsin & lsfdisablemaster))
        {
            m_journal.trace << "clear lsfdisablemaster.";
            uflagsout   &= ~lsfdisablemaster;
        }

        if (usetflag == asfnofreeze)
        {
            m_journal.trace << "set nofreeze flag";
            uflagsout   |= lsfnofreeze;
        }

        // anyone may set global freeze
        if (usetflag == asfglobalfreeze)
        {
            m_journal.trace << "set globalfreeze flag";
            uflagsout   |= lsfglobalfreeze;
        }

        // if you have set nofreeze, you may not clear globalfreeze
        // this prevents those who have set nofreeze from using
        // globalfreeze strategically.
        if ((usetflag != asfglobalfreeze) && (uclearflag == asfglobalfreeze) &&
            ((uflagsout & lsfnofreeze) == 0))
        {
            m_journal.trace << "clear globalfreeze flag";
            uflagsout   &= ~lsfglobalfreeze;
        }

        //
        // track transaction ids signed by this account in its root
        //

        if ((usetflag == asfaccounttxnid) && !mtxnaccount->isfieldpresent (sfaccounttxnid))
        {
            m_journal.trace << "set accounttxnid";
            mtxnaccount->makefieldpresent (sfaccounttxnid);
         }

        if ((uclearflag == asfaccounttxnid) && mtxnaccount->isfieldpresent (sfaccounttxnid))
        {
            m_journal.trace << "clear accounttxnid";
            mtxnaccount->makefieldabsent (sfaccounttxnid);
        }

        //
        // emailhash
        //

        if (mtxn.isfieldpresent (sfemailhash))
        {
            uint128 const uhash = mtxn.getfieldh128 (sfemailhash);

            if (!uhash)
            {
                m_journal.trace << "unset email hash";
                mtxnaccount->makefieldabsent (sfemailhash);
            }
            else
            {
                m_journal.trace << "set email hash";
                mtxnaccount->setfieldh128 (sfemailhash, uhash);
            }
        }

        //
        // walletlocator
        //

        if (mtxn.isfieldpresent (sfwalletlocator))
        {
            uint256 const uhash = mtxn.getfieldh256 (sfwalletlocator);

            if (!uhash)
            {
                m_journal.trace << "unset wallet locator";
                mtxnaccount->makefieldabsent (sfwalletlocator);
            }
            else
            {
                m_journal.trace << "set wallet locator";
                mtxnaccount->setfieldh256 (sfwalletlocator, uhash);
            }
        }

        //
        // messagekey
        //

        if (mtxn.isfieldpresent (sfmessagekey))
        {
            blob messagekey = mtxn.getfieldvl (sfmessagekey);

            if (messagekey.empty ())
            {
                m_journal.debug << "set message key";
                mtxnaccount->makefieldabsent (sfmessagekey);
            }
            if (messagekey.size () > public_bytes_max)
            {
                m_journal.trace << "message key too long";
                return telbad_public_key;
            }
            else
            {
                m_journal.debug << "set message key";
                mtxnaccount->setfieldvl (sfmessagekey, messagekey);
            }
        }

        //
        // domain
        //

        if (mtxn.isfieldpresent (sfdomain))
        {
            blob const domain = mtxn.getfieldvl (sfdomain);

            if (domain.size () > domain_bytes_max)
            {
                m_journal.trace << "domain too long";
                return telbad_domain;
            }

            if (domain.empty ())
            {
                m_journal.trace << "unset domain";
                mtxnaccount->makefieldabsent (sfdomain);
            }
            else
            {
                m_journal.trace << "set domain";
                mtxnaccount->setfieldvl (sfdomain, domain);
            }
        }

        //
        // transferrate
        //

        if (mtxn.isfieldpresent (sftransferrate))
        {
            std::uint32_t urate = mtxn.getfieldu32 (sftransferrate);

            if (!urate || urate == quality_one)
            {
                m_journal.trace << "unset transfer rate";
                mtxnaccount->makefieldabsent (sftransferrate);
            }
            else if (urate > quality_one)
            {
                m_journal.trace << "set transfer rate";
                mtxnaccount->setfieldu32 (sftransferrate, urate);
            }
            else
            {
                m_journal.trace << "bad transfer rate";
                return tembad_transfer_rate;
            }
        }

        if (uflagsin != uflagsout)
            mtxnaccount->setfieldu32 (sfflags, uflagsout);

        return tessuccess;
    }
};

ter
transact_setaccount (
    sttx const& txn,
    transactionengineparams params,
    transactionengine* engine)
{
    return setaccount(txn, params, engine).apply ();
}

}
