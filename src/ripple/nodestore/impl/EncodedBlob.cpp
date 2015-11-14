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
#include <ripple/nodestore/impl/encodedblob.h>
#include <beast/byteorder.h>
    
namespace ripple {
namespace nodestore {

void
encodedblob::prepare (nodeobject::ptr const& object)
{
    m_key = object->gethash().begin ();

    // this is how many bytes we need in the flat data
    m_size = object->getdata ().size () + 9;

    m_data.ensuresize(m_size);

    {
        // these 8 bytes are unused
        std::uint64_t* buf = static_cast <
            std::uint64_t*>(m_data.getdata ());
        *buf = 0;
    }

    {
        unsigned char* buf = static_cast <
            unsigned char*> (m_data.getdata ());
        buf [8] = static_cast <
            unsigned char> (object->gettype ());
        memcpy (&buf [9], object->getdata ().data(),
            object->getdata ().size());
    }
}

}
}
