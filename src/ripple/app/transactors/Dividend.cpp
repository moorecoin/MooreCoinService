#include <beastconfig.h>
#include <ripple/app/transactors/transactor.h>
#include <ripple/app/misc/dividendmaster.h>
#include <ripple/basics/log.h>
#include <ripple/protocol/indexes.h>
#include <ripple/protocol/txflags.h>

namespace ripple {

    class dividend
        : public transactor
    {
    public:
        dividend(
            sttx const& txn,
            transactionengineparams params,
            transactionengine* engine)
            : transactor(
            txn,
            params,
            engine,
            deprecatedlogs().journal("dividend"))
        {
        }

        ter checksig() override
        {
            //no signature, always return true
            return tessuccess;
        }

        ter checkseq() override
        {
            if ((mtxn.getsequence() != 0) || mtxn.isfieldpresent(sfprevioustxnid))
            {
                m_journal.warning << "bad sequence";
                return tembad_sequence;
            }

            return tessuccess;
        }

        ter payfee() override
        {
            if (mtxn.gettransactionfee() != stamount())
            {
                m_journal.warning << "non-zero fee";
                return tembad_fee;
            }

            return tessuccess;
        }

        ter precheck() override
        {
            if (mparams & tapopen_ledger)
            {
                m_journal.warning << "dividend transaction against open ledger";
                return teminvalid;
            }
            if (!mtxn.isfieldpresent(sfdividendtype))
            {
                m_journal.warning << "no dividend type";
                return tembad_div_type;
            }
            if (!mtxn.isfieldpresent(sfdividendledger))
            {
                m_journal.warning << "no dividend ledger";
                return teminvalid;
            }
            if (!mtxn.isfieldpresent(sfdividendcoins))
            {
                m_journal.warning << "no dividend coins";
                return teminvalid;
            }
            if (!mtxn.isfieldpresent(sfdividendcoinsvbc))
            {
                m_journal.warning << "no dividend coins vbc";
                return teminvalid;
            }
            
            uint8_t divtype = mtxn.getfieldu8(sfdividendtype);
            
            if (divtype == dividendmaster::divtype_start) {
                sle::pointer sle=mengine->getledger()->getdividendobject();
                if (sle && sle->getfieldindex(sfdividendstate)!=-1
                    && sle->getfieldu8(sfdividendstate)!=dividendmaster::divstate_done) {
                    m_journal.warning << "dividend in progress";
                    return tefbad_ledger;
                }
            } else {
                sle::pointer sle=mengine->getledger()->getdividendobject();
                if (!sle || sle->getfieldindex(sfdividendledger)==-1) {
                    m_journal.warning << "no dividend ledger";
                    return tefbad_ledger;
                }
                if (mtxn.getfieldu32(sfdividendledger) != sle->getfieldu32(sfdividendledger)) {
                    m_journal.warning << "dividend ledger mismatch";
                    return tefbad_ledger;
                }
                if (!mtxn.isfieldpresent(sfdividendvrank))
                {
                    m_journal.warning << "no dividend v rank";
                    return teminvalid;
                }
                if (!mtxn.isfieldpresent(sfdividendvsprd))
                {
                    m_journal.warning << "no dividend v spread";
                    return teminvalid;
                }
                switch (divtype) {
                    case dividendmaster::divtype_apply:
                    {
                        if (!mtxn.isfieldpresent(sfdestination))
                        {
                            m_journal.warning << "no dividend destination";
                            return temdst_needed;
                        }
                        if (!mtxn.isfieldpresent(sfdividendcoinsvbcrank))
                        {
                            m_journal.warning << "no dividend coins by rank";
                            return teminvalid;
                        }
                        if (!mtxn.isfieldpresent(sfdividendcoinsvbcsprd))
                        {
                            m_journal.warning << "no dividend coins by spread";
                            return teminvalid;
                        }
                        if (!mtxn.isfieldpresent(sfdividendtsprd))
                        {
                            m_journal.warning << "no dividend t spread";
                            return teminvalid;
                        }
                        break;
                    }
                    case dividendmaster::divtype_done:
                    {
                        if (!mtxn.isfieldpresent(sfdividendresulthash))
                        {
                            m_journal.warning << "no dividend result hash";
                            return teminvalid;
                        }
                        break;
                    }
                    default:
                        return tembad_div_type;
                }
            }
            return tessuccess;
        }

        bool musthavevalidaccount() override
        {
            return false;
        }

        //achieve consensus on which ledger to start dividend
        ter startcalc()
        {
            sle::pointer dividendobject = mengine->entrycache(ltdividend, getledgerdividendindex());

            if (!dividendobject)
            {
                dividendobject = mengine->entrycreate(ltdividend, getledgerdividendindex());
            }

            m_journal.info << "previous dividend object: " << dividendobject->gettext();

            uint32_t dividendledger = mtxn.getfieldu32(sfdividendledger);
            uint64_t dividendcoins = mtxn.getfieldu64(sfdividendcoins);
            uint64_t dividendcoinsvbc = mtxn.getfieldu64(sfdividendcoinsvbc);

            dividendobject->setfieldu8(sfdividendstate, dividendmaster::divstate_start);
            dividendobject->setfieldu32(sfdividendledger, dividendledger);
            dividendobject->setfieldu64(sfdividendcoins, dividendcoins);
            dividendobject->setfieldu64(sfdividendcoinsvbc, dividendcoinsvbc);

            mengine->entrymodify(dividendobject);

            m_journal.info << "current dividend object: " << dividendobject->gettext();

            return tessuccess;
        }

        //apply dividend result here
        ter applytx()
        {
            if (m_journal.debug.active())
                m_journal.debug << "moorecoin: apply dividend.";

            const account& account = mtxn.getfieldaccount160(sfdestination);

            if (m_journal.trace.active()) {
                m_journal.trace << "des account " << rippleaddress::createaccountid(account).humanaccountid();
            }
            
            uint64_t divcoinsvbc = mtxn.getfieldu64(sfdividendcoinsvbc);
            uint64_t divcoins = mtxn.getfieldu64(sfdividendcoins);
            
            sle::pointer sleaccoutmodified = mengine->entrycache(
                ltaccount_root, getaccountrootindex(account));

            if (sleaccoutmodified)
            {
                mengine->entrymodify(sleaccoutmodified);
                if (divcoinsvbc > 0)
                {
                    uint64_t prevbalancevbc = sleaccoutmodified->getfieldamount(sfbalancevbc).getnvalue();
                    sleaccoutmodified->setfieldamount(sfbalancevbc, prevbalancevbc + divcoinsvbc);
                    mengine->getledger()->createcoinsvbc(divcoinsvbc);
                }
                if (divcoins > 0)
                {
                    uint64_t prevbalance = sleaccoutmodified->getfieldamount(sfbalance).getnvalue();
                    sleaccoutmodified->setfieldamount(sfbalance, prevbalance + divcoins);
                    mengine->getledger()->createcoins(divcoins);
                }
                

                
                //record vspd, tspd, dividendledgerseq
                if (mtxn.isfieldpresent(sfdividendledger))
                {
                    std::uint32_t divledgerseq = mtxn.getfieldu32(sfdividendledger);
                    sleaccoutmodified->setfieldu32(sfdividendledger, divledgerseq);
                    
                    if (mtxn.isfieldpresent(sfdividendvrank))
                    {
                        std::uint64_t divvrank = mtxn.getfieldu64(sfdividendvrank);
                        sleaccoutmodified->setfieldu64(sfdividendvrank, divvrank);
                    }
                    
                    if (mtxn.isfieldpresent(sfdividendvsprd))
                    {
                        std::uint64_t divvspd = mtxn.getfieldu64(sfdividendvsprd);
                        sleaccoutmodified->setfieldu64(sfdividendvsprd, divvspd);
                    }
                    
                    if (mtxn.isfieldpresent(sfdividendtsprd))
                    {
                        std::uint64_t divtspd = mtxn.getfieldu64(sfdividendtsprd);
                        sleaccoutmodified->setfieldu64(sfdividendtsprd, divtspd);
                    }
                }
                
                if (m_journal.trace.active()) {
                    m_journal.trace << "dividend applied:" << sleaccoutmodified->gettext();
                }
                
                // convert refereces storage mothod
                if (sleaccoutmodified->isfieldpresent(sfreferences))
                {
                    rippleaddress address = sleaccoutmodified->getfieldaccount(sfaccount);
                    const starray& references = sleaccoutmodified->getfieldarray(sfreferences);
                    auto const referobjindex = getaccountreferindex (address.getaccountid());
                    sle::pointer slereferobj(mengine->entrycache(ltrefer, referobjindex));
                    if (slereferobj)
                    {
                        m_journal.warning << "has both sfreferences and referobj at the same time, this should not happen.";
                    }
                    else
                    {
                        slereferobj = mengine->entrycreate(ltrefer, referobjindex);
                        slereferobj->setfieldarray(sfreferences, references);
                        sleaccoutmodified->delfield(sfreferences);
                        m_journal.warning << address.getaccountid() << " references storage convert done.";
                    }
                }
                
                // convert refereces storage mothod
                if (sleaccoutmodified->isfieldpresent(sfreferences))
                {
                    // refer migrate needed, @todo: simply delete this if after migration.
                    rippleaddress address = sleaccoutmodified->getfieldaccount(sfaccount);
                    const starray& references = sleaccoutmodified->getfieldarray(sfreferences);
                    auto const referobjindex = getaccountreferindex (address.getaccountid());
                    sle::pointer slereferobj(mengine->entrycache(ltrefer, referobjindex));
                    if (slereferobj)
                    {
                        m_journal.error << "has both sfreferences and referobj at the same time for " <<  rippleaddress::createaccountid(account).humanaccountid() << ", this should not happen.";
                    }
                    else
                    {
                        slereferobj = mengine->entrycreate(ltrefer, referobjindex);
                        slereferobj->setfieldarray(sfreferences, references);
                        sleaccoutmodified->delfield(sfreferences);
                        m_journal.info << address.getaccountid() << " references storage convert done.";
                    }
                }
            }
            else {
                if (m_journal.warning.active()) {
                    m_journal.warning << "dividend account not found :" << rippleaddress::createaccountid(account).humanaccountid();
                }
            }
            
            return tessuccess;
        }

        //mark as we have done dividend apply
        ter doneapply()
        {
            uint256 dividendresulthash = mtxn.getfieldh256(sfdividendresulthash);

            sle::pointer dividendobject = mengine->entrycache(ltdividend, getledgerdividendindex());

            if (!dividendobject)
            {
                dividendobject = mengine->entrycreate(ltdividend, getledgerdividendindex());
            }

            m_journal.info << "previous dividend object: " << dividendobject->gettext();

            uint32_t dividendledger = mtxn.getfieldu32(sfdividendledger);
            uint64_t dividendcoins = mtxn.getfieldu64(sfdividendcoins);
            uint64_t dividendcoinsvbc = mtxn.getfieldu64(sfdividendcoinsvbc);

            dividendobject->setfieldu8(sfdividendstate, dividendmaster::divstate_done);
            dividendobject->setfieldu32(sfdividendledger, dividendledger);
            dividendobject->setfieldu64(sfdividendcoins, dividendcoins);
            dividendobject->setfieldu64(sfdividendcoinsvbc, dividendcoinsvbc);
            dividendobject->setfieldu64(sfdividendvrank, mtxn.getfieldu64(sfdividendvrank));
            dividendobject->setfieldu64(sfdividendvsprd, mtxn.getfieldu64(sfdividendvsprd));
            dividendobject->setfieldh256(sfdividendresulthash, dividendresulthash);

            mengine->entrymodify(dividendobject);

            m_journal.info << "current dividend object: " << dividendobject->gettext();

            return tessuccess;
        }

        ter doapply () override
        {
            if (mtxn.gettxntype() == ttdividend)
            {
                uint8_t divoptype = mtxn.isfieldpresent(sfdividendtype) ? mtxn.getfieldu8(sfdividendtype) : dividendmaster::divtype_start;
                switch (divoptype)
                {
                    case dividendmaster::divtype_start:
                    {
                        return startcalc();
                    }
                    case dividendmaster::divtype_apply:
                    {
                        return applytx();
                    }
                    case dividendmaster::divtype_done:
                    {
                        return doneapply();
                    }
                }
            }
            return temunknown;
        }
    };

    ter
    transact_dividend(
        sttx const& txn,
        transactionengineparams params,
        transactionengine* engine)
    {
	    return dividend (txn, params, engine).apply();
    }
}
