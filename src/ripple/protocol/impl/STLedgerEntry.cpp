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
#include <ripple/basics/log.h>
#include <ripple/json/to_string.h>
#include <ripple/protocol/indexes.h>
#include <ripple/protocol/staccount.h>
#include <ripple/protocol/stledgerentry.h>
#include <boost/format.hpp>

namespace ripple {

stledgerentry::stledgerentry (
    serializeriterator& sit, uint256 const& index)
    : stobject (sfledgerentry), mindex (index), mmutable (true)
{
    set (sit);
    setsletype ();
}

stledgerentry::stledgerentry (
    const serializer& s, uint256 const& index)
    : stobject (sfledgerentry), mindex (index), mmutable (true)
{
    // we know 's' isn't going away
    serializeriterator sit (const_cast<serializer&> (s));
    set (sit);
    setsletype ();
}

stledgerentry::stledgerentry (
    const stobject & object, uint256 const& index)
    : stobject (object), mindex(index),  mmutable (true)
{
    setsletype ();
}

void stledgerentry::setsletype ()
{
    mformat = ledgerformats::getinstance().findbytype (
        static_cast <ledgerentrytype> (getfieldu16 (sfledgerentrytype)));

    if (mformat == nullptr)
        throw std::runtime_error ("invalid ledger entry type");

    mtype = mformat->gettype ();
    if (!settype (mformat->elements))
    {
        writelog (lswarning, serializedledger)
            << "ledger entry not valid for type " << mformat->getname ();
        writelog (lswarning, serializedledger) << getjson (0);
        throw std::runtime_error ("ledger entry not valid for type");
    }
}

stledgerentry::stledgerentry (ledgerentrytype type, uint256 const& index) :
    stobject (sfledgerentry), mindex (index), mtype (type), mmutable (true)
{
    mformat = ledgerformats::getinstance().findbytype (type);

    if (mformat == nullptr)
        throw std::runtime_error ("invalid ledger entry type");

    set (mformat->elements);
    setfieldu16 (sfledgerentrytype,
        static_cast <std::uint16_t> (mformat->gettype ()));
}

stledgerentry::pointer stledgerentry::getmutable () const
{
    stledgerentry::pointer ret = std::make_shared<stledgerentry> (std::cref (*this));
    ret->mmutable = true;
    return ret;
}

std::string stledgerentry::getfulltext () const
{
    std::string ret = "\"";
    ret += to_string (mindex);
    ret += "\" = { ";
    ret += mformat->getname ();
    ret += ", ";
    ret += stobject::getfulltext ();
    ret += "}";
    return ret;
}

std::string stledgerentry::gettext () const
{
    return str (boost::format ("{ %s, %s }")
                % to_string (mindex)
                % stobject::gettext ());
}

json::value stledgerentry::getjson (int options) const
{
    json::value ret (stobject::getjson (options));

    ret["index"] = to_string (mindex);

    return ret;
}

bool stledgerentry::isthreadedtype ()
{
    return getfieldindex (sfprevioustxnid) != -1;
}

bool stledgerentry::isthreaded ()
{
    return isfieldpresent (sfprevioustxnid);
}

uint256 stledgerentry::getthreadedtransaction ()
{
    return getfieldh256 (sfprevioustxnid);
}

std::uint32_t stledgerentry::getthreadedledger ()
{
    return getfieldu32 (sfprevioustxnlgrseq);
}

bool stledgerentry::thread (uint256 const& txid, std::uint32_t ledgerseq,
                                    uint256& prevtxid, std::uint32_t& prevledgerid)
{
    uint256 oldprevtxid = getfieldh256 (sfprevioustxnid);
    writelog (lstrace, serializedledger) << "thread tx:" << txid << " prev:" << oldprevtxid;

    if (oldprevtxid == txid)
    {
        // this transaction is already threaded
        assert (getfieldu32 (sfprevioustxnlgrseq) == ledgerseq);
        return false;
    }

    prevtxid = oldprevtxid;
    prevledgerid = getfieldu32 (sfprevioustxnlgrseq);
    setfieldh256 (sfprevioustxnid, txid);
    setfieldu32 (sfprevioustxnlgrseq, ledgerseq);
    return true;
}

bool stledgerentry::hasoneowner ()
{
    return (mtype != ltaccount_root) && (getfieldindex (sfaccount) != -1);
}

bool stledgerentry::hastwoowners ()
{
    return mtype == ltripple_state;
}

rippleaddress stledgerentry::getowner ()
{
    return getfieldaccount (sfaccount);
}

rippleaddress stledgerentry::getfirstowner ()
{
    return rippleaddress::createaccountid (getfieldamount (sflowlimit).getissuer ());
}

rippleaddress stledgerentry::getsecondowner ()
{
    return rippleaddress::createaccountid (getfieldamount (sfhighlimit).getissuer ());
}

std::vector<uint256> stledgerentry::getowners ()
{
    std::vector<uint256> owners;
    account account;

    for (int i = 0, fields = getcount (); i < fields; ++i)
    {
        auto const& fc = getfieldstype (i);

        if ((fc == sfaccount) || (fc == sfowner))
        {
            auto entry = dynamic_cast<const staccount*> (peekatpindex (i));

            if ((entry != nullptr) && entry->getvalueh160 (account))
                owners.push_back (getaccountrootindex (account));
        }

        if ((fc == sflowlimit) || (fc == sfhighlimit))
        {
            auto entry = dynamic_cast<const stamount*> (peekatpindex (i));

            if ((entry != nullptr))
            {
                auto issuer = entry->getissuer ();

                if (issuer.isnonzero ())
                    owners.push_back (getaccountrootindex (issuer));
            }
        }
    }

    return owners;
}

} // ripple
