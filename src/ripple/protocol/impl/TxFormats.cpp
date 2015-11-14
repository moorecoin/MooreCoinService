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
#include <ripple/protocol/txformats.h>

namespace ripple {

txformats::txformats ()
{
    add ("accountset", ttaccount_set)
        << soelement (sfemailhash,           soe_optional)
        << soelement (sfwalletlocator,       soe_optional)
        << soelement (sfwalletsize,          soe_optional)
        << soelement (sfmessagekey,          soe_optional)
        << soelement (sfdomain,              soe_optional)
        << soelement (sftransferrate,        soe_optional)
        << soelement (sfsetflag,             soe_optional)
        << soelement (sfclearflag,           soe_optional)
        ;

    add ("trustset", tttrust_set)
        << soelement (sflimitamount,         soe_optional)
        << soelement (sfqualityin,           soe_optional)
        << soelement (sfqualityout,          soe_optional)
        ;

    add ("offercreate", ttoffer_create)
        << soelement (sftakerpays,           soe_required)
        << soelement (sftakergets,           soe_required)
        << soelement (sfexpiration,          soe_optional)
        << soelement (sfoffersequence,       soe_optional)
        ;

    add ("offercancel", ttoffer_cancel)
        << soelement (sfoffersequence,       soe_required)
        ;

    add ("setregularkey", ttregular_key_set)
        << soelement (sfregularkey,          soe_optional)
        ;

    add ("payment", ttpayment)
        << soelement (sfdestination,         soe_required)
        << soelement (sfamount,              soe_required)
        << soelement (sfsendmax,             soe_optional)
        << soelement (sfpaths,               soe_default)
        << soelement (sfinvoiceid,           soe_optional)
        << soelement (sfdestinationtag,      soe_optional)
        ;

    add ("enableamendment", ttamendment)
        << soelement (sfamendment,           soe_required)
        ;

    add ("setfee", ttfee)
        << soelement (sfbasefee,             soe_required)
        << soelement (sfreferencefeeunits,   soe_required)
        << soelement (sfreservebase,         soe_required)
        << soelement (sfreserveincrement,    soe_required)
        ;

    add ("ticketcreate", ttticket_create)
        << soelement (sftarget,              soe_optional)
        << soelement (sfexpiration,          soe_optional)
        ;

    add ("ticketcancel", ttticket_cancel)
        << soelement (sfticketid,            soe_required)
        ;

    add ("dividend", ttdividend)
        << soelement (sfdividendtype,        soe_required)
        << soelement (sfdividendledger,      soe_required)
        << soelement (sfdestination,         soe_optional)
        << soelement (sfdividendcoins,       soe_required)
        << soelement (sfdividendcoinsvbc,    soe_required)
        << soelement (sfdividendcoinsvbcrank,soe_optional)
        << soelement (sfdividendcoinsvbcsprd,soe_optional)
        << soelement (sfdividendvrank,       soe_optional)
        << soelement (sfdividendvsprd,       soe_optional)
        << soelement (sfdividendtsprd,       soe_optional)
        << soelement (sfdividendresulthash,  soe_optional)
        ;

    add("addreferee", ttaddreferee)
        << soelement(sfdestination,          soe_required)
        << soelement(sfamount,               soe_optional)
        ;

    add("activeaccount", ttactiveaccount)
        << soelement(sfreferee,              soe_required)
        << soelement(sfreference,            soe_required)
        << soelement(sfamount,               soe_optional)
        ;

    add("issue", ttissue)
        << soelement (sfdestination,         soe_required)
        << soelement (sfamount,              soe_required)
        << soelement (sfreleaseschedule,     soe_required)
        ;
}

void txformats::addcommonfields (item& item)
{
    item
        << soelement(sftransactiontype,     soe_required)
        << soelement(sfflags,               soe_optional)
        << soelement(sfsourcetag,           soe_optional)
        << soelement(sfaccount,             soe_required)
        << soelement(sfsequence,            soe_required)
        << soelement(sfprevioustxnid,       soe_optional) // deprecated: do not use
        << soelement(sflastledgersequence,  soe_optional)
        << soelement(sfaccounttxnid,        soe_optional)
        << soelement(sffee,                 soe_required)
        << soelement(sfoperationlimit,      soe_optional)
        << soelement(sfmemos,               soe_optional)
        << soelement(sfsigningpubkey,       soe_required)
        << soelement(sftxnsignature,        soe_optional)
        ;
}

txformats const&
txformats::getinstance ()
{
    static txformats instance;
    return instance;
}

} // ripple
