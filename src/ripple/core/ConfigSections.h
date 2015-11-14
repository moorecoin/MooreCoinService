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

#ifndef ripple_configsections_h_included
#define ripple_configsections_h_included

namespace ripple {

// vfalco deprecated in favor of the basicconfig interface
struct configsection
{
    static std::string nodedatabase ()       { return "node_db"; }
    static std::string tempnodedatabase ()   { return "temp_db"; }
    static std::string importnodedatabase () { return "import_db"; }
    static std::string transactiondatabase () { return "transaction_db"; }
};

// vfalco todo rename and replace these macros with variables.
#define section_account_probe_max       "account_probe_max"
#define section_cluster_nodes           "cluster_nodes"
#define section_database_path           "database_path"
#define section_debug_logfile           "debug_logfile"
#define section_elb_support             "elb_support"
#define section_fee_default             "fee_default"
#define section_fee_offer               "fee_offer"
#define section_fee_operation           "fee_operation"
#define section_fee_account_reserve     "fee_account_reserve"
#define section_fee_owner_reserve       "fee_owner_reserve"
#define section_fetch_depth             "fetch_depth"
#define section_ledger_history          "ledger_history"
#define section_ledger_history_index    "ledger_history_index"
#define section_insight                 "insight"
#define section_ips                     "ips"
#define section_ips_fixed               "ips_fixed"
#define section_network_quorum          "network_quorum"
#define section_node_seed               "node_seed"
#define section_node_size               "node_size"
#define section_path_search_old         "path_search_old"
#define section_path_search             "path_search"
#define section_path_search_fast        "path_search_fast"
#define section_path_search_max         "path_search_max"
#define section_peer_private            "peer_private"
#define section_peers_max               "peers_max"
#define section_rpc_admin_allow         "rpc_admin_allow"
#define section_rpc_startup             "rpc_startup"
#define section_sms_from                "sms_from"
#define section_sms_key                 "sms_key"
#define section_sms_secret              "sms_secret"
#define section_sms_to                  "sms_to"
#define section_sms_url                 "sms_url"
#define section_sntp                    "sntp_servers"
#define section_ssl_verify              "ssl_verify"
#define section_ssl_verify_file         "ssl_verify_file"
#define section_ssl_verify_dir          "ssl_verify_dir"
#define section_validators_file         "validators_file"
#define section_validation_quorum       "validation_quorum"
#define section_validation_seed         "validation_seed"
#define section_websocket_ping_freq     "websocket_ping_frequency"
#define section_validators              "validators"
#define section_validators_site         "validators_site"

} // ripple

#endif
