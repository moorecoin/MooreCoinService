#include <beastconfig.h>
#include <ripple/app/transactors/transactor.h>
#include <ripple/basics/log.h>
#include <ripple/protocol/indexes.h>
#include <ripple/protocol/txflags.h>

namespace ripple {
class activeaccount
    : public transactor
{
    /* the largest number of paths we allow */
    static std::size_t const maxpathsize = 6;

    /* the longest path we allow */
    static std::size_t const maxpathlength = 8;

public:
    activeaccount(
        sttx const& txn,
        transactionengineparams params,
        transactionengine* engine)
        : transactor (
            txn,
            params,
            engine,
            deprecatedlogs().journal("activeaccount"))
    {
    
    }
    
    void calculatefee () override
    {
        mfeedue = stamount (mengine->getledger ()->scalefeeload (
                        calculatebasefee (), mparams & tapadmin));

        config d;
        std::uint64_t feebytrans = 0;
        
        account const udstaccountid (mtxn.getfieldaccount160 (sfreference));
        auto const index = getaccountrootindex (udstaccountid);
        //dst account not exist yet, charge a fix amount of fee(0.01) for creating
        if (!mengine->entrycache (ltaccount_root, index))
        {
            feebytrans = d.fee_default_create;
        }

        //if currency is native(vrp/vbc), charge 1/1000 of transfer amount(1000 at least),
        //otherwise charge a fix amount of fee(0.001)
        stamount const amount (mtxn.getfieldamount (sfamount));
        feebytrans += amount.isnative() ? std::max(int(amount.getnvalue() * d.fee_default_rate_native), int(d.fee_default_min_native)) : d.fee_default_none_native;

        mfeedue = std::max(mfeedue, stamount(feebytrans, false));
    }
    
    ter doapply() override
    {
        // referee
        account const srcaccountid (mtxn.getfieldaccount160 (sfreferee));
        // reference
        account const dstaccountid (mtxn.getfieldaccount160(sfreference));
        account const midaccountid (mtxn.getfieldaccount160(sfaccount));
        
        stamount const sadstamount (mtxn.getfieldamount(sfamount));
        stamount maxsourceamount;

        if (sadstamount.isnative())
            maxsourceamount = sadstamount;
        else
            maxsourceamount = stamount(
                {sadstamount.getcurrency(), mtxnaccountid},
                sadstamount.mantissa(), sadstamount.exponent(),
                sadstamount < zero);

        auto const& usrccurrency = maxsourceamount.getcurrency ();
        auto const& udstcurrency = sadstamount.getcurrency ();
            
        m_journal.trace <<
            "maxsourceamount=" << maxsourceamount.getfulltext () <<
            " sadstamount=" << sadstamount.getfulltext ();
            
        if (!islegalnet(sadstamount) || !islegalnet (maxsourceamount))
            return tembad_amount;
        
        if (maxsourceamount < zero)
        {
            m_journal.trace <<
                "malformed transaction: bad max amount: " << maxsourceamount.
                    getfulltext();
            return tembad_amount;
        }
        else if (sadstamount < zero)
        {
            m_journal.trace <<
                "malformed transaction: bad dst amount: " << sadstamount.getfulltext ();

            return tembad_amount;
        }
        else if (badcurrency() == usrccurrency || badcurrency() == udstcurrency)
        {
            m_journal.trace <<
                "malformed transaction: bad currency.";

            return tembad_currency;
        }
        else if (mtxnaccountid == dstaccountid && usrccurrency == udstcurrency)
        {
            // you're signing yourself a payment.
            // if bpaths is true, you might be trying some arbitrage.
            m_journal.trace <<
                "malformed transaction: redundant transaction:" <<
                " src=" << to_string (mtxnaccountid) <<
                " dst=" << to_string (dstaccountid) <<
                " src_cur=" << to_string (usrccurrency) <<
                " dst_cur=" << to_string (udstcurrency);

            return temredundant;
        }
        
        //open a ledger for editing.
        auto const index = getaccountrootindex(dstaccountid);
        sle::pointer sledst (mengine->entrycache(ltaccount_root, index));
        
        if (!sledst)
        {
            // destination account dose not exist.
            if (!sadstamount.isnative())
            {
                m_journal.trace <<
                    "delay transaction: destination account does not exist.";

                // another transaction could create the account and then this
                // transaction would succeed.
                return tecno_dst;
            }
            else if (sadstamount.getnvalue() < mengine->getledger()->getreserve(0))
            {
                // getreserve() is the minimum amount that an account can have.
                // reserve is not scaled by load.
                m_journal.trace <<
                "delay transaction: destination account does not exist. " <<
                "insufficent payment to create account.";
                
                // todo: dedupe
                // another transaction could create the account and then this
                // transaction would succeed.
                return tecno_dst_insuf_xrp;
            }
            auto const newindex = getaccountrootindex (dstaccountid);
            sledst = mengine->entrycreate (ltaccount_root, newindex);
            sledst->setfieldaccount (sfaccount, dstaccountid);
            sledst->setfieldu32 (sfsequence, 1);
        }
        else {
            m_journal.trace << "account already created";
            return tefcreated;
        }

        ter terresult;
        
        // direct xrp payment.

        // uownercount is the number of entries in this legder for this account
        // that require a reserve.

        std::uint32_t const uownercount (mtxnaccount->getfieldu32 (sfownercount));

        // this is the total reserve in drops.
        // todo(tom): there should be a class for this.
        std::uint64_t const ureserve (mengine->getledger ()->getreserve (uownercount));

        // mpriorbalance is the balance on the sending account before the fees were charged.
        //
        // make sure have enough reserve to send. allow final spend to use
        // reserve for fee.
        auto const mmm = std::max(ureserve, mtxn.gettransactionfee ().getnvalue ());
        bool isvbctransaction = isvbc(sadstamount);
        if (mpriorbalance < (isvbctransaction?0:sadstamount) + mmm
            || (isvbctransaction && mtxnaccount->getfieldamount (sfbalancevbc) < sadstamount) )
        {
            // vote no.
            // however, transaction might succeed, if applied in a different order.
            m_journal.trace << "delay transaction: insufficient funds: " <<
                " " << mpriorbalance.gettext () <<
                " / " << (sadstamount + ureserve).gettext () <<
                " (" << ureserve << ")";

            terresult = tecunfunded_payment;
        }
        else
        {
            // the source account does have enough money, so do the arithmetic
            // for the transfer and make the ledger change.
            m_journal.info << "moorecoin: deduct coin "
                << isvbctransaction << msourcebalance << sadstamount;

            if (isvbctransaction)
            {
                mtxnaccount->setfieldamount(sfbalancevbc, mtxnaccount->getfieldamount (sfbalancevbc) - sadstamount);
                sledst->setfieldamount(sfbalancevbc, sledst->getfieldamount(sfbalancevbc) + sadstamount);
            }
            else
            {
                mtxnaccount->setfieldamount(sfbalance, msourcebalance - sadstamount);
                sledst->setfieldamount(sfbalance, sledst->getfieldamount(sfbalance) + sadstamount);
            }



            // re-arm the password change fee if we can and need to.
            if ((sledst->getflags () & lsfpasswordspent))
                sledst->clearflag (lsfpasswordspent);

            terresult = tessuccess;
        }

        std::string strtoken;
        std::string strhuman;

        if (transresultinfo (terresult, strtoken, strhuman))
        {
            m_journal.trace <<
                strtoken << ": " << strhuman;
        }
        else
        {
            assert (false);
        }

        return mengine->view ().addrefer(srcaccountid, dstaccountid);
    }
};

ter
transact_activeaccount (
    sttx const& txn,
    transactionengineparams params,
    transactionengine* engine)
{
    return activeaccount(txn, params, engine).apply();
};
}//ripple