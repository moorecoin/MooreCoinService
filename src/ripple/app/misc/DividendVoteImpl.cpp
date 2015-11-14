#include <ripple/app/ledger/ledgermaster.h>
#include <ripple/app/main/application.h>
#include <ripple/app/misc/dividendvote.h>
#include <ripple/app/misc/dividendmaster.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/app/misc/validations.h>
#include <ripple/basics/log.h>
#include <ripple/protocol/systemparameters.h>

namespace ripple {

class dividendvoteimpl : public dividendvote
{
public:
    dividendvoteimpl(beast::journal journal)
        : m_journal(journal)
    {
    }
    
    bool isstartledger(ledger::ref ledger) override
    {
        return ((ledger->getledgerseq() > 2) &&
                (ledger->gettotalcoins() < vrp_increase_max || ledger->gettotalcoinsvbc() < vbc_increase_max) &&
                (ledger->getclosetime().time_of_day().hours() == 1) &&
                ((ledger->getclosetimenc() - ledger->getdividendtimenc()) > 3600)
                );
    }
    
    bool isapplyledger(ledger::ref ledger) override
    {
        return (ledger->isdividendstarted() &&
                ((ledger->getclosetimenc() - ledger->getdividendtimenc()) >= 120)
                );
    }

    void dostartvalidation(ledger::ref lastclosedledger, stobject& basevalidation) override
    {
        // lcl must be validation ledger
        assert (isstartledger(lastclosedledger));
        
        std::uint32_t dividendledger = lastclosedledger->getledgerseq();
        std::uint64_t dividendcoins = vrp_increase_rate * lastclosedledger->gettotalcoinsvbc() / vrp_increase_rate_parts;
        if (dividendcoins + lastclosedledger->gettotalcoins() > vrp_increase_max) {
            dividendcoins = 0;
        }
        std::uint64_t dividendcoinsvbc = 0;
        if (lastclosedledger->gettotalcoinsvbc() < vbc_increase_max) {
            if (lastclosedledger->getclosetimenc() < vbc_dividend_period_1) {
                dividendcoinsvbc = vbc_increase_rate_1 * lastclosedledger->gettotalcoinsvbc() / vbc_increase_rate_1_parts;
            } else if (lastclosedledger->getclosetimenc() < vbc_dividend_period_2) {
                dividendcoinsvbc = vbc_increase_rate_2 * lastclosedledger->gettotalcoinsvbc() / vbc_increase_rate_2_parts;
            } else if (lastclosedledger->getclosetimenc() < vbc_dividend_period_3) {
                dividendcoinsvbc = vbc_increase_rate_3 * lastclosedledger->gettotalcoinsvbc() / vbc_increase_rate_3_parts;
            } else {
                dividendcoinsvbc = vbc_increase_rate_4 * lastclosedledger->gettotalcoinsvbc() / vbc_increase_rate_4_parts;
            }
            if (dividendcoinsvbc + lastclosedledger->gettotalcoinsvbc() > vbc_increase_max)
                dividendcoinsvbc = 0;
        }
        
        if (dividendcoins==0 && dividendcoinsvbc==0) {
            if (m_journal.warning)
                m_journal.warning << "not voting for a dividend start because both vrp and vbc will exceed max.";
            return;
        }
        
        basevalidation.setfieldu32 (sfdividendledger,   dividendledger);
        basevalidation.setfieldu64 (sfdividendcoins,    dividendcoins);
        basevalidation.setfieldu64 (sfdividendcoinsvbc, dividendcoinsvbc);
        
        if (m_journal.info)
            m_journal.info << "voting for a dividend start based " << dividendledger << " with vrp "<< dividendcoins << " vbc " << dividendcoinsvbc << " in " << lastclosedledger->gethash();
    }

    void dostartvoting(ledger::ref lastclosedledger, shamap::ref initialposition) override
    {
        // lcl must be validation ledger
        assert (isstartledger(lastclosedledger));
        
        std::uint32_t dividendledger = lastclosedledger->getledgerseq();
        std::map<std::pair<uint64_t, uint64_t>, int> votemap;
        // get validations for validation ledger
        validationset const set = getapp().getvalidations ().getvalidations (lastclosedledger->gethash ());
        for (auto const& e : set)
        {
            stvalidation const& val = *e.second;
            
            if (val.istrusted ())
            {
                if (val.isfieldpresent (sfdividendledger)
                    && val.isfieldpresent (sfdividendcoins)
                    && val.isfieldpresent (sfdividendcoinsvbc))
                {
                    uint32_t ledgerseq = val.getfieldu32 (sfdividendledger);
                    if (ledgerseq != dividendledger) {
                        m_journal.warning << "mismatch ledger seq " << ledgerseq << " from validator " << val.getnodeid() << " ours: " << dividendledger << " in " << lastclosedledger->gethash();
                        continue;
                    }
                    ++votemap[std::make_pair(val.getfieldu64 (sfdividendcoins), val.getfieldu64 (sfdividendcoinsvbc))];
                }
            }
        }
        
        std::pair<uint64_t, uint64_t> ourvote = std::make_pair(0, 0);
        int weight = 0;
        
        for (auto const& e : votemap)
        {
            // take most voted value
            if (e.second > weight)
            {
                ourvote = e.first;
                weight = e.second;
            }
        }
        if (weight < getapp().getledgermaster().getminvalidations()) {
            m_journal.warning << weight << " votes are not enough to start dividend for " << dividendledger;
            return;
        }
        
        if (ourvote.first==0 && ourvote.second==0) {
            if (m_journal.warning)
                m_journal.warning << "not voting for a dividend start because both vrp and vbc voted are 0";
            return;
        }
        
        if (ourvote.first + lastclosedledger->gettotalcoins() > vrp_increase_max &&
            ourvote.second + lastclosedledger->gettotalcoinsvbc() > vbc_increase_max) {
            if (m_journal.error)
                m_journal.error << "not voting for a dividend start because both vrp and vbc will exceed max.";
            return;
        }
        
        if (m_journal.warning)
            m_journal.warning << "we are voting for a dividend start based " << dividendledger << " with vrp " << ourvote.first << " vbc " << ourvote.second << " with " << weight << " same votes in " << lastclosedledger->gethash();

        sttx trans(ttdividend);
        trans.setfieldu8(sfdividendtype, dividendmaster::divtype_start);
        trans.setfieldaccount(sfaccount, account());
        trans.setfieldu32(sfdividendledger, dividendledger);
        trans.setfieldu64(sfdividendcoins, ourvote.first);
		trans.setfieldu64(sfdividendcoinsvbc, ourvote.second);

        uint256 txid = trans.gettransactionid();

        if (m_journal.warning)
            m_journal.warning << "vote: " << txid;

        serializer s;
        trans.add (s, true);

        shamapitem::pointer titem = std::make_shared<shamapitem> (txid, s.peekdata ());

        if (!initialposition->addgiveitem (titem, true, false))
        {
            if (m_journal.warning)
                m_journal.warning << "ledger already had dividend start";
        }
    }
    
    void doapplyvalidation(ledger::ref lastclosedledger, stobject& basevalidation) override
    {
        dividendmaster::pointer dividendmaster = getapp().getops().getdividendmaster();
        if (dividendmaster->trylock())
        {
            if (dividendmaster->isready())
            {
                uint32_t dividendledger = lastclosedledger->getdividendbaseledger();
                if (dividendledger == dividendmaster->getledgerseq())
                {
                    basevalidation.setfieldu32 (sfdividendledger, dividendledger);
                    basevalidation.setfieldh256 (sfdividendresulthash, dividendmaster->getresulthash());
                    
                    if (m_journal.info)
                        m_journal.info << "voting for a dividend apply based " << dividendledger << " with hash "<< dividendmaster->getresulthash() << " in " << lastclosedledger->gethash();
                }
                else
                {
                    if (m_journal.warning)
                        m_journal.warning << "wrong base ledger " << dividendmaster->getledgerseq() << " want "<< dividendledger;
                }
            }
            dividendmaster->unlock();
        }
    }
    
    bool doapplyvoting(ledger::ref lastclosedledger, shamap::ref initialposition) override
    {
        uint32_t dividendledger = lastclosedledger->getdividendbaseledger();
        
        int weight = 0;
        std::map<uint256, int> votes;
        
        // get validations for validation ledger
        validationset const set = getapp().getvalidations ().getvalidations (lastclosedledger->gethash ());
        for (auto const& e : set)
        {
            stvalidation const& val = *e.second;
            
            if (val.istrusted ())
            {
                if (val.isfieldpresent (sfdividendledger) &&
                    val.isfieldpresent (sfdividendresulthash))
                {
                    uint32_t ledgerseq = val.getfieldu32 (sfdividendledger);
                    if (ledgerseq != dividendledger)
                        continue;
                    const uint256 & dividendhash = val.getfieldh256 (sfdividendresulthash);
                    ++votes[dividendhash];
//                    if (ledgerseq != dividendledger || dividendhash != dividendresulthash) {
                    if (m_journal.debug)
                        m_journal.debug << "recv dividend apply vote based " << ledgerseq << " hash " << dividendhash << " from validator " << val.getnodeid() << " in " << lastclosedledger->gethash();
//                        continue;
//                    }
//                    ++weight;
                }
            }
        }
        
        uint256 dividendresulthash;
        for (auto const& v : votes)
        {
            if (v.second > weight)
            {
                dividendresulthash = v.first;
                weight = v.second;
            }
        }
        
        dividendmaster::pointer dividendmaster = getapp().getops().getdividendmaster();
        if (!dividendmaster->trylock())
        {
            if (weight >=getapp().getledgermaster().getminvalidations())
                return false;
            else
                return true;
        }
        
        if (!dividendmaster->isready() ||
            dividendledger != dividendmaster->getledgerseq() ||
            dividendresulthash != dividendmaster->getresulthash())
        {
            if (dividendmaster->isready())
                m_journal.warning << "we got mismatch dividend apply based " << dividendledger << " hash " << dividendresulthash << " ours " << dividendmaster->getresulthash() << " based " << dividendmaster->getledgerseq() << " in " << lastclosedledger->gethash();
            dividendmaster->unlock();
            if (weight >=getapp().getledgermaster().getminvalidations())
                return false;
            else
                return true;
        }
        
        if (weight >= getapp().getledgermaster().getminvalidations())
        {
            m_journal.warning << "we are voting for a dividend apply based " << dividendledger << " hash " << dividendresulthash << " with " << weight << " same votes in " << lastclosedledger->gethash();
            dividendmaster->filldivresult(initialposition);
            dividendmaster->filldivready(initialposition);
            dividendmaster->setready(false);
        }
        else
        {
            m_journal.warning << "we are cancelling a dividend apply with only " << weight << " same votes in " << lastclosedledger->gethash();
            dividendmaster->settotaldividend(0);
            dividendmaster->settotaldividendvbc(0);
            dividendmaster->setsumvrank(0);
            dividendmaster->setsumvspd(0);
            dividendmaster->setresulthash(uint256());
            dividendmaster->filldivready(initialposition);
            dividendmaster->setready(false);
        }
        
        dividendmaster->unlock();
        
        return true;
    }

private:
    beast::journal m_journal;
};
    

std::unique_ptr<dividendvote>
make_dividendvote(beast::journal journal)
{
    return std::make_unique<dividendvoteimpl>(journal);
}

}
