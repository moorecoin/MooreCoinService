#if (defined (_win32) || defined (_win64))
#include <psapi.h>
#else
#include <sys/time.h>
#include <sys/resource.h>
#endif
#include <boost/multiprecision/cpp_int.hpp>

#include <beast/threads/recursivemutex.h>

#include <ripple/app/ledger/ledgermaster.h>
#include <ripple/app/main/application.h>
#include <ripple/app/misc/defaultmissingnodehandler.h>
#include <ripple/app/misc/dividendmaster.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/basics/log.h>
#include <ripple/protocol/systemparameters.h>

namespace ripple {
        
static inline uint64_t memused(void) {
#if (defined (_win32) || defined (_win64))
    //carl windows implementation
    handle hprocess = getcurrentprocess();
    process_memory_counters pmc;
    bool bok = getprocessmemoryinfo(hprocess, &pmc, sizeof(pmc));
    return pmc.workingsetsize / (1024 * 1024);
#else
    struct rusage ru;
    getrusage(rusage_self, &ru);
    return ru.ru_maxrss/1024;
#endif
}
    
class dividendmasterimpl : public dividendmaster
{
public:
    dividendmasterimpl(beast::journal journal)
        : m_journal(journal),
        m_ready(false),
        m_dividendledgerseq(0),
        m_running(false)
    {
    }

    void lock() override
    {
        m_lock.lock();
    }
    void unlock() override
    {
        m_lock.unlock();
    }

    bool trylock() override
    {
        return m_lock.try_lock();
    }

    void setready(bool ready) override
    {
        m_ready = ready;
    }

    bool isready() override
    {
        if (m_ready)
        {
            ledger::pointer lastclosedledger = getapp().getops().getledgerbyhash(getapp().getops().getconsensuslcl());
            if (lastclosedledger)
            {
                std::uint32_t basedivledgerseq = lastclosedledger->getdividendbaseledger();
                if (basedivledgerseq > 0 && basedivledgerseq == m_dividendledgerseq)
                {
                    return true;
                }
            }
            m_ready = false;
        }
        return false;
    }
    
    void setrunning(bool running)
    {
        m_running = running;
    }
    
    bool isrunning()
    {
        return m_running;
    }

    accountsdividend& getdivresult() override
    {
        return m_divresult;
    }

    void settotaldividendvbc(uint64_t num) override
    {
        m_totaldividendvbc = num;
    }

    uint64_t gettotaldividendvbc() override
    {
        return m_totaldividendvbc;
    }

    void settotaldividend(uint64_t num) override
    {
        m_totaldividend = num;
    }

    uint64_t gettotaldividend() override
    {
        return m_totaldividend;
    }
    
    void setsumvrank(uint64_t num) override
    {
        m_sumvrank = num;
    }
    
    void setsumvspd(uint64_t num) override
    {
        m_sumvspd = num;
    }

    bool calcresulthash() override
    {
#ifdef moorecoin_async_dividend
        application& app = getapp();
        shamap::pointer txmap = std::make_shared<shamap>(
                                        smttransaction,
                                        app.getfullbelowcache(),
                                        app.gettreenodecache(),
                                        app.getnodestore(),
                                        defaultmissingnodehandler(),
                                        deprecatedlogs().journal("shamap"));
        
        for (const auto& it : m_divresult)
        {
            sttx trans(ttdividend);
            trans.setfieldu8(sfdividendtype, dividendmaster::divtype_apply);
            trans.setfieldaccount(sfaccount, account());
            trans.setfieldaccount(sfdestination, std::get<0>(it));
            trans.setfieldu32(sfdividendledger, m_dividendledgerseq);
            trans.setfieldu64(sfdividendcoins, std::get<1>(it));
            trans.setfieldu64(sfdividendcoinsvbc, std::get<2>(it));
            trans.setfieldu64(sfdividendcoinsvbcrank, std::get<3>(it));
            trans.setfieldu64(sfdividendcoinsvbcsprd, std::get<4>(it));
            trans.setfieldu64(sfdividendvrank, std::get<5>(it));
            trans.setfieldu64(sfdividendvsprd, std::get<6>(it));
            trans.setfieldu64(sfdividendtsprd, std::get<7>(it));

            uint256 txid = trans.gettransactionid();
            serializer s;
            trans.add(s, true);
            
            shamapitem::pointer titem = std::make_shared<shamapitem>(txid, s.peekdata());
            
            if (!txmap->addgiveitem(titem, true, false))
            {
                return false;
            }
        }
        m_resulthash = txmap->gethash();
#endif // moorecoin_async_dividend
        
        return true;
    }
    uint256 getresulthash() override
    {
        return m_resulthash;
    }
    
    void setresulthash(uint256 hash) override
    {
        m_resulthash = hash;
    }

    void filldivready(shamap::pointer initialposition) override
    {
        sttx trans(ttdividend);
        trans.setfieldu8(sfdividendtype, dividendmaster::divtype_done);
        trans.setfieldaccount(sfaccount, account());
        trans.setfieldu32(sfdividendledger, m_dividendledgerseq);
        trans.setfieldu64(sfdividendcoins, m_totaldividend);
        trans.setfieldu64(sfdividendcoinsvbc, m_totaldividendvbc);
        trans.setfieldu64(sfdividendvrank, m_sumvrank);
        trans.setfieldu64(sfdividendvsprd, m_sumvspd);
        trans.setfieldh256(sfdividendresulthash, m_resulthash);

        uint256 txid = trans.gettransactionid();
        serializer s;
        trans.add(s, true);

        shamapitem::pointer titem = std::make_shared<shamapitem>(txid, s.peekdata());

        if (!initialposition->addgiveitem(titem, true, false)) {
            if (m_journal.warning)
            {
                m_journal.warning << "ledger already had dividend ready";
            }
        }
        else
        {
            if (m_journal.debug)
            {
                m_journal.debug << "dividend ready add tx " << txid;
            }
        }
    }

    void filldivresult(shamap::pointer initialposition) override
    {
        for (const auto& it : m_divresult) {
            sttx trans(ttdividend);
            trans.setfieldu8(sfdividendtype, dividendmaster::divtype_apply);
            trans.setfieldaccount(sfaccount, account());
            trans.setfieldaccount(sfdestination, std::get<0>(it));
            trans.setfieldu32(sfdividendledger, m_dividendledgerseq);
            trans.setfieldu64(sfdividendcoins, std::get<1>(it));
            trans.setfieldu64(sfdividendcoinsvbc, std::get<2>(it));
            trans.setfieldu64(sfdividendcoinsvbcrank, std::get<3>(it));
            trans.setfieldu64(sfdividendcoinsvbcsprd, std::get<4>(it));
            trans.setfieldu64(sfdividendvrank, std::get<5>(it));
            trans.setfieldu64(sfdividendvsprd, std::get<6>(it));
            trans.setfieldu64(sfdividendtsprd, std::get<7>(it));

            uint256 txid = trans.gettransactionid();
            serializer s;
            trans.add(s, true);

            shamapitem::pointer titem = std::make_shared<shamapitem>(txid, s.peekdata());

            if (!initialposition->addgiveitem(titem, true, false)) {
                if (m_journal.warning.active())
                    m_journal.warning << "ledger already had dividend for " << std::get<0>(it);
            }
            else {
                if (m_journal.trace.active())
                    m_journal.trace << "dividend add tx " << txid << " for " << std::get<0>(it);
            }
        }
        if (m_journal.info)
            m_journal.info << "dividend add " << m_divresult.size() << " txs done. mem" << memused();
    }

    void setledgerseq(uint32_t seq) override
    {
        m_dividendledgerseq = seq;
    }

    uint32_t getledgerseq() override
    {
        return m_dividendledgerseq;
    }


private:
    beast::journal m_journal;
    beast::recursivemutex m_lock;
    bool m_ready;
    uint32_t m_dividendledgerseq;
    accountsdividend m_divresult;
    uint64_t m_totaldividend;
    uint64_t m_totaldividendvbc;
    uint64_t m_sumvrank=0;
    uint64_t m_sumvspd=0;
    uint256 m_resulthash;
    bool m_running;
};

void dividendmaster::calcdividend(ledger::ref lastclosedledger)
{
    auto sle = lastclosedledger->getdividendobject();
    
    if (!sle
        || sle->getfieldindex(sfdividendledger) == -1
        || sle->getfieldindex(sfdividendcoins) == -1
        || sle->getfieldindex(sfdividendcoinsvbc) == -1 ){
        writelog(lserror, dividendmaster) << "calcdividend called but info in dividend object missing";
        return;
    }
    
    uint32_t baseledgerseq = sle->getfieldu32(sfdividendledger);
    uint64_t dividendcoins = sle->getfieldu64(sfdividendcoins);
    uint64_t dividendcoinsvbc = sle->getfieldu64(sfdividendcoinsvbc);
    
    ledger::ref baseledger = getapp().getops().getledgerbyseq(baseledgerseq);
    
    if (!baseledger) {
        writelog(lswarning, dividendmaster) << "base ledger not found";
        return;
    }
    
    dividendmaster::pointer dividendmaster = getapp().getops().getdividendmaster();
    
    dividendmaster->lock();
    
    dividendmaster->setrunning(true);
    dividendmaster->setready(false);
    
    dividendmaster->setledgerseq(baseledgerseq);
    dividendmaster->getdivresult().clear();
    uint64_t actualtotaldividend = 0;
    uint64_t actualtotaldividendvbc = 0, sumvrank=0, sumvspd=0;
    
    if (!dividendmaster::calcdividendfunc(baseledger,
                                      dividendcoins,
                                      dividendcoinsvbc,
                                      dividendmaster->getdivresult(),
                                      actualtotaldividend,
                                      actualtotaldividendvbc, sumvrank, sumvspd))
    {
        writelog(lswarning, dividendmaster) << "calcdividend does not find any account";
        dividendmaster->setrunning(false);
        dividendmaster->unlock();
        return;
    }
    
    dividendmaster->settotaldividend(actualtotaldividend);
    dividendmaster->settotaldividendvbc(actualtotaldividendvbc);
    dividendmaster->setsumvrank(sumvrank);
    dividendmaster->setsumvspd(sumvspd);
    
    if (!dividendmaster->calcresulthash())
    {
        writelog(lswarning, dividendmaster) << "calcdividend fail to get result hash";
        dividendmaster->setrunning(false);
        dividendmaster->unlock();
        return;
    }
    
    dividendmaster->setready(true);
    dividendmaster->setrunning(false);
    
    dividendmaster->unlock();
}

static inline uint64_t adjust(uint64_t coin)
{
    return coin>=10000000000 ? coin+90000000000 : coin*10;
}

class accountsbyreference_less {
public:
    template <class t>
    bool operator()(const t &x, const t &y) const
    {
        // accounts by reference height needs to be descending order
        if (std::get<2>(x) > std::get<2>(y))
            return true;
        else if (std::get<2>(x) == std::get<2>(y))
            return std::get<1>(x) > std::get<1>(y);
        return false;
    }
};

bool dividendmaster::calcdividendfunc(ledger::ref baseledger, uint64_t dividendcoins, uint64_t dividendcoinsvbc, accountsdividend& accountsout, uint64_t& actualtotaldividend, uint64_t& actualtotaldividendvbc, uint64_t& sumvrank, uint64_t& sumvspd)
{
    writelog(lsinfo, dividendmaster) << "expected dividend: " << dividendcoins << " " << dividendcoinsvbc << " for ledger " << baseledger->getledgerseq() << " mem " << memused();
    
    // <balancevbc, <accountid, parentaccoutid, height>>, accounts sorted by balance
    std::multimap<uint64_t, std::tuple<account, account, uint32_t>> accountsbybalance;
    
    // <<accountid, parentid, height>, <balancevbc, vrank, vspd, tspd>>, accounts sorted by reference height and parent desc
    std::multimap<std::tuple<account, account, uint32_t>, std::tuple<uint64_t, uint32_t, uint64_t, uint64_t>, accountsbyreference_less> accountsbyreference;
    
    // visit account stats to fill accountsbybalance
    baseledger->visitstateitems([&accountsbybalance, &accountsbyreference, baseledger](sle::ref sle) {
        if (sle->gettype() == ltaccount_root) {
            uint64_t bal = sle->getfieldamount(sfbalancevbc).getnvalue();
            if (bal < system_currency_parts_vbc
                && !baseledger->hasrefer(sle->getfieldaccount(sfaccount).getaccountid())) {
                return;
            }
            uint32_t height = 0;
            account addrparent;
            if (sle->isfieldpresent(sfreferee) && sle->isfieldpresent(sfreferenceheight)) {
                height = sle->getfieldu32(sfreferenceheight);
                addrparent = sle->getfieldaccount(sfreferee).getaccountid();
            }
            if (bal < system_currency_parts_vbc)
                accountsbyreference.emplace(std::piecewise_construct,
                                            std::forward_as_tuple(sle->getfieldaccount(sfaccount).getaccountid(), addrparent, height),
                                            std::forward_as_tuple(bal, 0, 0, 0));
            else
                accountsbybalance.emplace(std::piecewise_construct,
                                          std::forward_as_tuple(bal),
                                          std::forward_as_tuple(sle->getfieldaccount(sfaccount).getaccountid(), addrparent, height));
        }
    });
    writelog(lsinfo, dividendmaster) << "calcdividend got " << accountsbybalance.size() << " accounts for ranking " << accountsbyreference.size() << " accounts for sprd mem " << memused();
    
    if (accountsbybalance.empty() && accountsbyreference.empty())
    {
        accountsout.clear();
        actualtotaldividend = 0;
        actualtotaldividendvbc = 0;
        sumvrank = 0;
        sumvspd = 0;
        return true;
    }
    
    // traverse accountsbybalance to caculate v ranking into vrank in accountsbyreference
    sumvrank = 0;
    {
        uint64_t lastbalance = 0;
        uint32_t pos = 1, rank = 1;
        for (auto it = accountsbybalance.begin(); it != accountsbybalance.end(); ++pos) {
            if (lastbalance < it->first) {
                rank = pos;
                lastbalance = it->first;
            }
            accountsbyreference.emplace(std::piecewise_construct, it->second, std::forward_as_tuple(it->first, rank, 0, 0));
            sumvrank += rank;
            it = accountsbybalance.erase(it);
        }
    }
    writelog(lsinfo, dividendmaster) << "calcdividend got v rank total: " << sumvrank << " mem " << memused();
    
    // traverse accountsbyreference to caculate v spreading into vspd
    sumvspd = 0;
    {
        // <accountid, <totalchildrenholding, totalchildrenvspd>>, children holding cache
        hash_map<account, std::pair<uint64_t, uint64_t>> childrenholdings;
        account lastparent;
        uint64_t totalchildrenvspd = 0, totalchildrenholding = 0, maxholding = 0;
        for (auto& it : accountsbyreference) {
            const account& accountparent = std::get<1>(it.first);
            if (lastparent != accountparent) {
                // no more for lastparent, store it
                if (totalchildrenvspd != 0) {
                    childrenholdings.emplace(lastparent, std::make_pair(totalchildrenholding, totalchildrenvspd - adjust(maxholding) + (static_cast<uint64_t>(pow(maxholding/system_currency_parts_vbc, 1.0 / 3))*system_currency_parts_vbc)));
                }
                totalchildrenvspd = totalchildrenholding = maxholding = 0;
                lastparent = accountparent;
            }
            
            const account& account = std::get<0>(it.first);
            
            uint64_t t = 0, v = 0;
            
            // pickup children holding.
            auto itholding = childrenholdings.find(account);
            if (itholding != childrenholdings.end()) {
                t = itholding->second.first;
                v = itholding->second.second;
                childrenholdings.erase(itholding);
            }
            
            uint64_t balance = std::get<0>(it.second);
            
            // store v spreading
            if (balance >= system_currency_parts_vbc) {
                std::get<2>(it.second) = v;
                sumvspd += v;
            }
            
            t += balance;
            std::get<3>(it.second) = t;
            
            if (accountparent.iszero())
                continue;
            
            totalchildrenholding += t;
            totalchildrenvspd += adjust(t);
            
            if (maxholding < t)
                maxholding = t;
        }
    }
    writelog(lsinfo, dividendmaster) << "calcdividend got v spread total: " << sumvspd << " mem " << memused();
    
    // traverse accountsbyreference to calc dividend
    accountsout.reserve(accountsbyreference.size()+1);
    actualtotaldividend = 0; actualtotaldividendvbc = 0;
    uint64_t totaldivvbcbyrank = dividendcoinsvbc / 2;
    uint64_t totaldivvbcbypower = dividendcoinsvbc - totaldivvbcbyrank;
    for (const auto& it : accountsbyreference) {
        uint64_t divvbc = 0;
        boost::multiprecision::uint128_t divvbcbyrank(0), divvbcbypower(0);
        if (dividendcoinsvbc > 0 && sumvspd > 0 && sumvrank > 0) {
            divvbcbyrank = totaldivvbcbyrank;
            divvbcbyrank *= std::get<1>(it.second);
            divvbcbyrank /= sumvrank;
            divvbcbypower = totaldivvbcbypower;
            divvbcbypower *= std::get<2>(it.second);
            divvbcbypower /= sumvspd;
            divvbc = static_cast<uint64_t>(divvbcbyrank + divvbcbypower);
            if (divvbc < vbc_dividend_min) {
                divvbc = 0;
                divvbcbyrank = 0;
                divvbcbypower = 0;
            }
            actualtotaldividendvbc += divvbc;
        }
        uint64_t div = 0;
        if (dividendcoins > 0 && (dividendcoinsvbc == 0 || divvbc >= vbc_dividend_min)) {
            div = std::get<0>(it.second) * vrp_increase_rate / vrp_increase_rate_parts;
            actualtotaldividend += div;
        }
        
        if (shouldlog(lsinfo, dividendmaster)) {
            writelog(lsinfo, dividendmaster) << "{\"account\":\"" << rippleaddress::createaccountid(std::get<0>(it.first)).humanaccountid() << "\",\"data\":{\"divvbcbyrank\":\"" << divvbcbyrank << "\",\"divvbcbypower\":\"" << divvbcbypower << "\",\"divvbc\":\"" << divvbc << "\",\"balance\":\"" << std::get<0>(it.second) << "\",\"vrank\":\"" << std::get<1>(it.second) << "\",\"vsprd\":\"" << std::get<2>(it.second) << "\",\"tsprd\":\"" << std::get<3>(it.second) << "\"}}";
        }
        
        if (div !=0 || divvbc !=0 || std::get<2>(it.second) > min_vspd_to_get_fee_share)
        {
            accountsout.push_back(std::make_tuple(std::get<0>(it.first), div, divvbc, static_cast<uint64_t>(divvbcbyrank), static_cast<uint64_t>(divvbcbypower), std::get<1>(it.second), std::get<2>(it.second), std::get<3>(it.second)));
        }
    }
    
    writelog(lsinfo, dividendmaster) << "calcdividend got actualtotaldividend " << actualtotaldividend << " actualtotaldividendvbc " << actualtotaldividendvbc << " mem " << memused();
    
    // collect remainning
    uint64_t remaincoins = 0, remaincoinsvbc = 0;
    if (dividendcoins > actualtotaldividend) {
        remaincoins = dividendcoins - actualtotaldividend;
        actualtotaldividend = dividendcoins;
    }
    if (dividendcoinsvbc > actualtotaldividendvbc) {
        remaincoinsvbc = dividendcoinsvbc - actualtotaldividendvbc;
        actualtotaldividendvbc = dividendcoinsvbc;
    }
    if (remaincoins > 0 || remaincoinsvbc > 0)
        accountsout.push_back(std::make_tuple(account("0x56ce5173b6a2cbedf203bd69159212094c651041"), remaincoins, remaincoinsvbc, 0, 0, 0, 0, 0));
    
    accountsbyreference.clear();
    writelog(lsinfo, dividendmaster) << "calcdividend done with " << accountsout.size() << " accounts mem " << memused();
    
    return true;
}

std::unique_ptr<dividendmaster>
make_dividendmaster(beast::journal journal)
{
    return std::make_unique<dividendmasterimpl>(journal);
}

}
