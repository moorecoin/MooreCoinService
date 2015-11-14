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

#ifndef ripple_networkops_h
#define ripple_networkops_h

#include <ripple/core/jobqueue.h>
#include <ripple/protocol/stvalidation.h>
#include <ripple/app/ledger/ledger.h>
#include <ripple/app/ledger/ledgerproposal.h>
#include <ripple/net/infosub.h>
#include <beast/cxx14/memory.h> // <memory>
#include <beast/threads/stoppable.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <tuple>
#include <ripple/app/misc/dividendmaster.h>

#include "ripple.pb.h"

namespace ripple {

// operations that clients may wish to perform against the network
// master operational handler, server sequencer, network tracker

class peer;
class ledgerconsensus;
class ledgermaster;

// this is the primary interface into the "client" portion of the program.
// code that wants to do normal operations on the network such as
// creating and monitoring accounts, creating transactions, and so on
// should use this interface. the rpc code will primarily be a light wrapper
// over this code.
//
// eventually, it will check the node's operating mode (synched, unsynched,
// etectera) and defer to the correct means of processing. the current
// code assumes this node is synched (and will continue to do so until
// there's a functional network.
//
/** provides server functionality for clients.

    clients include backend applications, local commands, and connected
    clients. this class acts as a proxy, fulfilling the command with local
    data if possible, or asking the network and returning the results if
    needed.

    a backend application or local client can trust a local instance of
    rippled / networkops. however, client software connecting to non-local
    instances of rippled will need to be hardened to protect against hostile
    or unreliable servers.
*/
class networkops
    : public infosub::source
{
protected:
    explicit networkops (stoppable& parent);

public:
    typedef beast::abstract_clock <std::chrono::steady_clock> clock_type;

    enum fault
    {
        // exceptions these functions can throw
        io_error    = 1,
        no_network  = 2,
    };

    enum operatingmode
    {
        // how we process transactions or account balance requests
        omdisconnected  = 0,    // not ready to process requests
        omconnected     = 1,    // convinced we are talking to the network
        omsyncing       = 2,    // fallen slightly behind
        omtracking      = 3,    // convinced we agree with the network
        omfull          = 4     // we have the ledger and can even validate
    };

    // vfalco todo fix orderbookdb to not need this unrelated type.
    //
    typedef hash_map <std::uint64_t, infosub::wptr> submaptype;

public:
    virtual ~networkops () = 0;

    //--------------------------------------------------------------------------
    //
    // network information
    //

    // our best estimate of wall time in seconds from 1/1/2000
    virtual std::uint32_t getnetworktimenc () const = 0;
    // our best estimate of current ledger close time
    virtual std::uint32_t getclosetimenc () const = 0;
    // use *only* to timestamp our own validation
    virtual std::uint32_t getvalidationtimenc () = 0;
    virtual void closetimeoffset (int) = 0;
    virtual boost::posix_time::ptime getnetworktimept (int& offset) const = 0;
    virtual std::uint32_t getledgerid (uint256 const& hash) = 0;
    virtual std::uint32_t getcurrentledgerid () = 0;

    virtual operatingmode getoperatingmode () const = 0;
    virtual std::string stroperatingmode () const = 0;
    virtual ledger::pointer getclosedledger () = 0;
    virtual ledger::pointer getvalidatedledger () = 0;
    virtual ledger::pointer getpublishedledger () = 0;
    virtual ledger::pointer getcurrentledger () = 0;
    virtual ledger::pointer getledgerbyhash (uint256 const& hash) = 0;
    virtual ledger::pointer getledgerbyseq (const std::uint32_t seq) = 0;
    virtual void            missingnodeinledger (const std::uint32_t seq) = 0;

    virtual uint256         getclosedledgerhash () = 0;

    // do we have this inclusive range of ledgers in our database
    virtual bool haveledgerrange (std::uint32_t from, std::uint32_t to) = 0;
    virtual bool haveledger (std::uint32_t seq) = 0;
    virtual std::uint32_t getvalidatedseq () = 0;
    virtual bool isvalidated (std::uint32_t seq) = 0;
    virtual bool isvalidated (std::uint32_t seq, uint256 const& hash) = 0;
    virtual bool isvalidated (ledger::ref l) = 0;
    virtual bool getvalidatedrange (std::uint32_t& minval, std::uint32_t& maxval) = 0;
    virtual bool getfullvalidatedrange (std::uint32_t& minval, std::uint32_t& maxval) = 0;

    virtual stvalidation::ref getlastvalidation () = 0;
    virtual void setlastvalidation (stvalidation::ref v) = 0;
    virtual sle::pointer getsle (ledger::pointer lpledger, uint256 const& uhash) = 0;
    virtual sle::pointer getslei (ledger::pointer lpledger, uint256 const& uhash) = 0;

    //--------------------------------------------------------------------------
    //
    // transaction processing
    //

    // must complete immediately
    // vfalco todo make this a txcallback structure
    typedef std::function<void (transaction::pointer, ter)> stcallback;
    virtual void submittransaction (job&, sttx::pointer,
        stcallback callback = stcallback ()) = 0;
    virtual transaction::pointer submittransactionsync (transaction::ref tptrans,
        bool badmin, bool blocal, bool bfailhard, bool bsubmit) = 0;
    virtual transaction::pointer processtransactioncb (transaction::pointer,
        bool badmin, bool blocal, bool bfailhard, stcallback) = 0;
    virtual transaction::pointer processtransaction (transaction::pointer transaction,
        bool badmin, bool blocal, bool bfailhard) = 0;
    virtual transaction::pointer findtransactionbyid (uint256 const& transactionid) = 0;
    virtual int findtransactionsbydestination (std::list<transaction::pointer>&,
        rippleaddress const& destinationaccount, std::uint32_t startledgerseq,
            std::uint32_t endledgerseq, int maxtransactions) = 0;

    //--------------------------------------------------------------------------
    //
    // account functions
    //

    virtual accountstate::pointer getaccountstate (ledger::ref lrledger,
        rippleaddress const& accountid) = 0;
    virtual sle::pointer getgenerator (ledger::ref lrledger,
        account const& ugeneratorid) = 0;

    //--------------------------------------------------------------------------
    //
    // directory functions
    //

    virtual stvector256 getdirnodeinfo (ledger::ref lrledger,
        uint256 const& urootindex, std::uint64_t& unodeprevious,
                                   std::uint64_t& unodenext) = 0;

    //--------------------------------------------------------------------------
    //
    // owner functions
    //

    virtual json::value getownerinfo (ledger::pointer lpledger,
        rippleaddress const& naaccount) = 0;

    //--------------------------------------------------------------------------
    //
    // book functions
    //

    virtual void getbookpage (
        bool badmin,
        ledger::pointer lpledger,
        book const& book,
        account const& utakerid,
        bool const bproof,
        const unsigned int ilimit,
        json::value const& jvmarker,
        json::value& jvresult) = 0;

    //--------------------------------------------------------------------------

    // ledger proposal/close functions
    virtual void processtrustedproposal (ledgerproposal::pointer proposal,
        std::shared_ptr<protocol::tmproposeset> set, rippleaddress nodepublic,
            uint256 checkledger, bool siggood) = 0;

    virtual shamapaddnode gottxdata (const std::shared_ptr<peer>& peer,
        uint256 const& hash, const std::list<shamapnodeid>& nodeids,
        const std::list< blob >& nodedata) = 0;

    virtual bool recvvalidation (stvalidation::ref val,
        std::string const& source) = 0;

    virtual void takeposition (int seq, shamap::ref position) = 0;

    virtual shamap::pointer gettxmap (uint256 const& hash) = 0;

    virtual bool hastxset (const std::shared_ptr<peer>& peer,
        uint256 const& set, protocol::txsetstatus status) = 0;

    virtual void mapcomplete (uint256 const& hash, shamap::ref map) = 0;

    virtual bool stillneedtxset (uint256 const& hash) = 0;

    // fetch packs
    virtual void makefetchpack (job&, std::weak_ptr<peer> peer,
        std::shared_ptr<protocol::tmgetobjectbyhash> request,
        uint256 wantledger, std::uint32_t uuptime) = 0;

    virtual bool shouldfetchpack (std::uint32_t seq) = 0;
    virtual void gotfetchpack (bool progress, std::uint32_t seq) = 0;
    virtual void addfetchpack (
        uint256 const& hash, std::shared_ptr< blob >& data) = 0;
    virtual bool getfetchpack (uint256 const& hash, blob& data) = 0;
    virtual int getfetchsize () = 0;
    virtual void sweepfetchpack () = 0;

    // network state machine
    virtual void endconsensus (bool correctlcl) = 0;
    virtual void setstandalone () = 0;
    virtual void setstatetimer () = 0;

    virtual void newlcl (
        int proposers, int convergetime, uint256 const& ledgerhash) = 0;
    // vfalco todo rename to setneednetworkledger
    virtual void neednetworkledger () = 0;
    virtual void clearneednetworkledger () = 0;
    virtual bool isneednetworkledger () = 0;
    virtual bool isfull () = 0;
    virtual void setproposing (bool isproposing, bool isvalidating) = 0;
    virtual bool isproposing () = 0;
    virtual bool isvalidating () = 0;
    virtual bool isamendmentblocked () = 0;
    virtual void setamendmentblocked () = 0;
    virtual void consensusviewchange () = 0;
    virtual int getpreviousproposers () = 0;
    virtual int getpreviousconvergetime () = 0;
    virtual std::uint32_t getlastclosetime () = 0;
    virtual void setlastclosetime (std::uint32_t t) = 0;

    virtual json::value getconsensusinfo () = 0;
    virtual json::value getserverinfo (bool human, bool admin) = 0;
    virtual void clearledgerfetch () = 0;
    virtual json::value getledgerfetchinfo () = 0;
    virtual std::uint32_t acceptledger () = 0;

    typedef hash_map <nodeid, std::list<ledgerproposal::pointer>> proposals;
    virtual proposals& peekstoredproposals () = 0;

    virtual void storeproposal (ledgerproposal::ref proposal,
        rippleaddress const& peerpublic) = 0;

    virtual uint256 getconsensuslcl () = 0;

    virtual void reportfeechange () = 0;

    virtual void updatelocaltx (ledger::ref newvalidledger) = 0;
    virtual void addlocaltx (ledger::ref openledger, sttx::ref txn) = 0;
    virtual std::size_t getlocaltxcount () = 0;

    //helper function to generate sql query to get transactions
    virtual std::string transactionssql (std::string selection,
        rippleaddress const& account, std::int32_t minledger, std::int32_t maxledger,
        bool descending, std::uint32_t offset, int limit, bool binary,
            bool count, bool badmin) = 0;

    // client information retrieval functions
    typedef std::pair<transaction::pointer, transactionmetaset::pointer>
    accounttx;

    typedef std::vector<accounttx> accounttxs;

    virtual accounttxs getaccounttxs (
        rippleaddress const& account,
        std::int32_t minledger, std::int32_t maxledger,  bool descending,
        std::uint32_t offset, int limit, bool badmin) = 0;

    virtual accounttxs gettxsaccount (
        rippleaddress const& account,
        std::int32_t minledger, std::int32_t maxledger, bool forward,
        json::value& token, int limit, bool badmin, const std::string& txtype) = 0;

    typedef std::tuple<std::string, std::string, std::uint32_t>
    txnmetaledgertype;

    typedef std::vector<txnmetaledgertype> metatxslist;

    virtual metatxslist getaccounttxsb (rippleaddress const& account,
        std::int32_t minledger, std::int32_t maxledger,  bool descending,
            std::uint32_t offset, int limit, bool badmin) = 0;

    virtual metatxslist gettxsaccountb (rippleaddress const& account,
        std::int32_t minledger, std::int32_t maxledger,  bool forward,
        json::value& token, int limit, bool badmin, const std::string& txtype) = 0;

    virtual std::vector<rippleaddress> getledgeraffectedaccounts (
        std::uint32_t ledgerseq) = 0;

    //--------------------------------------------------------------------------
    //
    // monitoring: publisher side
    //
    virtual void publedger (ledger::ref lpaccepted) = 0;
    virtual void pubproposedtransaction (ledger::ref lpcurrent,
        sttx::ref sttxn, ter terresult) = 0;

    virtual dividendmaster::pointer getdividendmaster() = 0;
};

std::unique_ptr<networkops>
make_networkops (networkops::clock_type& clock, bool standalone,
    std::size_t network_quorum, jobqueue& job_queue, ledgermaster& ledgermaster,
    beast::stoppable& parent, beast::journal journal);

json::value networkops_transjson (
    const sttx& sttxn, ter terresult, bool bvalidated,
    ledger::ref lpcurrent);
} // ripple

#endif
