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

#ifndef beast_net_dynamicbuffer_h_included
#define beast_net_dynamicbuffer_h_included

#include <string>
#include <vector>

namespace beast {

/** disjoint, but efficient buffer storage for network operations.
    this is designed to not require asio in order to compile.
*/
class dynamicbuffer
{
public:
    enum
    {
        defaultblocksize = 32 * 1024
    };

    typedef std::size_t             size_type;

    /** create the dynamic buffer with the specified block size. */
    explicit dynamicbuffer (size_type blocksize = defaultblocksize);
    
    dynamicbuffer (dynamicbuffer const& other);

    ~dynamicbuffer ();

    dynamicbuffer& operator= (dynamicbuffer const& other);

    /** swap the contents of this buffer with another.
        this is the preferred way to transfer ownership.
    */
    void swapwith (dynamicbuffer& other);

    /** returns the size of the input sequence. */
    size_type size () const;

    /** returns a buffer to the input sequence.
        constbuffertype must be constructible with this signature:
            constbuffertype (void const* buffer, size_type bytes);
    */
    template <typename constbuffertype>
    std::vector <constbuffertype>
    data () const
    {
        std::vector <constbuffertype> buffers;
        buffers.reserve (m_buffers.size());
        size_type amount (m_size);
        for (typename buffers::const_iterator iter (m_buffers.begin());
            amount > 0 && iter != m_buffers.end(); ++iter)
        {
            size_type const n (std::min (amount, m_blocksize));
            buffers.push_back (constbuffertype (*iter, n));
            amount -= n;
        }
        return buffers;
    }

    /** reserve space in the output sequence.
        this also returns a buffer suitable for writing.
        mutablebuffertype must be constructible with this signature:
            mutablebuffertype (void* buffer, size_type bytes);
    */
    template <typename mutablebuffertype>
    std::vector <mutablebuffertype>
    prepare (size_type amount)
    {
        std::vector <mutablebuffertype> buffers;
        buffers.reserve (m_buffers.size());
        reserve (amount);
        size_type offset (m_size % m_blocksize);
        for (buffers::iterator iter = m_buffers.begin () + (m_size / m_blocksize);
            amount > 0 && iter != m_buffers.end (); ++iter)
        {
            size_type const n (std::min (amount, m_blocksize - offset));
            buffers.push_back (mutablebuffertype (
                ((static_cast <char*> (*iter)) + offset), n));
            amount -= n;
            offset = 0;
        }
        return buffers;
    }

    /** reserve space in the output sequence. */
    void reserve (size_type n);

    /** move bytes from the output to the input sequence. */
    void commit (size_type n);

    /** release memory while preserving the input sequence. */
    void shrink_to_fit ();

    /** convert the entire buffer into a single string.
        this is mostly for diagnostics, it defeats the purpose of the class!
    */
    std::string to_string () const;

private:
    typedef std::vector <void*> buffers;

    size_type m_blocksize;
    size_type m_size;
    buffers m_buffers;
};

}

#endif
