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
#include <ripple/nodestore/impl/databaserotatingimp.h>

namespace ripple {
namespace nodestore {

// make sure to call it already locked!
std::shared_ptr <backend> databaserotatingimp::rotatebackends (
        std::shared_ptr <backend> const& newbackend)
{
    std::shared_ptr <backend> oldbackend = archivebackend_;
    archivebackend_ = writablebackend_;
    writablebackend_ = newbackend;

    return oldbackend;
}

nodeobject::ptr databaserotatingimp::fetchfrom (uint256 const& hash)
{
    backends b = getbackends();
    nodeobject::ptr object = fetchinternal (*b.writablebackend, hash);
    if (!object)
    {
        object = fetchinternal (*b.archivebackend, hash);
        if (object)
        {
            getwritablebackend()->store (object);
            m_negcache.erase (hash);
        }
    }

    return object;
}
}

}
