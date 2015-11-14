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
#include <ripple/nodestore/impl/decodedblob.h>
#include <beast/byteorder.h>
#include <algorithm>

namespace ripple {
namespace nodestore {

decodedblob::decodedblob (void const* key, void const* value, int valuebytes)
{
    /*  data format:

        bytes

        0...3       ledgerindex     32-bit big endian integer
        4...7       unused?         an unused copy of the ledgerindex
        8           char            one of nodeobjecttype
        9...end                     the body of the object data
    */

    m_success = false;
    m_key = key;
    // vfalco note ledger indexes should have started at 1
    m_objecttype = hotunknown;
    m_objectdata = nullptr;
    m_databytes = std::max (0, valuebytes - 9);

    // vfalco note what about bytes 4 through 7 inclusive?

    if (valuebytes > 8)
    {
        unsigned char const* byte = static_cast <unsigned char const*> (value);
        m_objecttype = static_cast <nodeobjecttype> (byte [8]);
    }

    if (valuebytes > 9)
    {
        m_objectdata = static_cast <unsigned char const*> (value) + 9;

        switch (m_objecttype)
        {
        default:
            break;

        case hotunknown:
        case hotledger:
        case hottransaction:
        case hotaccount_node:
        case hottransaction_node:
            m_success = true;
            break;
        }
    }
}

nodeobject::ptr decodedblob::createobject ()
{
    bassert (m_success);

    nodeobject::ptr object;

    if (m_success)
    {
        blob data(m_objectdata, m_objectdata + m_databytes);

        object = nodeobject::createobject (
            m_objecttype, std::move(data), uint256::fromvoid(m_key));
    }

    return object;
}

}
}
