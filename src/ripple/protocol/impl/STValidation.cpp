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
#include <ripple/protocol/stvalidation.h>
#include <ripple/protocol/hashprefix.h>
#include <ripple/basics/log.h>
#include <ripple/json/to_string.h>

namespace ripple {

stvalidation::stvalidation (serializeriterator& sit, bool checksignature)
    : stobject (getformat (), sit, sfvalidation)
    , mtrusted (false)
{
    mnodeid = rippleaddress::createnodepublic (getfieldvl (sfsigningpubkey)).getnodeid ();
    assert (mnodeid.isnonzero ());

    if  (checksignature && !isvalid ())
    {
        writelog (lstrace, ledger) << "invalid validation " << getjson (0);
        throw std::runtime_error ("invalid validation");
    }
}

stvalidation::stvalidation (
    uint256 const& ledgerhash, std::uint32_t signtime,
    rippleaddress const& rapub, bool isfull)
    : stobject (getformat (), sfvalidation)
    , mtrusted (false)
{
    // does not sign
    setfieldh256 (sfledgerhash, ledgerhash);
    setfieldu32 (sfsigningtime, signtime);

    setfieldvl (sfsigningpubkey, rapub.getnodepublic ());
    mnodeid = rapub.getnodeid ();
    assert (mnodeid.isnonzero ());

    if (!isfull)
        setflag (kfullflag);
}

void stvalidation::sign (rippleaddress const& rapriv)
{
    uint256 signinghash;
    sign (signinghash, rapriv);
}

void stvalidation::sign (uint256& signinghash, rippleaddress const& rapriv)
{
    setflag (vffullycanonicalsig);

    signinghash = getsigninghash ();
    blob signature;
    rapriv.signnodeprivate (signinghash, signature);
    setfieldvl (sfsignature, signature);
}

uint256 stvalidation::getsigninghash () const
{
    return stobject::getsigninghash (hashprefix::validation);
}

uint256 stvalidation::getledgerhash () const
{
    return getfieldh256 (sfledgerhash);
}

std::uint32_t stvalidation::getsigntime () const
{
    return getfieldu32 (sfsigningtime);
}

std::uint32_t stvalidation::getflags () const
{
    return getfieldu32 (sfflags);
}

bool stvalidation::isvalid () const
{
    return isvalid (getsigninghash ());
}

bool stvalidation::isvalid (uint256 const& signinghash) const
{
    try
    {
        const ecdsa fullycanonical = getflags () & vffullycanonicalsig ?
                                            ecdsa::strict : ecdsa::not_strict;
        rippleaddress   rapublickey = rippleaddress::createnodepublic (getfieldvl (sfsigningpubkey));
        return rapublickey.isvalid () &&
            rapublickey.verifynodepublic (signinghash, getfieldvl (sfsignature), fullycanonical);
    }
    catch (...)
    {
        writelog (lsinfo, ledger) << "exception validating validation";
        return false;
    }
}

rippleaddress stvalidation::getsignerpublic () const
{
    rippleaddress a;
    a.setnodepublic (getfieldvl (sfsigningpubkey));
    return a;
}

bool stvalidation::isfull () const
{
    return (getflags () & kfullflag) != 0;
}

blob stvalidation::getsignature () const
{
    return getfieldvl (sfsignature);
}

blob stvalidation::getsigned () const
{
    serializer s;
    add (s);
    return s.peekdata ();
}

sotemplate const& stvalidation::getformat ()
{
    struct formatholder
    {
        sotemplate format;

        formatholder ()
        {
            format.push_back (soelement (sfflags,           soe_required));
            format.push_back (soelement (sfledgerhash,      soe_required));
            format.push_back (soelement (sfledgersequence,  soe_optional));
            format.push_back (soelement (sfclosetime,       soe_optional));
            format.push_back (soelement (sfloadfee,         soe_optional));
            format.push_back (soelement (sfamendments,      soe_optional));
            format.push_back (soelement (sfbasefee,         soe_optional));
            format.push_back (soelement (sfreservebase,     soe_optional));
            format.push_back (soelement (sfreserveincrement, soe_optional));
            format.push_back (soelement (sfsigningtime,     soe_required));
            format.push_back (soelement (sfsigningpubkey,   soe_required));
            format.push_back (soelement (sfsignature,       soe_optional));
            format.push_back (soelement (sfdividendledger,  soe_optional));
            format.push_back (soelement (sfdividendcoins,   soe_optional));
            format.push_back (soelement (sfdividendcoinsvbc, soe_optional));
            format.push_back (soelement (sfdividendresulthash, soe_optional));
        }
    };

    static formatholder holder;

    return holder.format;
}

} // ripple
