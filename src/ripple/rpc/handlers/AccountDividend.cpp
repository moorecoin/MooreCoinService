#include <ripple/app/data/databasecon.h>
#include <ripple/app/tx/transactionmaster.h>

namespace ripple {

// account_dividend [account]
json::value doaccountdividend (rpc::context& context)
{
    if (!context.params.ismember ("account"))
    {
        return ripple::rpc::make_error(rpcact_not_found, "accountdividendnotfound");
    }
    json::value result;
    auto account = context.params["account"].asstring();
    result["account"] = account;
    
    ledger::pointer ledger = getapp().getops().getclosedledger();
    sle::pointer dividendsle = ledger->getdividendobject();
    if (dividendsle && dividendsle->isfieldpresent(sfdividendledger))
    {
        std::uint32_t baseledgerseq = dividendsle->getfieldu32(sfdividendledger);
        std::string sql =
        boost::str (boost::format ("select accounttransactions.transid from accounttransactions join transactions "
                                   "on accounttransactions.transid=transactions.transid "
                                   "where account='%s' and accounttransactions.ledgerseq>%d "
                                   "and transtype='dividend' "
                                   "order by accounttransactions.ledgerseq asc limit 1;")
                    % account.c_str()
                    % baseledgerseq);
        transaction::pointer txn = nullptr;
        {
            auto db = getapp().gettxndb().getdb();
            auto sl (getapp().gettxndb().lock());
            if (db->executesql(sql) && db->startiterrows())
            {
                std::string transid = "";
                db->getstr(0, transid);
                db->enditerrows();
                if (transid != "")
                {
                    uint256 txid (transid);
                    txn = getapp().getmastertransaction ().fetch (txid, true);
                }
            }
        }
        if (txn)
        {
            result["dividendcoins"] = to_string(txn->getstransaction()->getfieldu64(sfdividendcoins));
            result["dividendcoinsvbc"] = to_string(txn->getstransaction()->getfieldu64(sfdividendcoinsvbc));
            result["dividendcoinsvbcrank"] = to_string(txn->getstransaction()->getfieldu64(sfdividendcoinsvbcrank));
            result["dividendcoinsvbcsprd"] = to_string(txn->getstransaction()->getfieldu64(sfdividendcoinsvbcsprd));
            result["dividendtsprd"] = to_string(txn->getstransaction()->getfieldu64(sfdividendtsprd));
            result["dividendvrank"] = to_string(txn->getstransaction()->getfieldu64(sfdividendvrank));
            result["dividendvsprd"] = to_string(txn->getstransaction()->getfieldu64(sfdividendvsprd));
            result["dividendledger"] = to_string(txn->getstransaction()->getfieldu32(sfdividendledger));

            return result;
        }
    }
    result["dividendcoins"] = "0";
    result["dividendcoinsvbc"] = "0";
    result["dividendcoinsvbcrank"] = "0";
    result["dividendcoinsvbcsprd"] = "0";
    result["dividendtsprd"] = "0";
    result["dividendvrank"] = "0";
    result["dividendvsprd"] = "0";
    result["dividendledger"] = "0";
    return result;
}

} // ripple
