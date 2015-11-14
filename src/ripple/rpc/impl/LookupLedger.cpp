//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012-2014 ripple labs inc.

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
#include <ripple/rpc/impl/lookupledger.h>

namespace ripple {
namespace rpc {

static const int ledger_current = -1;
static const int ledger_closed = -2;
static const int ledger_validated = -3;

// the previous version of the lookupledger command would accept the
// "ledger_index" argument as a string and silently treat it as a request to
// return the current ledger which, while not strictly wrong, could cause a lot
// of confusion.
//
// the code now robustly validates the input and ensures that the only possible
// values for the "ledger_index" parameter are the index of a ledger passed as
// an integer or one of the strings "current", "closed" or "validated".
// additionally, the code ensures that the value passed in "ledger_hash" is a
// string and a valid hash. invalid values will return an appropriate error
// code.
//
// in the absence of the "ledger_hash" or "ledger_index" parameters, the code
// assumes that "ledger_index" has the value "current".
//
// returns a json::objectvalue.  if there was an error, it will be in that
// return value.  otherwise, the object contains the field "validated" and
// optionally the fields "ledger_hash", "ledger_index" and
// "ledger_current_index", if they are defined.
status lookupledger (
    json::value const& params,
    ledger::pointer& ledger,
    networkops& netops,
    json::value& jsonresult)
{
    using rpc::make_error;
    ledger.reset();

    auto jsonhash = params.get (jss::ledger_hash, json::value ("0"));
    auto jsonindex = params.get (jss::ledger_index, json::value ("current"));

    // support for deprecated "ledger" - attempt to deduce our input
    if (params.ismember (jss::ledger))
    {
        if (params[jss::ledger].asstring ().size () > 12)
        {
            jsonhash = params[jss::ledger];
            jsonindex = json::value ("");
        }
        else
        {
            jsonindex = params[jss::ledger];
            jsonhash = json::value ("0");
        }
    }

    uint256 ledgerhash;

    if (!jsonhash.isstring() || !ledgerhash.sethex (jsonhash.asstring ()))
        return {rpcinvalid_params, "ledgerhashmalformed"};

    std::int32_t ledgerindex = ledger_current;

    // we only try to parse a ledger index if we have not already
    // determined that we have a ledger hash.
    if (ledgerhash == zero)
    {
        if (jsonindex.isnumeric ())
        {
            ledgerindex = jsonindex.asint ();
        }
        else
        {
            std::string index = jsonindex.asstring ();

            if (index == "current")
                ledgerindex = ledger_current;
            else if (index == "closed")
                ledgerindex = ledger_closed;
            else if (index == "validated")
                ledgerindex = ledger_validated;
            else
                return {rpcinvalid_params, "ledgerindexmalformed"};
        }
    }
    else
    {
        ledger = netops.getledgerbyhash (ledgerhash);

        if (!ledger)
            return {rpclgr_not_found, "ledgernotfound"};

        ledgerindex = ledger->getledgerseq ();
    }

    int ledgerrequest = 0;

    if (ledgerindex <= 0) {
        switch (ledgerindex)
        {
        case ledger_current:
            ledger = netops.getcurrentledger ();
            break;

        case ledger_closed:
            ledger = getapp().getledgermaster ().getclosedledger ();
            break;

        case ledger_validated:
            ledger = netops.getvalidatedledger ();
            break;

        default:
            return {rpcinvalid_params, "ledgerindexmalformed"};
        }

        assert (ledger->isimmutable());
        assert (ledger->isclosed() == (ledgerindex != ledger_current));
        ledgerrequest = ledgerindex;
        ledgerindex = ledger->getledgerseq ();
    }

    if (!ledger)
    {
        ledger = netops.getledgerbyseq (ledgerindex);

        if (!ledger)
            return {rpclgr_not_found, "ledgernotfound"};
    }

    if (ledger->isclosed ())
    {
        if (ledgerhash != zero)
            jsonresult[jss::ledger_hash] = to_string (ledgerhash);

        jsonresult[jss::ledger_index] = ledgerindex;
    }
    else
    {
        jsonresult[jss::ledger_current_index] = ledgerindex;
    }

    if (ledger->isvalidated ())
    {
        jsonresult[jss::validated] = true;
    }
    else if (!ledger->isclosed ())
    {
        jsonresult[jss::validated] = false;
    }
    else
    {
        try
        {
            // use the skip list in the last validated ledger to see if ledger
            // comes after the last validated ledger (and thus has been
            // validated).
            auto next = getapp().getledgermaster ().walkhashbyseq (ledgerindex);
            if (ledgerhash == next)
            {
                ledger->setvalidated();
                jsonresult[jss::validated] = true;
            }
            else
            {
                jsonresult[jss::validated] = false;
            }
        }
        catch (shamapmissingnode const&)
        {
            jsonresult[jss::validated] = false;
        }
    }

    return status::ok;
}

json::value lookupledger (
    json::value const& params,
    ledger::pointer& ledger,
    networkops& netops)
{
    json::value value (json::objectvalue);
    if (auto status = lookupledger (params, ledger, netops, value))
        status.inject (value);

    return value;
}

} // rpc
} // ripple
