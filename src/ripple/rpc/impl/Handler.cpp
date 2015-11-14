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
#include <ripple/rpc/impl/handler.h>
#include <ripple/rpc/handlers/handlers.h>

namespace ripple {
namespace rpc {
namespace {

/** adjust an old-style handler to be call-by-reference. */
template <typename function>
handler::method<json::value> byref (function const& f)
{
    return [f] (context& context, json::value& result)
    {
        result = f (context);
        if (result.type() != json::objectvalue)
        {
            assert (false);
            result = rpc::makeobjectvalue (result);
        }

        return status();
    };
}

template <class object, class handlerimpl>
status handle (context& context, object& object)
{
    handlerimpl handler (context);

    auto status = handler.check ();
    if (status)
        status.inject (object);
    else
        handler.writeresult (object);
    return status;
};

class handlertable {
  public:
    handlertable (std::vector<handler> const& entries) {
        for (auto& entry: entries)
        {
            assert (table_.find(entry.name_) == table_.end());
            table_[entry.name_] = entry;
    }

        // this is where the new-style handlers are added.
        addhandler<ledgerhandler>();
    }

    const handler* gethandler(std::string name) {
        auto i = table_.find(name);
        return i == table_.end() ? nullptr : &i->second;
    }

  private:
    std::map<std::string, handler> table_;

    template <class handlerimpl>
    void addhandler()
    {
        assert (table_.find(handlerimpl::name()) == table_.end());

        handler h;
        h.name_ = handlerimpl::name(),
        h.valuemethod_ = &handle<json::value, handlerimpl>,
        h.role_ = handlerimpl::role(),
        h.condition_ = handlerimpl::condition(),
        h.objectmethod_ = &handle<object, handlerimpl>;

        table_[handlerimpl::name()] = h;
    };
};

handlertable handlers({
    // request-response methods
    {   "account_asset",        byref (&doaccountasset),        role::user,  needs_current_ledger  },
    {   "account_currencies",   byref (&doaccountcurrencies),   role::user,  needs_current_ledger  },
    {   "account_dividend",     byref (&doaccountdividend),     role::user,  needs_network_connection },
    {   "account_info",         byref (&doaccountinfo),         role::user,  needs_current_ledger  },
    {   "account_lines",        byref (&doaccountlines),        role::user,  needs_current_ledger  },
    {   "account_offers",       byref (&doaccountoffers),       role::user,  needs_current_ledger  },
    {   "account_tx",           byref (&doaccounttxswitch),     role::user,  needs_network_connection  },
    {   "ancestors",            byref (&doancestors),           role::user,  needs_network_connection },
    {   "blacklist",            byref (&doblacklist),           role::admin,   no_condition     },
    {   "book_offers",          byref (&dobookoffers),          role::user,  needs_current_ledger  },
    {   "can_delete",           byref (&docandelete),           role::admin,   no_condition     },
    {   "connect",              byref (&doconnect),             role::admin,   no_condition     },
    {   "consensus_info",       byref (&doconsensusinfo),       role::admin,   no_condition     },
    {   "dividend_object",      byref (&dodividendobject),      role::user,  needs_network_connection },
    {   "feature",              byref (&dofeature),             role::admin,   no_condition     },
    {   "fetch_info",           byref (&dofetchinfo),           role::admin,   no_condition     },
    {   "get_counts",           byref (&dogetcounts),           role::admin,   no_condition     },
    {   "internal",             byref (&dointernal),            role::admin,   no_condition     },
    {   "ledger_accept",        byref (&doledgeraccept),        role::admin, needs_current_ledger  },
    {   "ledger_cleaner",       byref (&doledgercleaner),       role::admin, needs_network_connection  },
    {   "ledger_closed",        byref (&doledgerclosed),        role::user,  needs_closed_ledger   },
    {   "ledger_current",       byref (&doledgercurrent),       role::user,  needs_current_ledger  },
    {   "ledger_data",          byref (&doledgerdata),          role::user,  needs_current_ledger  },
    {   "ledger_entry",         byref (&doledgerentry),         role::user,  needs_current_ledger  },
    {   "ledger_header",        byref (&doledgerheader),        role::user,  needs_current_ledger  },
    {   "ledger_request",       byref (&doledgerrequest),       role::admin,   no_condition     },
    {   "log_level",            byref (&dologlevel),            role::admin,   no_condition     },
    {   "logrotate",            byref (&dologrotate),           role::admin,   no_condition     },
    {   "owner_info",           byref (&doownerinfo),           role::user,  needs_current_ledger  },
    {   "path_find",            byref (&dopathfind),            role::user,  needs_current_ledger  },
    {   "peers",                byref (&dopeers),               role::admin,   no_condition     },
    {   "ping",                 byref (&doping),                role::user,    no_condition     },
    {   "print",                byref (&doprint),               role::admin,   no_condition     },
//      {   "profile",              byref (&doprofile),             role::user,  needs_current_ledger  },
    {   "random",               byref (&dorandom),              role::user,    no_condition     },
    {   "ripple_path_find",     byref (&doripplepathfind),      role::user,  needs_current_ledger  },
    {   "sign",                 byref (&dosign),                role::user,    no_condition     },
    {   "submit",               byref (&dosubmit),              role::user,  needs_current_ledger  },
    {   "server_info",          byref (&doserverinfo),          role::user,    no_condition     },
    {   "server_state",         byref (&doserverstate),         role::user,    no_condition     },
    {   "sms",                  byref (&dosms),                 role::admin,   no_condition     },
    {   "stop",                 byref (&dostop),                role::admin,   no_condition     },
    {   "transaction_entry",    byref (&dotransactionentry),    role::user,  needs_current_ledger  },
    {   "tx",                   byref (&dotx),                  role::user,  needs_network_connection  },
    {   "tx_history",           byref (&dotxhistory),           role::user,    no_condition     },
    {   "unl_add",              byref (&dounladd),              role::admin,   no_condition     },
    {   "unl_delete",           byref (&dounldelete),           role::admin,   no_condition     },
    {   "unl_list",             byref (&dounllist),             role::admin,   no_condition     },
    {   "unl_load",             byref (&dounlload),             role::admin,   no_condition     },
    {   "unl_network",          byref (&dounlnetwork),          role::admin,   no_condition     },
    {   "unl_reset",            byref (&dounlreset),            role::admin,   no_condition     },
    {   "unl_score",            byref (&dounlscore),            role::admin,   no_condition     },
    {   "validation_create",    byref (&dovalidationcreate),    role::admin,   no_condition     },
    {   "validation_seed",      byref (&dovalidationseed),      role::admin,   no_condition     },
    {   "wallet_accounts",      byref (&dowalletaccounts),      role::user,  needs_current_ledger  },
    {   "wallet_propose",       byref (&dowalletpropose),       role::admin,   no_condition     },
    {   "wallet_seed",          byref (&dowalletseed),          role::admin,   no_condition     },

    // evented methods
    {   "subscribe",            byref (&dosubscribe),           role::user,  no_condition     },
    {   "unsubscribe",          byref (&dounsubscribe),         role::user,  no_condition     },
});

} // namespace

const handler* gethandler(std::string const& name) {
    return handlers.gethandler(name);
}

} // rpc
} // ripple
