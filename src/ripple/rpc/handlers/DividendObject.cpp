namespace ripple {

// ledger_dividend [until]
json::value dodividendobject (rpc::context& context)
{
    sle::pointer dividendsle = nullptr;
    
    //time param specified, query from ledger before this time
    if (context.params.ismember ("until"))
    {
        auto time_value = context.params["until"];
        if (!time_value.isnumeric() || time_value.asuint() <= 0)
        {
            return ripple::rpc::make_error(rpcinvalid_params, "dividendobjectmalformed");
        }
        auto time = time_value.asuint();

        std::string sql =
        boost::str (boost::format (
                                   "select * from ledgers where closingtime <= %u order by ledgerseq desc limit 1")
                    % time);
        {
            auto db = getapp().getledgerdb().getdb();
            auto sl (getapp().getledgerdb ().lock ());
            if (db->executesql(sql) && db->startiterrows())
            {
                std::uint32_t ledgerseq = db->getint("ledgerseq");
                db->enditerrows();
                //carl should we find a seq more pricisely?
                ledger::pointer ledger = getapp().getops().getledgerbyseq(ledgerseq);
                dividendsle = ledger->getdividendobject();
            }
        }
    }
    else //no time param specified, query from the lastet closed ledger
    {
        ledger::pointer ledger = getapp().getops().getclosedledger();
        dividendsle = ledger->getdividendobject();
    }

    if (dividendsle)
    {
        return dividendsle->getjson(0);
    }
    else
    {
        return ripple::rpc::make_error(rpcdivobj_not_found, "dividendobjectnotfound");
    }
}

} // ripple
