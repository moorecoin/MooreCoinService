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

#ifndef beast_crypto_sha256_h_included
#define beast_crypto_sha256_h_included

#include <beast/config.h>

#include <array>
#include <cstdint>

//------------------------------------------------------------------------------

namespace beast {
namespace sha256 {

enum
{
    digestlength = 32,
    blocklength = 64
};

/** a container suitable for holding the resulting hash. */
typedef std::array <std::uint8_t, digestlength> digest_type;

namespace detail {
struct context
{
    std::uint32_t state[8];
    std::uint64_t bitcount;
    std::uint8_t  buffer[sha256::blocklength];
};
}

/** computes the sha256 hash of data. */
class context
{
public:
    /** create a new hasher prepared for input. */
    context();

    /** update the hashing context with the input sequence. */
    /** @{ */
    void update (void const* buffer, std::size_t bytes);

    void update (std::int8_t const* begin, std::int8_t const* end)
    {
        update (begin, end - begin);
    }

    void update (std::uint8_t const* begin, std::uint8_t const* end)
    {
        update (begin, end - begin);
    }

    template <typename t>
    void update (t const& t)
    {
        update (&t, sizeof(t));
    }
    /** @} */

    /** finalize the hash process and store the digest.
        the memory pointed to by `digest` must be at least digestlength
        bytes. this object may not be re-used after calling finish.
        @return a pointer to the passed hash buffer.
    */
    /** @{ */
    void* finish (void* digest);

    digest_type& finish (digest_type& digest)
    {
        finish (digest.data());
        return digest;
    }

    digest_type finish ()
    {
        digest_type digest;
        finish (digest);
        return digest;
    }
    /** @} */

private:
    detail::context m_context;
};

//------------------------------------------------------------------------------

/** returns the hash produced by a single octet equal to zero. */
digest_type const& empty_digest();

/** performs an entire hashing operation in a single step.
    a zero length input sequence produces the empty_digest().
    @return the resulting digest depending on the arguments.
*/
/** @{ */
void* hash (void const* buffer, std::size_t bytes, void* digest);
digest_type& hash ( void const* buffer, std::size_t bytes, digest_type& digest);
digest_type hash (void const* buffer, std::size_t bytes);
void* hash (std::int8_t const* begin, std::int8_t const* end, void* digest);
void* hash (std::uint8_t const* begin, std::uint8_t const* end, void* digest);
digest_type hash (std::int8_t const* begin, std::int8_t const* end);
digest_type hash (std::uint8_t const* begin, std::uint8_t const* end);

template <typename t>
void* hash (t const& t, void* digest)
{
    return hash (&t, sizeof(t), digest);
}

template <typename t>
digest_type& hash (t const& t, digest_type& digest)
{
    return hash (&t, sizeof(t), digest);
}

template <typename t>
digest_type hash (t const& t)
{
    digest_type digest;
    hash (&t, sizeof(t), digest);
    return digest;
}
/** @} */

/** calculate the hash of a hash in one step.
    the memory pointed to by source_digest must be at
    least digestlength bytes or undefined behavior results.
*/
/** @{ */
void* hash (void const* source_digest, void* digest);
digest_type& hash (void const* source_digest, digest_type& digest);
digest_type hash (void const* source_digest);;
/** @} */

}
}

#endif
