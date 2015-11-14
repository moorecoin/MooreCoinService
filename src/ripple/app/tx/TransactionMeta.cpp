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
#include <ripple/app/tx/transactionmeta.h>
#include <ripple/basics/log.h>
#include <ripple/json/to_string.h>
#include <ripple/protocol/staccount.h>
#include <boost/foreach.hpp>
#include <string>

namespace ripple {

// vfalco todo rename class to transactionmeta

transactionmetaset::transactionmetaset (uint256 const& txid, std::uint32_t ledger, blob const& vec) :
    mtransactionid (txid), mledger (ledger), mnodes (sfaffectednodes, 32)
{
    serializer s (vec);
    serializeriterator sit (s);

    std::unique_ptr<stbase> pobj = stobject::deserialize (sit, sfmetadata);
    stobject* obj = static_cast<stobject*> (pobj.get ());

    if (!obj)
        throw std::runtime_error ("bad metadata");

    mresult = obj->getfieldu8 (sftransactionresult);
    mindex = obj->getfieldu32 (sftransactionindex);
    mnodes = * dynamic_cast<starray*> (&obj->getfield (sfaffectednodes));

    if (obj->isfieldpresent (sfdeliveredamount))
        setdeliveredamount (obj->getfieldamount (sfdeliveredamount));
    
    if (obj->isfieldpresent (sffeesharetakers))
        setfeesharetakers(obj->getfieldarray(sffeesharetakers));
}

bool transactionmetaset::isnodeaffected (uint256 const& node) const
{
    boost_foreach (const stobject & it, mnodes)

    if (it.getfieldh256 (sfledgerindex) == node)
        return true;

    return false;
}

void transactionmetaset::setaffectednode (uint256 const& node, sfield::ref type,
                                          std::uint16_t nodetype)
{
    // make sure the node exists and force its type
    boost_foreach (stobject & it, mnodes)
    {
        if (it.getfieldh256 (sfledgerindex) == node)
        {
            it.setfname (type);
            it.setfieldu16 (sfledgerentrytype, nodetype);
            return;
        }
    }

    mnodes.push_back (stobject (type));
    stobject& obj = mnodes.back ();

    assert (obj.getfname () == type);
    obj.setfieldh256 (sfledgerindex, node);
    obj.setfieldu16 (sfledgerentrytype, nodetype);
}

static void addifunique (std::vector<rippleaddress>& vector, rippleaddress const& address)
{
    boost_foreach (const rippleaddress & a, vector)

    if (a == address)
        return;

    vector.push_back (address);
}

std::vector<rippleaddress> transactionmetaset::getaffectedaccounts ()
{
    std::vector<rippleaddress> accounts;
    accounts.reserve (10);

    // this code should match the behavior of the js method:
    // meta#getaffectedaccounts
    boost_foreach (const stobject & it, mnodes)
    {
        int index = it.getfieldindex ((it.getfname () == sfcreatednode) ? sfnewfields : sffinalfields);

        if (index != -1)
        {
            const stobject* inner = dynamic_cast<const stobject*> (&it.peekatindex (index));

            if (inner)
            {
                boost_foreach (const stbase & field, inner->peekdata ())
                {
                    const staccount* sa = dynamic_cast<const staccount*> (&field);

                    if (sa && field.getfname () != sfreferee)
                    {
                        addifunique (accounts, sa->getvaluenca ());
                    }
                    else if ((field.getfname () == sflowlimit) || (field.getfname () == sfhighlimit) ||
                             (field.getfname () == sftakerpays) || (field.getfname () == sftakergets))
                    {
                        const stamount* lim = dynamic_cast<const stamount*> (&field);

                        if (lim != nullptr)
                        {
                            auto issuer = lim->getissuer ();

                            if (issuer.isnonzero ())
                            {
                                rippleaddress na;
                                na.setaccountid (issuer);
                                addifunique (accounts, na);
                            }
                        }
                        else
                        {
                            writelog (lsfatal, transactionmetaset) << "limit is not amount " << field.getjson (0);
                        }
                    }
                }
            }
            else assert (false);
        }
    }

    return accounts;
}

stobject& transactionmetaset::getaffectednode (sle::ref node, sfield::ref type)
{
    assert (&type);
    uint256 index = node->getindex ();
    boost_foreach (stobject & it, mnodes)
    {
        if (it.getfieldh256 (sfledgerindex) == index)
            return it;
    }
    mnodes.push_back (stobject (type));
    stobject& obj = mnodes.back ();

    assert (obj.getfname () == type);
    obj.setfieldh256 (sfledgerindex, index);
    obj.setfieldu16 (sfledgerentrytype, node->getfieldu16 (sfledgerentrytype));

    return obj;
}

stobject& transactionmetaset::getaffectednode (uint256 const& node)
{
    boost_foreach (stobject & it, mnodes)
    {
        if (it.getfieldh256 (sfledgerindex) == node)
            return it;
    }
    assert (false);
    throw std::runtime_error ("affected node not found");
}

const stobject& transactionmetaset::peekaffectednode (uint256 const& node) const
{
    boost_foreach (const stobject & it, mnodes)

    if (it.getfieldh256 (sfledgerindex) == node)
        return it;

    throw std::runtime_error ("affected node not found");
}

void transactionmetaset::init (uint256 const& id, std::uint32_t ledger)
{
    mtransactionid = id;
    mledger = ledger;
    mnodes = starray (sfaffectednodes, 32);
    mdelivered = boost::optional <stamount> ();
    mfeesharetakers = boost::optional<starray>();
}

void transactionmetaset::swap (transactionmetaset& s)
{
    assert ((mtransactionid == s.mtransactionid) && (mledger == s.mledger));
    mnodes.swap (s.mnodes);
}

bool transactionmetaset::thread (stobject& node, uint256 const& prevtxid, std::uint32_t prevlgrid)
{
    if (node.getfieldindex (sfprevioustxnid) == -1)
    {
        assert (node.getfieldindex (sfprevioustxnlgrseq) == -1);
        node.setfieldh256 (sfprevioustxnid, prevtxid);
        node.setfieldu32 (sfprevioustxnlgrseq, prevlgrid);
        return true;
    }

    assert (node.getfieldh256 (sfprevioustxnid) == prevtxid);
    assert (node.getfieldu32 (sfprevioustxnlgrseq) == prevlgrid);
    return false;
}

static bool compare (const stobject& o1, const stobject& o2)
{
    return o1.getfieldh256 (sfledgerindex) < o2.getfieldh256 (sfledgerindex);
}

stobject transactionmetaset::getasobject () const
{
    stobject metadata (sftransactionmetadata);
    assert (mresult != 255);
    metadata.setfieldu8 (sftransactionresult, mresult);
    metadata.setfieldu32 (sftransactionindex, mindex);
    metadata.addobject (mnodes);
    if (hasdeliveredamount ())
        metadata.setfieldamount (sfdeliveredamount, getdeliveredamount ());
    if (hasfeesharetakers ())
        metadata.setfieldarray (sffeesharetakers, getfeesharetakers ());
    
    return metadata;
}

void transactionmetaset::addraw (serializer& s, ter result, std::uint32_t index)
{
    mresult = static_cast<int> (result);
    mindex = index;
    assert ((mresult == 0) || ((mresult > 100) && (mresult <= 255)));

    mnodes.sort (compare);

    getasobject ().add (s);
}

} // ripple
