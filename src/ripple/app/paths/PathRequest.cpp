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
#include <ripple/app/paths/accountcurrencies.h>
#include <ripple/app/paths/findpaths.h>
#include <ripple/app/paths/ripplecalc.h>
#include <ripple/app/paths/pathrequest.h>
#include <ripple/app/paths/pathrequests.h>
#include <ripple/app/main/application.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/basics/log.h>
#include <ripple/core/config.h>
#include <ripple/core/loadfeetrack.h>
#include <ripple/net/rpcerr.h>
#include <ripple/protocol/errorcodes.h>
#include <ripple/protocol/uinttypes.h>
#include <boost/log/trivial.hpp>
#include <tuple>

namespace ripple {

pathrequest::pathrequest (
    const std::shared_ptr<infosub>& subscriber,
    int id,
    pathrequests& owner,
    beast::journal journal)
        : m_journal (journal)
        , mowner (owner)
        , wpsubscriber (subscriber)
        , jvstatus (json::objectvalue)
        , bvalid (false)
        , mlastindex (0)
        , minprogress (false)
        , ilastlevel (0)
        , blastsuccess (false)
        , iidentifier (id)
{
    if (m_journal.debug)
        m_journal.debug << iidentifier << " created";
    ptcreated = boost::posix_time::microsec_clock::universal_time ();
}

static std::string const get_milli_diff (
    boost::posix_time::ptime const& after,
    boost::posix_time::ptime
    const& before)
{
    return beast::lexicalcastthrow <std::string> (
        static_cast <unsigned> ((after - before).total_milliseconds()));
}

static std::string const get_milli_diff (boost::posix_time::ptime const& before)
{
    return get_milli_diff(
        boost::posix_time::microsec_clock::universal_time(), before);
}

pathrequest::~pathrequest()
{
    std::string fast, full;
    if (!ptquickreply.is_not_a_date_time())
    {
        fast = " fast:";
        fast += get_milli_diff (ptquickreply, ptcreated);
        fast += "ms";
    }
    if (!ptfullreply.is_not_a_date_time())
    {
        full = " full:";
        full += get_milli_diff (ptfullreply, ptcreated);
        full += "ms";
    }
    if (m_journal.info)
        m_journal.info << iidentifier << " complete:" << fast << full <<
        " total:" << get_milli_diff(ptcreated) << "ms";
}

bool pathrequest::isvalid ()
{
    scopedlocktype sl (mlock);
    return bvalid;
}

bool pathrequest::isnew ()
{
    scopedlocktype sl (mindexlock);

    // does this path request still need its first full path
    return mlastindex == 0;
}

bool pathrequest::needsupdate (bool newonly, ledgerindex index)
{
    scopedlocktype sl (mindexlock);

    if (minprogress)
    {
        // another thread is handling this
        return false;
    }

    if (newonly && (mlastindex != 0))
    {
        // only handling new requests, this isn't new
        return false;
    }

    if (mlastindex >= index)
    {
        return false;
    }

    minprogress = true;
    return true;
}

void pathrequest::updatecomplete ()
{
    scopedlocktype sl (mindexlock);

    assert (minprogress);
    minprogress = false;
}

bool pathrequest::isvalid (ripplelinecache::ref crcache)
{
    scopedlocktype sl (mlock);
    bvalid = rasrcaccount.isset () && radstaccount.isset () &&
            sadstamount > zero;
    ledger::pointer lrledger = crcache->getledger ();

    if (bvalid)
    {
        auto assrc = getapp().getops ().getaccountstate (
            crcache->getledger(), rasrcaccount);

        if (!assrc)
        {
            // no source account
            bvalid = false;
            jvstatus = rpcerror (rpcsrc_act_not_found);
        }
        else
        {
            auto asdst = getapp().getops ().getaccountstate (
                lrledger, radstaccount);

            json::value& jvdestcur =
                    (jvstatus["destination_currencies"] = json::arrayvalue);

            if (!asdst)
            {
                // no destination account
                jvdestcur.append (json::value ("xrp"));

                if (!sadstamount.isnative ())
                {
                    // only xrp can be send to a non-existent account
                    bvalid = false;
                    jvstatus = rpcerror (rpcact_not_found);
                }
                else if (sadstamount < stamount (lrledger->getreserve (0)))
                {
                    // payment must meet reserve
                    bvalid = false;
                    jvstatus = rpcerror (rpcdst_amt_malformed);
                }
            }
            else
            {
                bool const disallowxrp (
                    asdst->peeksle ().getflags() & lsfdisallowxrp);

                auto usdestcurrid = accountdestcurrencies (
                        radstaccount, crcache, !disallowxrp);

                for (auto const& currency : usdestcurrid)
                    jvdestcur.append (to_string (currency));

                jvstatus["destination_tag"] =
                        (asdst->peeksle ().getflags () & lsfrequiredesttag)
                        != 0;
            }
        }
    }

    if (bvalid)
    {
        jvstatus["ledger_hash"] = to_string (lrledger->gethash ());
        jvstatus["ledger_index"] = lrledger->getledgerseq ();
    }
    return bvalid;
}

json::value pathrequest::docreate (
    ledger::ref lrledger,
    ripplelinecache::ref& cache,
    json::value const& value,
    bool& valid)
{

    json::value status;

    if (parsejson (value, true) != pfr_pj_invalid)
    {
        bvalid = isvalid (cache);

        if (bvalid)
            status = doupdate (cache, true);
        else
            status = jvstatus;
    }
    else
    {
        bvalid = false;
        status = jvstatus;
    }

    if (m_journal.debug)
    {
        if (bvalid)
        {
            m_journal.debug << iidentifier
                            << " valid: " << rasrcaccount.humanaccountid ();
            m_journal.debug << iidentifier
                            << " deliver: " << sadstamount.getfulltext ();
        }
        else
        {
            m_journal.debug << iidentifier << " invalid";
        }
    }

    valid = bvalid;
    return status;
}

int pathrequest::parsejson (json::value const& jvparams, bool complete)
{
    int ret = pfr_pj_nochange;

    if (jvparams.ismember ("source_account"))
    {
        if (!rasrcaccount.setaccountid (jvparams["source_account"].asstring ()))
        {
            jvstatus = rpcerror (rpcsrc_act_malformed);
            return pfr_pj_invalid;
        }
    }
    else if (complete)
    {
        jvstatus = rpcerror (rpcsrc_act_missing);
        return pfr_pj_invalid;
    }

    if (jvparams.ismember ("destination_account"))
    {
        if (!radstaccount.setaccountid (jvparams["destination_account"].asstring ()))
        {
            jvstatus = rpcerror (rpcdst_act_malformed);
            return pfr_pj_invalid;
        }
    }
    else if (complete)
    {
        jvstatus = rpcerror (rpcdst_act_missing);
        return pfr_pj_invalid;
    }

    if (jvparams.ismember ("destination_amount"))
    {
        if (! amountfromjsonnothrow (
                sadstamount, jvparams["destination_amount"]) ||
            (sadstamount.getcurrency ().iszero () &&
             sadstamount.getissuer ().isnonzero ()) ||
            (sadstamount.getcurrency () == badcurrency ()) ||
            sadstamount <= zero)
        {
            jvstatus = rpcerror (rpcdst_amt_malformed);
            return pfr_pj_invalid;
        }
    }
    else if (complete)
    {
        jvstatus = rpcerror (rpcdst_act_missing);
        return pfr_pj_invalid;
    }

    if (jvparams.ismember ("source_currencies"))
    {
        json::value const& jvsrccur = jvparams["source_currencies"];

        if (!jvsrccur.isarray ())
        {
            jvstatus = rpcerror (rpcsrc_cur_malformed);
            return pfr_pj_invalid;
        }

        scisourcecurrencies.clear ();

        for (unsigned i = 0; i < jvsrccur.size (); ++i)
        {
            json::value const& jvcur = jvsrccur[i];
            currency ucur;
            account uiss;

            if (!jvcur.isobject() || !jvcur.ismember ("currency") ||
                !to_currency (ucur, jvcur["currency"].asstring ()))
            {
                jvstatus = rpcerror (rpcsrc_cur_malformed);
                return pfr_pj_invalid;
            }

            if (jvcur.ismember ("issuer") &&
                !to_issuer (uiss, jvcur["issuer"].asstring ()))
            {
                jvstatus = rpcerror (rpcsrc_isr_malformed);
            }

            if (ucur.iszero () && uiss.isnonzero ())
            {
                jvstatus = rpcerror (rpcsrc_cur_malformed);
                return pfr_pj_invalid;
            }

            if (ucur.isnonzero() && uiss.iszero())
            {
                uiss = rasrcaccount.getaccountid();
            }

            scisourcecurrencies.insert ({ucur, uiss});
        }
    }

    if (jvparams.ismember ("id"))
        jvid = jvparams["id"];

    return ret;
}
json::value pathrequest::doclose (json::value const&)
{
    m_journal.debug << iidentifier << " closed";
    scopedlocktype sl (mlock);
    return jvstatus;
}

json::value pathrequest::dostatus (json::value const&)
{
    scopedlocktype sl (mlock);
    return jvstatus;
}

void pathrequest::resetlevel (int l)
{
    if (ilastlevel > l)
        ilastlevel = l;
}

json::value pathrequest::doupdate (ripplelinecache::ref cache, bool fast)
{
    m_journal.debug << iidentifier << " update " << (fast ? "fast" : "normal");

    scopedlocktype sl (mlock);

    if (!isvalid (cache))
        return jvstatus;
    jvstatus = json::objectvalue;

    auto sourcecurrencies = scisourcecurrencies;

    if (sourcecurrencies.empty ())
    {
        auto uscurrencies =
                accountsourcecurrencies (rasrcaccount, cache, true);
        bool sameaccount = rasrcaccount == radstaccount;
        for (auto const& c: uscurrencies)
        {
            if (!sameaccount || (c != sadstamount.getcurrency ()))
            {
                if (c.iszero ())
                    sourcecurrencies.insert ({c, xrpaccount()});
				else if (isvbc(c))
					sourcecurrencies.insert ({c, vbcaccount()});
                else
                    sourcecurrencies.insert ({c, rasrcaccount.getaccountid ()});
            }
        }
    }

    jvstatus["source_account"] = rasrcaccount.humanaccountid ();
    jvstatus["destination_account"] = radstaccount.humanaccountid ();
    jvstatus["destination_amount"] = sadstamount.getjson (0);

    if (!jvid.isnull ())
        jvstatus["id"] = jvid;

    json::value jvarray = json::arrayvalue;

    int ilevel = ilastlevel;
    bool loaded = getapp().getfeetrack().isloadedlocal();

    if (ilevel == 0)
    {
        // first pass
        if (loaded || fast)
            ilevel = getconfig().path_search_fast;
        else
            ilevel = getconfig().path_search;
    }
    else if ((ilevel == getconfig().path_search_fast) && !fast)
    {
        // leaving fast pathfinding
        ilevel = getconfig().path_search;
        if (loaded && (ilevel > getconfig().path_search_fast))
            --ilevel;
    }
    else if (blastsuccess)
    {
        // decrement, if possible
        if (ilevel > getconfig().path_search ||
            (loaded && (ilevel > getconfig().path_search_fast)))
            --ilevel;
    }
    else
    {
        // adjust as needed
        if (!loaded && (ilevel < getconfig().path_search_max))
            ++ilevel;
        if (loaded && (ilevel > getconfig().path_search_fast))
            --ilevel;
    }

    m_journal.debug << iidentifier << " processing at level " << ilevel;

    bool found = false;

    findpaths fp (
        cache,
        rasrcaccount.getaccountid (),
        radstaccount.getaccountid (),
        sadstamount,
        ilevel,
        4);  // imaxpaths
    for (auto const& currissuer: sourcecurrencies)
    {
        {
            stamount test (currissuer, 1);
            if (m_journal.debug)
            {
                m_journal.debug
                        << iidentifier
                        << " trying to find paths: " << test.getfulltext ();
            }
        }
        stpathset& spspaths = mcontext[currissuer];
        stpath fullliquiditypath;
        auto valid = fp.findpathsforissue (
            currissuer,
            spspaths,
            fullliquiditypath);
        condlog (!valid, lsdebug, pathrequest)
                << iidentifier << " pf request not valid";

        if (valid)
        {
            ledgerentryset lessandbox (cache->getledger (), tapnone);
            auto& sourceaccount = !isxrp (currissuer.account)
                    ? (!isvbc(currissuer.account)
                       ? currissuer.account
                       : isvbc(currissuer.currency)
                        ? vbcaccount()
                        : rasrcaccount.getaccountid())
                    : isxrp (currissuer.currency)
                        ? xrpaccount()
                        : rasrcaccount.getaccountid ();
            stamount samaxamount ({currissuer.currency, sourceaccount}, 1);

            samaxamount.negate ();
            m_journal.debug << iidentifier
                            << " paths found, calling ripplecalc";
            auto rc = path::ripplecalc::ripplecalculate (
                lessandbox,
                samaxamount,
                sadstamount,
                radstaccount.getaccountid (),
                rasrcaccount.getaccountid (),
                spspaths);

            if (!fullliquiditypath.empty() &&
                (rc.result () == terno_line || rc.result () == tecpath_partial))
            {
                m_journal.debug
                        << iidentifier << " trying with an extra path element";
                spspaths.push_back (fullliquiditypath);
                lessandbox.clear();
                rc = path::ripplecalc::ripplecalculate (
                    lessandbox,
                    samaxamount,
                    sadstamount,
                    radstaccount.getaccountid (),
                    rasrcaccount.getaccountid (),
                    spspaths);
                if (rc.result () != tessuccess)
                    m_journal.warning
                        << iidentifier << " failed with covering path "
                        << transhuman (rc.result ());
                else
                    m_journal.debug
                        << iidentifier << " extra path element gives "
                        << transhuman (rc.result ());
            }

            if (rc.result () == tessuccess)
            {
                json::value jventry (json::objectvalue);
                rc.actualamountin.setissuer (sourceaccount);

                jventry["source_amount"] = rc.actualamountin.getjson (0);
                jventry["paths_computed"] = spspaths.getjson (0);
                found  = true;
                jvarray.append (jventry);
            }
            else
            {
                m_journal.debug << iidentifier << " ripplecalc returns "
                    << transhuman (rc.result ());
            }
        }
        else
        {
            m_journal.debug << iidentifier << " no paths found";
        }
    }

    ilastlevel = ilevel;
    blastsuccess = found;

    if (fast && ptquickreply.is_not_a_date_time())
    {
        ptquickreply = boost::posix_time::microsec_clock::universal_time();
        mowner.reportfast ((ptquickreply-ptcreated).total_milliseconds());
    }
    else if (!fast && ptfullreply.is_not_a_date_time())
    {
        ptfullreply = boost::posix_time::microsec_clock::universal_time();
        mowner.reportfull ((ptfullreply-ptcreated).total_milliseconds());
    }

    jvstatus["alternatives"] = jvarray;
    return jvstatus;
}

infosub::pointer pathrequest::getsubscriber ()
{
    return wpsubscriber.lock ();
}

} // ripple
