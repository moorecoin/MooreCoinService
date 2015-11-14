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

#include <beast/crypto/sha256.h>

namespace beast {
namespace sha256 {

#ifndef  little_endian
# define little_endian 1234
#endif
#ifndef  big_endian
# define big_endian 4321
#endif
#if !defined(byte_order)
# if beast_big_endian
#  define byte_order big_endian
# else
#  define byte_order little_endian
# endif
#endif

//#define sha2_use_inttypes_h

namespace detail {
#include <beast/crypto/impl/sha2/sha2.c>
}

context::context ()
{
    detail::sha256_init (&m_context);
}

void context::update (void const* buffer, std::size_t bytes)
{
    detail::sha256_update (&m_context, static_cast <std::uint8_t const*> (buffer), bytes);
}

void* context::finish (void* hash)
{
    detail::sha256_final (static_cast <std::uint8_t*> (hash), &m_context);
    return hash;
}

//------------------------------------------------------------------------------

digest_type const& empty_digest()
{
    struct holder
    {
        holder ()
        {
            std::uint8_t zero (0);
            hash (zero, digest);
        }

        digest_type digest;
    };

    static holder const holder;

    return holder.digest;
}

void* hash (void const* buffer, std::size_t bytes, void* digest)
{
    context h;
    h.update (buffer, bytes);
    h.finish (digest);
    return digest;
}

digest_type& hash (void const* buffer, std::size_t bytes, digest_type& digest)
{
    hash (buffer, bytes, digest.data());
    return digest;
}

digest_type hash (void const* buffer, std::size_t bytes)
{
    digest_type digest;
    hash (buffer, bytes, digest);
    return digest;
}

void* hash (std::int8_t const* begin, std::int8_t const* end, void* digest)
{
    return hash (begin, end - begin, digest);
}

void* hash (std::uint8_t const* begin, std::uint8_t const* end, void* digest)
{
    return hash (begin, end - begin, digest);
}

digest_type hash (std::int8_t const* begin, std::int8_t const* end)
{
    digest_type digest;
    hash (begin, end - begin, digest);
    return digest;
}

digest_type hash (std::uint8_t const* begin, std::uint8_t const* end)
{
    digest_type digest;
    hash (begin, end - begin, digest);
    return digest;
}

void* hash (void const* source_digest, void* digest)
{
    return hash (source_digest, digestlength, digest);
}

digest_type& hash (void const* source_digest, digest_type& digest)
{
    return hash (source_digest, digestlength, digest);
}

digest_type hash (void const* source_digest)
{
    digest_type digest;
    hash (source_digest, digestlength, digest);
    return digest;
}

}
}
