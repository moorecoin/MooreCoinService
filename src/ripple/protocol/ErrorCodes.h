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

#ifndef ripple_protocol_errorcodes_h_included
#define ripple_protocol_errorcodes_h_included

#include <ripple/protocol/jsonfields.h>
#include <ripple/json/json_value.h>

namespace ripple {

// vfalco note these are outside the rpc namespace

enum error_code_i
{
    rpcunknown = -1,     // represents codes not listed in this enumeration

    rpcsuccess = 0,

    rpcbad_syntax,  // must be 1 to print usage to command line.
    rpcjson_rpc,
    rpcforbidden,

    // error numbers beyond this line are not stable between versions.
    // programs should use error tokens.

    // misc failure
    rpcgeneral,
    rpcload_failed,
    rpcno_permission,
    rpcno_events,
    rpcnot_standalone,
    rpctoo_busy,
    rpcslow_down,
    rpchigh_fee,
    rpcnot_enabled,
    rpcnot_ready,

    // networking
    rpcno_closed,
    rpcno_current,
    rpcno_network,

    // ledger state
    rpcact_exists,
    rpcact_not_found,
    rpcinsuf_funds,
    rpclgr_not_found,
    rpcmaster_disabled,
    rpcno_account,
    rpcno_path,
    rpcpasswd_changed,
    rpcsrc_missing,
    rpcsrc_unclaimed,
    rpctxn_not_found,
    rpcwrong_seed,

    // malformed command
    rpcinvalid_params,
    rpcunknown_command,
    rpcno_pf_request,

    // bad parameter
    rpcact_bitcoin,
    rpcact_malformed,
    rpcquality_malformed,
    rpcbad_blob,
    rpcbad_feature,
    rpcbad_issuer,
    rpcbad_market,
    rpcbad_secret,
    rpcbad_seed,
    rpccommand_missing,
    rpcdst_act_malformed,
    rpcdst_act_missing,
    rpcdst_amt_malformed,
    rpcdst_isr_malformed,
    rpcgets_act_malformed,
    rpcgets_amt_malformed,
    rpchost_ip_malformed,
    rpclgr_idxs_invalid,
    rpclgr_idx_malformed,
    rpcpays_act_malformed,
    rpcpays_amt_malformed,
    rpcport_malformed,
    rpcpublic_malformed,
    rpcsrc_act_malformed,
    rpcsrc_act_missing,
    rpcsrc_act_not_found,
    rpcsrc_amt_malformed,
    rpcsrc_cur_malformed,
    rpcsrc_isr_malformed,
    rpcatx_deprecated,
    
    //dividend
    rpcdivobj_not_found,

    // internal error (should never happen)
    rpcinternal,        // generic internal error.
    rpcfail_gen_decrypt,
    rpcnot_impl,
    rpcnot_supported,
    rpcno_gen_decrypt,
};

//------------------------------------------------------------------------------

// vfalco note these should probably not be in the rpc namespace.

namespace rpc {

/** maps an rpc error code to its token and default message. */
struct errorinfo
{
    errorinfo (error_code_i code_, std::string const& token_,
        std::string const& message_)
        : code (code_)
        , token (token_)
        , message (message_)
    { }

    error_code_i code;
    std::string token;
    std::string message;
};

/** returns an errorinfo that reflects the error code. */
errorinfo const& get_error_info (error_code_i code);

/** add or update the json update to reflect the error code. */
/** @{ */
template <class jsonvalue>
void inject_error (error_code_i code, jsonvalue& json)
{
    errorinfo const& info (get_error_info (code));
    json [jss::error] = info.token;
    json [jss::error_code] = info.code;
    json [jss::error_message] = info.message;
}

template <class jsonvalue>
void inject_error (int code, jsonvalue& json)
{
    inject_error (error_code_i (code), json);
}

template <class jsonvalue>
void inject_error (
    error_code_i code, std::string const& message, jsonvalue& json)
{
    errorinfo const& info (get_error_info (code));
    json [jss::error] = info.token;
    json [jss::error_code] = info.code;
    json [jss::error_message] = message;
}

/** @} */

/** returns a new json object that reflects the error code. */
/** @{ */
json::value make_error (error_code_i code);
json::value make_error (error_code_i code, std::string const& message);
/** @} */

/** returns a new json object that indicates invalid parameters. */
/** @{ */
inline json::value make_param_error (std::string const& message)
{
    return make_error (rpcinvalid_params, message);
}

inline std::string missing_field_message (std::string const& name)
{
    return "missing field '" + name + "'.";
}

inline json::value missing_field_error (std::string const& name)
{
    return make_param_error (missing_field_message (name));
}

inline std::string object_field_message (std::string const& name)
{
    return "invalid field '" + name + "', not object.";
}

inline json::value object_field_error (std::string const& name)
{
    return make_param_error (object_field_message (name));
}

inline std::string invalid_field_message (std::string const& name)
{
    return "invalid field '" + name + "'.";
}

inline json::value invalid_field_error (std::string const& name)
{
    return make_param_error (invalid_field_message (name));
}

inline std::string expected_field_message (
    std::string const& name, std::string const& type)
{
    return "invalid field '" + name + "', not " + type + ".";
}

inline json::value expected_field_error (
    std::string const& name, std::string const& type)
{
    return make_param_error (expected_field_message (name, type));
}

/** @} */

/** returns `true` if the json contains an rpc error specification. */
bool contains_error (json::value const& json);

}

}

#endif
