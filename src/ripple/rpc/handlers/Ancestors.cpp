namespace ripple {

// ancestors [account]
json::value doancestors (rpc::context& context)
{
    if (!context.params.ismember ("account"))
    {
        return rpc::missing_field_error ("account");
    }
    json::value result;
    auto account = context.params["account"].asstring();
    rippleaddress accountid;
    if (!accountid.setaccountid (account))
    {
        return ripple::rpc::make_error(rpcinvalid_params, "invalidaccoutparam");
    }
    ledger::pointer ledger = getapp().getops().getvalidatedledger();
    rippleaddress curaccountid = accountid;
    int counter = 2000;
    while (counter-- > 0)
    {
        sle::pointer sle = ledger->getslei (getaccountrootindex (curaccountid));
        if (!sle)
            break;
        json::value record;
        record["account"] = curaccountid.humanaccountid();
        std::uint32_t height = sle->isfieldpresent(sfreferenceheight) ? sle->getfieldu32(sfreferenceheight) : 0;
        record["height"] = to_string(height);
        if (height > 0)
        {
            rippleaddress refereeaccountid = sle->getfieldaccount(sfreferee);
            record["referee"] = refereeaccountid.humanaccountid();
            curaccountid = refereeaccountid;
        }
        result.append(record);
        if (height == 0)
        {
            break;
        }
    }
    if (result == json::nullvalue)
    {
        result["account"] = accountid.humanaccountid ();
        result            = rpcerror (rpcact_not_found, result);
    }
    return result;
}

} // ripple
