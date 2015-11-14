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

#ifndef ripple_nodestore_databaserotating_h_included
#define ripple_nodestore_databaserotating_h_included

#include <ripple/nodestore/database.h>

namespace ripple {
namespace nodestore {

/* this class has two key-value store backend objects for persisting shamap
 * records. this facilitates online deletion of data. new backends are
 * rotated in. old ones are rotated out and deleted.
 */

class databaserotating
{
public:
    virtual ~databaserotating() = default;

    virtual taggedcache <uint256, nodeobject>& getpositivecache() = 0;

    virtual std::mutex& peekmutex() const = 0;

    virtual std::shared_ptr <backend> const& getwritablebackend() const = 0;

    virtual std::shared_ptr <backend> const& getarchivebackend () const = 0;

    virtual std::shared_ptr <backend> rotatebackends (
            std::shared_ptr <backend> const& newbackend) = 0;

    /** ensure that node is in writablebackend */
    virtual nodeobject::ptr fetchnode (uint256 const& hash) = 0;
};

}
}

#endif
