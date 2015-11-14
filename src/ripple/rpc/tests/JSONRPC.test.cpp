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
#include <ripple/app/paths/findpaths.h>
#include <ripple/basics/stringutilities.h>
#include <ripple/json/json_reader.h>
#include <ripple/protocol/txflags.h>
#include <ripple/rpc/impl/transactionsign.h>
#include <beast/unit_test.h>

namespace ripple {

namespace rpc {

// struct used to test calls to transactionsign and transactionsubmit.
struct txntestdata
{
    // gah, without constexpr i can't make this an enum and initialize
    // or operators at compile time.  punting with integer constants.
    static unsigned int const allgood         = 0x0;
    static unsigned int const signfail        = 0x1;
    static unsigned int const submitfail      = 0x2;

    char const* const json;
    unsigned int result;

    txntestdata () = delete;
    txntestdata (txntestdata const&) = delete;
    txntestdata& operator= (txntestdata const&) = delete;
    txntestdata (char const* jsonin, unsigned int resultin)
    : json (jsonin)
    , result (resultin)
    { }
};

// declare storage for statics to avoid link errors.
unsigned int const txntestdata::allgood;
unsigned int const txntestdata::signfail;
unsigned int const txntestdata::submitfail;


static txntestdata const txntestarray [] =
{

// minimal payment.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "tx_json": {
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::allgood},

// pass in fee with minimal payment.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "tx_json": {
        "fee": 10,
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::allgood},

// pass in sequence.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "tx_json": {
        "sequence": 0,
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::allgood},

// pass in sequence and fee with minimal payment.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "tx_json": {
        "sequence": 0,
        "fee": 10,
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::allgood},

// add "fee_mult_max" field.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "fee_mult_max": 7,
    "tx_json": {
        "sequence": 0,
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::allgood},

// "fee_mult_max is ignored if "fee" is present.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "fee_mult_max": 0,
    "tx_json": {
        "sequence": 0,
        "fee": 10,
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::allgood},

// invalid "fee_mult_max" field.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "fee_mult_max": "notafeemultiplier",
    "tx_json": {
        "sequence": 0,
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::signfail | txntestdata::submitfail},

// invalid value for "fee_mult_max" field.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "fee_mult_max": 0,
    "tx_json": {
        "sequence": 0,
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::signfail | txntestdata::submitfail},

// missing "amount".
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "tx_json": {
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::signfail | txntestdata::submitfail},

// invalid "amount".
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "tx_json": {
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "notanamount",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::signfail | txntestdata::submitfail},

// missing "destination".
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "tx_json": {
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "transactiontype": "payment"
    }
})", txntestdata::signfail | txntestdata::submitfail},

// invalid "destination".
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "tx_json": {
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "notadestination",
        "transactiontype": "payment"
    }
})", txntestdata::signfail | txntestdata::submitfail},

// cannot create xrp to xrp paths.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "build_path": 1,
    "tx_json": {
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::signfail | txntestdata::submitfail},

// successful "build_path".
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "build_path": 1,
    "tx_json": {
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": {
            "value": "10",
            "currency": "usd",
            "issuer": "0123456789012345678901234567890123456789"
        },
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::allgood},

// not valid to include both "paths" and "build_path".
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "build_path": 1,
    "tx_json": {
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": {
            "value": "10",
            "currency": "usd",
            "issuer": "0123456789012345678901234567890123456789"
        },
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "paths": "",
        "transactiontype": "payment"
    }
})", txntestdata::signfail | txntestdata::submitfail},

// successful "sendmax".
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "build_path": 1,
    "tx_json": {
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": {
            "value": "10",
            "currency": "usd",
            "issuer": "0123456789012345678901234567890123456789"
        },
        "sendmax": {
            "value": "5",
            "currency": "usd",
            "issuer": "0123456789012345678901234567890123456789"
        },
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::allgood},

// even though "amount" may not be xrp for pathfinding, "sendmax" may be xrp.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "build_path": 1,
    "tx_json": {
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": {
            "value": "10",
            "currency": "usd",
            "issuer": "0123456789012345678901234567890123456789"
        },
        "sendmax": 10000,
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::allgood},

// "secret" must be present.
{r"({
    "command": "submit",
    "tx_json": {
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::signfail | txntestdata::submitfail},

// "secret" must be non-empty.
{r"({
    "command": "submit",
    "secret": "",
    "tx_json": {
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::signfail | txntestdata::submitfail},

// "tx_json" must be present.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "rx_json": {
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::signfail | txntestdata::submitfail},

// "transactiontype" must be present.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "tx_json": {
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
    }
})", txntestdata::signfail | txntestdata::submitfail},

// the "transactiontype" must be one of the pre-established transaction types.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "tx_json": {
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "tt"
    }
})", txntestdata::signfail | txntestdata::submitfail},

// the "transactiontype", however, may be represented with an integer.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "tx_json": {
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": 0
    }
})", txntestdata::allgood},

// "account" must be present.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "tx_json": {
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::signfail | txntestdata::submitfail},

// "account" must be well formed.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "tx_json": {
        "account": "notanaccount",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::signfail | txntestdata::submitfail},

// the "offline" tag may be added to the transaction.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "offline": 0,
    "tx_json": {
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::allgood},

// if "offline" is true then a "sequence" field must be supplied.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "offline": 1,
    "tx_json": {
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::signfail | txntestdata::submitfail},

// valid transaction if "offline" is true.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "offline": 1,
    "tx_json": {
        "sequence": 0,
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::allgood},

// a "flags' field may be specified.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "tx_json": {
        "flags": 0,
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::allgood},

// the "flags" field must be numeric.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "tx_json": {
        "flags": "notgoodflags",
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::signfail | txntestdata::submitfail},

// it's okay to add a "debug_signing" field.
{r"({
    "command": "submit",
    "secret": "masterpassphrase",
    "debug_signing": 0,
    "tx_json": {
        "account": "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth",
        "amount": "1000000000",
        "destination": "rnuy2shtrb9dubspmkjzuxtf5fcndgryea",
        "transactiontype": "payment"
    }
})", txntestdata::allgood},

};

class jsonrpc_test : public beast::unit_test::suite
{
public:
    void testautofillfees ()
    {
        rippleaddress rootseedmaster
                = rippleaddress::createseedgeneric ("masterpassphrase");
        rippleaddress rootgeneratormaster
                = rippleaddress::creategeneratorpublic (rootseedmaster);
        rippleaddress rootaddress
                = rippleaddress::createaccountpublic (rootgeneratormaster, 0);
        std::uint64_t startamount (100000);
        ledger::pointer ledger (std::make_shared <ledger> (
            rootaddress, startamount, startamount));

        using namespace rpcdetail;
        ledgerfacade facade (ledgerfacade::nonetops, ledger);

       {
            json::value req;
            json::value result;
            json::reader ().parse (
                r"({ "fee_mult_max" : 1, "tx_json" : { } } )"
                , req);
            autofill_fee (req, facade, result, true);

            expect (! contains_error (result));
        }

        {
            json::value req;
            json::value result;
            json::reader ().parse (
                r"({ "fee_mult_max" : 0, "tx_json" : { } } )"
                , req);
            autofill_fee (req, facade, result, true);

            expect (contains_error (result));
        }
    }

    void testtransactionrpc ()
    {
        // this loop is forward-looking for when there are separate
        // transactionsign () and transcationsubmit () functions.  for now
        // they just have a bool (false = sign, true = submit) and a flag
        // to help classify failure types.
        using teststuff = std::pair <bool, unsigned int>;
        static teststuff const testfuncs [] =
        {
            teststuff {false, txntestdata::signfail},
            teststuff {true,  txntestdata::submitfail},
        };

        for (auto testfunc : testfuncs)
        {
            // for each json test.
            for (auto const& txntest : txntestarray)
            {
                json::value req;
                json::reader ().parse (txntest.json, req);
                if (contains_error (req))
                    throw std::runtime_error (
                        "internal jsonrpc_test error.  bad test json.");

                static role const testedroles[] =
                    {role::guest, role::user, role::admin, role::forbid};

                for (role testrole : testedroles)
                {
                    // mock so we can run without a ledger.
                    rpcdetail::ledgerfacade fakenetops (
                        rpcdetail::ledgerfacade::nonetops);

                    json::value result = transactionsign (
                        req,
                        testfunc.first,
                        true,
                        fakenetops,
                        testrole);

                    expect (contains_error (result) ==
                        static_cast <bool> (txntest.result & testfunc.second));
                }
            }
        }
    }

    void run ()
    {
        testautofillfees ();
        testtransactionrpc ();
    }
};

beast_define_testsuite(jsonrpc,ripple_app,ripple);

} // rpc
} // ripple
