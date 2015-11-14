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
#include <ripple/protocol/sfield.h>
#include <map>
#include <memory>
#include <string>
#include <utility>

namespace ripple {

// these must stay at the top of this file, and in this order
// files-cope statics are preferred here because the sfields must be
// file-scope.  the following 3 objects must have scope prior to
// the file-scope sfields.
static std::mutex sfield_mutex;
static std::map<int, sfield::ptr> knowncodetofield;
static std::map<int, std::unique_ptr<sfield const>> unknowncodetofield;

int sfield::num = 0;

typedef std::lock_guard <std::mutex> staticscopedlocktype;

// give this translation unit only, permission to construct sfields
struct sfield::make
{
#ifndef _msc_ver
    template <class ...args>
    static sfield one(sfield const* p, args&& ...args)
    {
        sfield result(std::forward<args>(args)...);
        knowncodetofield[result.fieldcode] = p;
        return result;
    }
#else  // remove this when vs gets variadic templates
    template <class a0>
    static sfield one(sfield const* p, a0&& arg0)
    {
        sfield result(std::forward<a0>(arg0));
        knowncodetofield[result.fieldcode] = p;
        return result;
    }

    template <class a0, class a1, class a2>
    static sfield one(sfield const* p, a0&& arg0, a1&& arg1, a2&& arg2)
    {
        sfield result(std::forward<a0>(arg0), std::forward<a1>(arg1),
                      std::forward<a2>(arg2));
        knowncodetofield[result.fieldcode] = p;
        return result;
    }

    template <class a0, class a1, class a2, class a3>
    static sfield one(sfield const* p, a0&& arg0, a1&& arg1, a2&& arg2,
                                       a3&& arg3)
    {
        sfield result(std::forward<a0>(arg0), std::forward<a1>(arg1),
                      std::forward<a2>(arg2), std::forward<a3>(arg3));
        knowncodetofield[result.fieldcode] = p;
        return result;
    }

    template <class a0, class a1, class a2, class a3, class a4>
    static sfield one(sfield const* p, a0&& arg0, a1&& arg1, a2&& arg2,
                                       a3&& arg3, a4&& arg4)
    {
        sfield result(std::forward<a0>(arg0), std::forward<a1>(arg1),
                      std::forward<a2>(arg2), std::forward<a3>(arg3),
                      std::forward<a4>(arg4));
        knowncodetofield[result.fieldcode] = p;
        return result;
    }
#endif
};

using make = sfield::make;

// construct all compile-time sfields, and register them in the knowncodetofield
// database:

sfield const sfinvalid     = make::one(&sfinvalid, -1);
sfield const sfgeneric     = make::one(&sfgeneric, 0);
sfield const sfledgerentry = make::one(&sfledgerentry, sti_ledgerentry, 257, "ledgerentry");
sfield const sftransaction = make::one(&sftransaction, sti_transaction, 257, "transaction");
sfield const sfvalidation  = make::one(&sfvalidation,  sti_validation,  257, "validation");
sfield const sfmetadata    = make::one(&sfmetadata,    sti_metadata,    257, "metadata");
sfield const sfhash        = make::one(&sfhash,        sti_hash256,     257, "hash");
sfield const sfindex       = make::one(&sfindex,       sti_hash256,     258, "index");

// 8-bit integers
sfield const sfcloseresolution   = make::one(&sfcloseresolution,   sti_uint8, 1, "closeresolution");
sfield const sftemplateentrytype = make::one(&sftemplateentrytype, sti_uint8, 2, "templateentrytype");
sfield const sftransactionresult = make::one(&sftransactionresult, sti_uint8, 3, "transactionresult");

sfield const sfdividendstate    = make::one(&sfdividendstate,    sti_uint8, 181, "dividendstate");
sfield const sfdividendtype     = make::one(&sfdividendtype,     sti_uint8, 182, "dividendtype");

// 16-bit integers
sfield const sfledgerentrytype = make::one(&sfledgerentrytype, sti_uint16, 1, "ledgerentrytype", sfield::smd_never);
sfield const sftransactiontype = make::one(&sftransactiontype, sti_uint16, 2, "transactiontype");

// 32-bit integers (common)
sfield const sfflags             = make::one(&sfflags,             sti_uint32,  2, "flags");
sfield const sfsourcetag         = make::one(&sfsourcetag,         sti_uint32,  3, "sourcetag");
sfield const sfsequence          = make::one(&sfsequence,          sti_uint32,  4, "sequence");
sfield const sfprevioustxnlgrseq = make::one(&sfprevioustxnlgrseq, sti_uint32,  5, "previoustxnlgrseq", sfield::smd_deletefinal);
sfield const sfledgersequence    = make::one(&sfledgersequence,    sti_uint32,  6, "ledgersequence");
sfield const sfclosetime         = make::one(&sfclosetime,         sti_uint32,  7, "closetime");
sfield const sfparentclosetime   = make::one(&sfparentclosetime,   sti_uint32,  8, "parentclosetime");
sfield const sfsigningtime       = make::one(&sfsigningtime,       sti_uint32,  9, "signingtime");
sfield const sfexpiration        = make::one(&sfexpiration,        sti_uint32, 10, "expiration");
sfield const sftransferrate      = make::one(&sftransferrate,      sti_uint32, 11, "transferrate");
sfield const sfwalletsize        = make::one(&sfwalletsize,        sti_uint32, 12, "walletsize");
sfield const sfownercount        = make::one(&sfownercount,        sti_uint32, 13, "ownercount");
sfield const sfdestinationtag    = make::one(&sfdestinationtag,    sti_uint32, 14, "destinationtag");

sfield const sfdividendledger    = make::one(&sfdividendledger,    sti_uint32, 181, "dividendledger");
sfield const sfreferenceheight   = make::one(&sfreferenceheight,   sti_uint32, 182, "referenceheight");
sfield const sfreleaserate       = make::one(&sfreleaserate,       sti_uint32, 183, "releaserate");
sfield const sfnextreleasetime   = make::one(&sfnextreleasetime,   sti_uint32, 184, "nextreleasetime");

// 32-bit integers (uncommon)
sfield const sfhighqualityin       = make::one(&sfhighqualityin,       sti_uint32, 16, "highqualityin");
sfield const sfhighqualityout      = make::one(&sfhighqualityout,      sti_uint32, 17, "highqualityout");
sfield const sflowqualityin        = make::one(&sflowqualityin,        sti_uint32, 18, "lowqualityin");
sfield const sflowqualityout       = make::one(&sflowqualityout,       sti_uint32, 19, "lowqualityout");
sfield const sfqualityin           = make::one(&sfqualityin,           sti_uint32, 20, "qualityin");
sfield const sfqualityout          = make::one(&sfqualityout,          sti_uint32, 21, "qualityout");
sfield const sfstampescrow         = make::one(&sfstampescrow,         sti_uint32, 22, "stampescrow");
sfield const sfbondamount          = make::one(&sfbondamount,          sti_uint32, 23, "bondamount");
sfield const sfloadfee             = make::one(&sfloadfee,             sti_uint32, 24, "loadfee");
sfield const sfoffersequence       = make::one(&sfoffersequence,       sti_uint32, 25, "offersequence");
sfield const sffirstledgersequence = make::one(&sffirstledgersequence, sti_uint32, 26, "firstledgersequence");  // deprecated: do not use
sfield const sflastledgersequence  = make::one(&sflastledgersequence,  sti_uint32, 27, "lastledgersequence");
sfield const sftransactionindex    = make::one(&sftransactionindex,    sti_uint32, 28, "transactionindex");
sfield const sfoperationlimit      = make::one(&sfoperationlimit,      sti_uint32, 29, "operationlimit");
sfield const sfreferencefeeunits   = make::one(&sfreferencefeeunits,   sti_uint32, 30, "referencefeeunits");
sfield const sfreservebase         = make::one(&sfreservebase,         sti_uint32, 31, "reservebase");
sfield const sfreserveincrement    = make::one(&sfreserveincrement,    sti_uint32, 32, "reserveincrement");
sfield const sfsetflag             = make::one(&sfsetflag,             sti_uint32, 33, "setflag");
sfield const sfclearflag           = make::one(&sfclearflag,           sti_uint32, 34, "clearflag");

// 64-bit integers
sfield const sfindexnext     = make::one(&sfindexnext,     sti_uint64, 1, "indexnext");
sfield const sfindexprevious = make::one(&sfindexprevious, sti_uint64, 2, "indexprevious");
sfield const sfbooknode      = make::one(&sfbooknode,      sti_uint64, 3, "booknode");
sfield const sfownernode     = make::one(&sfownernode,     sti_uint64, 4, "ownernode");
sfield const sfbasefee       = make::one(&sfbasefee,       sti_uint64, 5, "basefee");
sfield const sfexchangerate  = make::one(&sfexchangerate,  sti_uint64, 6, "exchangerate");
sfield const sflownode       = make::one(&sflownode,       sti_uint64, 7, "lownode");
sfield const sfhighnode      = make::one(&sfhighnode,      sti_uint64, 8, "highnode");

sfield const sfdividendcoinsvbc = make::one(&sfdividendcoinsvbc, sti_uint64, 181, "dividendcoinsvbc");
sfield const sfdividendcoinsvbcrank = make::one(&sfdividendcoinsvbcrank, sti_uint64, 182, "dividendcoinsvbcrank");
sfield const sfdividendcoinsvbcsprd = make::one(&sfdividendcoinsvbcsprd, sti_uint64, 183, "dividendcoinsvbcsprd");
sfield const sfdividendvrank    = make::one(&sfdividendvrank,    sti_uint64, 184, "dividendvrank");
sfield const sfdividendvsprd    = make::one(&sfdividendvsprd,    sti_uint64, 185, "dividendvsprd");
sfield const sfdividendcoins = make::one(&sfdividendcoins, sti_uint64, 186, "dividendcoins");
sfield const sfdividendtsprd    = make::one(&sfdividendtsprd,    sti_uint64, 187, "dividendtsprd");

// 128-bit
sfield const sfemailhash = make::one(&sfemailhash, sti_hash128, 1, "emailhash");

// 256-bit (common)
sfield const sfledgerhash      = make::one(&sfledgerhash,      sti_hash256, 1, "ledgerhash");
sfield const sfparenthash      = make::one(&sfparenthash,      sti_hash256, 2, "parenthash");
sfield const sftransactionhash = make::one(&sftransactionhash, sti_hash256, 3, "transactionhash");
sfield const sfaccounthash     = make::one(&sfaccounthash,     sti_hash256, 4, "accounthash");
sfield const sfprevioustxnid   = make::one(&sfprevioustxnid,   sti_hash256, 5, "previoustxnid", sfield::smd_deletefinal);
sfield const sfledgerindex     = make::one(&sfledgerindex,     sti_hash256, 6, "ledgerindex");
sfield const sfwalletlocator   = make::one(&sfwalletlocator,   sti_hash256, 7, "walletlocator");
sfield const sfrootindex       = make::one(&sfrootindex,       sti_hash256, 8, "rootindex", sfield::smd_always);
sfield const sfaccounttxnid    = make::one(&sfaccounttxnid,    sti_hash256, 9, "accounttxnid");
sfield const sfdividendresulthash = make::one(&sfdividendresulthash, sti_hash256, 181, "dividendresulthash");
    
// 256-bit (uncommon)
sfield const sfbookdirectory = make::one(&sfbookdirectory, sti_hash256, 16, "bookdirectory");
sfield const sfinvoiceid     = make::one(&sfinvoiceid,     sti_hash256, 17, "invoiceid");
sfield const sfnickname      = make::one(&sfnickname,      sti_hash256, 18, "nickname");
sfield const sfamendment     = make::one(&sfamendment,     sti_hash256, 19, "amendment");
sfield const sfticketid      = make::one(&sfticketid,      sti_hash256, 20, "ticketid");

// 160-bit (common)
sfield const sftakerpayscurrency = make::one(&sftakerpayscurrency, sti_hash160, 1, "takerpayscurrency");
sfield const sftakerpaysissuer   = make::one(&sftakerpaysissuer,   sti_hash160, 2, "takerpaysissuer");
sfield const sftakergetscurrency = make::one(&sftakergetscurrency, sti_hash160, 3, "takergetscurrency");
sfield const sftakergetsissuer   = make::one(&sftakergetsissuer,   sti_hash160, 4, "takergetsissuer");

// currency amount (common)
sfield const sfamount      = make::one(&sfamount,      sti_amount, 1, "amount");
sfield const sfbalance     = make::one(&sfbalance,     sti_amount, 2, "balance");
sfield const sflimitamount = make::one(&sflimitamount, sti_amount, 3, "limitamount");
sfield const sftakerpays   = make::one(&sftakerpays,   sti_amount, 4, "takerpays");
sfield const sftakergets   = make::one(&sftakergets,   sti_amount, 5, "takergets");
sfield const sflowlimit    = make::one(&sflowlimit,    sti_amount, 6, "lowlimit");
sfield const sfhighlimit   = make::one(&sfhighlimit,   sti_amount, 7, "highlimit");
sfield const sffee         = make::one(&sffee,         sti_amount, 8, "fee");
sfield const sfsendmax     = make::one(&sfsendmax,     sti_amount, 9, "sendmax");

sfield const sfbalancevbc  = make::one(&sfbalancevbc,  sti_amount, 181, "balancevbc");

// currency amount (uncommon)
sfield const sfminimumoffer    = make::one(&sfminimumoffer,    sti_amount, 16, "minimumoffer");
sfield const sfrippleescrow    = make::one(&sfrippleescrow,    sti_amount, 17, "rippleescrow");
sfield const sfdeliveredamount = make::one(&sfdeliveredamount, sti_amount, 18, "deliveredamount");

sfield const sfreserve         = make::one(&sfreserve,         sti_amount, 182, "reserve");

// variable length
sfield const sfpublickey     = make::one(&sfpublickey,     sti_vl,  1, "publickey");
sfield const sfmessagekey    = make::one(&sfmessagekey,    sti_vl,  2, "messagekey");
sfield const sfsigningpubkey = make::one(&sfsigningpubkey, sti_vl,  3, "signingpubkey");
sfield const sftxnsignature  = make::one(&sftxnsignature,  sti_vl,  4, "txnsignature", sfield::smd_default, false);
sfield const sfgenerator     = make::one(&sfgenerator,     sti_vl,  5, "generator");
sfield const sfsignature     = make::one(&sfsignature,     sti_vl,  6, "signature", sfield::smd_default, false);
sfield const sfdomain        = make::one(&sfdomain,        sti_vl,  7, "domain");
sfield const sffundcode      = make::one(&sffundcode,      sti_vl,  8, "fundcode");
sfield const sfremovecode    = make::one(&sfremovecode,    sti_vl,  9, "removecode");
sfield const sfexpirecode    = make::one(&sfexpirecode,    sti_vl, 10, "expirecode");
sfield const sfcreatecode    = make::one(&sfcreatecode,    sti_vl, 11, "createcode");
sfield const sfmemotype      = make::one(&sfmemotype,      sti_vl, 12, "memotype");
sfield const sfmemodata      = make::one(&sfmemodata,      sti_vl, 13, "memodata");
sfield const sfmemoformat    = make::one(&sfmemoformat,    sti_vl, 14, "memoformat");

// account
sfield const sfaccount     = make::one(&sfaccount,     sti_account, 1, "account");
sfield const sfowner       = make::one(&sfowner,       sti_account, 2, "owner");
sfield const sfdestination = make::one(&sfdestination, sti_account, 3, "destination");
sfield const sfissuer      = make::one(&sfissuer,      sti_account, 4, "issuer");
sfield const sftarget      = make::one(&sftarget,      sti_account, 7, "target");
sfield const sfregularkey  = make::one(&sfregularkey,  sti_account, 8, "regularkey");

sfield const sfreferee     = make::one(&sfreferee,     sti_account, 181, "referee");
sfield const sfreference   = make::one(&sfreference,   sti_account, 182, "reference");

// path set
sfield const sfpaths = make::one(&sfpaths, sti_pathset, 1, "paths");

// vector of 256-bit
sfield const sfindexes    = make::one(&sfindexes,    sti_vector256, 1, "indexes", sfield::smd_never);
sfield const sfhashes     = make::one(&sfhashes,     sti_vector256, 2, "hashes");
sfield const sfamendments = make::one(&sfamendments, sti_vector256, 3, "amendments");

// inner object
// object/1 is reserved for end of object
sfield const sftransactionmetadata = make::one(&sftransactionmetadata, sti_object,  2, "transactionmetadata");
sfield const sfcreatednode         = make::one(&sfcreatednode,         sti_object,  3, "creatednode");
sfield const sfdeletednode         = make::one(&sfdeletednode,         sti_object,  4, "deletednode");
sfield const sfmodifiednode        = make::one(&sfmodifiednode,        sti_object,  5, "modifiednode");
sfield const sfpreviousfields      = make::one(&sfpreviousfields,      sti_object,  6, "previousfields");
sfield const sffinalfields         = make::one(&sffinalfields,         sti_object,  7, "finalfields");
sfield const sfnewfields           = make::one(&sfnewfields,           sti_object,  8, "newfields");
sfield const sftemplateentry       = make::one(&sftemplateentry,       sti_object,  9, "templateentry");
sfield const sfmemo                = make::one(&sfmemo,                sti_object, 10, "memo");

sfield const sfreferenceholder     = make::one(&sfreferenceholder,     sti_object, 181, "referenceholder");
sfield const sffeesharetaker       = make::one(&sffeesharetaker,       sti_object, 182, "feesharetaker");
sfield const sfreleasepoint        = make::one(&sfreleasepoint,        sti_object, 183, "releasepoint");

// array of objects
// array/1 is reserved for end of array
sfield const sfsigningaccounts = make::one(&sfsigningaccounts, sti_array, 2, "signingaccounts");
sfield const sftxnsignatures   = make::one(&sftxnsignatures,   sti_array, 3, "txnsignatures", sfield::smd_default, false);
sfield const sfsignatures      = make::one(&sfsignatures,      sti_array, 4, "signatures");
sfield const sftemplate        = make::one(&sftemplate,        sti_array, 5, "template");
sfield const sfnecessary       = make::one(&sfnecessary,       sti_array, 6, "necessary");
sfield const sfsufficient      = make::one(&sfsufficient,      sti_array, 7, "sufficient");
sfield const sfaffectednodes   = make::one(&sfaffectednodes,   sti_array, 8, "affectednodes");
sfield const sfmemos           = make::one(&sfmemos,           sti_array, 9, "memos");

sfield const sfreferences      = make::one(&sfreferences,      sti_array, 181, "references");
sfield const sffeesharetakers  = make::one(&sffeesharetakers,  sti_array, 182, "feesharetakers");
sfield const sfreleaseschedule = make::one(&sfreleaseschedule, sti_array, 183, "releaseschedule");

sfield::sfield (serializedtypeid tid, int fv, const char* fn,
                int meta, bool signing)
    : fieldcode (field_code (tid, fv))
    , fieldtype (tid)
    , fieldvalue (fv)
    , fieldname (fn)
    , fieldmeta (meta)
    , fieldnum (++num)
    , signingfield (signing)
    , rawjsonname (getname ())
    , jsonname (rawjsonname.c_str ())
{
}

sfield::sfield (int fc)
    : fieldcode (fc)
    , fieldtype (sti_unknown)
    , fieldvalue (0)
    , fieldmeta (smd_never)
    , fieldnum (++num)
    , signingfield (true)
    , rawjsonname (getname ())
    , jsonname (rawjsonname.c_str ())
{
}

// call with the map mutex to protect num.
// this is naturally done with no extra expense
// from getfield(int code).
sfield::sfield (serializedtypeid tid, int fv)
        : fieldcode (field_code (tid, fv)), fieldtype (tid), fieldvalue (fv),
          fieldmeta (smd_default),
          fieldnum (++num),
          signingfield (true),
          jsonname (nullptr)
{
    fieldname = std::to_string (tid) + '/' + std::to_string (fv);
    rawjsonname = getname ();
    jsonname = json::staticstring (rawjsonname.c_str ());
    assert ((fv != 1) || ((tid != sti_array) && (tid != sti_object)));
}

sfield::ref sfield::getfield (int code)
{
    auto it = knowncodetofield.find (code);

    if (it != knowncodetofield.end ())
    {
        // 99+% of the time, it will be a valid, known field
        return * (it->second);
    }

    int type = code >> 16;
    int field = code & 0xffff;

    // don't dynamically extend types that have no binary encoding.
    if ((field > 255) || (code < 0))
        return sfinvalid;

    switch (type)
    {
    // types we are willing to dynamically extend
    // types (common)
    case sti_uint16:
    case sti_uint32:
    case sti_uint64:
    case sti_hash128:
    case sti_hash256:
    case sti_amount:
    case sti_vl:
    case sti_account:
    case sti_object:
    case sti_array:
    // types (uncommon)
    case sti_uint8:
    case sti_hash160:
    case sti_pathset:
    case sti_vector256:
        break;

    default:
        return sfinvalid;
    }

    {
        // lookup in the run-time data base, and create if it does not
        // yet exist.
        staticscopedlocktype sl (sfield_mutex);

        auto it = unknowncodetofield.find (code);

        if (it != unknowncodetofield.end ())
            return * (it->second);
        return *(unknowncodetofield[code] = std::unique_ptr<sfield const>(
                       new sfield(static_cast<serializedtypeid>(type), field)));
    }
}

int sfield::compare (sfield::ref f1, sfield::ref f2)
{
    // -1 = f1 comes before f2, 0 = illegal combination, 1 = f1 comes after f2
    if ((f1.fieldcode <= 0) || (f2.fieldcode <= 0))
        return 0;

    if (f1.fieldcode < f2.fieldcode)
        return -1;

    if (f2.fieldcode < f1.fieldcode)
        return 1;

    return 0;
}

std::string sfield::getname () const
{
    if (!fieldname.empty ())
        return fieldname;

    if (fieldvalue == 0)
        return "";

    return std::to_string(static_cast<int> (fieldtype)) + "/" +
            std::to_string(fieldvalue);
}

sfield::ref sfield::getfield (std::string const& fieldname)
{
    for (auto const & fieldpair : knowncodetofield)
    {
        if (fieldpair.second->fieldname == fieldname)
            return * (fieldpair.second);
    }
    {
        staticscopedlocktype sl (sfield_mutex);

        for (auto const & fieldpair : unknowncodetofield)
        {
            if (fieldpair.second->fieldname == fieldname)
                return * (fieldpair.second);
        }
    }
    return sfinvalid;
}

} // ripple
