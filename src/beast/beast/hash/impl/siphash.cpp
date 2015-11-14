//------------------------------- siphash.h ------------------------------------
// 
// this software is in the public domain.  the only restriction on its use is
// that no one can remove it from the public domain by claiming ownership of it,
// including the original authors.
// 
// there is no warranty of correctness on the software contained herein.  use
// at your own risk.
//
// derived from:
// 
// siphash reference c implementation
// 
// written in 2012 by jean-philippe aumasson <jeanphilippe.aumasson@gmail.com>
// daniel j. bernstein <djb@cr.yp.to>
// 
// to the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. this software is distributed without any warranty.
// 
// you should have received a copy of the cc0 public domain dedication along
// with this software. if not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
// 
//------------------------------------------------------------------------------

#include <beast/hash/siphash.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>

// namespace acme is used to demonstrate example code.  it is not proposed.

namespace beast {
namespace detail {

typedef std::uint64_t u64;
typedef std::uint32_t u32;
typedef std::uint8_t u8;

inline
u64
rotl(u64 x, u64 b)
{
    return (x << b) | (x >> (64 - b));
}

inline
u64
u8to64_le(const u8* p)
{
#if beast_little_endian
    return *static_cast<u64 const*>(static_cast<void const*>(p));
#else
    return static_cast<u64>(p[7]) << 56 | static_cast<u64>(p[6]) << 48 |
           static_cast<u64>(p[5]) << 40 | static_cast<u64>(p[4]) << 32 |
           static_cast<u64>(p[3]) << 24 | static_cast<u64>(p[2]) << 16 |
           static_cast<u64>(p[1]) <<  8 | static_cast<u64>(p[0]);
#endif
}

inline
void
sipround(u64& v0, u64& v1, u64& v2, u64& v3)
{
    v0 += v1;
    v1 = rotl(v1, 13);
    v1 ^= v0;
    v0 = rotl(v0, 32);
    v2 += v3;
    v3 = rotl(v3, 16);
    v3 ^= v2;
    v0 += v3;
    v3 = rotl(v3, 21);
    v3 ^= v0;
    v2 += v1;
    v1 = rotl(v1, 17);
    v1 ^= v2;
    v2 = rotl(v2, 32);
}

} // detail

siphash::siphash(std::uint64_t k0, std::uint64_t k1) noexcept
{
    v3_ ^= k1;
    v2_ ^= k0;
    v1_ ^= k1;
    v0_ ^= k0;
}

void
siphash::append (void const* key, std::size_t inlen) noexcept
{
    using namespace detail;
    u8 const* in = static_cast<const u8*>(key);
    total_length_ += inlen;
    if (bufsize_ + inlen < 8)
    {
        std::copy(in, in+inlen, buf_ + bufsize_);
        bufsize_ += inlen;
        return;
    }
    if (bufsize_ > 0)
    {
        auto t = 8 - bufsize_;
        std::copy(in, in+t, buf_ + bufsize_);
        u64 m = u8to64_le( buf_ );
        v3_ ^= m;
        sipround(v0_, v1_, v2_, v3_);
        sipround(v0_, v1_, v2_, v3_);
        v0_ ^= m;
        in += t;
        inlen -= t;
    }
    bufsize_ = inlen & 7;
    u8 const* const end = in + (inlen - bufsize_);
    for ( ; in != end; in += 8 )
    {
        u64 m = u8to64_le( in );
        v3_ ^= m;
        sipround(v0_, v1_, v2_, v3_);
        sipround(v0_, v1_, v2_, v3_);
        v0_ ^= m;
    }
    std::copy(end, end + bufsize_, buf_);
}

siphash::operator std::size_t() noexcept
{
    using namespace detail;
    std::size_t b = static_cast<u64>(total_length_) << 56;
    switch(bufsize_)
    {
    case 7:
        b |= static_cast<u64>(buf_[6]) << 48;
    case 6:
        b |= static_cast<u64>(buf_[5]) << 40;
    case 5:
        b |= static_cast<u64>(buf_[4]) << 32;
    case 4:
        b |= static_cast<u64>(buf_[3]) << 24;
    case 3:
        b |= static_cast<u64>(buf_[2]) << 16;
    case 2:
        b |= static_cast<u64>(buf_[1]) << 8;
    case 1:
        b |= static_cast<u64>(buf_[0]);
    case 0:
        break;
    }
    v3_ ^= b;
    sipround(v0_, v1_, v2_, v3_);
    sipround(v0_, v1_, v2_, v3_);
    v0_ ^= b;
    v2_ ^= 0xff;
    sipround(v0_, v1_, v2_, v3_);
    sipround(v0_, v1_, v2_, v3_);
    sipround(v0_, v1_, v2_, v3_);
    sipround(v0_, v1_, v2_, v3_);
    b = v0_ ^ v1_ ^ v2_  ^ v3_;
    return b;
}

} // beast
