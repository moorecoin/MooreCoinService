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

#ifndef ripple_ledger_h
#define ripple_ledger_h

#include <ripple/shamap/shamap.h>
#include <ripple/app/tx/transaction.h>
#include <ripple/app/tx/transactionmeta.h>
#include <ripple/app/misc/accountstate.h>
#include <ripple/protocol/stledgerentry.h>
#include <ripple/basics/countedobject.h>
#include <ripple/protocol/serializer.h>
#include <ripple/protocol/book.h>
#include <set>

namespace ripple {

class job;

enum ledgerstateparms
{
    lepnone         = 0,    // no special flags

    // input flags
    lepcreate       = 1,    // create if not present

    // output flags
    lepokay         = 2,    // success
    lepmissing      = 4,    // no node in that slot
    lepwrongtype    = 8,    // node of different type there
    lepcreated      = 16,   // node was created
    leperror        = 32,   // error
};

#define ledger_json_dump_txrp   0x1
#define ledger_json_dump_state  0x2
#define ledger_json_expand      0x4
#define ledger_json_full        0x8
#define ledger_json_dump_txdiv  0x10

class sqlitestatement;

// vfalco todo figure out exactly how this thing works.
//         it seems like some ledger database is stored as a global, static in the
//         class. but then what is the meaning of a ledger object? is this
//         really two classes in one? storeofallledgers + singleledgerobject?
//
/** holds some or all of a ledger.
    this can hold just the header, a partial set of data, or the entire set
    of data. it all depends on what is in the corresponding shamap entry.
    various functions are provided to populate or depopulate the caches that
    the object holds references to.

    ledgers are constructed as either mutable or immutable.

    1) if you are the sole owner of a mutable ledger, you can do whatever you
    want with no need for locks.

    2) if you have an immutable ledger, you cannot ever change it, so no need
    for locks.

    3) mutable ledgers cannot be shared.
*/
class ledger
    : public std::enable_shared_from_this <ledger>
    , public countedobject <ledger>
{
public:
    static char const* getcountedobjectname () { return "ledger"; }

    typedef std::shared_ptr<ledger>           pointer;
    typedef const std::shared_ptr<ledger>&    ref;

    enum transresult
    {
        tr_error    = -1,
        tr_success  = 0,
        tr_notfound = 1,
        tr_already  = 2,

        // the transaction itself is corrupt
        tr_badtrans = 3,

        // one of the accounts is invalid
        tr_badacct  = 4,

        // the sending(apply)/receiving(remove) account is broke
        tr_insuff   = 5,

        // account is past this transaction
        tr_pastaseq = 6,

        // account is missing transactions before this
        tr_preaseq  = 7,

        // ledger too early
        tr_badlseq  = 8,

        // amount is less than tx fee
        tr_toosmall = 9,
    };

    // ledger close flags
    static const std::uint32_t slcf_noconsensustime = 1;

public:

    // used for the starting bootstrap ledger
    ledger(const rippleaddress & masterid, std::uint64_t startamount, std::uint64_t startamountvbc = 0);

    ledger (uint256 const& parenthash, uint256 const& transhash,
            uint256 const& accounthash,
            std::uint64_t totcoins, std::uint64_t totcoinsvbc, std::uint32_t closetime,
            std::uint32_t parentclosetime, int closeflags, int closeresolution,
            std::uint32_t dividendledger, std::uint32_t ledgerseq, bool & loaded);
    // used for database ledgers

    ledger (std::uint32_t ledgerseq, std::uint32_t closetime);
    ledger (blob const & rawledger, bool hasprefix);
    ledger (std::string const& rawledger, bool hasprefix);
    ledger (bool dummy, ledger & previous); // ledger after this one
    ledger (ledger & target, bool ismutable); // snapshot

    ledger (ledger const&) = delete;
    ledger& operator= (ledger const&) = delete;

    ~ledger ();

    static ledger::pointer getsql (std::string const& sqlstatement);
    static ledger::pointer getsql1 (sqlitestatement*);
    static void getsql2 (ledger::ref);
    static ledger::pointer getlastfullledger ();
    static std::uint32_t roundclosetime (
        std::uint32_t closetime, std::uint32_t closeresolution);

    void updatehash ();
    void setclosed ()
    {
        mclosed = true;
    }
    void setvalidated()
    {
        mvalidated = true;
    }
    void setaccepted (
        std::uint32_t closetime, int closeresolution, bool correctclosetime);

    void setaccepted ();
    void setimmutable ();
    bool isclosed () const
    {
        return mclosed;
    }
    bool isaccepted () const
    {
        return maccepted;
    }
    bool isvalidated () const
    {
        return mvalidated;
    }
    bool isimmutable () const
    {
        return mimmutable;
    }
    bool isfixed () const
    {
        return mclosed || mimmutable;
    }
    void setfull ()
    {
        mtransactionmap->setledgerseq (mledgerseq);
        maccountstatemap->setledgerseq (mledgerseq);
    }

    bool enforcefreeze () const;

    // ledger signature operations
    void addraw (serializer & s) const;
    void setraw (serializer & s, bool hasprefix);

    uint256 gethash ();
    uint256 const& getparenthash () const
    {
        return mparenthash;
    }
    uint256 const& gettranshash () const
    {
        return mtranshash;
    }
    uint256 const& getaccounthash () const
    {
        return maccounthash;
    }
    std::uint64_t gettotalcoins () const
    {
        return mtotcoins;
    }
    std::uint64_t gettotalcoinsvbc() const
    {
        return mtotcoinsvbc;
    }
    void destroycoins (std::uint64_t fee)
    {
        mtotcoins -= fee;
    }
    void createcoins (std::uint64_t dividend)
    {
        mtotcoins += dividend;
    }
    void createcoinsvbc(std::uint64_t dividendvbc)
    {
        mtotcoinsvbc += dividendvbc;
    }
    void settotalcoins (std::uint64_t totcoins)
    {
        mtotcoins = totcoins;
    }
    void settotalcoinsvbc(std::uint64_t totcoinsvbc)
    {
        mtotcoinsvbc = totcoinsvbc;
    }
    std::uint32_t getclosetimenc () const
    {
        return mclosetime;
    }
    std::uint32_t getparentclosetimenc () const
    {
        return mparentclosetime;
    }
    std::uint32_t getledgerseq () const
    {
        return mledgerseq;
    }
    int getcloseresolution () const
    {
        return mcloseresolution;
    }
    bool getcloseagree () const
    {
        return (mcloseflags & slcf_noconsensustime) == 0;
    }

    // close time functions
    void setclosetime (std::uint32_t ct)
    {
        assert (!mimmutable);
        mclosetime = ct;
    }
    void setclosetime (boost::posix_time::ptime);
    boost::posix_time::ptime getclosetime () const;

    std::uint32_t getdividendledger() const
    {
        return mdividendledger;
    }

    void setdividendledger(std::uint32_t dl)
    {
        mdividendledger = dl;
    }

    void initializedividendledger();

    // low level functions
    shamap::ref peektransactionmap () const
    {
        return mtransactionmap;
    }
    shamap::ref peekaccountstatemap () const
    {
        return maccountstatemap;
    }

    // returns false on error
    bool addsle (sle const& sle);

    // ledger sync functions
    void setacquiring (void);
    bool isacquiring (void) const;
    bool isacquiringtx (void) const;
    bool isacquiringas (void) const;

    // transaction functions
    bool addtransaction (uint256 const& id, serializer const& txn);
    bool addtransaction (
        uint256 const& id, serializer const& txn, serializer const& metadata);
    bool hastransaction (uint256 const& transid) const
    {
        return mtransactionmap->hasitem (transid);
    }
    transaction::pointer gettransaction (uint256 const& transid) const;
    bool gettransaction (
        uint256 const& transid,
        transaction::pointer & txn, transactionmetaset::pointer & txmeta) const;
    bool gettransactionmeta (
        uint256 const& transid, transactionmetaset::pointer & txmeta) const;
    bool getmetahex (uint256 const& transid, std::string & hex) const;

    static sttx::pointer getstransaction (
        shamapitem::ref, shamaptreenode::tntype);
    sttx::pointer getsmtransaction (
        shamapitem::ref, shamaptreenode::tntype,
        transactionmetaset::pointer & txmeta) const;

    // high-level functions
    bool hasaccount (const rippleaddress & acctid) const;
    accountstate::pointer getaccountstate (const rippleaddress & acctid) const;
    ledgerstateparms writeback (ledgerstateparms parms, sle::ref);
    sle::pointer getaccountroot (account const& accountid) const;
    sle::pointer getaccountroot (const rippleaddress & naaccountid) const;
    void updateskiplist ();

    void visitaccountitems (
        account const& accountid, std::function<void (sle::ref)>) const;
    bool visitaccountitems (
        account const& accountid,
        uint256 const& startafter, // entry to start after
        std::uint64_t const hint,  // hint which page to start at
        unsigned int limit,
        std::function <bool (sle::ref)>) const;
    void visitstateitems (std::function<void (sle::ref)>) const;

    // database functions (low-level)
    static ledger::pointer loadbyindex (std::uint32_t ledgerindex);
    static ledger::pointer loadbyhash (uint256 const& ledgerhash);
    static uint256 gethashbyindex (std::uint32_t index);
    static bool gethashesbyindex (
        std::uint32_t index, uint256 & ledgerhash, uint256 & parenthash);
    static std::map< std::uint32_t, std::pair<uint256, uint256> >
                  gethashesbyindex (std::uint32_t minseq, std::uint32_t maxseq);
    bool pendsavevalidated (bool issynchronous, bool iscurrent);

    // next/prev function
    sle::pointer getsle (uint256 const& uhash) const; // sle is mutable
    sle::pointer getslei (uint256 const& uhash) const; // sle is immutable

    // vfalco note these seem to let you walk the list of ledgers
    //
    uint256 getfirstledgerindex () const;
    uint256 getlastledgerindex () const;

    // first node >hash
    uint256 getnextledgerindex (uint256 const& uhash) const;

    // first node >hash, <end
    uint256 getnextledgerindex (uint256 const& uhash, uint256 const& uend) const;

    // last node <hash
    uint256 getprevledgerindex (uint256 const& uhash) const;

    // last node <hash, >begin
    uint256 getprevledgerindex (uint256 const& uhash, uint256 const& ubegin) const;

    // ledger hash table function
    uint256 getledgerhash (std::uint32_t ledgerindex);
    typedef std::vector<std::pair<std::uint32_t, uint256>> ledgerhashes;
    ledgerhashes getledgerhashes () const;

    std::vector<uint256> getledgeramendments () const;

    std::vector<uint256> getneededtransactionhashes (
        int max, shamapsyncfilter* filter) const;
    std::vector<uint256> getneededaccountstatehashes (
        int max, shamapsyncfilter* filter) const;

    //
    // refer functions
    //
    sle::pointer getreferobject(const account& account) const;
    bool hasrefer (const account & account) const;
    
    //
    // dividend functions
    //
    sle::pointer getdividendobject () const;
    std::uint64_t getdividendcoins() const;
    std::uint64_t getdividendcoinsvbc() const;
    std::uint32_t getdividendtimenc() const;
    bool isdividendstarted() const;
    std::uint32_t getdividendbaseledger() const;

    //
    // generator map functions
    //

    sle::pointer getgenerator (account const& ugeneratorid) const;

    //
    // offer functions
    //

    sle::pointer getoffer (uint256 const& uindex) const;
    sle::pointer getoffer (account const& account, std::uint32_t usequence) const;

    //
    // directory functions
    // directories are doubly linked lists of nodes.

    // given a directory root and and index compute the index of a node.
    static void ownerdirdescriber (sle::ref, bool, account const& owner);

    // return a node: root or normal
    sle::pointer getdirnode (uint256 const& unodeindex) const;

    //
    // quality
    //

    static void qualitydirdescriber (
        sle::ref, bool,
        currency const& utakerpayscurrency, account const& utakerpaysissuer,
        currency const& utakergetscurrency, account const& utakergetsissuer,
        const std::uint64_t & urate);

    //
    // ripple functions : credit lines
    //

    sle::pointer
    getripplestate (uint256 const& unode) const;

    sle::pointer
    getripplestate (
        account const& a, account const& b, currency const& currency) const;

    std::uint32_t getreferencefeeunits ()
    {
        // returns the cost of the reference transaction in fee units
        updatefees ();
        return mreferencefeeunits;
    }

    std::uint64_t getbasefee ()
    {
        // returns the cost of the reference transaction in drops
        updatefees ();
        return mbasefee;
    }

    std::uint64_t getreserve (int increments)
    {
        // returns the required reserve in drops
        updatefees ();
        return static_cast<std::uint64_t> (increments) * mreserveincrement
            + mreservebase;
    }

    std::uint64_t getreserveinc ()
    {
        updatefees ();
        return mreserveincrement;
    }

    std::uint64_t scalefeebase (std::uint64_t fee);
    std::uint64_t scalefeeload (std::uint64_t fee, bool badmin);

    static std::set<std::uint32_t> getpendingsaves();

    /** const version of gethash() which gets the current value without calling
        updatehash(). */
    uint256 const& getrawhash () const
    {
        return mhash;
    }

    bool walkledger () const;
    bool assertsane () const;

protected:
    sle::pointer getasnode (
        ledgerstateparms& parms, uint256 const& nodeid, ledgerentrytype let) const;

    // returned sle is immutable
    sle::pointer getasnodei (uint256 const& nodeid, ledgerentrytype let) const;

    void savevalidatedledgerasync(job&, bool current)
    {
        savevalidatedledger(current);
    }
    bool savevalidatedledger (bool current);

private:
    void initializefees ();
    void updatefees ();

    // the basic ledger structure, can be opened, closed, or synching
    uint256       mhash;
    uint256       mparenthash;
    uint256       mtranshash;
    uint256       maccounthash;
    std::uint64_t mtotcoins;
    std::uint64_t mtotcoinsvbc;
    std::uint32_t mledgerseq;

    // when this ledger closed
    std::uint32_t mclosetime;

    // when the previous ledger closed
    std::uint32_t mparentclosetime;

    // the resolution for this ledger close time (2-120 seconds)
    int           mcloseresolution;

    // flags indicating how this ledger close took place
    std::uint32_t mcloseflags;

    std::uint32_t mdividendledger; /// @todo remove this

    bool          mclosed, mvalidated, mvalidhash, maccepted, mimmutable;

    // fee units for the reference transaction
    std::uint32_t mreferencefeeunits;

    // reserve basse and increment in fee units
    std::uint32_t mreservebase, mreserveincrement;

    // ripple cost of the reference transaction
    std::uint64_t mbasefee;

    shamap::pointer mtransactionmap;
    shamap::pointer maccountstatemap;

    typedef ripplemutex staticlocktype;
    typedef std::lock_guard <staticlocktype> staticscopedlocktype;

    // ledgers not fully saved, validated ledger present but db may not be
    // correct yet.
    static staticlocktype spendingsavelock;

    static std::set<std::uint32_t>  spendingsaves;
};

inline ledgerstateparms operator| (
    const ledgerstateparms& l1, const ledgerstateparms& l2)
{
    return static_cast<ledgerstateparms> (
        static_cast<int> (l1) | static_cast<int> (l2));
}

inline ledgerstateparms operator& (
    const ledgerstateparms& l1, const ledgerstateparms& l2)
{
    return static_cast<ledgerstateparms> (
        static_cast<int> (l1) & static_cast<int> (l2));
}

} // ripple

#endif
