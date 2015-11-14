#include <beastconfig.h>
#include <ripple/app/transactors/transactor.h>
#include <ripple/basics/log.h>
#include <ripple/protocol/indexes.h>
#include <ripple/protocol/txflags.h>

namespace ripple
{
class issueasset
    : public transactor
{
public:
    issueasset(
        sttx const& txn,
        transactionengineparams params,
        transactionengine* engine)
        : transactor(
              txn,
              params,
              engine,
              deprecatedlogs().journal("issueasset"))
    {
    }

    ter doapply() override
    {
        account const udstaccountid(mtxn.getfieldaccount160(sfdestination));
        if (!udstaccountid) {
            m_journal.trace << "malformed transaction: issue destination account not specified.";
            return temdst_needed;
        } else if (mtxnaccountid == udstaccountid) {
            m_journal.trace << "malformed transaction: can not issue asset to self.";
            return temdst_is_src;
        }

        stamount const sadstamount(mtxn.getfieldamount(sfamount));
        if (sadstamount <= zero) {
            m_journal.trace << "malformed transaction: bad amount: " << sadstamount.getfulltext();
            return tembad_amount;
        } else if (sadstamount.getissuer() != mtxnaccountid) {
            m_journal.trace << "malformed transaction: bad issuer: " << sadstamount.getfulltext();
            return tembad_issuer;
        }

        currency const currency(sadstamount.getcurrency());
        if (currency != assetcurrency()) {
            m_journal.trace << "malformed transaction: bad currency: " << sadstamount.getfulltext();
            return tembad_currency;
        }

        starray const releaseschedule(mtxn.getfieldarray(sfreleaseschedule));
        int64_t lastexpiration = -1, lastreleaserate = -1;
        for (const auto& releasepoint : releaseschedule) {
            const auto& rate = releasepoint.getfieldu32(sfreleaserate);
            if (rate <= lastreleaserate || rate > quality_one)
                return tembad_release_schedule;

            const auto& expire = releasepoint.getfieldu32(sfexpiration);

            if (rate == 0 && expire != 0)
                return tembad_release_schedule;

            if (expire <= lastexpiration || expire % getconfig ().asset_interval_min != 0)
                return tembad_release_schedule;

            lastexpiration = expire;
            lastreleaserate = rate;
        }

        sle::pointer sleasset(mengine->entrycache(ltasset, getassetindex(mtxnaccountid, currency)));
        if (sleasset) {
            m_journal.trace << "asset already issued.";
            return tefcreated;
        }

        sle::pointer sledst(mengine->entrycache(ltaccount_root, getaccountrootindex(udstaccountid)));
        if (!sledst) {
            m_journal.trace << "delay transaction: destination account does not exist.";
            return tecno_dst;
        }

        sleasset = mengine->entrycreate(ltasset, getassetindex(mtxnaccountid, currency));
        sleasset->setfieldamount(sfamount, sadstamount);
        sleasset->setfieldaccount(sfregularkey, udstaccountid);
        sleasset->setfieldarray(sfreleaseschedule, releaseschedule);
        
        return mengine->view ().ripplecredit (mtxnaccountid, udstaccountid, sadstamount, false);
    }
};

ter transact_issue(
    sttx const& txn,
    transactionengineparams params,
    transactionengine* engine)
{
    return issueasset(txn, params, engine).apply();
}

} // ripple
