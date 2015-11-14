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
#include <ripple/nodestore/nodeobject.h>
#include <memory>

namespace ripple {

//------------------------------------------------------------------------------

nodeobject::nodeobject (
    nodeobjecttype type,
    blob&& data,
    uint256 const& hash,
    privateaccess)
    : mtype (type)
    , mhash (hash)
{
    mdata = std::move (data);
}

nodeobject::ptr
nodeobject::createobject (
    nodeobjecttype type,
    blob&& data,
    uint256 const& hash)
{
    return std::make_shared <nodeobject> (
        type, std::move (data), hash, privateaccess ());
}

nodeobjecttype
nodeobject::gettype () const
{
    return mtype;
}

uint256 const&
nodeobject::gethash () const
{
    return mhash;
}

blob const&
nodeobject::getdata () const
{
    return mdata;
}

bool
nodeobject::iscloneof (nodeobject::ptr const& other) const
{
    if (mtype != other->mtype)
        return false;

    if (mhash != other->mhash)
        return false;

    if (mdata != other->mdata)
        return false;

    return true;
}

//------------------------------------------------------------------------------

}
