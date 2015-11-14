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

#ifndef ripple_core_config_h_included
#define ripple_core_config_h_included

#include <ripple/basics/basicconfig.h>
#include <ripple/protocol/systemparameters.h>
#include <ripple/protocol/rippleaddress.h>
#include <ripple/json/json_value.h>
#include <beast/http/url.h>
#include <beast/net/ipendpoint.h>
#include <beast/module/core/files/file.h>
#include <beast/module/core/text/stringpairarray.h>
#include <beast/utility/ci_char_traits.h>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <cstdint>
#include <map>
#include <string>
#include <type_traits>
#include <vector>

namespace ripple {

inifilesections
parseinifile (std::string const& strinput, const bool btrim);

bool
getsinglesection (inifilesections& secsource,
    std::string const& strsection, std::string& strvalue);

int
countsectionentries (inifilesections& secsource, std::string const& strsection);

inifilesections::mapped_type*
getinifilesection (inifilesections& secsource, std::string const& strsection);

/** parse a section of lines as a key/value array.
    each line is in the form <key>=<value>.
    spaces are considered part of the key and value.
*/
// deprecated
beast::stringpairarray
parsekeyvaluesection (inifilesections& secsource, std::string const& strsection);

//------------------------------------------------------------------------------

enum sizeditemname
{
    sisweepinterval,
    sivalidationssize,
    sivalidationsage,
    sinodecachesize,
    sinodecacheage,
    sitreecachesize,
    sitreecacheage,
    sislecachesize,
    sislecacheage,
    siledgersize,
    siledgerage,
    siledgerfetch,
    sihashnodedbcache,
    sitxndbcache,
    silgrdbcache,
};

struct sizeditem
{
    sizeditemname   item;
    int             sizes[5];
};

// vfalco note this entire derived class is deprecated
//             for new config information use the style implied
//             in the base class. for existing config information
//             try to refactor code to use the new style.
//
class config : public basicconfig
{
public:
    struct helpers
    {
        // this replaces config_file_name
        static char const* getconfigfilename ()
        {
            return "moorecoind.cfg";
        }

        static char const* getdatabasedirname ()
        {
            return "db";
        }

        static char const* getvalidatorsfilename ()
        {
            return "validators.txt";
        }
    };

    //--------------------------------------------------------------------------

    // settings related to the configuration file location and directories

    /** returns the directory from which the configuration file was loaded. */
    beast::file getconfigdir () const;

    /** returns the directory in which the current database files are located. */
    beast::file getdatabasedir () const;

    /** returns the full path and filename of the debug log file. */
    boost::filesystem::path getdebuglogfile () const;

    // legacy fields, remove asap
    boost::filesystem::path config_file; // used by uniquenodelist
private:
    boost::filesystem::path config_dir;
    boost::filesystem::path debug_logfile;
public:
    // vfalco todo make this private and fix callers to go through getdatabasedir()
    boost::filesystem::path data_dir;

    //--------------------------------------------------------------------------

    // settings related to validators

    /** return the path to the separate, optional validators file. */
    beast::file getvalidatorsfile () const;

    /** returns the optional url to a trusted network source of validators. */
    beast::url getvalidatorsurl () const;

    // deprecated
    boost::filesystem::path     validators_file;        // as specifed in rippled.cfg.

    /** list of validators entries from rippled.cfg */
    std::vector <std::string> validators;

private:
    /** the folder where new module databases should be located */
    beast::file m_moduledbpath;

public:
    //--------------------------------------------------------------------------
    /** returns the location were databases should be located
        the location may be a file, in which case databases should be placed in
        the file, or it may be a directory, in which cases databases should be
        stored in a file named after the module (e.g. "peerfinder.sqlite") that
        is inside that directory.
    */
    beast::file const& getmoduledatabasepath ();

    //--------------------------------------------------------------------------

    /** parameters for the insight collection module */
    beast::stringpairarray insightsettings;

    /** parameters for the main nodestore database.

        this is 1 or more strings of the form <key>=<value>
        the 'type' and 'path' keys are required, see rippled-example.cfg

        @see database
    */
    beast::stringpairarray nodedatabase;

    /** parameters for the ephemeral nodestore database.

        this is an auxiliary database for the nodestore, usually placed
        on a separate faster volume. however, the volume data may not persist
        between launches. use of the ephemeral database is optional.

        the format is the same as that for @ref nodedatabase

        @see database
    */
    beast::stringpairarray ephemeralnodedatabase;

    /** parameters for importing an old database in to the current node database.
        if this is not empty, then it specifies the key/value parameters for
        another node database from which to import all data into the current
        node database specified by @ref nodedatabase.
        the format of this string is in the form:
            <key>'='<value>['|'<key>'='value]
        @see parsedelimitedkeyvaluestring
    */
    bool doimport;
    beast::stringpairarray importnodedatabase;
    
    /** parameters for the transaction database.
     
     this is 1 or more strings of the form <key>=<value>
     the 'type' and 'path' keys are required, see rippled-example.cfg
     
     @see database
     */
    beast::stringpairarray transactiondatabase;


    //
    //
    //--------------------------------------------------------------------------
public:
    // configuration parameters
    bool                        quiet;

    bool                        elb_support;            // support amazon elb

    std::string                 validators_site;        // where to find validators.txt on the internet.
    std::string                 validators_uri;         // uri of validators.txt.
    std::string                 validators_base;        // name
    std::vector<std::string>    ips;                    // peer ips from rippled.cfg.
    std::vector<std::string>    ips_fixed;              // fixed peer ips from rippled.cfg.
    std::vector<std::string>    sntp_servers;           // sntp servers from rippled.cfg.

    enum startuptype
    {
        fresh,
        normal,
        load,
        load_file,
        replay,
        network
    };
    startuptype                 start_up;



    std::string                 start_ledger;

    // database
    std::string                 database_path;
    
    // network parameters
    int                         transaction_fee_base;   // the number of fee units a reference transaction costs

    /** operate in stand-alone mode.

        in stand alone mode:

        - peer connections are not attempted or accepted
        - the ledger is not advanced automatically.
        - if no ledger is loaded, the default ledger with the root
          account is created.
    */
    bool                        run_standalone;

    // note: the following parameters do not relate to the unl or trust at all
    std::size_t                 network_quorum;         // minimum number of nodes to consider the network present
    int                         validation_quorum;      // minimum validations to consider ledger authoritative

    // peer networking parameters
    bool                        peer_private;           // true to ask peers not to relay current ip.
    unsigned int                peers_max;

    int                         websocket_ping_freq;

    // rpc parameters
    std::vector<beast::ip::endpoint>   rpc_admin_allow;
    json::value                     rpc_startup;

    // path searching
    int                         path_search_old;
    int                         path_search;
    int                         path_search_fast;
    int                         path_search_max;

    // validation
    rippleaddress               validation_seed;
    rippleaddress               validation_pub;
    rippleaddress               validation_priv;

    // node/cluster
    std::vector<std::string>    cluster_nodes;
    rippleaddress               node_seed;
    rippleaddress               node_pub;
    rippleaddress               node_priv;

    // fee schedule (all below values are in fee units)
    std::uint64_t                      fee_default;            // default fee.
    std::uint64_t                      fee_account_reserve;    // amount of units not allowed to send.
    std::uint64_t                      fee_owner_reserve;      // amount of units not allowed to send per owner entry.
    std::uint64_t                      fee_offer;              // rate per day.
    int                                fee_contract_operation; // fee for each contract operation
    
    std::uint64_t                      fee_default_create;     // fee for create account
    std::uint64_t                      fee_default_none_native;// fee for nonnative payment
    double                             fee_default_rate_native;// fee rate for native(vbc/vrp) payment
    std::uint64_t                      fee_default_min_native; // minimal fee for native transaction

    int                                asset_tx_min;           // minimal amount for asset txs.
    std::uint64_t                       asset_limit_default;    // default limit for asset.
    int                                 asset_interval_min;     // minimal interval for asset release schedule.

    // node storage configuration
    std::uint32_t                      ledger_history;
    std::uint32_t                      ledger_history_index;
    std::uint32_t                      fetch_depth;
    int                         node_size;

    // client behavior
    int                         account_probe_max;      // how far to scan for accounts.

    bool                        ssl_verify;
    std::string                 ssl_verify_file;
    std::string                 ssl_verify_dir;

    std::string                 sms_from;
    std::string                 sms_key;
    std::string                 sms_secret;
    std::string                 sms_to;
    std::string                 sms_url;

public:
    config ();

    int getsize (sizeditemname) const;
    void setup (std::string const& strconf, bool bquiet);
    void load ();
};

// vfalco deprecated
extern config& getconfig();

} // ripple

#endif
