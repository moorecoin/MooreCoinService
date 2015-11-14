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
#include <ripple/core/config.h>
#include <ripple/core/configsections.h>
#include <ripple/basics/log.h>
#include <ripple/json/json_reader.h>
#include <ripple/protocol/systemparameters.h>
#include <ripple/net/httpclient.h>
#include <beast/http/url.h>
#include <beast/module/core/text/lexicalcast.h>
#include <beast/streams/debug_ostream.h>
#include <boost/algorithm/string.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/regex.hpp>
#include <fstream>
#include <iostream>

#ifndef dump_config
#define dump_config 0
#endif

namespace ripple {

//
// todo: check permissions on config file before using it.
//

// fees are in xrp.
#define default_fee_default             1000
#define default_fee_account_reserve     0
#define default_fee_owner_reserve       0
#define default_fee_offer               default_fee_default
#define default_fee_operation           1

//fee config for moorecoin payment tx
#define default_fee_create              10000
#define default_fee_none_native         1000
#define default_fee_rate_native         0.001
#define default_fee_min_native          1000

// fee in fee units
#define default_transaction_fee_base    1000

#define default_asset_tx_min            5
#define default_asset_limit_default     10000000
#define default_asset_interval_min      86400

#define section_default_name    ""

inifilesections
parseinifile (std::string const& strinput, const bool btrim)
{
    std::string strdata (strinput);
    std::vector<std::string> vlines;
    inifilesections secresult;

    // convert dos format to unix.
    boost::algorithm::replace_all (strdata, "\r\n", "\n");

    // convert macos format to unix.
    boost::algorithm::replace_all (strdata, "\r", "\n");

    boost::algorithm::split (vlines, strdata,
        boost::algorithm::is_any_of ("\n"));

    // set the default section name.
    std::string strsection  = section_default_name;

    // initialize the default section.
    secresult[strsection]   = inifilesections::mapped_type ();

    // parse each line.
    boost_foreach (std::string & strvalue, vlines)
    {
        if (strvalue.empty () || strvalue[0] == '#')
        {
            // blank line or comment, do nothing.
        }
        else if (strvalue[0] == '[' && strvalue[strvalue.length () - 1] == ']')
        {
            // new section.
            strsection              = strvalue.substr (1, strvalue.length () - 2);
            secresult.emplace(strsection, inifilesections::mapped_type{});
        }
        else
        {
            // another line for section.
            if (btrim)
                boost::algorithm::trim (strvalue);

            if (!strvalue.empty ())
                secresult[strsection].push_back (strvalue);
        }
    }

    return secresult;
}

inifilesections::mapped_type*
getinifilesection (inifilesections& secsource, std::string const& strsection)
{
    inifilesections::iterator it;
    inifilesections::mapped_type* smtresult;
    it  = secsource.find (strsection);
    if (it == secsource.end ())
        smtresult   = 0;
    else
        smtresult   = & (it->second);
    return smtresult;
}

int
countsectionentries (inifilesections& secsource, std::string const& strsection)
{
    inifilesections::mapped_type* pmtentries =
        getinifilesection (secsource, strsection);

    return pmtentries ? pmtentries->size () : 0;
}

bool getsinglesection (inifilesections& secsource,
    std::string const& strsection, std::string& strvalue)
{
    inifilesections::mapped_type* pmtentries =
        getinifilesection (secsource, strsection);
    bool bsingle = pmtentries && 1 == pmtentries->size ();

    if (bsingle)
    {
        strvalue    = (*pmtentries)[0];
    }
    else if (pmtentries)
    {
        writelog (lswarning, parseinifile) << boost::str (boost::format ("section [%s]: requires 1 line not %d lines.")
                                              % strsection
                                              % pmtentries->size ());
    }

    return bsingle;
}

beast::stringpairarray
parsekeyvaluesection (inifilesections& secsource, std::string const& strsection)
{
    beast::stringpairarray result;

    typedef inifilesections::mapped_type entries;

    entries* const entries = getinifilesection (secsource, strsection);

    if (entries != nullptr)
    {
        for (entries::const_iterator iter = entries->begin ();
            iter != entries->end (); ++iter)
        {
            std::string const line (iter->c_str ());

            int const equalpos = line.find ('=');

            if (equalpos != std::string::npos)
            {
                result.set (
                    line.substr (0, equalpos),
                    line.substr (equalpos + 1));
            }
        }
    }

    return result;
}

/** parses a set of strings into ip::endpoint
      strings which fail to parse are not included in the output. if a stream is
      provided, human readable diagnostic error messages are written for each
      failed parse.
      @param out an outputsequence to store the ip::endpoint list
      @param first the begining of the string input sequence
      @param last the one-past-the-end of the string input sequence
*/
template <class outputsequence, class inputiterator>
void
parseaddresses (outputsequence& out, inputiterator first, inputiterator last,
    beast::journal::stream stream = beast::journal::stream ())
{
    while (first != last)
    {
        auto const str (*first);
        ++first;
        {
            beast::ip::endpoint const addr (
                beast::ip::endpoint::from_string (str));
            if (! is_unspecified (addr))
            {
                out.push_back (addr);
                continue;
            }
        }
        {
            beast::ip::endpoint const addr (
                beast::ip::endpoint::from_string_altform (str));
            if (! is_unspecified (addr))
            {
                out.push_back (addr);
                continue;
            }
        }
        if (stream) stream <<
            "config: \"" << str << "\" is not a valid ip address.";
    }
}

//------------------------------------------------------------------------------
//
// config (deprecated)
//
//------------------------------------------------------------------------------

config::config ()
{
    //
    // defaults
    //

    websocket_ping_freq     = (5 * 60);

    rpc_admin_allow.push_back (beast::ip::endpoint::from_string("127.0.0.1"));

    peer_private            = false;
    peers_max               = 0;    // indicates "use default"

    transaction_fee_base    = default_transaction_fee_base;

    network_quorum          = 0;    // don't need to see other nodes
    validation_quorum       = 1;    // only need one node to vouch

    fee_account_reserve     = default_fee_account_reserve;
    fee_owner_reserve       = default_fee_owner_reserve;
    fee_offer               = default_fee_offer;
    fee_default             = default_fee_default;
    fee_contract_operation  = default_fee_operation;
    
    fee_default_create      = default_fee_create;
    fee_default_none_native = default_fee_none_native;
    fee_default_rate_native = default_fee_rate_native;
    fee_default_min_native  = default_fee_min_native;
    
    asset_tx_min            = default_asset_tx_min;
    asset_limit_default     = default_asset_limit_default;
    asset_interval_min      = default_asset_interval_min;

    ledger_history          = 256;
    ledger_history_index    = 0;
    fetch_depth             = 1000000000;

    // an explanation of these magical values would be nice.
    path_search_old         = 7;
    path_search             = 7;
    path_search_fast        = 2;
    path_search_max         = 10;

    account_probe_max       = 10;

    validators_site         = "";

    ssl_verify              = true;

    elb_support             = false;
    run_standalone          = false;
    doimport                = false;
    start_up                = normal;
}

static
std::string
getenvvar (char const* name)
{
    std::string value;

    auto const v = getenv (name);

    if (v != nullptr)
        value = v;

    return value;
}

void config::setup (std::string const& strconf, bool bquiet)
{
    boost::system::error_code   ec;
    std::string                 strdbpath, strconffile;

    //
    // determine the config and data directories.
    // if the config file is found in the current working directory, use the current working directory as the config directory and
    // that with "db" as the data directory.
    //

    quiet       = bquiet;
    node_size   = 0;

    strdbpath           = helpers::getdatabasedirname ();
    strconffile         = strconf.empty () ? helpers::getconfigfilename () : strconf;

    validators_base     = helpers::getvalidatorsfilename ();

    validators_uri      = boost::str (boost::format ("/%s") % validators_base);

    if (!strconf.empty ())
    {
        // --conf=<path> : everything is relative that file.
        config_file             = strconffile;
        config_dir              = boost::filesystem::absolute (config_file);
        config_dir.remove_filename ();
        data_dir                = config_dir / strdbpath;
    }
    else
    {
        config_dir              = boost::filesystem::current_path ();
        config_file             = config_dir / strconffile;
        data_dir                = config_dir / strdbpath;

        // construct xdg config and data home.
        // http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html
        std::string strhome          = getenvvar ("home");
        std::string strxdgconfighome = getenvvar ("xdg_config_home");
        std::string strxdgdatahome   = getenvvar ("xdg_data_home");

        if (boost::filesystem::exists (config_file)
                // can we figure out xdg dirs?
                || (strhome.empty () && (strxdgconfighome.empty () || strxdgdatahome.empty ())))
        {
            // current working directory is fine, put dbs in a subdir.
        }
        else
        {
            if (strxdgconfighome.empty ())
            {
                // $xdg_config_home was not set, use default based on $home.
                strxdgconfighome    = strhome + "/.config";
            }

            if (strxdgdatahome.empty ())
            {
                // $xdg_data_home was not set, use default based on $home.
                strxdgdatahome  = strhome + "/.local/share";
            }

            config_dir  = strxdgconfighome + "/" + systemname ();
            config_file = config_dir / strconffile;
            data_dir    = strxdgdatahome + "/" + systemname ();

            boost::filesystem::create_directories (config_dir, ec);

            if (ec)
                throw std::runtime_error (boost::str (boost::format ("can not create %s") % config_dir));
        }
    }

    httpclient::initializesslcontext();

    // update default values
    load ();

    boost::filesystem::create_directories (data_dir, ec);

    if (ec)
        throw std::runtime_error (boost::str (boost::format ("can not create %s") % data_dir));

    // create the new unified database
    m_moduledbpath = getdatabasedir();

    // this code is temporarily disabled, and modules will fall back to using
    // per-module databases (e.g. "peerfinder.sqlite") under the module db path
    //
    //if (m_moduledbpath.isdirectory ())
    //    m_moduledbpath = m_moduledbpath.getchildfile("rippled.sqlite");
}

void config::load ()
{
    if (!quiet)
        std::cerr << "loading: " << config_file << std::endl;

    std::ifstream   ifsconfig (config_file.c_str (), std::ios::in);

    if (!ifsconfig)
    {
        std::cerr << "failed to open '" << config_file << "'." << std::endl;
    }
    else
    {
        std::string file_contents;
        file_contents.assign ((std::istreambuf_iterator<char> (ifsconfig)),
                              std::istreambuf_iterator<char> ());

        if (ifsconfig.bad ())
        {
           std::cerr << "failed to read '" << config_file << "'." << std::endl;
        }
        else
        {
            inifilesections secconfig = parseinifile (file_contents, true);
            std::string strtemp;

            build (secconfig);

            // xxx leak
            inifilesections::mapped_type* smttmp;

            smttmp = getinifilesection (secconfig, section_validators);

            if (smttmp)
            {
                validators  = *smttmp;
            }

            smttmp = getinifilesection (secconfig, section_cluster_nodes);

            if (smttmp)
            {
                cluster_nodes = *smttmp;
            }

            smttmp  = getinifilesection (secconfig, section_ips);

            if (smttmp)
            {
                ips = *smttmp;
            }

            smttmp  = getinifilesection (secconfig, section_ips_fixed);

            if (smttmp)
            {
                ips_fixed = *smttmp;
            }

            smttmp = getinifilesection (secconfig, section_sntp);

            if (smttmp)
            {
                sntp_servers = *smttmp;
            }

            smttmp  = getinifilesection (secconfig, section_rpc_startup);

            if (smttmp)
            {
                rpc_startup = json::arrayvalue;

                boost_foreach (std::string const& strjson, *smttmp)
                {
                    json::reader    jrreader;
                    json::value     jvcommand;

                    if (!jrreader.parse (strjson, jvcommand))
                        throw std::runtime_error (
                            boost::str (boost::format (
                                "couldn't parse [" section_rpc_startup "] command: %s") % strjson));

                    rpc_startup.append (jvcommand);
                }
            }

            if (getsinglesection (secconfig, section_database_path, database_path))
                data_dir    = database_path;

            (void) getsinglesection (secconfig, section_validators_site, validators_site);

            if (getsinglesection (secconfig, section_peer_private, strtemp))
                peer_private        = beast::lexicalcastthrow <bool> (strtemp);

            if (getsinglesection (secconfig, section_peers_max, strtemp))
                peers_max           = beast::lexicalcastthrow <int> (strtemp);

            smttmp = getinifilesection (secconfig, section_rpc_admin_allow);

            if (smttmp)
            {
                std::vector<beast::ip::endpoint> parsedaddresses;
                //parseaddresses<std::vector<beast::ip::endpoint>, std::vector<std::string>::const_iterator>
                //    (parsedaddresses, (*smttmp).cbegin(), (*smttmp).cend());
                parseaddresses (parsedaddresses, (*smttmp).cbegin(), (*smttmp).cend());
                rpc_admin_allow.insert (rpc_admin_allow.end(),
                        parsedaddresses.cbegin (), parsedaddresses.cend ());
            }

            insightsettings = parsekeyvaluesection (secconfig, section_insight);

            nodedatabase = parsekeyvaluesection (
                secconfig, configsection::nodedatabase ());

            ephemeralnodedatabase = parsekeyvaluesection (
                secconfig, configsection::tempnodedatabase ());

            importnodedatabase = parsekeyvaluesection (
                secconfig, configsection::importnodedatabase ());
            
            transactiondatabase = parsekeyvaluesection (
                secconfig, configsection::transactiondatabase ());

            if (getsinglesection (secconfig, section_node_size, strtemp))
            {
                if (strtemp == "tiny")
                    node_size = 0;
                else if (strtemp == "small")
                    node_size = 1;
                else if (strtemp == "medium")
                    node_size = 2;
                else if (strtemp == "large")
                    node_size = 3;
                else if (strtemp == "huge")
                    node_size = 4;
                else
                {
                    node_size = beast::lexicalcastthrow <int> (strtemp);

                    if (node_size < 0)
                        node_size = 0;
                    else if (node_size > 4)
                        node_size = 4;
                }
            }

            if (getsinglesection (secconfig, section_elb_support, strtemp))
                elb_support         = beast::lexicalcastthrow <bool> (strtemp);

            if (getsinglesection (secconfig, section_websocket_ping_freq, strtemp))
                websocket_ping_freq = beast::lexicalcastthrow <int> (strtemp);

            getsinglesection (secconfig, section_ssl_verify_file, ssl_verify_file);
            getsinglesection (secconfig, section_ssl_verify_dir, ssl_verify_dir);

            if (getsinglesection (secconfig, section_ssl_verify, strtemp))
                ssl_verify          = beast::lexicalcastthrow <bool> (strtemp);

            if (getsinglesection (secconfig, section_validation_seed, strtemp))
            {
                validation_seed.setseedgeneric (strtemp);

                if (validation_seed.isvalid ())
                {
                    validation_pub = rippleaddress::createnodepublic (validation_seed);
                    validation_priv = rippleaddress::createnodeprivate (validation_seed);
                }
            }

            if (getsinglesection (secconfig, section_node_seed, strtemp))
            {
                node_seed.setseedgeneric (strtemp);

                if (node_seed.isvalid ())
                {
                    node_pub = rippleaddress::createnodepublic (node_seed);
                    node_priv = rippleaddress::createnodeprivate (node_seed);
                }
            }

            if (getsinglesection (secconfig, section_network_quorum, strtemp))
                network_quorum      = beast::lexicalcastthrow <std::size_t> (strtemp);

            if (getsinglesection (secconfig, section_validation_quorum, strtemp))
                validation_quorum   = std::max (0, beast::lexicalcastthrow <int> (strtemp));

            if (getsinglesection (secconfig, section_fee_account_reserve, strtemp))
                fee_account_reserve = beast::lexicalcastthrow <std::uint64_t> (strtemp);

            if (getsinglesection (secconfig, section_fee_owner_reserve, strtemp))
                fee_owner_reserve   = beast::lexicalcastthrow <std::uint64_t> (strtemp);

            if (getsinglesection (secconfig, section_fee_offer, strtemp))
                fee_offer           = beast::lexicalcastthrow <int> (strtemp);

            if (getsinglesection (secconfig, section_fee_default, strtemp))
                fee_default         = beast::lexicalcastthrow <int> (strtemp);

            if (getsinglesection (secconfig, section_fee_operation, strtemp))
                fee_contract_operation  = beast::lexicalcastthrow <int> (strtemp);

            if (getsinglesection (secconfig, section_ledger_history, strtemp))
            {
                boost::to_lower (strtemp);

                if (strtemp == "full")
                    ledger_history = 1000000000u;
                else if (strtemp == "none")
                    ledger_history = 0;
                else
                    ledger_history = beast::lexicalcastthrow <std::uint32_t> (strtemp);
            }
            if (getsinglesection(secconfig, section_ledger_history_index, strtemp))
            {
                ledger_history_index = beast::lexicalcastthrow <std::uint32_t>(strtemp);
            }

            if (getsinglesection (secconfig, section_fetch_depth, strtemp))
            {
                boost::to_lower (strtemp);

                if (strtemp == "none")
                    fetch_depth = 0;
                else if (strtemp == "full")
                    fetch_depth = 1000000000u;
                else
                    fetch_depth = beast::lexicalcastthrow <std::uint32_t> (strtemp);

                if (fetch_depth < 10)
                    fetch_depth = 10;
            }

            if (getsinglesection (secconfig, section_path_search_old, strtemp))
                path_search_old     = beast::lexicalcastthrow <int> (strtemp);
            if (getsinglesection (secconfig, section_path_search, strtemp))
                path_search         = beast::lexicalcastthrow <int> (strtemp);
            if (getsinglesection (secconfig, section_path_search_fast, strtemp))
                path_search_fast    = beast::lexicalcastthrow <int> (strtemp);
            if (getsinglesection (secconfig, section_path_search_max, strtemp))
                path_search_max     = beast::lexicalcastthrow <int> (strtemp);

            if (getsinglesection (secconfig, section_account_probe_max, strtemp))
                account_probe_max   = beast::lexicalcastthrow <int> (strtemp);

            (void) getsinglesection (secconfig, section_sms_from, sms_from);
            (void) getsinglesection (secconfig, section_sms_key, sms_key);
            (void) getsinglesection (secconfig, section_sms_secret, sms_secret);
            (void) getsinglesection (secconfig, section_sms_to, sms_to);
            (void) getsinglesection (secconfig, section_sms_url, sms_url);

            if (getsinglesection (secconfig, section_validators_file, strtemp))
            {
                validators_file     = strtemp;
            }

            if (getsinglesection (secconfig, section_debug_logfile, strtemp))
                debug_logfile       = strtemp;
        }
    }
}

int config::getsize (sizeditemname item) const
{
    sizeditem sizetable[] =   //    tiny    small   medium  large       huge
    {

        { sisweepinterval,      {   10,     30,     60,     90,         120     } },

        { siledgerfetch,        {   2,      2,      3,      3,          3       } },

        { sivalidationssize,    {   256,    256,    512,    1024,       1024    } },
        { sivalidationsage,     {   500,    500,    500,    500,        500     } },

        { sinodecachesize,      {   16384,  32768,  131072, 262144,     0       } },
        { sinodecacheage,       {   60,     90,     120,    900,        0       } },

        { sitreecachesize,      {   128000, 256000, 512000, 768000,     0       } },
        { sitreecacheage,       {   30,     60,     90,     120,        900     } },

        { sislecachesize,       {   4096,   8192,   16384,  65536,      0       } },
        { sislecacheage,        {   30,     60,     90,     120,        300     } },

        { siledgersize,         {   32,     128,    256,    384,        0       } },
        { siledgerage,          {   30,     90,     180,    240,        900     } },

        { sihashnodedbcache,    {   4,      12,     24,     64,         128      } },
        { sitxndbcache,         {   4,      12,     24,     64,         128      } },
        { silgrdbcache,         {   4,      8,      16,     32,         128      } },
    };

    for (int i = 0; i < (sizeof (sizetable) / sizeof (sizeditem)); ++i)
    {
        if (sizetable[i].item == item)
            return sizetable[i].sizes[node_size];
    }

    assert (false);
    return -1;
}

boost::filesystem::path config::getdebuglogfile () const
{
    auto log_file = debug_logfile;

    if (!log_file.empty () && !log_file.is_absolute ())
    {
        // unless an absolute path for the log file is specified, the
        // path is relative to the config file directory.
        log_file = boost::filesystem::absolute (
            log_file, getconfig ().config_dir);
    }

    if (!log_file.empty ())
    {
        auto log_dir = log_file.parent_path ();

        if (!boost::filesystem::is_directory (log_dir))
        {
            boost::system::error_code ec;
            boost::filesystem::create_directories (log_dir, ec);

            // if we fail, we warn but continue so that the calling code can
            // decide how to handle this situation.
            if (ec)
            {
                std::cerr <<
                    "unable to create log file path " << log_dir <<
                    ": " << ec.message() << '\n';
            }
        }
    }

    return log_file;
}

//------------------------------------------------------------------------------
//
// vfalco note clean members area
//

config& getconfig ()
{
    static config config;
    return config;
}

//------------------------------------------------------------------------------

beast::file config::getconfigdir () const
{
    beast::string const s (config_file.native().c_str ());
    if (s.isnotempty ())
        return beast::file (s).getparentdirectory ();
    return beast::file::nonexistent ();
}

beast::file config::getdatabasedir () const
{
    beast::string const s (data_dir.native().c_str());
    if (s.isnotempty ())
        return beast::file (s);
    return beast::file::nonexistent ();
}

beast::file config::getvalidatorsfile () const
{
    beast::string const s (validators_file.native().c_str());
    if (s.isnotempty() && getconfigdir() != beast::file::nonexistent())
        return getconfigdir().getchildfile (s);
    return beast::file::nonexistent ();
}

beast::url config::getvalidatorsurl () const
{
    return beast::parse_url (validators_site).second;
}

beast::file const& config::getmoduledatabasepath ()
{
    return m_moduledbpath;
}

} // ripple
