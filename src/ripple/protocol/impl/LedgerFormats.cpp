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
#include <ripple/protocol/ledgerformats.h>
#include <beast/module/core/memory/sharedsingleton.h>

namespace ripple {

ledgerformats::ledgerformats ()
{
    add ("accountroot", ltaccount_root)
            << soelement (sfaccount,             soe_required)
            << soelement (sfsequence,            soe_required)
            << soelement (sfbalance,             soe_required)
			<< soelement (sfbalancevbc,          soe_required)
            << soelement (sfownercount,          soe_required)
            << soelement (sfprevioustxnid,       soe_required)
            << soelement (sfprevioustxnlgrseq,   soe_required)
            << soelement (sfaccounttxnid,        soe_optional)
            << soelement (sfregularkey,          soe_optional)
            << soelement (sfemailhash,           soe_optional)
            << soelement (sfwalletlocator,       soe_optional)
            << soelement (sfwalletsize,          soe_optional)
            << soelement (sfmessagekey,          soe_optional)
            << soelement (sftransferrate,        soe_optional)
            << soelement (sfdomain,              soe_optional)
            << soelement (sfreferee,             soe_optional)
            << soelement (sfreferences,          soe_optional)
            << soelement (sfreferenceheight,     soe_optional)
            << soelement (sfdividendledger,      soe_optional)
            << soelement (sfdividendvrank,       soe_optional)
            << soelement (sfdividendvsprd,       soe_optional)
            << soelement (sfdividendtsprd,       soe_optional)
            ;
    
    add ("asset", ltasset)
            << soelement (sfamount,              soe_required)  // initial amount
            << soelement (sfregularkey,          soe_required)  // hot wallet
            << soelement (sfreleaseschedule,     soe_optional)
            << soelement (sfprevioustxnid,       soe_required)
            << soelement (sfprevioustxnlgrseq,   soe_required)
            ;

    add ("assetstate", ltasset_state)
            << soelement (sfaccount,             soe_required)  // asset holder
            << soelement (sfamount,              soe_required)  // initial amount
            << soelement (sfdeliveredamount,     soe_optional)  // amount delieverd
            << soelement (sfprevioustxnid,       soe_required)
            << soelement (sfprevioustxnlgrseq,   soe_required)
            << soelement (sflownode,             soe_optional)
            << soelement (sfhighnode,            soe_optional)
            << soelement (sfnextreleasetime,     soe_optional)
            ;
    
    add ("directorynode", ltdir_node)
            << soelement (sfowner,               soe_optional)  // for owner directories
            << soelement (sftakerpayscurrency,   soe_optional)  // for order book directories
            << soelement (sftakerpaysissuer,     soe_optional)  // for order book directories
            << soelement (sftakergetscurrency,   soe_optional)  // for order book directories
            << soelement (sftakergetsissuer,     soe_optional)  // for order book directories
            << soelement (sfexchangerate,        soe_optional)  // for order book directories
            << soelement (sfindexes,             soe_required)
            << soelement (sfrootindex,           soe_required)
            << soelement (sfindexnext,           soe_optional)
            << soelement (sfindexprevious,       soe_optional)
            ;

    add ("generatormap", ltgenerator_map)
            << soelement (sfgenerator,           soe_required)
            ;

    add ("offer", ltoffer)
            << soelement (sfaccount,             soe_required)
            << soelement (sfsequence,            soe_required)
            << soelement (sftakerpays,           soe_required)
            << soelement (sftakergets,           soe_required)
            << soelement (sfbookdirectory,       soe_required)
            << soelement (sfbooknode,            soe_required)
            << soelement (sfownernode,           soe_required)
            << soelement (sfprevioustxnid,       soe_required)
            << soelement (sfprevioustxnlgrseq,   soe_required)
            << soelement (sfexpiration,          soe_optional)
            ;

    add ("ripplestate", ltripple_state)
            << soelement (sfbalance,             soe_required)
            << soelement (sfreserve,             soe_optional)
            << soelement (sflowlimit,            soe_required)
            << soelement (sfhighlimit,           soe_required)
            << soelement (sfprevioustxnid,       soe_required)
            << soelement (sfprevioustxnlgrseq,   soe_required)
            << soelement (sflownode,             soe_optional)
            << soelement (sflowqualityin,        soe_optional)
            << soelement (sflowqualityout,       soe_optional)
            << soelement (sfhighnode,            soe_optional)
            << soelement (sfhighqualityin,       soe_optional)
            << soelement (sfhighqualityout,      soe_optional)
            ;

    add ("ledgerhashes", ltledger_hashes)
            << soelement (sffirstledgersequence, soe_optional) // remove if we do a ledger restart
            << soelement (sflastledgersequence,  soe_optional)
            << soelement (sfhashes,              soe_required)
            ;

    add ("enabledamendments", ltamendments)
            << soelement (sfamendments, soe_required)
            ;

    add ("feesettings", ltfee_settings)
            << soelement (sfbasefee,             soe_required)
            << soelement (sfreferencefeeunits,   soe_required)
            << soelement (sfreservebase,         soe_required)
            << soelement (sfreserveincrement,    soe_required)
            ;

    add ("ticket", ltticket)
            << soelement (sfaccount,             soe_required)
            << soelement (sfsequence,            soe_required)
            << soelement (sfownernode,           soe_required)
            << soelement (sftarget,              soe_optional)
            << soelement (sfexpiration,          soe_optional)
            ;

    add ("dividend", ltdividend)
            << soelement (sfdividendstate,       soe_required)
            << soelement (sfdividendledger,      soe_required)
            << soelement (sfdividendcoins,       soe_required)
            << soelement (sfdividendcoinsvbc,    soe_required)
            << soelement (sfdividendvrank,       soe_optional)
            << soelement (sfdividendvsprd,       soe_optional)
            << soelement (sfdividendresulthash,  soe_optional)
            ;

    add("refer", ltrefer)
            << soelement (sfaccount,             soe_optional)
            << soelement (sfreferences,          soe_optional)
            ;
}

void ledgerformats::addcommonfields (item& item)
{
    item
        << soelement(sfledgerindex,             soe_optional)
        << soelement(sfledgerentrytype,         soe_required)
        << soelement(sfflags,                   soe_required)
        ;
}

ledgerformats const&
ledgerformats::getinstance ()
{
    static ledgerformats instance;
    return instance;
}

} // ripple
