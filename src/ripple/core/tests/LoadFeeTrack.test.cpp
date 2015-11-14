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
#include <ripple/core/impl/loadfeetrackimp.h>
#include <ripple/core/config.h>
#include <beast/unit_test/suite.h>

namespace ripple {

class loadfeetrack_test : public beast::unit_test::suite
{
public:
    void run ()
    {
        config d; // get a default configuration object
        loadfeetrackimp l;

        expect (l.scalefeebase (10000, d.fee_default, d.transaction_fee_base) == 10000);
        expect (l.scalefeeload (10000, d.fee_default, d.transaction_fee_base, false) == 10000);
        expect (l.scalefeebase (1, d.fee_default, d.transaction_fee_base) == 1);
        expect (l.scalefeeload (1, d.fee_default, d.transaction_fee_base, false) == 1);

        // check new default fee values give same fees as old defaults
        expect (l.scalefeebase (d.fee_default, d.fee_default, d.transaction_fee_base) == 1000);
        expect (l.scalefeebase (d.fee_account_reserve, d.fee_default, d.transaction_fee_base) == 0 * system_currency_parts);
        expect (l.scalefeebase (d.fee_owner_reserve, d.fee_default, d.transaction_fee_base) == 0 * system_currency_parts);
        expect (l.scalefeebase (d.fee_offer, d.fee_default, d.transaction_fee_base) == 1000);
        expect (l.scalefeebase (d.fee_contract_operation, d.fee_default, d.transaction_fee_base) == 1);
    }
};

beast_define_testsuite(loadfeetrack,ripple_core,ripple);

} // ripple
