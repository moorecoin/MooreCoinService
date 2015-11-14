#include <beastconfig.h>
#include <ripple/rpc/impl/tuning.h>
#include <ripple/rpc/impl/accountfromstring.h>
#include <ripple/app/paths/ripplestate.h>
#include <ripple/protocol/indexes.h>

namespace ripple {

void addline (json::value& jsonlines, ripplestate const& line, ledger::pointer ledger);

// {
//   account: [<account>|<account_public_key>]
//   peer: [<account>|<account_public_key>]
//   currency: <currency>
//   ledger_hash : <ledger>
//   ledger_index : <ledger_index>
// }
json::value doaccountasset (rpc::context& context)
{
    auto const& params(context.params);
    if (!params.ismember(jss::account))
        return rpc::missing_field_error("account");
    if (!params.ismember(jss::peer))
        return rpc::missing_field_error("peer");
    if (!params.ismember(jss::currency))
        return rpc::missing_field_error("currency");

    ledger::pointer ledger;
    json::value result(rpc::lookupledger(params, ledger, context.netops));
    if (!ledger)
        return result;

    std::string strident(params[jss::account].asstring());
    bool bindex(params.ismember(jss::account_index));
    int iindex(bindex ? params[jss::account_index].asuint() : 0);
    rippleaddress rippleaddress;

    json::value const jv(rpc::accountfromstring(ledger, rippleaddress, bindex,
                                                strident, iindex, false, context.netops));
    if (!jv.empty()) {
        for (json::value::const_iterator it(jv.begin()); it != jv.end(); ++it)
            result[it.membername()] = it.key();

        return result;
    }

    if (!ledger->hasaccount(rippleaddress))
        return rpcerror(rpcact_not_found);

    std::string strpeer(params.ismember(jss::peer) ? params[jss::peer].asstring() : "");
    bool bpeerindex(params.ismember(jss::peer_index));
    int ipeerindex(bindex ? params[jss::peer_index].asuint() : 0);
    rippleaddress rippleaddresspeer;

    json::value const jvpeer(rpc::accountfromstring(ledger, rippleaddresspeer, bpeerindex,
                                                    strpeer, ipeerindex, false, context.netops));
    if (!jvpeer.empty()) {
        return result;
    }

    if (!ledger->hasaccount(rippleaddresspeer))
        return rpcerror(rpcact_not_found);

    currency ucurrency;
    if (!to_currency(ucurrency, params[jss::currency].asstring()))
        return rpc::make_error(rpcsrc_cur_malformed, "invalid field 'currency', bad currency.");

    account const& raaccount(rippleaddress.getaccountid());
    account const& rapeeraccount(rippleaddresspeer.getaccountid());

    uint256 unodeindex = getripplestateindex(raaccount, rapeeraccount, ucurrency);
    auto slenode = context.netops.getslei(ledger, unodeindex);
    auto const line(ripplestate::makeitem(raaccount, slenode));

    if (line == nullptr ||
        raaccount != line->getaccountid() ||
        rapeeraccount != line->getaccountidpeer())
        return result;

    json::value jsonlines(json::arrayvalue);
    addline(jsonlines, *line, ledger);
    result[jss::lines] = jsonlines[0u];
    
    json::value& jsonassetstates(result[jss::states] = json::arrayvalue);
    
    // get asset_states for currency asset.
    if (assetcurrency() == line->getbalance().getcurrency()) {
        auto lpledger = std::make_shared<ledger> (std::ref (*ledger), false);
        ledgerentryset les(lpledger, tapnone);
        auto sleripplestate = les.entrycache(ltripple_state, getripplestateindex(raaccount, rapeeraccount, assetcurrency()));
        les.assetrelease(raaccount, rapeeraccount, assetcurrency(), sleripplestate);

        uint256 baseindex = getassetstateindex(line->getaccountid(), line->getaccountidpeer(), assetcurrency());
        uint256 assetstateindex = getqualityindex(baseindex);
        uint256 assetstateend = getqualitynext(assetstateindex);

        for (;;) {
            auto const& sle = les.entrycache(ltasset_state, assetstateindex);
            if (sle) {
                stamount amount = sle->getfieldamount(sfamount);
                stamount released = sle->getfieldamount(sfdeliveredamount);

                if (sle->getfieldaccount160(sfaccount) == line->getaccountidpeer()) {
                    amount.negate();
                    released.negate();
                }

                auto reserved = released ? amount - released : amount;

                json::value& state(jsonassetstates.append(json::objectvalue));
                state[jss::date] = static_cast<json::uint>(getquality(assetstateindex));
                state[jss::amount] = amount.gettext();
                state[jss::reserve] = reserved.gettext();
            }

            auto const nextassetstate(
                les.getnextledgerindex(assetstateindex, assetstateend));

            if (nextassetstate.iszero())
                break;

            assetstateindex = nextassetstate;
        }
    }

    context.loadtype = resource::feemediumburdenrpc;
    return result;
}

} // ripple
