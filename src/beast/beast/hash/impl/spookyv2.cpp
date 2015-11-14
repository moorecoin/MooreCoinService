// spooky hash
// a 128-bit noncryptographic hash, for checksums and table lookup
// by bob jenkins.  public domain.
//   oct 31 2010: published framework, disclaimer shorthash isn't right
//   nov 7 2010: disabled shorthash
//   oct 31 2011: replace end, shortmix, shortend, enable shorthash again
//   april 10 2012: buffer overflow on platforms without unaligned reads
//   july 12 2012: was passing out variables in final to in/out in short
//   july 30 2012: i reintroduced the buffer overflow
//   august 5 2012: spookyv2: d = should be d += in short hash, and remove extra mix from long hash

#include <memory.h>
#include <beast/hash/impl/spookyv2.h>

#ifdef _msc_ver
#pragma warning (push)
#pragma warning (disable: 4127) // conditional expression is constant
#pragma warning (disable: 4244) // conversion from 'size_t' to 'uint8', possible loss of data
#endif

#define allow_unaligned_reads 1

//
// short hash ... it could be used on any message, 
// but it's used by spooky just for short messages.
//
void spookyhash::short(
    const void *message,
    size_t length,
    uint64 *hash1,
    uint64 *hash2)
{
    uint64 buf[2*sc_numvars];
    union 
    { 
        const uint8 *p8; 
        uint32 *p32;
        uint64 *p64; 
        size_t i; 
    } u;

    u.p8 = (const uint8 *)message;
    
    if (!allow_unaligned_reads && (u.i & 0x7))
    {
        memcpy(buf, message, length);
        u.p64 = buf;
    }

    size_t remainder = length%32;
    uint64 a=*hash1;
    uint64 b=*hash2;
    uint64 c=sc_const;
    uint64 d=sc_const;

    if (length > 15)
    {
        const uint64 *end = u.p64 + (length/32)*4;
        
        // handle all complete sets of 32 bytes
        for (; u.p64 < end; u.p64 += 4)
        {
            c += u.p64[0];
            d += u.p64[1];
            shortmix(a,b,c,d);
            a += u.p64[2];
            b += u.p64[3];
        }
        
        //handle the case of 16+ remaining bytes.
        if (remainder >= 16)
        {
            c += u.p64[0];
            d += u.p64[1];
            shortmix(a,b,c,d);
            u.p64 += 2;
            remainder -= 16;
        }
    }
    
    // handle the last 0..15 bytes, and its length
    d += ((uint64)length) << 56;
    switch (remainder)
    {
    case 15:
    d += ((uint64)u.p8[14]) << 48;
    case 14:
        d += ((uint64)u.p8[13]) << 40;
    case 13:
        d += ((uint64)u.p8[12]) << 32;
    case 12:
        d += u.p32[2];
        c += u.p64[0];
        break;
    case 11:
        d += ((uint64)u.p8[10]) << 16;
    case 10:
        d += ((uint64)u.p8[9]) << 8;
    case 9:
        d += (uint64)u.p8[8];
    case 8:
        c += u.p64[0];
        break;
    case 7:
        c += ((uint64)u.p8[6]) << 48;
    case 6:
        c += ((uint64)u.p8[5]) << 40;
    case 5:
        c += ((uint64)u.p8[4]) << 32;
    case 4:
        c += u.p32[0];
        break;
    case 3:
        c += ((uint64)u.p8[2]) << 16;
    case 2:
        c += ((uint64)u.p8[1]) << 8;
    case 1:
        c += (uint64)u.p8[0];
        break;
    case 0:
        c += sc_const;
        d += sc_const;
    }
    shortend(a,b,c,d);
    *hash1 = a;
    *hash2 = b;
}




// do the whole hash in one call
void spookyhash::hash128(
    const void *message, 
    size_t length, 
    uint64 *hash1, 
    uint64 *hash2)
{
    if (length < sc_bufsize)
    {
        short(message, length, hash1, hash2);
        return;
    }

    uint64 h0,h1,h2,h3,h4,h5,h6,h7,h8,h9,h10,h11;
    uint64 buf[sc_numvars];
    uint64 *end;
    union 
    { 
        const uint8 *p8; 
        uint64 *p64; 
        size_t i; 
    } u;
    size_t remainder;
    
    h0=h3=h6=h9  = *hash1;
    h1=h4=h7=h10 = *hash2;
    h2=h5=h8=h11 = sc_const;
    
    u.p8 = (const uint8 *)message;
    end = u.p64 + (length/sc_blocksize)*sc_numvars;

    // handle all whole sc_blocksize blocks of bytes
    if (allow_unaligned_reads || ((u.i & 0x7) == 0))
    {
        while (u.p64 < end)
        { 
            mix(u.p64, h0,h1,h2,h3,h4,h5,h6,h7,h8,h9,h10,h11);
        u.p64 += sc_numvars;
        }
    }
    else
    {
        while (u.p64 < end)
        {
            memcpy(buf, u.p64, sc_blocksize);
            mix(buf, h0,h1,h2,h3,h4,h5,h6,h7,h8,h9,h10,h11);
        u.p64 += sc_numvars;
        }
    }

    // handle the last partial block of sc_blocksize bytes
    remainder = (length - ((const uint8 *)end-(const uint8 *)message));
    memcpy(buf, end, remainder);
    memset(((uint8 *)buf)+remainder, 0, sc_blocksize-remainder);
    ((uint8 *)buf)[sc_blocksize-1] =
        static_cast<uint8>(remainder);
    
    // do some final mixing 
    end(buf, h0,h1,h2,h3,h4,h5,h6,h7,h8,h9,h10,h11);
    *hash1 = h0;
    *hash2 = h1;
}



// init spooky state
void spookyhash::init(uint64 seed1, uint64 seed2)
{
    m_length = 0;
    m_remainder = 0;
    m_state[0] = seed1;
    m_state[1] = seed2;
}


// add a message fragment to the state
void spookyhash::update(const void *message, size_t length)
{
    uint64 h0,h1,h2,h3,h4,h5,h6,h7,h8,h9,h10,h11;
    size_t newlength = length + m_remainder;
    uint8  remainder;
    union 
    { 
        const uint8 *p8; 
        uint64 *p64; 
        size_t i; 
    } u;
    const uint64 *end;
    
    // is this message fragment too short?  if it is, stuff it away.
    if (newlength < sc_bufsize)
    {
        memcpy(&((uint8 *)m_data)[m_remainder], message, length);
        m_length = length + m_length;
        m_remainder = (uint8)newlength;
        return;
    }
    
    // init the variables
    if (m_length < sc_bufsize)
    {
        h0=h3=h6=h9  = m_state[0];
        h1=h4=h7=h10 = m_state[1];
        h2=h5=h8=h11 = sc_const;
    }
    else
    {
        h0 = m_state[0];
        h1 = m_state[1];
        h2 = m_state[2];
        h3 = m_state[3];
        h4 = m_state[4];
        h5 = m_state[5];
        h6 = m_state[6];
        h7 = m_state[7];
        h8 = m_state[8];
        h9 = m_state[9];
        h10 = m_state[10];
        h11 = m_state[11];
    }
    m_length = length + m_length;
    
    // if we've got anything stuffed away, use it now
    if (m_remainder)
    {
        uint8 prefix = sc_bufsize-m_remainder;
        memcpy(&(((uint8 *)m_data)[m_remainder]), message, prefix);
        u.p64 = m_data;
        mix(u.p64, h0,h1,h2,h3,h4,h5,h6,h7,h8,h9,h10,h11);
        mix(&u.p64[sc_numvars], h0,h1,h2,h3,h4,h5,h6,h7,h8,h9,h10,h11);
        u.p8 = ((const uint8 *)message) + prefix;
        length -= prefix;
    }
    else
    {
        u.p8 = (const uint8 *)message;
    }
    
    // handle all whole blocks of sc_blocksize bytes
    end = u.p64 + (length/sc_blocksize)*sc_numvars;
    remainder = (uint8)(length-((const uint8 *)end-u.p8));
    if (allow_unaligned_reads || (u.i & 0x7) == 0)
    {
        while (u.p64 < end)
        { 
            mix(u.p64, h0,h1,h2,h3,h4,h5,h6,h7,h8,h9,h10,h11);
        u.p64 += sc_numvars;
        }
    }
    else
    {
        while (u.p64 < end)
        { 
            memcpy(m_data, u.p8, sc_blocksize);
            mix(m_data, h0,h1,h2,h3,h4,h5,h6,h7,h8,h9,h10,h11);
        u.p64 += sc_numvars;
        }
    }

    // stuff away the last few bytes
    m_remainder = remainder;
    memcpy(m_data, end, remainder);
    
    // stuff away the variables
    m_state[0] = h0;
    m_state[1] = h1;
    m_state[2] = h2;
    m_state[3] = h3;
    m_state[4] = h4;
    m_state[5] = h5;
    m_state[6] = h6;
    m_state[7] = h7;
    m_state[8] = h8;
    m_state[9] = h9;
    m_state[10] = h10;
    m_state[11] = h11;
}


// report the hash for the concatenation of all message fragments so far
void spookyhash::final(uint64 *hash1, uint64 *hash2)
{
    // init the variables
    if (m_length < sc_bufsize)
    {
        *hash1 = m_state[0];
        *hash2 = m_state[1];
        short( m_data, m_length, hash1, hash2);
        return;
    }
    
    const uint64 *data = (const uint64 *)m_data;
    uint8 remainder = m_remainder;
    
    uint64 h0 = m_state[0];
    uint64 h1 = m_state[1];
    uint64 h2 = m_state[2];
    uint64 h3 = m_state[3];
    uint64 h4 = m_state[4];
    uint64 h5 = m_state[5];
    uint64 h6 = m_state[6];
    uint64 h7 = m_state[7];
    uint64 h8 = m_state[8];
    uint64 h9 = m_state[9];
    uint64 h10 = m_state[10];
    uint64 h11 = m_state[11];

    if (remainder >= sc_blocksize)
    {
        // m_data can contain two blocks; handle any whole first block
        mix(data, h0,h1,h2,h3,h4,h5,h6,h7,h8,h9,h10,h11);
        data += sc_numvars;
        remainder -= sc_blocksize;
    }

    // mix in the last partial block, and the length mod sc_blocksize
    memset(&((uint8 *)data)[remainder], 0, (sc_blocksize-remainder));

    ((uint8 *)data)[sc_blocksize-1] = remainder;
    
    // do some final mixing
    end(data, h0,h1,h2,h3,h4,h5,h6,h7,h8,h9,h10,h11);

    *hash1 = h0;
    *hash2 = h1;
}

#ifdef _msc_ver
#pragma warning (pop)
#endif
