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
#include <ripple/protocol/errorcodes.h>
#include <unordered_map>
#include <utility>

namespace std {

template <>
struct hash <ripple::error_code_i>
{
    std::size_t operator() (ripple::error_code_i value) const
    {
        return value;
    }
};

}
namespace ripple {
namespace rpc {

namespace detail {

class errorcategory
{
public:
    using map = std::unordered_map <error_code_i, errorinfo> ;

    errorcategory ()
        : m_unknown (rpcunknown, "unknown", "an unknown error code.")
    {
        add (rpcact_bitcoin,           "actbitcoin",        "account is bitcoin address.");
        add (rpcact_exists,            "actexists",         "account already exists.");
        add (rpcact_malformed,         "actmalformed",      "account malformed.");
        add (rpcact_not_found,         "actnotfound",       "account not found.");
        add (rpcatx_deprecated,        "deprecated",        "use the new api or specify a ledger range.");
        add (rpcbad_blob,              "badblob",           "blob must be a non-empty hex string.");
        add (rpcbad_feature,           "badfeature",        "feature unknown or invalid.");
        add (rpcbad_issuer,            "badissuer",         "issuer account malformed.");
        add (rpcbad_market,            "badmarket",         "no such market.");
        add (rpcbad_secret,            "badsecret",         "secret does not match account.");
        add (rpcbad_seed,              "badseed",           "disallowed seed.");
        add (rpcbad_syntax,            "badsyntax",         "syntax error.");
        add (rpccommand_missing,       "commandmissing",    "missing command entry.");
        add (rpcdst_act_malformed,     "dstactmalformed",   "destination account is malformed.");
        add (rpcdst_act_missing,       "dstactmissing",     "destination account does not exist.");
        add (rpcdst_amt_malformed,     "dstamtmalformed",   "destination amount/currency/issuer is malformed.");
        add (rpcdst_isr_malformed,     "dstisrmalformed",   "destination issuer is malformed.");
        add (rpcfail_gen_decrypt,      "failgendecrypt",    "failed to decrypt generator.");
        add (rpcforbidden,             "forbidden",         "bad credentials.");
        add (rpcgeneral,               "general",           "generic error reason.");
        add (rpcgets_act_malformed,    "getsactmalformed",  "gets account malformed.");
        add (rpcgets_amt_malformed,    "getsamtmalformed",  "gets amount malformed.");
        add (rpchigh_fee,              "highfee",           "current transaction fee exceeds your limit.");
        add (rpchost_ip_malformed,     "hostipmalformed",   "host ip is malformed.");
        add (rpcinsuf_funds,           "insuffunds",        "insufficient funds.");
        add (rpcinternal,              "internal",          "internal error.");
        add (rpcinvalid_params,        "invalidparams",     "invalid parameters.");
        add (rpcjson_rpc,              "json_rpc",          "json-rpc transport error.");
        add (rpclgr_idxs_invalid,      "lgridxsinvalid",    "ledger indexes invalid.");
        add (rpclgr_idx_malformed,     "lgridxmalformed",   "ledger index malformed.");
        add (rpclgr_not_found,         "lgrnotfound",       "ledger not found.");
        add (rpcload_failed,           "loadfailed",        "load failed");
        add (rpcmaster_disabled,       "masterdisabled",    "master key is disabled.");
        add (rpcnot_enabled,           "notenabled",        "not enabled in configuration.");
        add (rpcnot_impl,              "notimpl",           "not implemented.");
        add (rpcnot_ready,             "notready",          "not ready to handle this request.");
        add (rpcnot_standalone,        "notstandalone",     "operation valid in debug mode only.");
        add (rpcnot_supported,         "notsupported",      "operation not supported.");
        add (rpcno_account,            "noaccount",         "no such account.");
        add (rpcno_closed,             "noclosed",          "closed ledger is unavailable.");
        add (rpcno_current,            "nocurrent",         "current ledger is unavailable.");
        add (rpcno_events,             "noevents",          "current transport does not support events.");
        add (rpcno_gen_decrypt,        "nogendecrypt",      "password failed to decrypt master public generator.");
        add (rpcno_network,            "nonetwork",         "not synced to moorecoin network.");
        add (rpcno_path,               "nopath",            "unable to find a moorecoin path.");
        add (rpcno_permission,         "nopermission",      "you don't have permission for this command.");
        add (rpcno_pf_request,         "nopathrequest",     "no pathfinding request in progress.");
        add (rpcpasswd_changed,        "passwdchanged",     "wrong key, password changed.");
        add (rpcpays_act_malformed,    "paysactmalformed",  "pays account malformed.");
        add (rpcpays_amt_malformed,    "paysamtmalformed",  "pays amount malformed.");
        add (rpcport_malformed,        "portmalformed",     "port is malformed.");
        add (rpcpublic_malformed,      "publicmalformed",   "public key is malformed.");
        add (rpcquality_malformed,     "qualitymalformed",  "quality malformed.");
        add (rpcslow_down,             "slowdown",          "you are placing too much load on the server.");
        add (rpcsrc_act_malformed,     "srcactmalformed",   "source account is malformed.");
        add (rpcsrc_act_missing,       "srcactmissing",     "source account not provided.");
        add (rpcsrc_act_not_found,     "srcactnotfound",    "source account not found.");
        add (rpcsrc_amt_malformed,     "srcamtmalformed",   "source amount/currency/issuer is malformed.");
        add (rpcsrc_cur_malformed,     "srccurmalformed",   "source currency is malformed.");
        add (rpcsrc_isr_malformed,     "srcisrmalformed",   "source issuer is malformed.");
        add (rpcsrc_missing,           "srcmissing",        "source is missing.");
        add (rpcsrc_unclaimed,         "srcunclaimed",      "source account is not claimed.");
        add (rpctoo_busy,              "toobusy",           "the server is too busy to help you now.");
        add (rpctxn_not_found,         "txnnotfound",       "transaction not found.");
        add (rpcunknown_command,       "unknowncmd",        "unknown method.");
        add (rpcwrong_seed,            "wrongseed",         "the regular key does not point as the master key.");
        add (rpcdivobj_not_found,      "divobjnotfound",    "dividend object not found");
        
    }

    errorinfo const& get (error_code_i code) const
    {
        map::const_iterator const iter (m_map.find (code));
        if (iter != m_map.end())
            return iter->second;
        return m_unknown;
    }

private:
    void add (error_code_i code, std::string const& token,
        std::string const& message)
    {
        std::pair <map::iterator, bool> result (
            m_map.emplace (std::piecewise_construct,
                std::forward_as_tuple (code), std::forward_as_tuple (
                    code, token, message)));
        if (! result.second)
            throw std::invalid_argument ("duplicate error code");
    }

private:
    map m_map;
    errorinfo m_unknown;
};

}

//------------------------------------------------------------------------------

errorinfo const& get_error_info (error_code_i code)
{
    static detail::errorcategory category;
    return category.get (code);
}

json::value make_error (error_code_i code)
{
    json::value json;
    inject_error (code, json);
    return json;
}

json::value make_error (error_code_i code, std::string const& message)
{
    json::value json;
    inject_error (code, message, json);
    return json;
}

bool contains_error (json::value const& json)
{
    if (json.isobject() && json.ismember ("error"))
        return true;
    return false;
}

}
}
