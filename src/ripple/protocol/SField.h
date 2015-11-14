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

#ifndef ripple_protocol_fieldnames_h_included
#define ripple_protocol_fieldnames_h_included

#include <ripple/basics/basictypes.h>
#include <ripple/json/json_value.h>

namespace ripple {

enum serializedtypeid
{
    // special types
    sti_unknown     = -2,
    sti_done        = -1,
    sti_notpresent  = 0,

    // // types (common)
    sti_uint16 = 1,
    sti_uint32 = 2,
    sti_uint64 = 3,
    sti_hash128 = 4,
    sti_hash256 = 5,
    sti_amount = 6,
    sti_vl = 7,
    sti_account = 8,
    // 9-13 are reserved
    sti_object = 14,
    sti_array = 15,

    // types (uncommon)
    sti_uint8 = 16,
    sti_hash160 = 17,
    sti_pathset = 18,
    sti_vector256 = 19,

    // high level types
    // cannot be serialized inside other types
    sti_transaction = 10001,
    sti_ledgerentry = 10002,
    sti_validation  = 10003,
    sti_metadata    = 10004,
};

// constexpr
inline
int
field_code(serializedtypeid id, int index)
{
    return (static_cast<int>(id) << 16) | index;
}

// constexpr
inline
int
field_code(int id, int index)
{
    return (id << 16) | index;
}

/** identifies fields.

    fields are necessary to tag data in signed transactions so that
    the binary format of the transaction can be canonicalized.

    there are two categories of these fields:

    1.  those that are created at compile time.
    2.  those that are created at run time.

    both are always const.  category 1 can only be created in fieldnames.cpp.
    this is enforced at compile time.  category 2 can only be created by
    calling getfield with an as yet unused fieldtype and fieldvalue (or the
    equivalent fieldcode).

    each sfield, once constructed, lives until program termination, and there
    is only one instance per fieldtype/fieldvalue pair which serves the entire
    application.
*/
class sfield
{
public:
    typedef const sfield&   ref;
    typedef sfield const*   ptr;

    enum
    {
        smd_never          = 0x00,
        smd_changeorig     = 0x01, // original value when it changes
        smd_changenew      = 0x02, // new value when it changes
        smd_deletefinal    = 0x04, // final value when it is deleted
        smd_create         = 0x08, // value when it's created
        smd_always         = 0x10, // value when node containing it is affected at all
        smd_default        = smd_changeorig | smd_changenew | smd_deletefinal | smd_create
    };

    const int               fieldcode;      // (type<<16)|index
    const serializedtypeid  fieldtype;      // sti_*
    const int               fieldvalue;     // code number for protocol
    std::string             fieldname;
    int                     fieldmeta;
    int                     fieldnum;
    bool                    signingfield;
    std::string             rawjsonname;
    json::staticstring      jsonname;

    sfield(sfield const&) = delete;
    sfield& operator=(sfield const&) = delete;
#ifndef _msc_ver
    sfield(sfield&&) = default;
#else  // remove this when vs gets defaulted move members
    sfield(sfield&& sf)
        : fieldcode (std::move(sf.fieldcode))
        , fieldtype (std::move(sf.fieldtype))
        , fieldvalue (std::move(sf.fieldvalue))
        , fieldname (std::move(sf.fieldname))
        , fieldmeta (std::move(sf.fieldmeta))
        , fieldnum (std::move(sf.fieldnum))
        , signingfield (std::move(sf.signingfield))
        , rawjsonname (std::move(sf.rawjsonname))
        , jsonname (rawjsonname.c_str ())
    {}
#endif

private:
    // these constructors can only be called from fieldnames.cpp
    sfield (serializedtypeid tid, int fv, const char* fn,
            int meta = smd_default, bool signing = true);
    explicit sfield (int fc);
    sfield (serializedtypeid id, int val);

public:
    // getfield will dynamically construct a new sfield if necessary
    static sfield::ref getfield (int fieldcode);
    static sfield::ref getfield (std::string const& fieldname);
    static sfield::ref getfield (int type, int value)
    {
        return getfield (field_code (type, value));
    }
    static sfield::ref getfield (serializedtypeid type, int value)
    {
        return getfield (field_code (type, value));
    }

    std::string getname () const;
    bool hasname () const
    {
        return !fieldname.empty ();
    }

    json::staticstring const& getjsonname () const
    {
        return jsonname;
    }

    bool isgeneric () const
    {
        return fieldcode == 0;
    }
    bool isinvalid () const
    {
        return fieldcode == -1;
    }
    bool isuseful () const
    {
        return fieldcode > 0;
    }
    bool isknown () const
    {
        return fieldtype != sti_unknown;
    }
    bool isbinary () const
    {
        return fieldvalue < 256;
    }

    // a discardable field is one that cannot be serialized, and
    // should be discarded during serialization,like 'hash'.
    // you cannot serialize an object's hash inside that object,
    // but you can have it in the json representation.
    bool isdiscardable () const
    {
        return fieldvalue > 256;
    }

    int getcode () const
    {
        return fieldcode;
    }
    int getnum () const
    {
        return fieldnum;
    }
    static int getnumfields ()
    {
        return num;
    }

    bool issigningfield () const
    {
        return signingfield;
    }
    void notsigningfield ()
    {
        signingfield = false;
    }
    bool shouldmeta (int c) const
    {
        return (fieldmeta & c) != 0;
    }
    void setmeta (int c)
    {
        fieldmeta = c;
    }

    bool shouldinclude (bool withsigningfield) const
    {
        return (fieldvalue < 256) && (withsigningfield || signingfield);
    }

    bool operator== (const sfield& f) const
    {
        return fieldcode == f.fieldcode;
    }

    bool operator!= (const sfield& f) const
    {
        return fieldcode != f.fieldcode;
    }

    static int compare (sfield::ref f1, sfield::ref f2);

    struct make;  // public, but still an implementation detail

private:
    static int num;
};

extern sfield const sfinvalid;
extern sfield const sfgeneric;
extern sfield const sfledgerentry;
extern sfield const sftransaction;
extern sfield const sfvalidation;
extern sfield const sfmetadata;

// 8-bit integers
extern sfield const sfcloseresolution;
extern sfield const sftemplateentrytype;
extern sfield const sftransactionresult;

    
extern sfield const sfdividendstate;
extern sfield const sfdividendtype;

// 16-bit integers
extern sfield const sfledgerentrytype;
extern sfield const sftransactiontype;

// 32-bit integers (common)
extern sfield const sfflags;
extern sfield const sfsourcetag;
extern sfield const sfsequence;
extern sfield const sfprevioustxnlgrseq;
extern sfield const sfledgersequence;
extern sfield const sfclosetime;
extern sfield const sfparentclosetime;
extern sfield const sfsigningtime;
extern sfield const sfexpiration;
extern sfield const sftransferrate;
extern sfield const sfwalletsize;
extern sfield const sfownercount;
extern sfield const sfdestinationtag;

extern sfield const sfdividendledger;
extern sfield const sfreferenceheight;
extern sfield const sfreleaserate;
extern sfield const sfnextreleasetime;

// 32-bit integers (uncommon)
extern sfield const sfhighqualityin;
extern sfield const sfhighqualityout;
extern sfield const sflowqualityin;
extern sfield const sflowqualityout;
extern sfield const sfqualityin;
extern sfield const sfqualityout;
extern sfield const sfstampescrow;
extern sfield const sfbondamount;
extern sfield const sfloadfee;
extern sfield const sfoffersequence;
extern sfield const sffirstledgersequence;  // deprecated: do not use
extern sfield const sflastledgersequence;
extern sfield const sftransactionindex;
extern sfield const sfoperationlimit;
extern sfield const sfreferencefeeunits;
extern sfield const sfreservebase;
extern sfield const sfreserveincrement;
extern sfield const sfsetflag;
extern sfield const sfclearflag;

// 64-bit integers
extern sfield const sfindexnext;
extern sfield const sfindexprevious;
extern sfield const sfbooknode;
extern sfield const sfownernode;
extern sfield const sfbasefee;
extern sfield const sfexchangerate;
extern sfield const sflownode;
extern sfield const sfhighnode;

extern sfield const sfdividendcoins;
extern sfield const sfdividendcoinsvbc;
extern sfield const sfdividendcoinsvbcrank;
extern sfield const sfdividendcoinsvbcsprd;
extern sfield const sfdividendvrank;
extern sfield const sfdividendvsprd;
extern sfield const sfdividendtsprd;

// 128-bit
extern sfield const sfemailhash;

// 256-bit (common)
extern sfield const sfledgerhash;
extern sfield const sfparenthash;
extern sfield const sftransactionhash;
extern sfield const sfaccounthash;
extern sfield const sfprevioustxnid;
extern sfield const sfledgerindex;
extern sfield const sfwalletlocator;
extern sfield const sfrootindex;
extern sfield const sfaccounttxnid;

extern sfield const sfdividendresulthash;

// 256-bit (uncommon)
extern sfield const sfbookdirectory;
extern sfield const sfinvoiceid;
extern sfield const sfnickname;
extern sfield const sfamendment;
extern sfield const sfticketid;

// 160-bit (common)
extern sfield const sftakerpayscurrency;
extern sfield const sftakerpaysissuer;
extern sfield const sftakergetscurrency;
extern sfield const sftakergetsissuer;

// currency amount (common)
extern sfield const sfamount;
extern sfield const sfbalance;
extern sfield const sfbalancevbc;
extern sfield const sflimitamount;
extern sfield const sftakerpays;
extern sfield const sftakergets;
extern sfield const sflowlimit;
extern sfield const sfhighlimit;
extern sfield const sffee;
extern sfield const sfsendmax;

// currency amount (uncommon)
extern sfield const sfminimumoffer;
extern sfield const sfrippleescrow;
extern sfield const sfdeliveredamount;
extern sfield const sfreserve;

// variable length
extern sfield const sfpublickey;
extern sfield const sfmessagekey;
extern sfield const sfsigningpubkey;
extern sfield const sftxnsignature;
extern sfield const sfgenerator;
extern sfield const sfsignature;
extern sfield const sfdomain;
extern sfield const sffundcode;
extern sfield const sfremovecode;
extern sfield const sfexpirecode;
extern sfield const sfcreatecode;
extern sfield const sfmemotype;
extern sfield const sfmemodata;
extern sfield const sfmemoformat;

// account
extern sfield const sfaccount;
extern sfield const sfowner;
extern sfield const sfdestination;
extern sfield const sfissuer;
extern sfield const sftarget;
extern sfield const sfregularkey;

extern sfield const sfreferee;
extern sfield const sfreference;

// path set
extern sfield const sfpaths;

// vector of 256-bit
extern sfield const sfindexes;
extern sfield const sfhashes;
extern sfield const sfamendments;

// inner object
// object/1 is reserved for end of object
extern sfield const sftransactionmetadata;
extern sfield const sfcreatednode;
extern sfield const sfdeletednode;
extern sfield const sfmodifiednode;
extern sfield const sfpreviousfields;
extern sfield const sffinalfields;
extern sfield const sfnewfields;
extern sfield const sftemplateentry;
extern sfield const sfmemo;

extern sfield const sfreferenceholder;
extern sfield const sffeesharetaker;
extern sfield const sfreleasepoint;

// array of objects
// array/1 is reserved for end of array
extern sfield const sfsigningaccounts;
extern sfield const sftxnsignatures;
extern sfield const sfsignatures;
extern sfield const sftemplate;
extern sfield const sfnecessary;
extern sfield const sfsufficient;
extern sfield const sfaffectednodes;
extern sfield const sfmemos;

extern sfield const sfreferences;
extern sfield const sffeesharetakers;
extern sfield const sfreleaseschedule;

} // ripple

#endif
