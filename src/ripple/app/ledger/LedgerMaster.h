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

#ifndef ripple_ledgermaster_h_included
#define ripple_ledgermaster_h_included

#include <ripple/app/ledger/ledgerentryset.h>
#include <ripple/basics/stringutilities.h>
#include <ripple/protocol/rippleledgerhash.h>
#include <ripple/core/config.h>
#include <beast/insight/collector.h>
#include <beast/threads/stoppable.h>
#include <beast/threads/unlockguard.h>
#include <beast/utility/propertystream.h>

namespace ripple {

// tracks the current ledger and any ledgers in the process of closing
// tracks ledger history
// tracks held transactions

// vfalco todo rename to ledgers
//        it sounds like this holds all the ledgers...
//
class ledgermaster
    : public beast::stoppable
{
protected:
    explicit ledgermaster (stoppable& parent);

public:
    typedef std::function <void (ledger::ref)> callback;

public:
    typedef ripplerecursivemutex locktype;
    typedef std::unique_lock <locktype> scopedlocktype;
    typedef beast::genericscopedunlock <locktype> scopedunlocktype;

    virtual ~ledgermaster () = 0;

    virtual ledgerindex getcurrentledgerindex () = 0;
    virtual ledgerindex getvalidledgerindex () = 0;

    virtual locktype& peekmutex () = 0;

    // the current ledger is the ledger we believe new transactions should go in
    virtual ledger::pointer getcurrentledger () = 0;

    // the finalized ledger is the last closed/accepted ledger
    virtual ledger::pointer getclosedledger () = 0;

    // the validated ledger is the last fully validated ledger
    virtual ledger::pointer getvalidatedledger () = 0;

    // this is the last ledger we published to clients and can lag the validated ledger
    virtual ledger::ref getpublishedledger () = 0;

    virtual int getpublishedledgerage () = 0;
    virtual int getvalidatedledgerage () = 0;
    virtual bool iscaughtup(std::string& reason) = 0;

    virtual ter dotransaction (
        sttx::ref txn,
            transactionengineparams params, bool& didapply) = 0;

    virtual int getminvalidations () = 0;

    virtual void setminvalidations (int v) = 0;

    virtual std::uint32_t getearliestfetch () = 0;

    virtual void pushledger (ledger::pointer newledger) = 0;
    virtual void pushledger (ledger::pointer newlcl, ledger::pointer newol) = 0;
    virtual bool storeledger (ledger::pointer) = 0;
    virtual void forcevalid (ledger::pointer) = 0;

    virtual void setfullledger (ledger::pointer ledger, bool issynchronous, bool iscurrent) = 0;

    virtual void switchledgers (ledger::pointer lastclosed, ledger::pointer newcurrent) = 0;

    virtual void failedsave(std::uint32_t seq, uint256 const& hash) = 0;

    virtual std::string getcompleteledgers () = 0;

    virtual void applyheldtransactions () = 0;

    /** get a ledger's hash by sequence number using the cache
    */
    virtual uint256 gethashbyseq (std::uint32_t index) = 0;

    /** walk to a ledger's hash using the skip list
    */
    virtual uint256 walkhashbyseq (std::uint32_t index) = 0;
    virtual uint256 walkhashbyseq (std::uint32_t index, ledger::ref referenceledger) = 0;

    virtual ledger::pointer findacquireledger (std::uint32_t index, uint256 const& hash) = 0;

    virtual ledger::pointer getledgerbyseq (std::uint32_t index) = 0;

    virtual ledger::pointer getledgerbyhash (uint256 const& hash) = 0;

    virtual void setledgerrangepresent (std::uint32_t minv, std::uint32_t maxv) = 0;

    virtual uint256 getledgerhash(std::uint32_t desiredseq, ledger::ref knowngoodledger) = 0;

    virtual void addheldtransaction (transaction::ref trans) = 0;
    virtual void fixmismatch (ledger::ref ledger) = 0;

    virtual bool haveledgerrange (std::uint32_t from, std::uint32_t to) = 0;
    virtual bool haveledger (std::uint32_t seq) = 0;
    virtual void clearledger (std::uint32_t seq) = 0;
    virtual bool getvalidatedrange (std::uint32_t& minval, std::uint32_t& maxval) = 0;
    virtual bool getfullvalidatedrange (std::uint32_t& minval, std::uint32_t& maxval) = 0;

    virtual void tune (int size, int age) = 0;
    virtual void sweep () = 0;
    virtual float getcachehitrate () = 0;
    virtual void addvalidatecallback (callback& c) = 0;

    virtual void checkaccept (ledger::ref ledger) = 0;
    virtual void checkaccept (uint256 const& hash, std::uint32_t seq) = 0;
    virtual void consensusbuilt (ledger::ref ledger) = 0;

    virtual ledgerindex getbuildingledger () = 0;
    virtual void setbuildingledger (ledgerindex index) = 0;

    virtual void tryadvance () = 0;
    virtual void newpathrequest () = 0;
    virtual bool isnewpathrequest () = 0;
    virtual void neworderbookdb () = 0;

    virtual bool fixindex (ledgerindex ledgerindex, ledgerhash const& ledgerhash) = 0;
    virtual void doledgercleaner(json::value const& parameters) = 0;

    virtual beast::propertystream::source& getpropertysource () = 0;

    static bool shouldacquire (std::uint32_t currentledgerid, 
        std::uint32_t ledgerhistory, std::uint32_t ledgerhistoryindex,
        std::uint32_t targetledger);

    virtual void clearpriorledgers (ledgerindex seq) = 0;

    virtual void clearledgercacheprior (ledgerindex seq) = 0;
};

std::unique_ptr <ledgermaster>
make_ledgermaster (config const& config, beast::stoppable& parent,
    beast::insight::collector::ptr const& collector, beast::journal journal);

} // ripple

#endif
