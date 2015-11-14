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
#include <ripple/net/rpccall.h>
#include <ripple/net/rpcerr.h>
#include <ripple/basics/log.h>
#include <ripple/core/config.h>
#include <ripple/json/json_reader.h>
#include <ripple/json/to_string.h>
#include <ripple/net/httpclient.h>
#include <ripple/protocol/jsonfields.h>
#include <ripple/protocol/errorcodes.h>
#include <ripple/protocol/systemparameters.h>
#include <ripple/server/serverhandler.h>
#include <beast/module/core/text/lexicalcast.h>
#include <boost/asio/streambuf.hpp>
#include <boost/regex.hpp>
#include <iostream>
#include <type_traits>

namespace ripple {

class rpcparser;

static inline bool isswitchchar (char c)
{
#ifdef __wxmsw__
    return c == '-' || c == '/';
#else
    return c == '-';
#endif
}

//
// http protocol
//
// this ain't apache.  we're just using http header for the length field
// and to be compatible with other json-rpc implementations.
//

std::string createhttppost (
    std::string const& strhost,
    std::string const& strpath,
    std::string const& strmsg,
    std::map<std::string, std::string> const& maprequestheaders)
{
    std::ostringstream s;

    // checkme this uses a different version than the replies below use. is
    //         this by design or an accident or should it be using
    //         buildinfo::getfullversionstring () as well?

    s << "post "
      << (strpath.empty () ? "/" : strpath)
      << " http/1.0\r\n"
      << "user-agent: " << systemname () << "-json-rpc/v1\r\n"
      << "host: " << strhost << "\r\n"
      << "content-type: application/json\r\n"
      << "content-length: " << strmsg.size () << "\r\n"
      << "accept: application/json\r\n";

    for (auto const& item : maprequestheaders)
        s << item.first << ": " << item.second << "\r\n";

    s << "\r\n" << strmsg;

    return s.str ();
}

class rpcparser
{
private:
    // todo new routine for parsing ledger parameters, other routines should standardize on this.
    static bool jvparseledger (json::value& jvrequest, std::string const& strledger)
    {
        if (strledger == "current" || strledger == "closed" || strledger == "validated")
        {
            jvrequest["ledger_index"]   = strledger;
        }
        else if (strledger.length () == 64)
        {
            // yyy could confirm this is a uint256.
            jvrequest["ledger_hash"]    = strledger;
        }
        else
        {
            jvrequest["ledger_index"]   = beast::lexicalcast <std::uint32_t> (strledger);
        }

        return true;
    }

    // build a object { "currency" : "xyz", "issuer" : "rxyx" }
    static json::value jvparsecurrencyissuer (std::string const& strcurrencyissuer)
    {
        static boost::regex recuriss ("\\`([[:alpha:]]{3})(?:/(.+))?\\'");

        boost::smatch   smmatch;

        if (boost::regex_match (strcurrencyissuer, smmatch, recuriss))
        {
            json::value jvresult (json::objectvalue);
            std::string strcurrency = smmatch[1];
            std::string strissuer   = smmatch[2];

            jvresult["currency"]    = strcurrency;

            if (strissuer.length ())
            {
                // could confirm issuer is a valid ripple address.
                jvresult["issuer"]      = strissuer;
            }

            return jvresult;
        }
        else
        {
            return rpc::make_param_error (std::string ("invalid currency/issuer '") +
                    strcurrencyissuer + "'");
        }
    }

private:
    typedef json::value (rpcparser::*parsefuncptr) (json::value const& jvparams);

    json::value parseasis (json::value const& jvparams)
    {
        json::value v (json::objectvalue);

        if (jvparams.isarray () && (jvparams.size () > 0))
            v["params"] = jvparams;

        return v;
    }

    json::value parseinternal (json::value const& jvparams)
    {
        json::value v (json::objectvalue);
        v["internal_command"] = jvparams[0u];

        json::value params (json::arrayvalue);

        for (unsigned i = 1; i < jvparams.size (); ++i)
            params.append (jvparams[i]);

        v["params"] = params;

        return v;
    }

    // fetch_info [clear]
    json::value parsefetchinfo (json::value const& jvparams)
    {
        json::value     jvrequest (json::objectvalue);
        unsigned int    iparams = jvparams.size ();

        if (iparams != 0)
            jvrequest[jvparams[0u].asstring()] = true;

        return jvrequest;
    }

    // account_tx accountid [ledger_min [ledger_max [limit [offset]]]] [binary] [count] [descending]
    json::value parseaccounttransactions (json::value const& jvparams)
    {
        json::value     jvrequest (json::objectvalue);
        rippleaddress   raaccount;
        unsigned int    iparams = jvparams.size ();

        if (!raaccount.setaccountid (jvparams[0u].asstring ()))
            return rpcerror (rpcact_malformed);

        jvrequest["account"]    = raaccount.humanaccountid ();

        bool            bdone   = false;

        while (!bdone && iparams >= 2)
        {
            if (jvparams[iparams - 1].asstring () == "binary")
            {
                jvrequest["binary"]     = true;
                --iparams;
            }
            else if (jvparams[iparams - 1].asstring () == "count")
            {
                jvrequest["count"]      = true;
                --iparams;
            }
            else if (jvparams[iparams - 1].asstring () == "descending")
            {
                jvrequest["descending"] = true;
                --iparams;
            }
            else if (jvparams[iparams - 1].asstring () == "dividend"
                     || jvparams[iparams - 1].asstring () == "payment"
                     || jvparams[iparams - 1].asstring () == "offercreate"
                     || jvparams[iparams - 1].asstring () == "offercancel"
                     )
            {
                jvrequest["tx_type"] = jvparams[iparams - 1].asstring ();
                --iparams;
            }
            else
            {
                bdone   = true;
            }
        }

        if (1 == iparams)
        {
        }
        else if (2 == iparams)
        {
            if (!jvparseledger (jvrequest, jvparams[1u].asstring ()))
                return jvrequest;
        }
        else
        {
            std::int64_t   uledgermin  = jvparams[1u].asint ();
            std::int64_t   uledgermax  = jvparams[2u].asint ();

            if (uledgermax != -1 && uledgermax < uledgermin)
            {
                return rpcerror (rpclgr_idxs_invalid);
            }

            jvrequest["ledger_index_min"]   = jvparams[1u].asint ();
            jvrequest["ledger_index_max"]   = jvparams[2u].asint ();

            if (iparams >= 4)
                jvrequest["limit"]  = jvparams[3u].asint ();

            if (iparams >= 5)
                jvrequest["offset"] = jvparams[4u].asint ();
        }

        return jvrequest;
    }

    // tx_account accountid [ledger_min [ledger_max [limit]]]] [binary] [count] [forward]
    json::value parsetxaccount (json::value const& jvparams)
    {
        json::value     jvrequest (json::objectvalue);
        rippleaddress   raaccount;
        unsigned int    iparams = jvparams.size ();

        if (!raaccount.setaccountid (jvparams[0u].asstring ()))
            return rpcerror (rpcact_malformed);

        jvrequest["account"]    = raaccount.humanaccountid ();

        bool            bdone   = false;

        while (!bdone && iparams >= 2)
        {
            if (jvparams[iparams - 1].asstring () == "binary")
            {
                jvrequest["binary"]     = true;
                --iparams;
            }
            else if (jvparams[iparams - 1].asstring () == "count")
            {
                jvrequest["count"]      = true;
                --iparams;
            }
            else if (jvparams[iparams - 1].asstring () == "forward")
            {
                jvrequest["forward"] = true;
                --iparams;
            }
            else
            {
                bdone   = true;
            }
        }

        if (1 == iparams)
        {
        }
        else if (2 == iparams)
        {
            if (!jvparseledger (jvrequest, jvparams[1u].asstring ()))
                return jvrequest;
        }
        else
        {
            std::int64_t   uledgermin  = jvparams[1u].asint ();
            std::int64_t   uledgermax  = jvparams[2u].asint ();

            if (uledgermax != -1 && uledgermax < uledgermin)
            {
                return rpcerror (rpclgr_idxs_invalid);
            }

            jvrequest["ledger_index_min"]   = jvparams[1u].asint ();
            jvrequest["ledger_index_max"]   = jvparams[2u].asint ();

            if (iparams >= 4)
                jvrequest["limit"]  = jvparams[3u].asint ();
        }

        return jvrequest;
    }

    // book_offers <taker_pays> <taker_gets> [<taker> [<ledger> [<limit> [<proof> [<marker>]]]]]
    // limit: 0 = no limit
    // proof: 0 or 1
    //
    // mnemonic: taker pays --> offer --> taker gets
    json::value parsebookoffers (json::value const& jvparams)
    {
        json::value     jvrequest (json::objectvalue);

        json::value     jvtakerpays = jvparsecurrencyissuer (jvparams[0u].asstring ());
        json::value     jvtakergets = jvparsecurrencyissuer (jvparams[1u].asstring ());

        if (isrpcerror (jvtakerpays))
        {
            return jvtakerpays;
        }
        else
        {
            jvrequest["taker_pays"] = jvtakerpays;
        }

        if (isrpcerror (jvtakergets))
        {
            return jvtakergets;
        }
        else
        {
            jvrequest["taker_gets"] = jvtakergets;
        }

        if (jvparams.size () >= 3)
        {
            jvrequest["issuer"] = jvparams[2u].asstring ();
        }

        if (jvparams.size () >= 4 && !jvparseledger (jvrequest, jvparams[3u].asstring ()))
            return jvrequest;

        if (jvparams.size () >= 5)
        {
            int     ilimit  = jvparams[5u].asint ();

            if (ilimit > 0)
                jvrequest["limit"]  = ilimit;
        }

        if (jvparams.size () >= 6 && jvparams[5u].asint ())
        {
            jvrequest["proof"]  = true;
        }

        if (jvparams.size () == 7)
            jvrequest["marker"] = jvparams[6u];

        return jvrequest;
    }

    // can_delete [<ledgerid>|<ledgerhash>|now|always|never]
    json::value parsecandelete (json::value const& jvparams)
    {
        json::value     jvrequest (json::objectvalue);

        if (!jvparams.size ())
            return jvrequest;

        std::string input = jvparams[0u].asstring();
        if (input.find_first_not_of("0123456789") ==
                std::string::npos)
            jvrequest["can_delete"] = jvparams[0u].asuint();
        else
            jvrequest["can_delete"] = input;

        return jvrequest;
    }

    // connect <ip> [port]
    json::value parseconnect (json::value const& jvparams)
    {
        json::value     jvrequest (json::objectvalue);

        jvrequest["ip"] = jvparams[0u].asstring ();

        if (jvparams.size () == 2)
            jvrequest["port"]   = jvparams[1u].asuint ();

        return jvrequest;
    }

    // return an error for attemping to subscribe/unsubscribe via rpc.
    json::value parseevented (json::value const& jvparams)
    {
        return rpcerror (rpcno_events);
    }

    // feature [<feature>] [true|false]
    json::value parsefeature (json::value const& jvparams)
    {
        json::value     jvrequest (json::objectvalue);

        if (jvparams.size () > 0)
            jvrequest["feature"]    = jvparams[0u].asstring ();

        if (jvparams.size () > 1)
            jvrequest["vote"]       = beast::lexicalcastthrow <bool> (jvparams[1u].asstring ());

        return jvrequest;
    }

    // get_counts [<min_count>]
    json::value parsegetcounts (json::value const& jvparams)
    {
        json::value     jvrequest (json::objectvalue);

        if (jvparams.size ())
            jvrequest["min_count"]  = jvparams[0u].asuint ();

        return jvrequest;
    }

    // json <command> <json>
    json::value parsejson (json::value const& jvparams)
    {
        json::reader    reader;
        json::value     jvrequest;

        writelog (lstrace, rpcparser) << "rpc method: " << jvparams[0u];
        writelog (lstrace, rpcparser) << "rpc json: " << jvparams[1u];

        if (reader.parse (jvparams[1u].asstring (), jvrequest))
        {
            if (!jvrequest.isobject ())
                return rpcerror (rpcinvalid_params);

            jvrequest["method"] = jvparams[0u];

            return jvrequest;
        }

        return rpcerror (rpcinvalid_params);
    }

    // ledger [id|index|current|closed|validated] [full]
    json::value parseledger (json::value const& jvparams)
    {
        json::value     jvrequest (json::objectvalue);

        if (!jvparams.size ())
        {
            return jvrequest;
        }

        jvparseledger (jvrequest, jvparams[0u].asstring ());

        if (2 == jvparams.size () && jvparams[1u].asstring () == "full")
        {
            jvrequest["full"]   = bool (1);
        }

        return jvrequest;
    }

    // ledger_header <id>|<index>
    json::value parseledgerid (json::value const& jvparams)
    {
        json::value     jvrequest (json::objectvalue);

        std::string     strledger   = jvparams[0u].asstring ();

        if (strledger.length () == 32)
        {
            jvrequest["ledger_hash"]    = strledger;
        }
        else
        {
            jvrequest["ledger_index"]   = beast::lexicalcast <std::uint32_t> (strledger);
        }

        return jvrequest;
    }
    
    //dividend_object <time>
    json::value parsedividendtime (json::value const& jvparams)
    {
        json::value jvrequest (json::objectvalue);
        if (jvparams.size() >= 1)
        {
            std::string strdividendtime = jvparams[0u].asstring ();
            jvrequest["until"]   = beast::lexicalcast <std::uint32_t> (strdividendtime);
        }
        return jvrequest;
    }

    //dividend_object <time>
    json::value parseaccountdividend (json::value const& jvparams)
    {
        json::value jvrequest (json::objectvalue);
        if (jvparams.size() >= 1)
        {
            jvrequest["account"]= jvparams[0u].asstring ();
        }
        return jvrequest;
    }

    //ancestors <account>
    json::value parseancestors (json::value const& jvparams)
    {
        json::value jvrequest (json::objectvalue);
        if (jvparams.size() >= 1)
        {
            jvrequest["account"]= jvparams[0u].asstring ();
        }
        return jvrequest;
    }

    // log_level:                           get log levels
    // log_level <severity>:                set master log level to the specified severity
    // log_level <partition> <severity>:    set specified partition to specified severity
    json::value parseloglevel (json::value const& jvparams)
    {
        json::value     jvrequest (json::objectvalue);

        if (jvparams.size () == 1)
        {
            jvrequest["severity"] = jvparams[0u].asstring ();
        }
        else if (jvparams.size () == 2)
        {
            jvrequest["partition"] = jvparams[0u].asstring ();
            jvrequest["severity"] = jvparams[1u].asstring ();
        }

        return jvrequest;
    }

    // owner_info <account>|<account_public_key>
    // owner_info <seed>|<pass_phrase>|<key> [<ledger>]
    // account_info <account>|<account_public_key>
    // account_info <seed>|<pass_phrase>|<key> [<ledger>]
    // account_offers <account>|<account_public_key> [<ledger>]
    json::value parseaccountitems (json::value const& jvparams)
    {
        return parseaccountraw (jvparams, false);
    }

    json::value parseaccountcurrencies (json::value const& jvparams)
    {
        return parseaccountraw (jvparams, false);
    }

    // account_lines <account> <account>|"" [<ledger>]
    json::value parseaccountlines (json::value const& jvparams)
    {
        return parseaccountraw (jvparams, true);
    }

    // todo: get index from an alternate syntax: rxyz:<index>
    json::value parseaccountraw (json::value const& jvparams, bool bpeer)
    {
        std::string     strident    = jvparams[0u].asstring ();
        unsigned int    icursor     = jvparams.size ();
        bool            bstrict     = false;
        std::string     strpeer;

        if (!bpeer && icursor >= 2 && jvparams[icursor - 1] == "strict")
        {
            bstrict = true;
            --icursor;
        }

        if (bpeer && icursor >= 2)
            strpeer = jvparams[icursor].asstring ();

        int             iindex      = 0;
        //  int             iindex      = jvparams.size() >= 2 ? beast::lexicalcast <int>(jvparams[1u].asstring()) : 0;

        rippleaddress   raaddress;

        if (!raaddress.setaccountpublic (strident) && !raaddress.setaccountid (strident) && !raaddress.setseedgeneric (strident))
            return rpcerror (rpcact_malformed);

        // get info on account.
        json::value jvrequest (json::objectvalue);

        jvrequest["account"]    = strident;

        if (bstrict)
            jvrequest["strict"]     = 1;

        if (iindex)
            jvrequest["account_index"]  = iindex;

        if (!strpeer.empty ())
        {
            rippleaddress   rapeer;

            if (!rapeer.setaccountpublic (strpeer) && !rapeer.setaccountid (strpeer) && !rapeer.setseedgeneric (strpeer))
                return rpcerror (rpcact_malformed);

            jvrequest["peer"]   = strpeer;
        }

        if (icursor == (2 + bpeer) && !jvparseledger (jvrequest, jvparams[1u + bpeer].asstring ()))
            return rpcerror (rpclgr_idx_malformed);

        return jvrequest;
    }

    // proof_create [<difficulty>] [<secret>]
    json::value parseproofcreate (json::value const& jvparams)
    {
        json::value     jvrequest;

        if (jvparams.size () >= 1)
            jvrequest["difficulty"] = jvparams[0u].asint ();

        if (jvparams.size () >= 2)
            jvrequest["secret"] = jvparams[1u].asstring ();

        return jvrequest;
    }

    // proof_solve <token>
    json::value parseproofsolve (json::value const& jvparams)
    {
        json::value     jvrequest;

        jvrequest["token"] = jvparams[0u].asstring ();

        return jvrequest;
    }

    // proof_verify <token> <solution> [<difficulty>] [<secret>]
    json::value parseproofverify (json::value const& jvparams)
    {
        json::value     jvrequest;

        jvrequest["token"] = jvparams[0u].asstring ();
        jvrequest["solution"] = jvparams[1u].asstring ();

        if (jvparams.size () >= 3)
            jvrequest["difficulty"] = jvparams[2u].asint ();

        if (jvparams.size () >= 4)
            jvrequest["secret"] = jvparams[3u].asstring ();

        return jvrequest;
    }

    // ripple_path_find <json> [<ledger>]
    json::value parseripplepathfind (json::value const& jvparams)
    {
        json::reader    reader;
        json::value     jvrequest;
        bool            bledger     = 2 == jvparams.size ();

        writelog (lstrace, rpcparser) << "rpc json: " << jvparams[0u];

        if (reader.parse (jvparams[0u].asstring (), jvrequest))
        {
            if (bledger)
            {
                jvparseledger (jvrequest, jvparams[1u].asstring ());
            }

            return jvrequest;
        }

        return rpcerror (rpcinvalid_params);
    }

    // sign/submit any transaction to the network
    //
    // sign <private_key> <json> offline
    // submit <private_key> <json>
    // submit <tx_blob>
    json::value parsesignsubmit (json::value const& jvparams)
    {
        json::value     txjson;
        json::reader    reader;
        bool            boffline    = 3 == jvparams.size () && jvparams[2u].asstring () == "offline";

        if (1 == jvparams.size ())
        {
            // submitting tx_blob

            json::value jvrequest;

            jvrequest["tx_blob"]    = jvparams[0u].asstring ();

            return jvrequest;
        }
        else if ((2 == jvparams.size () || boffline)
                 && reader.parse (jvparams[1u].asstring (), txjson))
        {
            // signing or submitting tx_json.
            json::value jvrequest;

            jvrequest["secret"]     = jvparams[0u].asstring ();
            jvrequest["tx_json"]    = txjson;

            if (boffline)
                jvrequest["offline"]    = true;

            return jvrequest;
        }

        return rpcerror (rpcinvalid_params);
    }

    // sms <text>
    json::value parsesms (json::value const& jvparams)
    {
        json::value     jvrequest;

        jvrequest["text"]   = jvparams[0u].asstring ();

        return jvrequest;
    }

    // tx <transaction_id>
    json::value parsetx (json::value const& jvparams)
    {
        json::value jvrequest;

        if (jvparams.size () > 1)
        {
            if (jvparams[1u].asstring () == "binary")
                jvrequest["binary"] = true;
        }

        jvrequest["transaction"]    = jvparams[0u].asstring ();
        return jvrequest;
    }

    // tx_history <index>
    json::value parsetxhistory (json::value const& jvparams)
    {
        json::value jvrequest;

        jvrequest["start"]  = jvparams[0u].asuint ();

        return jvrequest;
    }

    // unl_add <domain>|<node_public> [<comment>]
    json::value parseunladd (json::value const& jvparams)
    {
        std::string strnode     = jvparams[0u].asstring ();
        std::string strcomment  = (jvparams.size () == 2) ? jvparams[1u].asstring () : "";

        rippleaddress   nanodepublic;

        if (strnode.length ())
        {
            json::value jvrequest;

            jvrequest["node"]       = strnode;

            if (strcomment.length ())
                jvrequest["comment"]    = strcomment;

            return jvrequest;
        }

        return rpcerror (rpcinvalid_params);
    }

    // unl_delete <domain>|<public_key>
    json::value parseunldelete (json::value const& jvparams)
    {
        json::value jvrequest;

        jvrequest["node"]       = jvparams[0u].asstring ();

        return jvrequest;
    }

    // validation_create [<pass_phrase>|<seed>|<seed_key>]
    //
    // note: it is poor security to specify secret information on the command line.  this information might be saved in the command
    // shell history file (e.g. .bash_history) and it may be leaked via the process status command (i.e. ps).
    json::value parsevalidationcreate (json::value const& jvparams)
    {
        json::value jvrequest;

        if (jvparams.size ())
            jvrequest["secret"]     = jvparams[0u].asstring ();

        return jvrequest;
    }

    // validation_seed [<pass_phrase>|<seed>|<seed_key>]
    //
    // note: it is poor security to specify secret information on the command line.  this information might be saved in the command
    // shell history file (e.g. .bash_history) and it may be leaked via the process status command (i.e. ps).
    json::value parsevalidationseed (json::value const& jvparams)
    {
        json::value jvrequest;

        if (jvparams.size ())
            jvrequest["secret"]     = jvparams[0u].asstring ();

        return jvrequest;
    }

    // wallet_accounts <seed>
    json::value parsewalletaccounts (json::value const& jvparams)
    {
        json::value jvrequest;

        jvrequest["seed"]       = jvparams[0u].asstring ();

        return jvrequest;
    }

    // wallet_propose [<passphrase>]
    // <passphrase> is only for testing. master seeds should only be generated randomly.
    json::value parsewalletpropose (json::value const& jvparams)
    {
        json::value jvrequest;

        if (jvparams.size ())
            jvrequest["passphrase"]     = jvparams[0u].asstring ();

        return jvrequest;
    }

    // wallet_seed [<seed>|<passphrase>|<passkey>]
    json::value parsewalletseed (json::value const& jvparams)
    {
        json::value jvrequest;

        if (jvparams.size ())
            jvrequest["secret"]     = jvparams[0u].asstring ();

        return jvrequest;
    }

public:
    //--------------------------------------------------------------------------

    static std::string encodebase64 (std::string const& s)
    {
        // fixme: this performs terribly
        bio* b64, *bmem;
        buf_mem* bptr;

        // vfalco todo what the heck is bio and buf_mem!?
        //             surely we don't need openssl or dynamic allocations
        //             to perform a base64 encoding...
        //
        b64 = bio_new (bio_f_base64 ());
        bio_set_flags (b64, bio_flags_base64_no_nl);
        bmem = bio_new (bio_s_mem ());
        b64 = bio_push (b64, bmem);
        bio_write (b64, s.data (), s.size ());
        (void) bio_flush (b64);
        bio_get_mem_ptr (b64, &bptr);

        std::string result (bptr->data, bptr->length);
        bio_free_all (b64);

        return result;
    }

    //--------------------------------------------------------------------------

    // convert a rpc method and params to a request.
    // <-- { method: xyz, params: [... ] } or { error: ..., ... }
    json::value parsecommand (std::string strmethod, json::value jvparams, bool allowanycommand)
    {
        if (shouldlog (lstrace, rpcparser))
        {
            writelog (lstrace, rpcparser) << "rpc method:" << strmethod;
            writelog (lstrace, rpcparser) << "rpc params:" << jvparams;
        }

        struct command
        {
            const char*     name;
            parsefuncptr    parse;
            int             minparams;
            int             maxparams;
        };

        // fixme: replace this with a function-static std::map and the lookup
        // code with std::map::find when the problem with magic statics on
        // visual studio is fixed.
        static
        command const commands[] =
        {
            // request-response methods
            // - returns an error, or the request.
            // - to modify the method, provide a new method in the request.
            {   "account_currencies",   &rpcparser::parseaccountcurrencies,     1,  2   },
            {   "account_info",         &rpcparser::parseaccountitems,          1,  2   },
            {   "account_lines",        &rpcparser::parseaccountlines,          1,  5   },
            {   "account_offers",       &rpcparser::parseaccountitems,          1,  4   },
            {   "account_tx",           &rpcparser::parseaccounttransactions,   1,  8   },
            {   "book_offers",          &rpcparser::parsebookoffers,            2,  7   },
            {   "can_delete",           &rpcparser::parsecandelete,             0,  1   },
            {   "connect",              &rpcparser::parseconnect,               1,  2   },
            {   "consensus_info",       &rpcparser::parseasis,                  0,  0   },
            {   "feature",              &rpcparser::parsefeature,               0,  2   },
            {   "fetch_info",           &rpcparser::parsefetchinfo,             0,  1   },
            {   "get_counts",           &rpcparser::parsegetcounts,             0,  1   },
            {   "json",                 &rpcparser::parsejson,                  2,  2   },
            {   "ledger",               &rpcparser::parseledger,                0,  2   },
            {   "ledger_accept",        &rpcparser::parseasis,                  0,  0   },
            {   "ledger_closed",        &rpcparser::parseasis,                  0,  0   },
            {   "ledger_current",       &rpcparser::parseasis,                  0,  0   },
    //      {   "ledger_entry",         &rpcparser::parseledgerentry,          -1, -1   },
            {   "ledger_header",        &rpcparser::parseledgerid,              1,  1   },
            {   "ledger_request",       &rpcparser::parseledgerid,              1,  1   },
            {   "dividend_object",      &rpcparser::parsedividendtime,          0,  1   },
            {   "account_dividend",     &rpcparser::parseaccountdividend,          0,  1   },
            {   "ancestors",            &rpcparser::parseancestors,          0,  1   },
            {   "log_level",            &rpcparser::parseloglevel,              0,  2   },
            {   "logrotate",            &rpcparser::parseasis,                  0,  0   },
            {   "owner_info",           &rpcparser::parseaccountitems,          1,  2   },
            {   "peers",                &rpcparser::parseasis,                  0,  0   },
            {   "ping",                 &rpcparser::parseasis,                  0,  0   },
            {   "print",                &rpcparser::parseasis,                  0,  1   },
    //      {   "profile",              &rpcparser::parseprofile,               1,  9   },
            {   "proof_create",         &rpcparser::parseproofcreate,           0,  2   },
            {   "proof_solve",          &rpcparser::parseproofsolve,            1,  1   },
            {   "proof_verify",         &rpcparser::parseproofverify,           2,  4   },
            {   "random",               &rpcparser::parseasis,                  0,  0   },
            {   "ripple_path_find",     &rpcparser::parseripplepathfind,        1,  2   },
            {   "sign",                 &rpcparser::parsesignsubmit,            2,  3   },
            {   "sms",                  &rpcparser::parsesms,                   1,  1   },
            {   "submit",               &rpcparser::parsesignsubmit,            1,  3   },
            {   "server_info",          &rpcparser::parseasis,                  0,  0   },
            {   "server_state",         &rpcparser::parseasis,                  0,  0   },
            {   "stop",                 &rpcparser::parseasis,                  0,  0   },
    //      {   "transaction_entry",    &rpcparser::parsetransactionentry,     -1,  -1  },
            {   "tx",                   &rpcparser::parsetx,                    1,  2   },
            {   "tx_account",           &rpcparser::parsetxaccount,             1,  7   },
            {   "tx_history",           &rpcparser::parsetxhistory,             1,  1   },
            {   "unl_add",              &rpcparser::parseunladd,                1,  2   },
            {   "unl_delete",           &rpcparser::parseunldelete,             1,  1   },
            {   "unl_list",             &rpcparser::parseasis,                  0,  0   },
            {   "unl_load",             &rpcparser::parseasis,                  0,  0   },
            {   "unl_network",          &rpcparser::parseasis,                  0,  0   },
            {   "unl_reset",            &rpcparser::parseasis,                  0,  0   },
            {   "unl_score",            &rpcparser::parseasis,                  0,  0   },
            {   "validation_create",    &rpcparser::parsevalidationcreate,      0,  1   },
            {   "validation_seed",      &rpcparser::parsevalidationseed,        0,  1   },
            {   "wallet_accounts",      &rpcparser::parsewalletaccounts,        1,  1   },
            {   "wallet_propose",       &rpcparser::parsewalletpropose,         0,  1   },
            {   "wallet_seed",          &rpcparser::parsewalletseed,            0,  1   },
            {   "internal",             &rpcparser::parseinternal,              1,  -1  },

            // evented methods
            {   "path_find",            &rpcparser::parseevented,               -1, -1  },
            {   "subscribe",            &rpcparser::parseevented,               -1, -1  },
            {   "unsubscribe",          &rpcparser::parseevented,               -1, -1  },
        };

        auto const count = jvparams.size ();

        for (auto const& command : commands)
        {
            if (strmethod == command.name)
            {
                if ((command.minparams >= 0 && count < command.minparams) ||
                    (command.maxparams >= 0 && count > command.maxparams))
                {
                    writelog (lsdebug, rpcparser) <<
                        "wrong number of parameters for " << command.name <<
                        " minimum=" << command.minparams <<
                        " maximum=" << command.maxparams <<
                        " actual=" << count;

                    return rpcerror (rpcbad_syntax);
                }

                return (this->* (command.parse)) (jvparams);
            }
        }

        // the command could not be found
        if (!allowanycommand)
            return rpcerror (rpcunknown_command);

        return parseasis (jvparams);
    }
};

//------------------------------------------------------------------------------

//
// json-rpc protocol.  bitcoin speaks version 1.0 for maximum compatibility,
// but uses json-rpc 1.1/2.0 standards for parts of the 1.0 standard that were
// unspecified (http errors and contents of 'error').
//
// 1.0 spec: http://json-rpc.org/wiki/specification
// 1.2 spec: http://groups.google.com/group/json-rpc/web/json-rpc-over-http
//

std::string jsonrpcrequest (std::string const& strmethod, json::value const& params, json::value const& id)
{
    json::value request;
    request[jss::method] = strmethod;
    request[jss::params] = params;
    request[jss::id] = id;
    return to_string (request) + "\n";
}

struct rpccallimp
{
    // vfalco note is this a to-do comment or a doc comment?
    // place the async result somewhere useful.
    static void callrpchandler (json::value* jvoutput, json::value const& jvinput)
    {
        (*jvoutput) = jvinput;
    }

    static bool onresponse (
        std::function<void (json::value const& jvinput)> callbackfuncp,
            const boost::system::error_code& ecresult, int istatus,
                std::string const& strdata)
    {
        if (callbackfuncp)
        {
            // only care about the result, if we care to deliver it callbackfuncp.

            // receive reply
            if (istatus == 401)
                throw std::runtime_error ("incorrect rpcuser or rpcpassword (authorization failed)");
            else if ((istatus >= 400) && (istatus != 400) && (istatus != 404) && (istatus != 500)) // ?
                throw std::runtime_error (std::string ("server returned http error ") + std::to_string (istatus));
            else if (strdata.empty ())
                throw std::runtime_error ("no response from server");

            // parse reply
            writelog (lsdebug, rpcparser) << "rpc reply: " << strdata << std::endl;

            json::reader    reader;
            json::value     jvreply;

            if (!reader.parse (strdata, jvreply))
                throw std::runtime_error ("couldn't parse reply from server");

            if (jvreply.isnull ())
                throw std::runtime_error ("expected reply to have result, error and id properties");

            json::value     jvresult (json::objectvalue);

            jvresult["result"] = jvreply;

            (callbackfuncp) (jvresult);
        }

        return false;
    }

    // build the request.
    static void onrequest (std::string const& strmethod, json::value const& jvparams,
        const std::map<std::string, std::string>& mheaders, std::string const& strpath,
            boost::asio::streambuf& sb, std::string const& strhost)
    {
        writelog (lsdebug, rpcparser) << "requestrpc: strpath='" << strpath << "'";

        std::ostream    osrequest (&sb);
        osrequest <<
                  createhttppost (
                      strhost,
                      strpath,
                      jsonrpcrequest (strmethod, jvparams, json::value (1)),
                      mheaders);
    }
};

//------------------------------------------------------------------------------

int rpccall::fromcommandline (const std::vector<std::string>& vcmd)
{
    json::value jvoutput;
    int         nret = 0;
    json::value jvrequest (json::objectvalue);

    try
    {
        rpcparser   rpparser;
        json::value jvrpcparams (json::arrayvalue);

        if (vcmd.empty ()) return 1;                                            // 1 = print usage.

        for (int i = 1; i != vcmd.size (); i++)
            jvrpcparams.append (vcmd[i]);

        json::value jvrpc   = json::value (json::objectvalue);

        jvrpc["method"] = vcmd[0];
        jvrpc["params"] = jvrpcparams;

        jvrequest   = rpparser.parsecommand (vcmd[0], jvrpcparams, true);

        writelog (lstrace, rpcparser) << "rpc request: " << jvrequest << std::endl;

        if (jvrequest.ismember ("error"))
        {
            jvoutput            = jvrequest;
            jvoutput["rpc"]     = jvrpc;
        }
        else
        {
            auto const setup = setup_serverhandler(getconfig(), std::cerr);

            json::value jvparams (json::arrayvalue);

            if (!setup.client.admin_user.empty ())
                jvrequest["admin_user"] = setup.client.admin_user;

            if (!setup.client.admin_password.empty ())
                jvrequest["admin_password"] = setup.client.admin_password;

            jvparams.append (jvrequest);

            {
                boost::asio::io_service isservice;
                fromnetwork (
                    isservice,
                    setup.client.ip,
                    setup.client.port,
                    setup.client.user,
                    setup.client.password,
                    "",
                    jvrequest.ismember ("method")           // allow parser to rewrite method.
                        ? jvrequest["method"].asstring () : vcmd[0],
                    jvparams,                               // parsed, execute.
                    setup.client.secure != 0,                // use ssl
                    std::bind (rpccallimp::callrpchandler, &jvoutput,
                               std::placeholders::_1));
                isservice.run(); // this blocks until there is no more outstanding async calls.
            }

            if (jvoutput.ismember ("result"))
            {
                // had a successful json-rpc 2.0 call.
                jvoutput    = jvoutput["result"];

                // jvoutput may report a server side error.
                // it should report "status".
            }
            else
            {
                // transport error.
                json::value jvrpcerror  = jvoutput;

                jvoutput            = rpcerror (rpcjson_rpc);
                jvoutput["result"]  = jvrpcerror;
            }

            // if had an error, supply invokation in result.
            if (jvoutput.ismember ("error"))
            {
                jvoutput["rpc"]             = jvrpc;            // how the command was seen as method + params.
                jvoutput["request_sent"]    = jvrequest;        // how the command was translated.
            }
        }

        if (jvoutput.ismember ("error"))
        {
            jvoutput["status"]  = "error";

            nret    = jvoutput.ismember ("error_code")
                      ? beast::lexicalcast <int> (jvoutput["error_code"].asstring ())
                      : 1;
        }

        // yyy we could have a command line flag for single line output for scripts.
        // yyy we would intercept output here and simplify it.
    }
    catch (std::exception& e)
    {
        jvoutput                = rpcerror (rpcinternal);
        jvoutput["error_what"]  = e.what ();
        nret                    = rpcinternal;
    }
    catch (...)
    {
        jvoutput                = rpcerror (rpcinternal);
        jvoutput["error_what"]  = "exception";
        nret                    = rpcinternal;
    }

    std::cout << jvoutput.tostyledstring ();

    return nret;
}

//------------------------------------------------------------------------------

void rpccall::fromnetwork (
    boost::asio::io_service& io_service,
    std::string const& strip, const int iport,
    std::string const& strusername, std::string const& strpassword,
    std::string const& strpath, std::string const& strmethod,
    json::value const& jvparams, const bool bssl,
    std::function<void (json::value const& jvinput)> callbackfuncp)
{
    // connect to localhost
    if (!getconfig ().quiet)
    {
        std::cerr << (bssl ? "securely connecting to " : "connecting to ") <<
            strip << ":" << iport << std::endl;
    }

    // http basic authentication
    auto const auth = rpcparser::encodebase64 (strusername + ":" + strpassword);

    std::map<std::string, std::string> maprequestheaders;

    maprequestheaders["authorization"] = std::string ("basic ") + auth;

    // send request

    const int rpc_reply_max_bytes (256*1024*1024);
    const int rpc_notify_seconds (600);

    httpclient::request (
        bssl,
        io_service,
        strip,
        iport,
        std::bind (
            &rpccallimp::onrequest,
            strmethod,
            jvparams,
            maprequestheaders,
            strpath, std::placeholders::_1, std::placeholders::_2),
        rpc_reply_max_bytes,
        boost::posix_time::seconds (rpc_notify_seconds),
        std::bind (&rpccallimp::onresponse, callbackfuncp,
                   std::placeholders::_1, std::placeholders::_2,
                   std::placeholders::_3));
}

} // ripple
