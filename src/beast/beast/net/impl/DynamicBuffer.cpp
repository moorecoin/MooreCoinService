//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

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

#if beast_include_beastconfig
#include "../../beastconfig.h"
#endif

#include <beast/net/dynamicbuffer.h>

#include <algorithm>
#include <memory>

namespace beast {

dynamicbuffer::dynamicbuffer (size_type blocksize)
    : m_blocksize (blocksize)
    , m_size (0)
{
}

dynamicbuffer::~dynamicbuffer ()
{
    for (buffers::iterator iter (m_buffers.begin());
        iter != m_buffers.end(); ++iter)
        free (*iter);
}

void dynamicbuffer::swapwith (dynamicbuffer& other)
{
    std::swap (m_blocksize, other.m_blocksize);
    std::swap (m_size, other.m_size);
    m_buffers.swap (other.m_buffers);
}

void dynamicbuffer::commit (size_type n)
{
    m_size += n;
    bassert (m_size <= m_buffers.size () * m_blocksize);
}

dynamicbuffer::size_type dynamicbuffer::size () const
{
    return m_size;
}

void dynamicbuffer::reserve (size_type n)
{
    size_type count ((m_size + n + m_blocksize - 1) / m_blocksize);
    if (count > m_buffers.size ())
        for (count -= m_buffers.size (); count-- > 0;)
            m_buffers.push_back (malloc (m_blocksize));
}

void dynamicbuffer::shrink_to_fit ()
{
    size_type const count ((m_size + m_blocksize - 1) / m_blocksize);
    while (m_buffers.size () > count)
    {
        free (m_buffers.back ());
        m_buffers.erase (m_buffers.end () - 1);
    }   
}

std::string dynamicbuffer::to_string () const
{
    std::string (s);
    s.reserve (m_size);
    std::size_t amount (m_size);
    for (buffers::const_iterator iter (m_buffers.begin());
        amount > 0 && iter != m_buffers.end(); ++iter)
    {
        char const* p (static_cast <char const*> (*iter));
        size_type const n (std::min (amount, m_blocksize));
        s.append (p, p + n);
        amount -= n;
    }
    return s;
}

}
