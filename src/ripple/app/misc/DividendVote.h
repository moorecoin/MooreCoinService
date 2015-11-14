#ifndef ripple_app_dividend_h_included
#define ripple_app_dividend_h_included

#include <ripple/app/ledger/ledger.h>
#include <ripple/shamap/shamap.h>

namespace ripple {

class dividendvote
{
public:

    virtual ~dividendvote() {}
    
    virtual bool isstartledger(ledger::ref ledger) = 0;

    virtual void dostartvalidation (ledger::ref lastclosedledger,
                                    stobject& basevalidation) = 0;

    virtual void dostartvoting (ledger::ref lastclosedledger,
                                shamap::ref initialposition) = 0;
    
    virtual bool isapplyledger(ledger::ref ledger) = 0;
    
    virtual void doapplyvalidation (ledger::ref lastclosedledger,
                                    stobject& basevalidation) = 0;
    
    virtual bool doapplyvoting (ledger::ref lastclosedledger,
                                shamap::ref initialposition) = 0;

};

std::unique_ptr<dividendvote>
make_dividendvote(beast::journal journal);

}

#endif
