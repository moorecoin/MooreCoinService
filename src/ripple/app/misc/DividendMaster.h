#ifndef ripple_app_dividend_master_h_included
#define ripple_app_dividend_master_h_included

#include <ripple/app/ledger/ledger.h>
#include <ripple/shamap/shamap.h>

namespace ripple {

class dividendmaster
{
public:
    typedef const std::shared_ptr<dividendmaster>& ref;
    typedef std::shared_ptr<dividendmaster> pointer;
    typedef enum { divtype_done = 0, divtype_start = 1, divtype_apply = 2 } divdendtype;
    typedef enum { divstate_done = 0, divstate_start = 1 } divdendstate;

    virtual void lock() = 0;
    virtual void unlock() = 0;
    virtual bool trylock() = 0;
    virtual void setready(bool ready) = 0;
    virtual bool isready() = 0;
    virtual void setrunning(bool running) = 0;
    virtual bool isrunning() = 0;
    
    // <accountid, divcoins, divcoinsvbc, divcoinsvbcrank, divcoinsvbcspd, vrank, vspd>
    typedef std::vector<std::tuple<account, uint64_t, uint64_t, uint64_t, uint64_t, uint32_t, uint64_t, uint64_t>> accountsdividend;
    
    virtual accountsdividend& getdivresult() = 0;
    virtual void settotaldividendvbc(uint64_t) = 0;
    virtual uint64_t gettotaldividendvbc() = 0;
    virtual void settotaldividend(uint64_t) = 0;
    virtual uint64_t gettotaldividend() = 0;
    virtual void setsumvrank(uint64_t) = 0;
    virtual void setsumvspd(uint64_t) = 0;
    virtual bool calcresulthash() = 0;
    virtual uint256 getresulthash() = 0;
    virtual void setresulthash(uint256) = 0;
    virtual void filldivready(shamap::pointer preset) = 0;
    virtual void filldivresult(shamap::pointer preset) = 0;
    virtual void setledgerseq(uint32_t seq) = 0;
    virtual uint32_t getledgerseq() = 0;
    
    static void calcdividend(ledger::ref lastclosedledger);
    
    
    /// @return true: needs dividend.
    static bool calcdividendfunc(ledger::ref baseledger, uint64_t dividendcoins, uint64_t dividendcoinsvbc, accountsdividend& accountsout, uint64_t& actualtotaldividend, uint64_t& actualtotaldividendvbc, uint64_t& sumvrank, uint64_t& sumvspd);
};

std::unique_ptr<dividendmaster>
make_dividendmaster(beast::journal journal);

}

#endif //ripple_app_dividend_master_h_included
