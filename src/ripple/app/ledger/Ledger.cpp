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

#include <beastconfig.h>
#include <ripple/app/ledger/ledger.h>
#include <ripple/app/ledger/acceptedledger.h>
#include <ripple/app/ledger/inboundledgers.h>
#include <ripple/app/ledger/ledgermaster.h>
#include <ripple/app/ledger/ledgertiming.h>
#include <ripple/app/ledger/ledgertojson.h>
#include <ripple/app/ledger/orderbookdb.h>
#include <ripple/app/data/databasecon.h>
#include <ripple/app/data/sqlitedatabase.h>
#include <ripple/app/main/application.h>
#include <ripple/app/misc/defaultmissingnodehandler.h>
#include <ripple/app/misc/ihashrouter.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/app/tx/transactionmaster.h>
#include <ripple/basics/log.h>
#include <ripple/basics/loggedtimings.h>
#include <ripple/basics/stringutilities.h>
#include <ripple/protocol/indexes.h>
#include <ripple/protocol/jsonfields.h>
#include <ripple/core/config.h>
#include <ripple/core/jobqueue.h>
#include <ripple/core/loadfeetrack.h>
#include <ripple/json/to_string.h>
#include <ripple/nodestore/database.h>
#include <ripple/protocol/hashprefix.h>
#include <beast/unit_test/suite.h>

namespace ripple {

ledger::ledger(rippleaddress const& masterid, std::uint64_t startamount, std::uint64_t startamountvbc)
    : mtotcoins(startamount)
    , mtotcoinsvbc(startamountvbc) // remark - please review this startamount for mtotcoinsvbc
    , mledgerseq(1) // first ledger
    , mclosetime(0)
    , mparentclosetime(0)
    , mcloseresolution(ledger_time_accuracy)
    , mcloseflags(0)
    , mclosed(false)
    , mvalidated(false)
    , mvalidhash(false)
    , maccepted(false)
    , mimmutable(false)
    , mtransactionmap  (std::make_shared <shamap> (smttransaction,
       getapp().getfullbelowcache(), getapp().gettreenodecache(),
           getapp().getnodestore(), defaultmissingnodehandler(),
               deprecatedlogs().journal("shamap")))
    , maccountstatemap (std::make_shared <shamap> (smtstate,
        getapp().getfullbelowcache(), getapp().gettreenodecache(),
           getapp().getnodestore(), defaultmissingnodehandler(),
               deprecatedlogs().journal("shamap")))
{
    // special case: put coins in root account
    auto startaccount = std::make_shared<accountstate> (masterid);
    auto& sle = startaccount->peeksle ();
    sle.setfieldamount (sfbalance, startamount);
    sle.setfieldamount (sfbalancevbc, startamountvbc);
    sle.setfieldu32 (sfsequence, 1);

    writelog (lstrace, ledger)
        << "root account: " << startaccount->peeksle ().getjson (0);

    writeback (lepcreate, startaccount->getsle ());

    maccountstatemap->flushdirty (hotaccount_node, mledgerseq);

    initializefees ();
    initializedividendledger ();
}

ledger::ledger (uint256 const& parenthash,
                uint256 const& transhash,
                uint256 const& accounthash,
                std::uint64_t totcoins,
                std::uint64_t totcoinsvbc,
                std::uint32_t closetime,
                std::uint32_t parentclosetime,
                int closeflags,
                int closeresolution,
                std::uint32_t dividendledger,
                std::uint32_t ledgerseq,
                bool& loaded)
    : mparenthash (parenthash)
    , mtranshash (transhash)
    , maccounthash (accounthash)
    , mtotcoins (totcoins)
    , mtotcoinsvbc(totcoinsvbc)
    , mledgerseq (ledgerseq)
    , mclosetime (closetime)
    , mparentclosetime (parentclosetime)
    , mcloseresolution (closeresolution)
    , mcloseflags (closeflags)
    , mdividendledger (dividendledger)
    , mclosed (false)
    , mvalidated (false)
    , mvalidhash (false)
    , maccepted (false)
    , mimmutable (true)
    , mtransactionmap (std::make_shared <shamap> (
        smttransaction, transhash, getapp().getfullbelowcache(),
            getapp().gettreenodecache(), getapp().getnodestore(),
                defaultmissingnodehandler(), deprecatedlogs().journal("shamap")))
    , maccountstatemap (std::make_shared <shamap> (smtstate, accounthash,
        getapp().getfullbelowcache(), getapp().gettreenodecache(),
            getapp().getnodestore(), defaultmissingnodehandler(),
                deprecatedlogs().journal("shamap")))
{
    updatehash ();
    loaded = true;

    if (mtranshash.isnonzero () &&
        !mtransactionmap->fetchroot (mtranshash, nullptr))
    {
        loaded = false;
        writelog (lswarning, ledger) << "don't have tx root for ledger";
    }

    if (maccounthash.isnonzero () &&
        !maccountstatemap->fetchroot (maccounthash, nullptr))
    {
        loaded = false;
        writelog (lswarning, ledger) << "don't have as root for ledger";
    }

    mtransactionmap->setimmutable ();
    maccountstatemap->setimmutable ();

    initializefees ();
}

// create a new ledger that's a snapshot of this one
ledger::ledger (ledger& ledger,
                bool ismutable)
    : mparenthash (ledger.mparenthash)
    , mtotcoins (ledger.mtotcoins)
    , mtotcoinsvbc (ledger.mtotcoinsvbc)
    , mledgerseq (ledger.mledgerseq)
    , mclosetime (ledger.mclosetime)
    , mparentclosetime (ledger.mparentclosetime)
    , mcloseresolution (ledger.mcloseresolution)
    , mcloseflags (ledger.mcloseflags)
    , mdividendledger (ledger.mdividendledger)
    , mclosed (ledger.mclosed)
    , mvalidated (ledger.mvalidated)
    , mvalidhash (false)
    , maccepted (ledger.maccepted)
    , mimmutable (!ismutable)
    , mtransactionmap (ledger.mtransactionmap->snapshot (ismutable))
    , maccountstatemap (ledger.maccountstatemap->snapshot (ismutable))
{
    updatehash ();
    initializefees ();
}

// create a new ledger that follows this one
ledger::ledger (bool /* dummy */,
                ledger& prevledger)
    : mtotcoins (prevledger.mtotcoins)
    , mtotcoinsvbc (prevledger.mtotcoinsvbc)
    , mledgerseq (prevledger.mledgerseq + 1)
    , mparentclosetime (prevledger.mclosetime)
    , mcloseresolution (prevledger.mcloseresolution)
    , mcloseflags (0)
    , mdividendledger (prevledger.mdividendledger)
    , mclosed (false)
    , mvalidated (false)
    , mvalidhash (false)
    , maccepted (false)
    , mimmutable (false)
    , mtransactionmap (std::make_shared <shamap> (smttransaction,
        getapp().getfullbelowcache(), getapp().gettreenodecache(),
            getapp().getnodestore(), defaultmissingnodehandler(),
                deprecatedlogs().journal("shamap")))
    , maccountstatemap (prevledger.maccountstatemap->snapshot (true))
{
    prevledger.updatehash ();

    mparenthash = prevledger.gethash ();

    assert (mparenthash.isnonzero ());

    mcloseresolution = continuousledgertiming::getnextledgertimeresolution (
                           prevledger.mcloseresolution,
                           prevledger.getcloseagree (),
                           mledgerseq);

    if (prevledger.mclosetime == 0)
    {
        mclosetime = roundclosetime (
            getapp().getops ().getclosetimenc (), mcloseresolution);
    }
    else
    {
        mclosetime = prevledger.mclosetime + mcloseresolution;
    }

    initializefees ();
}

ledger::ledger (blob const& rawledger,
                bool hasprefix)
    : mclosed (false)
    , mvalidated (false)
    , mvalidhash (false)
    , maccepted (false)
    , mimmutable (true)
{
    serializer s (rawledger);

    setraw (s, hasprefix);

    initializefees ();
}

ledger::ledger (std::string const& rawledger, bool hasprefix)
    : mclosed (false)
    , mvalidated (false)
    , mvalidhash (false)
    , maccepted (false)
    , mimmutable (true)
{
    serializer s (rawledger);
    setraw (s, hasprefix);
    initializefees ();
}

/** used for ledgers loaded from json files */
ledger::ledger (std::uint32_t ledgerseq, std::uint32_t closetime)
    : mtotcoins (0),
      mtotcoinsvbc (0),
      mledgerseq (ledgerseq),
      mclosetime (closetime),
      mparentclosetime (0),
      mcloseresolution (ledger_time_accuracy),
      mcloseflags (0),
      mclosed (false),
      mvalidated (false),
      mvalidhash (false),
      maccepted (false),
      mimmutable (false),
      mtransactionmap (std::make_shared <shamap> (
          smttransaction, getapp().getfullbelowcache(),
            getapp().gettreenodecache(), getapp().getnodestore(),
                defaultmissingnodehandler(), deprecatedlogs().journal("shamap"))),
      maccountstatemap (std::make_shared <shamap> (
          smtstate, getapp().getfullbelowcache(),
            getapp().gettreenodecache(), getapp().getnodestore(),
                defaultmissingnodehandler(), deprecatedlogs().journal("shamap")))
{
    initializefees ();
    initializedividendledger();
}


ledger::~ledger ()
{
    if (mtransactionmap)
    {
        logtimeddestroy <ledger> (mtransactionmap, "mtransactionmap");
    }

    if (maccountstatemap)
    {
        logtimeddestroy <ledger> (maccountstatemap, "maccountstatemap");
    }
}

bool ledger::enforcefreeze () const
{

    // temporarily, the freze code can run in either
    // enforcing mode or non-enforcing mode. in
    // non-enforcing mode, freeze flags can be
    // manipulated, but freezing is not actually
    // enforced. once freeze enforcing has been
    // enabled, this function can be removed

    // let freeze enforcement be tested
    // if you wish to test non-enforcing mode,
    // you must remove this line
    if (getconfig().run_standalone)
        return true;

    // freeze enforcing date is september 15, 2014
    static std::uint32_t const enforcedate =
        itoseconds (boost::posix_time::ptime (
            boost::gregorian::date (2014, boost::gregorian::sep, 15)));

    return mparentclosetime >= enforcedate;
}

void ledger::setimmutable ()
{
    // updates the hash and marks the ledger and its maps immutable

    updatehash ();
    mimmutable = true;

    if (mtransactionmap)
        mtransactionmap->setimmutable ();

    if (maccountstatemap)
        maccountstatemap->setimmutable ();
}

void ledger::updatehash ()
{
    if (!mimmutable)
    {
        if (mtransactionmap)
            mtranshash = mtransactionmap->gethash ();
        else
            mtranshash.zero ();

        if (maccountstatemap)
            maccounthash = maccountstatemap->gethash ();
        else
            maccounthash.zero ();
    }

    // vfalco todo fix this hard coded magic number 122
    serializer s;
    s.add32 (hashprefix::ledgermaster);
    addraw (s);
    mhash = s.getsha512half ();
    mvalidhash = true;
}

void ledger::setraw (serializer& s, bool hasprefix)
{
    serializeriterator sit (s);

    if (hasprefix)
        sit.get32 ();

    mledgerseq =        sit.get32 ();
    mtotcoins =         sit.get64 ();
    mtotcoinsvbc =      sit.get64();
    mparenthash =       sit.get256 ();
    mtranshash =        sit.get256 ();
    maccounthash =      sit.get256 ();
    mparentclosetime =  sit.get32 ();
    mclosetime =        sit.get32 ();
    mdividendledger =   sit.get32 ();
    mcloseresolution =  sit.get8 ();
    mcloseflags =       sit.get8 ();
    updatehash ();

    if (mvalidhash)
    {
        application& app = getapp();
        mtransactionmap = std::make_shared<shamap> (smttransaction, mtranshash,
            app.getfullbelowcache(), app.gettreenodecache(),
                getapp().getnodestore(), defaultmissingnodehandler(),
                    deprecatedlogs().journal("shamap"));
        maccountstatemap = std::make_shared<shamap> (smtstate, maccounthash,
            app.getfullbelowcache(), app.gettreenodecache(),
                getapp().getnodestore(), defaultmissingnodehandler(),
                    deprecatedlogs().journal("shamap"));
    }
}

void ledger::addraw (serializer& s) const
{
    s.add32 (mledgerseq);
    s.add64 (mtotcoins);
    s.add64 (mtotcoinsvbc);
    s.add256 (mparenthash);
    s.add256 (mtranshash);
    s.add256 (maccounthash);
    s.add32 (mparentclosetime);
    s.add32 (mclosetime);
    s.add32 (mdividendledger);
    s.add8 (mcloseresolution);
    s.add8 (mcloseflags);
}

void ledger::setaccepted (
    std::uint32_t closetime, int closeresolution, bool correctclosetime)
{
    // used when we witnessed the consensus.  rounds the close time, updates the
    // hash, and sets the ledger accepted and immutable.
    assert (mclosed && !maccepted);
    mclosetime = correctclosetime ? roundclosetime (closetime, closeresolution)
            : closetime;
    mcloseresolution = closeresolution;
    mcloseflags = correctclosetime ? 0 : slcf_noconsensustime;
    maccepted = true;
    setimmutable ();
}

void ledger::setaccepted ()
{
    // used when we acquired the ledger
    // fixme assert(mclosed && (mclosetime != 0) && (mcloseresolution != 0));
    if ((mcloseflags & slcf_noconsensustime) == 0)
        mclosetime = roundclosetime (mclosetime, mcloseresolution);

    maccepted = true;
    setimmutable ();
}

bool ledger::hasaccount (rippleaddress const& accountid) const
{
    return maccountstatemap->hasitem (getaccountrootindex (accountid));
}

bool ledger::addsle (sle const& sle)
{
    shamapitem item (sle.getindex(), sle.getserializer());
    return maccountstatemap->additem(item, false, false);
}

accountstate::pointer ledger::getaccountstate (rippleaddress const& accountid) const
{
    sle::pointer sle = getslei(getaccountrootindex(accountid));
    sle::pointer slerefer = getreferobject(accountid.getaccountid());
    if (!sle)
    {
        writelog (lsdebug, ledger) << "ledger:getaccountstate:" <<
            " not found: " << accountid.humanaccountid () <<
            ": " << to_string (getaccountrootindex (accountid));

        return accountstate::pointer ();
    }

    if (sle->gettype () != ltaccount_root)
        return accountstate::pointer ();

    return std::make_shared<accountstate> (sle, accountid, slerefer);
}

bool ledger::addtransaction (uint256 const& txid, const serializer& txn)
{
    // low-level - just add to table
    auto item = std::make_shared<shamapitem> (txid, txn.peekdata ());

    if (!mtransactionmap->addgiveitem (item, true, false))
    {
        writelog (lswarning, ledger)
                << "attempt to add transaction to ledger that already had it";
        return false;
    }

    mvalidhash = false;
    return true;
}

bool ledger::addtransaction (
    uint256 const& txid, const serializer& txn, const serializer& md)
{
    // low-level - just add to table
    serializer s (txn.getdatalength () + md.getdatalength () + 16);
    s.addvl (txn.peekdata ());
    s.addvl (md.peekdata ());
    auto item = std::make_shared<shamapitem> (txid, s.peekdata ());

    if (!mtransactionmap->addgiveitem (item, true, true))
    {
        writelog (lsfatal, ledger)
                << "attempt to add transaction+md to ledger that already had it";
        return false;
    }

    mvalidhash = false;
    return true;
}

transaction::pointer ledger::gettransaction (uint256 const& transid) const
{
    shamaptreenode::tntype type;
    shamapitem::pointer item = mtransactionmap->peekitem (transid, type);

    if (!item)
        return transaction::pointer ();

    auto txn = getapp().getmastertransaction ().fetch (transid, false);

    if (txn)
        return txn;

    if (type == shamaptreenode::tntransaction_nm)
        txn = transaction::sharedtransaction (item->peekdata (), validate::yes);
    else if (type == shamaptreenode::tntransaction_md)
    {
        blob txndata;
        int txnlength;

        if (!item->peekserializer ().getvl (txndata, 0, txnlength))
            return transaction::pointer ();

        txn = transaction::sharedtransaction (txndata, validate::no);
    }
    else
    {
        assert (false);
        return transaction::pointer ();
    }

    if (txn->getstatus () == new)
        txn->setstatus (mclosed ? committed : included, mledgerseq);

    getapp().getmastertransaction ().canonicalize (&txn);
    return txn;
}

sttx::pointer ledger::getstransaction (
    shamapitem::ref item, shamaptreenode::tntype type)
{
    serializeriterator sit (item->peekserializer ());

    if (type == shamaptreenode::tntransaction_nm)
        return std::make_shared<sttx> (sit);

    if (type == shamaptreenode::tntransaction_md)
    {
        serializer stxn (sit.getvl ());
        serializeriterator tsit (stxn);
        return std::make_shared<sttx> (tsit);
    }

    return sttx::pointer ();
}

sttx::pointer ledger::getsmtransaction (
    shamapitem::ref item, shamaptreenode::tntype type,
    transactionmetaset::pointer& txmeta) const
{
    serializeriterator sit (item->peekserializer ());

    if (type == shamaptreenode::tntransaction_nm)
    {
        txmeta.reset ();
        return std::make_shared<sttx> (sit);
    }
    else if (type == shamaptreenode::tntransaction_md)
    {
        serializer stxn (sit.getvl ());
        serializeriterator tsit (stxn);

        txmeta = std::make_shared<transactionmetaset> (
            item->gettag (), mledgerseq, sit.getvl ());
        return std::make_shared<sttx> (tsit);
    }

    txmeta.reset ();
    return sttx::pointer ();
}

bool ledger::gettransaction (
    uint256 const& txid, transaction::pointer& txn,
    transactionmetaset::pointer& meta) const
{
    shamaptreenode::tntype type;
    shamapitem::pointer item = mtransactionmap->peekitem (txid, type);

    if (!item)
        return false;

    if (type == shamaptreenode::tntransaction_nm)
    {
        // in tree with no metadata
        txn = getapp().getmastertransaction ().fetch (txid, false);
        meta.reset ();

        if (!txn)
        {
            txn = transaction::sharedtransaction (
                item->peekdata (), validate::yes);
        }
    }
    else if (type == shamaptreenode::tntransaction_md)
    {
        // in tree with metadata
        serializeriterator it (item->peekserializer ());
        txn = getapp().getmastertransaction ().fetch (txid, false);

        if (!txn)
            txn = transaction::sharedtransaction (it.getvl (), validate::yes);
        else
            it.getvl (); // skip transaction

        meta = std::make_shared<transactionmetaset> (
            txid, mledgerseq, it.getvl ());
    }
    else
        return false;

    if (txn->getstatus () == new)
        txn->setstatus (mclosed ? committed : included, mledgerseq);

    getapp().getmastertransaction ().canonicalize (&txn);
    return true;
}

bool ledger::gettransactionmeta (
    uint256 const& txid, transactionmetaset::pointer& meta) const
{
    shamaptreenode::tntype type;
    shamapitem::pointer item = mtransactionmap->peekitem (txid, type);

    if (!item)
        return false;

    if (type != shamaptreenode::tntransaction_md)
        return false;

    serializeriterator it (item->peekserializer ());
    it.getvl (); // skip transaction
    meta = std::make_shared<transactionmetaset> (txid, mledgerseq, it.getvl ());

    return true;
}

bool ledger::getmetahex (uint256 const& transid, std::string& hex) const
{
    shamaptreenode::tntype type;
    shamapitem::pointer item = mtransactionmap->peekitem (transid, type);

    if (!item)
        return false;

    if (type != shamaptreenode::tntransaction_md)
        return false;

    serializeriterator it (item->peekserializer ());
    it.getvl (); // skip transaction
    hex = strhex (it.getvl ());
    return true;
}

uint256 ledger::gethash ()
{
    if (!mvalidhash)
        updatehash ();

    return mhash;
}

bool ledger::savevalidatedledger (bool current)
{
    // todo(tom): fix this hard-coded sql!
    writelog (lstrace, ledger)
        << "savevalidatedledger "
        << (current ? "" : "fromacquire ") << getledgerseq ();
    static boost::format deleteledger (
        "delete from ledgers where ledgerseq = %u;");
    static boost::format deletetrans1 (
        "delete from transactions where ledgerseq = %u;");
    static boost::format deletetrans2 (
        "delete from accounttransactions where ledgerseq = %u;");
    static boost::format deleteaccttrans (
        "delete from accounttransactions where transid = '%s';");
    static boost::format transexists (
        "select status from transactions where transid = '%s';");
    static boost::format updatetx (
        "update transactions set ledgerseq = %u, status = '%c', txnmeta = %s "
        "where transid = '%s';");
    static boost::format addledger (
        "insert or replace into ledgers "
        "(ledgerhash,ledgerseq,prevhash,totalcoins,totalcoinsvbc,closingtime,prevclosingtime,"
        "closetimeres,closeflags,dividendledger,accountsethash,transsethash) values "
        "('%s','%u','%s','%s','%s','%u','%u','%d','%u','%u','%s','%s');");

    if (!getaccounthash ().isnonzero ())
    {
        writelog (lsfatal, ledger) << "ah is zero: "
                                   << getjson (*this);
        assert (false);
    }

    if (getaccounthash () != maccountstatemap->gethash ())
    {
        writelog (lsfatal, ledger) << "sal: " << getaccounthash ()
                                   << " != " << maccountstatemap->gethash ();
        writelog (lsfatal, ledger) << "saveacceptedledger: seq="
                                   << mledgerseq << ", current=" << current;
        assert (false);
    }

    assert (gettranshash () == mtransactionmap->gethash ());

    // save the ledger header in the hashed object store
    {
        serializer s;
        s.add32 (hashprefix::ledgermaster);
        addraw (s);
        getapp().getnodestore ().store (
            hotledger, std::move (s.moddata ()), mhash);
    }

    acceptedledger::pointer aledger;
    try
    {
        if (getapp().gettxndb().getdb()->getdbtype()!=database::type::null)
            aledger = acceptedledger::makeacceptedledger (shared_from_this ());
    }
    catch (...)
    {
        writelog (lswarning, ledger) << "an accepted ledger was missing nodes";
        getapp().getledgermaster().failedsave(mledgerseq, mhash);
        {
            // clients can now trust the database for information about this
            // ledger sequence.
            staticscopedlocktype sl (spendingsavelock);
            spendingsaves.erase(getledgerseq());
        }
        return false;
    }

    {
        auto sl (getapp().getledgerdb ().lock ());
        getapp().getledgerdb ().getdb ()->executesql (
            boost::str (deleteledger % mledgerseq));
    }
    
    if (getapp().gettxndb().getdb()->getdbtype()!=database::type::null)
    {
        auto db = getapp().gettxndb ().getdb ();
        auto dblock (getapp().gettxndb ().lock ());
        db->batchstart();
        db->begintransaction();

        db->executesql (boost::str (deletetrans1 % getledgerseq ()));
        db->executesql (boost::str (deletetrans2 % getledgerseq ()));

        std::string const ledgerseq (std::to_string (getledgerseq ()));

        for (auto const& vt : aledger->getmap ())
        {
            uint256 transactionid = vt.second->gettransactionid ();

            getapp().getmastertransaction ().inledger (
                transactionid, getledgerseq ());

            std::string const txnid (to_string (transactionid));
            std::string const txnseq (std::to_string (vt.second->gettxnseq ()));

            db->executesql (boost::str (deleteaccttrans % transactionid));

            auto const& accts = vt.second->getaffected ();

            if (!accts.empty ())
            {
                std::string sql ("insert into accounttransactions "
                                 "(transid, account, ledgerseq, txnseq) values ");

                // try to make an educated guess on how much space we'll need
                // for our arguments. in argument order we have:
                // 64 + 34 + 10 + 10 = 118 + 10 extra = 128 bytes
                sql.reserve (sql.length () + (accts.size () * 128));

                bool first = true;
                for (auto const& it : accts)
                {
                    if (!first)
                        sql += ", ('";
                    else
                    {
                        sql += "('";
                        first = false;
                    }

                    sql += txnid;
                    sql += "','";
                    sql += it.humanaccountid ();
                    sql += "',";
                    sql += ledgerseq;
                    sql += ",";
                    sql += txnseq;
                    sql += ")";
                }
                sql += ";";
                if (shouldlog (lstrace, ledger))
                {
                    writelog (lstrace, ledger) << "acttx: " << sql;
                }
                db->executesql (sql);
            }
            else
                writelog (lswarning, ledger)
                    << "transaction in ledger " << mledgerseq
                    << " affects no accounts";

            db->executesql (
                sttx::getmetasqlinsertreplaceheader (db->getdbtype()) +
                vt.second->gettxn ()->getmetasql (
                    getledgerseq (), vt.second->getescmeta (), mclosetime) + ";");
        }
        db->endtransaction();
        db->batchcommit();
    }

    {
        auto sl (getapp().getledgerdb ().lock ());

        // todo(tom): arg!
        getapp().getledgerdb ().getdb ()->executesql (boost::str (addledger %
                to_string (gethash ()) % mledgerseq % to_string (mparenthash) %
                beast::lexicalcastthrow <std::string>(mtotcoins) % 
                beast::lexicalcastthrow <std::string>(mtotcoinsvbc) % mclosetime %
                mparentclosetime % mcloseresolution % mcloseflags % mdividendledger %
                to_string (maccounthash) % to_string (mtranshash)));
    }

    {
        // clients can now trust the database for information about this ledger
        // sequence.
        staticscopedlocktype sl (spendingsavelock);
        spendingsaves.erase(getledgerseq());
    }
    return true;
}

#ifndef no_sqlite3_prepare

ledger::pointer ledger::loadbyindex (std::uint32_t ledgerindex)
{
    ledger::pointer ledger;
    {
        auto db = getapp().getledgerdb ().getdb ();
        auto sl (getapp().getledgerdb ().lock ());

        sqlitestatement pst (
            db->getsqlitedb (), "select "
            "ledgerhash,prevhash,accountsethash,transsethash,totalcoins,totalcoinsvbc,"
            "closingtime,prevclosingtime,closetimeres,closeflags,dividendledger,ledgerseq"
            " from ledgers where ledgerseq = ?;");

        pst.bind (1, ledgerindex);
        ledger = getsql1 (&pst);
    }

    if (ledger)
    {
        ledger::getsql2 (ledger);
        ledger->setfull ();
    }

    return ledger;
}

ledger::pointer ledger::loadbyhash (uint256 const& ledgerhash)
{
    ledger::pointer ledger;
    {
        auto db = getapp().getledgerdb ().getdb ();
        auto sl (getapp().getledgerdb ().lock ());

        sqlitestatement pst (
            db->getsqlitedb (), "select "
            "ledgerhash,prevhash,accountsethash,transsethash,totalcoins,totalcoinsvbc,"
            "closingtime,prevclosingtime,closetimeres,closeflags,dividendledger,ledgerseq"
            " from ledgers where ledgerhash = ?;");

        pst.bind (1, to_string (ledgerhash));
        ledger = getsql1 (&pst);
    }

    if (ledger)
    {
        assert (ledger->gethash () == ledgerhash);
        ledger::getsql2 (ledger);
        ledger->setfull ();
    }

    return ledger;
}

#else

ledger::pointer ledger::loadbyindex (std::uint32_t ledgerindex)
{
    // this is a low-level function with no caching.
    std::string sql = "select * from ledgers where ledgerseq='";
    sql.append (beast::lexicalcastthrow <std::string> (ledgerindex));
    sql.append ("';");
    return getsql (sql);
}

ledger::pointer ledger::loadbyhash (uint256 const& ledgerhash)
{
    // this is a low-level function with no caching and only gets accepted
    // ledgers.
    std::string sql = "select * from ledgers where ledgerhash='";
    sql.append (to_string (ledgerhash));
    sql.append ("';");
    return getsql (sql);
}

#endif

ledger::pointer ledger::getsql (std::string const& sql)
{
    // only used with sqlite3 prepared statements not used
    uint256 ledgerhash, prevhash, accounthash, transhash;
    std::uint64_t totcoins, totcoinsvbc;
    std::uint32_t closingtime, prevclosingtime, ledgerseq;
    int closeresolution;
    unsigned closeflags;
    std::string hash;
    std::uint32_t dividendledger;

    {
        auto db = getapp().getledgerdb ().getdb ();
        auto sl (getapp().getledgerdb ().lock ());

        if (!db->executesql (sql) || !db->startiterrows ())
            return ledger::pointer ();

        db->getstr ("ledgerhash", hash);
        ledgerhash.sethexexact (hash);
        db->getstr ("prevhash", hash);
        prevhash.sethexexact (hash);
        db->getstr ("accountsethash", hash);
        accounthash.sethexexact (hash);
        db->getstr ("transsethash", hash);
        transhash.sethexexact (hash);
        totcoins = db->getbigint ("totalcoins");
        totcoinsvbc = db->getbigint("totalcoinsvbc");
        closingtime = db->getbigint ("closingtime");
        prevclosingtime = db->getbigint ("prevclosingtime");
        closeresolution = db->getbigint ("closetimeres");
        closeflags = db->getbigint ("closeflags");
        dividendledger = db->getbigint ("dividendledger");
        ledgerseq = db->getbigint ("ledgerseq");
        db->enditerrows ();
    }

    // caution: code below appears in two places
    bool loaded;
    ledger::pointer ret (new ledger (
        prevhash, transhash, accounthash, totcoins, totcoinsvbc, closingtime,
        prevclosingtime, closeflags, closeresolution, dividendledger, ledgerseq, loaded));

    if (!loaded)
        return ledger::pointer ();

    ret->setclosed ();

    if (getapp().getops ().haveledger (ledgerseq))
    {
        ret->setaccepted ();
        ret->setvalidated ();
    }

    if (ret->gethash () != ledgerhash)
    {
        if (shouldlog (lserror, ledger))
        {
            writelog (lserror, ledger) << "failed on ledger";
            json::value p;
            addjson (p, {*ret, ledger_json_full});
            writelog (lserror, ledger) << p;
        }

        assert (false);
        return ledger::pointer ();
    }

    writelog (lstrace, ledger) << "loaded ledger: " << ledgerhash;
    return ret;
}

ledger::pointer ledger::getsql1 (sqlitestatement* stmt)
{
    int iret = stmt->step ();

    if (stmt->isdone (iret))
        return ledger::pointer ();

    if (!stmt->isrow (iret))
    {
        writelog (lsinfo, ledger)
                << "ledger not found: " << iret
                << " = " << stmt->geterror (iret);
        return ledger::pointer ();
    }

    uint256 ledgerhash, prevhash, accounthash, transhash;
    std::uint64_t totcoins, totcoinsvbc;
    std::uint32_t closingtime, prevclosingtime, ledgerseq;
    int closeresolution;
    unsigned closeflags;
    std::uint32_t dividendledger;

    ledgerhash.sethexexact (stmt->peekstring (0));
    prevhash.sethexexact (stmt->peekstring (1));
    accounthash.sethexexact (stmt->peekstring (2));
    transhash.sethexexact (stmt->peekstring (3));
    totcoins = stmt->getint64 (4);
    totcoinsvbc = stmt->getint64(5);
    closingtime = stmt->getuint32 (6);
    prevclosingtime = stmt->getuint32 (7);
    closeresolution = stmt->getuint32 (8);
    closeflags = stmt->getuint32 (9);
    dividendledger = stmt->getuint32 (10);
    ledgerseq = stmt->getuint32 (11);

    // caution: code below appears in two places
    bool loaded;
    ledger::pointer ret (new ledger (
        prevhash, transhash, accounthash, totcoins, totcoinsvbc, closingtime,
        prevclosingtime, closeflags, closeresolution, dividendledger, ledgerseq, loaded));

    if (!loaded)
        return ledger::pointer ();

    return ret;
}

void ledger::getsql2 (ledger::ref ret)
{
    ret->setclosed ();
    ret->setimmutable ();

    if (getapp().getops ().haveledger (ret->getledgerseq ()))
        ret->setaccepted ();

    writelog (lstrace, ledger)
            << "loaded ledger: " << to_string (ret->gethash ());
}

uint256 ledger::gethashbyindex (std::uint32_t ledgerindex)
{
    uint256 ret;

    std::string sql =
        "select ledgerhash from ledgers indexed by seqledger where ledgerseq='";
    sql.append (beast::lexicalcastthrow <std::string> (ledgerindex));
    sql.append ("';");

    std::string hash;
    {
        auto db = getapp().getledgerdb ().getdb ();
        auto sl (getapp().getledgerdb ().lock ());

        if (!db->executesql (sql) || !db->startiterrows ())
            return ret;

        db->getstr ("ledgerhash", hash);
        db->enditerrows ();
    }

    ret.sethexexact (hash);
    return ret;
}

bool ledger::gethashesbyindex (
    std::uint32_t ledgerindex, uint256& ledgerhash, uint256& parenthash)
{
#ifndef no_sqlite3_prepare

    auto& con = getapp().getledgerdb ();
    auto sl (con.lock ());

    sqlitestatement pst (con.getdb ()->getsqlitedb (),
                         "select ledgerhash,prevhash from ledgers "
                         "indexed by seqledger where ledgerseq = ?;");

    pst.bind (1, ledgerindex);

    int ret = pst.step ();

    if (pst.isdone (ret))
    {
        writelog (lstrace, ledger) << "don't have ledger " << ledgerindex;
        return false;
    }

    if (!pst.isrow (ret))
    {
        assert (false);
        writelog (lsfatal, ledger) << "unexpected statement result " << ret;
        return false;
    }

    ledgerhash.sethexexact (pst.peekstring (0));
    parenthash.sethexexact (pst.peekstring (1));

    return true;

#else

    std::string sql =
            "select ledgerhash,prevhash from ledgers where ledgerseq='";
    sql.append (beast::lexicalcastthrow <std::string> (ledgerindex));
    sql.append ("';");

    std::string hash, prevhash;
    {
        auto db = getapp().getledgerdb ().getdb ();
        auto sl (getapp().getledgerdb ().lock ());

        if (!db->executesql (sql) || !db->startiterrows ())
            return false;

        db->getstr ("ledgerhash", hash);
        db->getstr ("prevhash", prevhash);
        db->enditerrows ();
    }

    ledgerhash.sethexexact (hash);
    parenthash.sethexexact (prevhash);

    assert (ledgerhash.isnonzero () &&
            (ledgerindex == 0 || parenthash.isnonzero ());

    return true;

#endif
}

std::map< std::uint32_t, std::pair<uint256, uint256> >
ledger::gethashesbyindex (std::uint32_t minseq, std::uint32_t maxseq)
{
    std::map< std::uint32_t, std::pair<uint256, uint256> > ret;

    std::string sql =
        "select ledgerseq,ledgerhash,prevhash from ledgers where ledgerseq >= ";
    sql.append (beast::lexicalcastthrow <std::string> (minseq));
    sql.append (" and ledgerseq <= ");
    sql.append (beast::lexicalcastthrow <std::string> (maxseq));
    sql.append (";");

    auto& con = getapp().getledgerdb ();
    auto sl (con.lock ());

    sqlitestatement pst (con.getdb ()->getsqlitedb (), sql);

    while (pst.isrow (pst.step ()))
    {
        std::pair<uint256, uint256>& hashes = ret[pst.getuint32 (0)];
        hashes.first.sethexexact (pst.peekstring (1));
        hashes.second.sethexexact (pst.peekstring (2));
    }

    return ret;
}

ledger::pointer ledger::getlastfullledger ()
{
    try
    {
        return getsql ("select * from ledgers order by ledgerseq desc limit 1;");
    }
    catch (shamapmissingnode& sn)
    {
        writelog (lswarning, ledger)
                << "database contains ledger with missing nodes: " << sn;
        return ledger::pointer ();
    }
}

void ledger::setacquiring (void)
{
    if (!mtransactionmap || !maccountstatemap)
        throw std::runtime_error ("invalid map");

    mtransactionmap->setsynching ();
    maccountstatemap->setsynching ();
}

bool ledger::isacquiring (void) const
{
    return isacquiringtx () || isacquiringas ();
}

bool ledger::isacquiringtx (void) const
{
    return mtransactionmap->issynching ();
}

bool ledger::isacquiringas (void) const
{
    return maccountstatemap->issynching ();
}

boost::posix_time::ptime ledger::getclosetime () const
{
    return ptfromseconds (mclosetime);
}

void ledger::setclosetime (boost::posix_time::ptime ptm)
{
    assert (!mimmutable);
    mclosetime = itoseconds (ptm);
}

ledgerstateparms ledger::writeback (ledgerstateparms parms, sle::ref entry)
{
    bool create = false;

    if (!maccountstatemap->hasitem (entry->getindex ()))
    {
        if ((parms & lepcreate) == 0)
        {
            writelog (lserror, ledger)
                << "writeback non-existent node without create";
            return lepmissing;
        }

        create = true;
    }

    auto item = std::make_shared<shamapitem> (entry->getindex ());
    entry->add (item->peekserializer ());

    if (create)
    {
        assert (!maccountstatemap->hasitem (entry->getindex ()));

        if (!maccountstatemap->addgiveitem (item, false, false))
        {
            assert (false);
            return leperror;
        }

        return lepcreated;
    }

    if (!maccountstatemap->updategiveitem (item, false, false))
    {
        assert (false);
        return leperror;
    }

    return lepokay;
}

sle::pointer ledger::getsle (uint256 const& uhash) const
{
    shamapitem::pointer node = maccountstatemap->peekitem (uhash);

    if (!node)
        return sle::pointer ();

    return std::make_shared<sle> (node->peekserializer (), node->gettag ());
}

sle::pointer ledger::getslei (uint256 const& uid) const
{
    uint256 hash;

    shamapitem::pointer node = maccountstatemap->peekitem (uid, hash);

    if (!node)
        return sle::pointer ();

    sle::pointer ret = getapp().getslecache ().fetch (hash);

    if (!ret)
    {
        ret = std::make_shared<sle> (node->peekserializer (), node->gettag ());
        ret->setimmutable ();
        getapp().getslecache ().canonicalize (hash, ret);
    }

    return ret;
}

void ledger::visitaccountitems (
    account const& accountid, std::function<void (sle::ref)> func) const
{
    // visit each item in this account's owner directory
    uint256 rootindex       = getownerdirindex (accountid);
    uint256 currentindex    = rootindex;

    while (1)
    {
        sle::pointer ownerdir   = getslei (currentindex);

        if (!ownerdir || (ownerdir->gettype () != ltdir_node))
            return;

        for (auto const& node : ownerdir->getfieldv256 (sfindexes).peekvalue ())
        {
            func (getslei (node));
        }

        std::uint64_t unodenext = ownerdir->getfieldu64 (sfindexnext);

        if (!unodenext)
            return;

        currentindex = getdirnodeindex (rootindex, unodenext);
    }

}

bool ledger::visitaccountitems (
    account const& accountid,
    uint256 const& startafter,
    std::uint64_t const hint,
    unsigned int limit,
    std::function <bool (sle::ref)> func) const
{
    // visit each item in this account's owner directory
    uint256 const rootindex (getownerdirindex (accountid));
    uint256 currentindex (rootindex);

    // if startafter is not zero try jumping to that page using the hint
    if (startafter.isnonzero ())
    {
        uint256 const hintindex (getdirnodeindex (rootindex, hint));
        sle::pointer hintdir (getslei (hintindex));
        if (hintdir != nullptr)
        {
            for (auto const& node : hintdir->getfieldv256 (sfindexes))
            {
                if (node == startafter)
                {
                    // we found the hint, we can start here
                    currentindex = hintindex;
                    break;
                }
            }
        }

        bool found (false);
        for (;;)
        {
            sle::pointer ownerdir (getslei (currentindex));

            if (! ownerdir || ownerdir->gettype () != ltdir_node)
                return found;

            for (auto const& node : ownerdir->getfieldv256 (sfindexes))
            {
                if (!found)
                {
                    if (node == startafter)
                        found = true;
                }
                else
                {
                    auto sle = getslei (node);
                    if (!sle)
                    {
                        writelog (lswarning, ledger) << "bad accout item " << node << " for " << accountid;
                    }
                    else if (func (getslei (node)) && limit-- <= 1)
                    {
                        return found;
                    }
                }
            }

            std::uint64_t const unodenext (ownerdir->getfieldu64 (sfindexnext));

            if (unodenext == 0)
                return found;

            currentindex = getdirnodeindex (rootindex, unodenext);
        }
    }
    else
    {
        for (;;)
        {
            sle::pointer ownerdir (getslei (currentindex));

            if (! ownerdir || ownerdir->gettype () != ltdir_node)
                return true;

            for (auto const& node : ownerdir->getfieldv256 (sfindexes))
            {
                auto sle = getslei (node);
                if (!sle)
                {
                    writelog (lswarning, ledger) << "bad accout item " << node << " for " << accountid;
                }
                else if (func (sle) && limit-- <= 1)
                    return true;
            }

            std::uint64_t const unodenext (ownerdir->getfieldu64 (sfindexnext));

            if (unodenext == 0)
                return true;

            currentindex = getdirnodeindex (rootindex, unodenext);
        }
    }
}

static void visithelper (
    std::function<void (sle::ref)>& function, shamapitem::ref item)
{
    function (std::make_shared<sle> (item->peekserializer (), item->gettag ()));
}

void ledger::visitstateitems (std::function<void (sle::ref)> function) const
{
    try
    {
        if (maccountstatemap)
        {
            maccountstatemap->visitleaves(
                std::bind(&visithelper, std::ref(function),
                          std::placeholders::_1));
        }
    }
    catch (shamapmissingnode&)
    {
        if (mhash.isnonzero ())
        {
            getapp().getinboundledgers().findcreate(
                mhash, mledgerseq, inboundledger::fcgeneric);
        }
        throw;
    }
}

uint256 ledger::getfirstledgerindex () const
{
    shamapitem::pointer node = maccountstatemap->peekfirstitem ();
    return node ? node->gettag () : uint256 ();
}

uint256 ledger::getlastledgerindex () const
{
    shamapitem::pointer node = maccountstatemap->peeklastitem ();
    return node ? node->gettag () : uint256 ();
}

uint256 ledger::getnextledgerindex (uint256 const& uhash) const
{
    shamapitem::pointer node = maccountstatemap->peeknextitem (uhash);
    return node ? node->gettag () : uint256 ();
}

uint256 ledger::getnextledgerindex (uint256 const& uhash, uint256 const& uend) const
{
    shamapitem::pointer node = maccountstatemap->peeknextitem (uhash);

    if ((!node) || (node->gettag () > uend))
        return uint256 ();

    return node->gettag ();
}

uint256 ledger::getprevledgerindex (uint256 const& uhash) const
{
    shamapitem::pointer node = maccountstatemap->peekprevitem (uhash);
    return node ? node->gettag () : uint256 ();
}

uint256 ledger::getprevledgerindex (uint256 const& uhash, uint256 const& ubegin) const
{
    shamapitem::pointer node = maccountstatemap->peeknextitem (uhash);

    if ((!node) || (node->gettag () < ubegin))
        return uint256 ();

    return node->gettag ();
}

sle::pointer ledger::getasnodei (uint256 const& nodeid, ledgerentrytype let) const
{
    sle::pointer node = getslei (nodeid);

    if (node && (node->gettype () != let))
        node.reset ();

    return node;
}

sle::pointer ledger::getasnode (
    ledgerstateparms& parms, uint256 const& nodeid, ledgerentrytype let) const
{
    shamapitem::pointer account = maccountstatemap->peekitem (nodeid);

    if (!account)
    {
        if ( (parms & lepcreate) == 0 )
        {
            parms = lepmissing;

            return sle::pointer ();
        }

        parms = parms | lepcreated | lepokay;
        sle::pointer sle = std::make_shared<sle> (let, nodeid);

        return sle;
    }

    sle::pointer sle =
        std::make_shared<sle> (account->peekserializer (), nodeid);

    if (sle->gettype () != let)
    {
        // maybe it's a currency or something
        parms = parms | lepwrongtype;
        return sle::pointer ();
    }

    parms = parms | lepokay;

    return sle;
}

sle::pointer ledger::getaccountroot (account const& accountid) const
{
    return getasnodei (getaccountrootindex (accountid), ltaccount_root);
}

sle::pointer ledger::getaccountroot (rippleaddress const& naaccountid) const
{
    return getasnodei (getaccountrootindex (
        naaccountid.getaccountid ()), ltaccount_root);
}

sle::pointer ledger::getdirnode (uint256 const& unodeindex) const
{
    return getasnodei (unodeindex, ltdir_node);
}
            
sle::pointer ledger::getreferobject(const account& account) const
{
    auto referindex = getaccountreferindex(account);
    auto sle = getslei(referindex);
    return sle;
}

bool ledger::hasrefer (const account& account) const
{
    return maccountstatemap->hasitem (getaccountreferindex (account));
}

sle::pointer ledger::getdividendobject () const
{
    return getasnodei (getledgerdividendindex(), ltdividend);
}

uint64_t ledger::getdividendcoins() const
{
    auto sle = getasnodei(getledgerdividendindex(), ltdividend);
    
    if (!sle || sle->getfieldindex(sfdividendcoins)==-1) return 0;
    
    return sle->getfieldu64(sfdividendcoins);
}

uint64_t ledger::getdividendcoinsvbc() const
{
    auto sle = getasnodei(getledgerdividendindex(), ltdividend);
    
    if (!sle || sle->getfieldindex(sfdividendcoinsvbc) == -1) return 0;
    
    return sle->getfieldu64(sfdividendcoinsvbc);
}

bool ledger::isdividendstarted() const
{
    auto sle = getasnodei(getledgerdividendindex(), ltdividend);
    
    if (!sle || sle->getfieldindex(sfdividendstate) == -1) return false;
    
    uint8_t type = sle->getfieldu8(sfdividendstate);
    return type == dividendmaster::divtype_start;
}

std::uint32_t ledger::getdividendbaseledger() const
{
    auto sle = getasnodei(getledgerdividendindex(), ltdividend);
    
    if (!sle || sle->getfieldindex(sfdividendledger) == -1)
    {
        return 0;
    }
    
    return sle->getfieldu32(sfdividendledger);
}

uint32_t ledger::getdividendtimenc() const
{
    auto sle = getasnodei(getledgerdividendindex(), ltdividend);
    
    if (!sle || sle->getfieldindex(sfdividendledger) == -1) return 0;
    
    uint32_t dividendledger = sle->getfieldu32(sfdividendledger);
    if (dividendledger==0) return 0;
    
    auto ledger=getapp().getledgermaster().getledgerbyseq(dividendledger);
    if (!ledger) return 0;
    
    return ledger->getclosetimenc();
}

sle::pointer ledger::getgenerator (account const& ugeneratorid) const
{
    return getasnodei (getgeneratorindex (ugeneratorid), ltgenerator_map);
}

sle::pointer
ledger::getoffer (uint256 const& uindex) const
{
    return getasnodei (uindex, ltoffer);
}

sle::pointer
ledger::getoffer (account const& account, std::uint32_t usequence) const
{
    return getoffer (getofferindex (account, usequence));
}

sle::pointer
ledger::getripplestate (uint256 const& unode) const
{
    return getasnodei (unode, ltripple_state);
}

sle::pointer
ledger::getripplestate (
    account const& a, account const& b, currency const& currency) const
{
    return getripplestate (getripplestateindex (a, b, currency));
}

uint256 ledger::getledgerhash (std::uint32_t ledgerindex)
{
    // return the hash of the specified ledger, 0 if not available

    // easy cases...
    if (ledgerindex > mledgerseq)
    {
        writelog (lswarning, ledger) << "can't get seq " << ledgerindex
                                     << " from " << mledgerseq << " future";
        return uint256 ();
    }

    if (ledgerindex == mledgerseq)
        return gethash ();

    if (ledgerindex == (mledgerseq - 1))
        return mparenthash;

    // within 256...
    int diff = mledgerseq - ledgerindex;

    if (diff <= 256)
    {
        auto hashindex = getslei (getledgerhashindex ());

        if (hashindex)
        {
            assert (hashindex->getfieldu32 (sflastledgersequence) ==
                    (mledgerseq - 1));
            stvector256 vec = hashindex->getfieldv256 (sfhashes);

            if (vec.size () >= diff)
                return vec[vec.size () - diff];

            writelog (lswarning, ledger)
                    << "ledger " << mledgerseq
                    << " missing hash for " << ledgerindex
                    << " (" << vec.size () << "," << diff << ")";
        }
        else
        {
            writelog (lswarning, ledger)
                    << "ledger " << mledgerseq
                    << ":" << gethash () << " missing normal list";
        }
    }

    if ((ledgerindex & 0xff) != 0)
    {
        writelog (lswarning, ledger) << "can't get seq " << ledgerindex
                                     << " from " << mledgerseq << " past";
        return uint256 ();
    }

    // in skiplist
    auto hashindex = getslei (getledgerhashindex (ledgerindex));

    if (hashindex)
    {
        int lastseq = hashindex->getfieldu32 (sflastledgersequence);
        assert (lastseq >= ledgerindex);
        assert ((lastseq & 0xff) == 0);
        int sdiff = (lastseq - ledgerindex) >> 8;

        stvector256 vec = hashindex->getfieldv256 (sfhashes);

        if (vec.size () > sdiff)
            return vec[vec.size () - sdiff - 1];
    }

    writelog (lswarning, ledger) << "can't get seq " << ledgerindex
                                 << " from " << mledgerseq << " error";
    return uint256 ();
}

ledger::ledgerhashes ledger::getledgerhashes () const
{
    ledgerhashes ret;
    sle::pointer hashindex = getslei (getledgerhashindex ());

    if (hashindex)
    {
        stvector256 vec = hashindex->getfieldv256 (sfhashes);
        int size = vec.size ();
        ret.reserve (size);
        auto seq = hashindex->getfieldu32 (sflastledgersequence) - size;

        for (int i = 0; i < size; ++i)
            ret.push_back (std::make_pair (++seq, vec[i]));
    }

    return ret;
}

std::vector<uint256> ledger::getledgeramendments () const
{
    std::vector<uint256> usamendments;
    sle::pointer sleamendments = getslei (getledgeramendmentindex ());

    if (sleamendments)
        usamendments = sleamendments->getfieldv256 (sfamendments).peekvalue ();

    return usamendments;
}

bool ledger::walkledger () const
{
    std::vector <shamapmissingnode> missingnodes1;
    std::vector <shamapmissingnode> missingnodes2;

    maccountstatemap->walkmap (missingnodes1, 32);

    if (shouldlog (lsinfo, ledger) && !missingnodes1.empty ())
    {
        writelog (lsinfo, ledger)
            << missingnodes1.size () << " missing account node(s)";
        writelog (lsinfo, ledger)
            << "first: " << missingnodes1[0];
    }

    mtransactionmap->walkmap (missingnodes2, 32);

    if (shouldlog (lsinfo, ledger) && !missingnodes2.empty ())
    {
        writelog (lsinfo, ledger)
            << missingnodes2.size () << " missing transaction node(s)";
        writelog (lsinfo, ledger)
            << "first: " << missingnodes2[0];
    }

    return missingnodes1.empty () && missingnodes2.empty ();
}

bool ledger::assertsane () const
{
    if (mhash.isnonzero () &&
            maccounthash.isnonzero () &&
            maccountstatemap &&
            mtransactionmap &&
            (maccounthash == maccountstatemap->gethash ()) &&
            (mtranshash == mtransactionmap->gethash ()))
    {
        return true;
    }

    writelog (lsfatal, ledger) << "ledger is not sane";

    json::value j = getjson (*this);

    j [jss::accounttreehash] = to_string (maccounthash);
    j [jss::transtreehash] = to_string (mtranshash);

    assert (false);

    return false;
}

// update the skip list with the information from our previous ledger
void ledger::updateskiplist ()
{
    if (mledgerseq == 0) // genesis ledger has no previous ledger
        return;

    std::uint32_t previndex = mledgerseq - 1;

    // update record of every 256th ledger
    if ((previndex & 0xff) == 0)
    {
        uint256 hash = getledgerhashindex (previndex);
        sle::pointer skiplist = getsle (hash);
        std::vector<uint256> hashes;

        // vfalco todo document this skip list concept
        if (!skiplist)
            skiplist = std::make_shared<sle> (ltledger_hashes, hash);
        else
            hashes = skiplist->getfieldv256 (sfhashes).peekvalue ();

        assert (hashes.size () <= 256);
        hashes.push_back (mparenthash);
        skiplist->setfieldv256 (sfhashes, stvector256 (hashes));
        skiplist->setfieldu32 (sflastledgersequence, previndex);

        if (writeback (lepcreate, skiplist) == leperror)
        {
            assert (false);
        }
    }

    // update record of past 256 ledger
    uint256 hash = getledgerhashindex ();

    sle::pointer skiplist = getsle (hash);

    std::vector <uint256> hashes;

    if (!skiplist)
    {
        skiplist = std::make_shared<sle> (ltledger_hashes, hash);
    }
    else
    {
        hashes = skiplist->getfieldv256 (sfhashes).peekvalue ();
    }

    assert (hashes.size () <= 256);

    if (hashes.size () == 256)
        hashes.erase (hashes.begin ());

    hashes.push_back (mparenthash);
    skiplist->setfieldv256 (sfhashes, stvector256 (hashes));
    skiplist->setfieldu32 (sflastledgersequence, previndex);

    if (writeback (lepcreate, skiplist) == leperror)
    {
        assert (false);
    }
}

std::uint32_t ledger::roundclosetime (
    std::uint32_t closetime, std::uint32_t closeresolution)
{
    if (closetime == 0)
        return 0;

    closetime += (closeresolution / 2);
    return closetime - (closetime % closeresolution);
}

/** save, or arrange to save, a fully-validated ledger
    returns false on error
*/
bool ledger::pendsavevalidated (bool issynchronous, bool iscurrent)
{
    if (!getapp().gethashrouter ().setflag (gethash (), sf_saved))
    {
        writelog (lsdebug, ledger) << "double pend save for " << getledgerseq();
        return true;
    }

    assert (isimmutable ());

    {
        staticscopedlocktype sl (spendingsavelock);
        if (!spendingsaves.insert(getledgerseq()).second)
        {
            writelog (lsdebug, ledger)
                << "pend save with seq in pending saves " << getledgerseq();
            return true;
        }
    }

    if (issynchronous)
    {
        return savevalidatedledger(iscurrent);
    }
    else if (iscurrent)
    {
        getapp().getjobqueue ().addjob (jtpubledger, "ledger::pendsave",
            std::bind (&ledger::savevalidatedledgerasync, shared_from_this (),
                       std::placeholders::_1, iscurrent));
    }
    else
    {
        getapp().getjobqueue ().addjob (jtpuboldledger, "ledger::pendoldsave",
            std::bind (&ledger::savevalidatedledgerasync, shared_from_this (),
                       std::placeholders::_1, iscurrent));
    }

    return true;
}

std::set<std::uint32_t> ledger::getpendingsaves()
{
   staticscopedlocktype sl (spendingsavelock);
   return spendingsaves;
}

void ledger::ownerdirdescriber (sle::ref sle, bool, account const& owner)
{
    sle->setfieldaccount (sfowner, owner);
}

void ledger::qualitydirdescriber (
    sle::ref sle, bool isnew,
    currency const& utakerpayscurrency, account const& utakerpaysissuer,
    currency const& utakergetscurrency, account const& utakergetsissuer,
    const std::uint64_t& urate)
{
    sle->setfieldh160 (sftakerpayscurrency, utakerpayscurrency);
    sle->setfieldh160 (sftakerpaysissuer, utakerpaysissuer);
    sle->setfieldh160 (sftakergetscurrency, utakergetscurrency);
    sle->setfieldh160 (sftakergetsissuer, utakergetsissuer);
    sle->setfieldu64 (sfexchangerate, urate);
    if (isnew)
    {
        getapp().getorderbookdb().addorderbook(
            {{utakerpayscurrency, utakerpaysissuer},
                {utakergetscurrency, utakergetsissuer}});
    }
}

void ledger::initializefees ()
{
    mbasefee = 0;
    mreferencefeeunits = 0;
    mreservebase = 0;
    mreserveincrement = 0;
}

void ledger::updatefees ()
{
    if (mbasefee)
        return;
    std::uint64_t basefee = getconfig ().fee_default;
    std::uint32_t referencefeeunits = getconfig ().transaction_fee_base;
    std::uint32_t reservebase = getconfig ().fee_account_reserve;
    std::int64_t reserveincrement = getconfig ().fee_owner_reserve;

    ledgerstateparms p = lepnone;
    auto sle = getasnode (p, getledgerfeeindex (), ltfee_settings);

    if (sle)
    {
        if (sle->getfieldindex (sfbasefee) != -1)
            basefee = sle->getfieldu64 (sfbasefee);

        if (sle->getfieldindex (sfreferencefeeunits) != -1)
            referencefeeunits = sle->getfieldu32 (sfreferencefeeunits);

        if (sle->getfieldindex (sfreservebase) != -1)
            reservebase = sle->getfieldu32 (sfreservebase);

        if (sle->getfieldindex (sfreserveincrement) != -1)
            reserveincrement = sle->getfieldu32 (sfreserveincrement);
    }

    {
        staticscopedlocktype sl (spendingsavelock);
        if (mbasefee == 0)
        {
            mbasefee = basefee;
            mreferencefeeunits = referencefeeunits;
            mreservebase = reservebase;
            mreserveincrement = reserveincrement;
        }
    }
}

void ledger::initializedividendledger()
{
    mdividendledger = 0;
}

std::uint64_t ledger::scalefeebase (std::uint64_t fee)
{
    // converts a fee in fee units to a fee in drops
    updatefees ();
    return getapp().getfeetrack ().scalefeebase (
        fee, mbasefee, mreferencefeeunits);
}

std::uint64_t ledger::scalefeeload (std::uint64_t fee, bool badmin)
{
    updatefees ();
    return getapp().getfeetrack ().scalefeeload (
        fee, mbasefee, mreferencefeeunits, badmin);
}

std::vector<uint256> ledger::getneededtransactionhashes (
    int max, shamapsyncfilter* filter) const
{
    std::vector<uint256> ret;

    if (mtranshash.isnonzero ())
    {
        if (mtransactionmap->gethash ().iszero ())
            ret.push_back (mtranshash);
        else
            ret = mtransactionmap->getneededhashes (max, filter);
    }

    return ret;
}

std::vector<uint256> ledger::getneededaccountstatehashes (
    int max, shamapsyncfilter* filter) const
{
    std::vector<uint256> ret;

    if (maccounthash.isnonzero ())
    {
        if (maccountstatemap->gethash ().iszero ())
            ret.push_back (maccounthash);
        else
            ret = maccountstatemap->getneededhashes (max, filter);
    }

    return ret;
}

ledger::staticlocktype ledger::spendingsavelock;
std::set<std::uint32_t> ledger::spendingsaves;

} // ripple
