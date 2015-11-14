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

#ifndef ripple_nodestore_encodedblob_h_included
#define ripple_nodestore_encodedblob_h_included

#include <ripple/nodestore/nodeobject.h>
#include <beast/module/core/memory/memoryblock.h>
#include <beast/utility/noexcept.h>
#include <cstddef>

namespace ripple {
namespace nodestore {

/** utility for producing flattened node objects.
    @note this defines the database format of a nodeobject!
*/
// vfalco todo make allocator aware and use short_alloc
struct encodedblob
{
public:
    void prepare (nodeobject::ptr const& object);
    void const* getkey () const noexcept { return m_key; }
    std::size_t getsize () const noexcept { return m_size; }
    void const* getdata () const noexcept { return m_data.getdata (); }

private:
    void const* m_key;
    beast::memoryblock m_data;
    std::size_t m_size;
};

}
}

#endif
