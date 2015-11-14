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
#include <ripple/basics/log.h>
#include <ripple/protocol/serializer.h>
#include <openssl/ripemd.h>
#include <openssl/pem.h>

#ifdef use_sha512_asm
#include <beast/crypto/sha512asm.h>
#endif

namespace ripple {

int serializer::addzeros (size_t ubytes)
{
    int ret = mdata.size ();

    while (ubytes--)
        mdata.push_back (0);

    return ret;
}

int serializer::add16 (std::uint16_t i)
{
    int ret = mdata.size ();
    mdata.push_back (static_cast<unsigned char> (i >> 8));
    mdata.push_back (static_cast<unsigned char> (i & 0xff));
    return ret;
}

int serializer::add32 (std::uint32_t i)
{
    int ret = mdata.size ();
    mdata.push_back (static_cast<unsigned char> (i >> 24));
    mdata.push_back (static_cast<unsigned char> ((i >> 16) & 0xff));
    mdata.push_back (static_cast<unsigned char> ((i >> 8) & 0xff));
    mdata.push_back (static_cast<unsigned char> (i & 0xff));
    return ret;
}

int serializer::add64 (std::uint64_t i)
{
    int ret = mdata.size ();
    mdata.push_back (static_cast<unsigned char> (i >> 56));
    mdata.push_back (static_cast<unsigned char> ((i >> 48) & 0xff));
    mdata.push_back (static_cast<unsigned char> ((i >> 40) & 0xff));
    mdata.push_back (static_cast<unsigned char> ((i >> 32) & 0xff));
    mdata.push_back (static_cast<unsigned char> ((i >> 24) & 0xff));
    mdata.push_back (static_cast<unsigned char> ((i >> 16) & 0xff));
    mdata.push_back (static_cast<unsigned char> ((i >> 8) & 0xff));
    mdata.push_back (static_cast<unsigned char> (i & 0xff));
    return ret;
}

template <> int serializer::addinteger(unsigned char i) { return add8(i); }
template <> int serializer::addinteger(std::uint16_t i) { return add16(i); }
template <> int serializer::addinteger(std::uint32_t i) { return add32(i); }
template <> int serializer::addinteger(std::uint64_t i) { return add64(i); }

int serializer::add128 (const uint128& i)
{
    int ret = mdata.size ();
    mdata.insert (mdata.end (), i.begin (), i.end ());
    return ret;
}

int serializer::add256 (uint256 const& i)
{
    int ret = mdata.size ();
    mdata.insert (mdata.end (), i.begin (), i.end ());
    return ret;
}

int serializer::addraw (blob const& vector)
{
    int ret = mdata.size ();
    mdata.insert (mdata.end (), vector.begin (), vector.end ());
    return ret;
}

int serializer::addraw (const serializer& s)
{
    int ret = mdata.size ();
    mdata.insert (mdata.end (), s.begin (), s.end ());
    return ret;
}

int serializer::addraw (const void* ptr, int len)
{
    int ret = mdata.size ();
    mdata.insert (mdata.end (), (const char*) ptr, ((const char*)ptr) + len);
    return ret;
}

bool serializer::get16 (std::uint16_t& o, int offset) const
{
    if ((offset + 2) > mdata.size ()) return false;

    const unsigned char* ptr = &mdata[offset];
    o = *ptr++;
    o <<= 8;
    o |= *ptr;
    return true;
}

bool serializer::get32 (std::uint32_t& o, int offset) const
{
    if ((offset + 4) > mdata.size ()) return false;

    const unsigned char* ptr = &mdata[offset];
    o = *ptr++;
    o <<= 8;
    o |= *ptr++;
    o <<= 8;
    o |= *ptr++;
    o <<= 8;
    o |= *ptr;
    return true;
}

bool serializer::get64 (std::uint64_t& o, int offset) const
{
    if ((offset + 8) > mdata.size ()) return false;

    const unsigned char* ptr = &mdata[offset];
    o = *ptr++;
    o <<= 8;
    o |= *ptr++;
    o <<= 8;
    o |= *ptr++;
    o <<= 8;
    o |= *ptr++;
    o <<= 8;
    o |= *ptr++;
    o <<= 8;
    o |= *ptr++;
    o <<= 8;
    o |= *ptr++;
    o <<= 8;
    o |= *ptr;
    return true;
}

bool serializer::get128 (uint128& o, int offset) const
{
    if ((offset + (128 / 8)) > mdata.size ()) return false;

    memcpy (o.begin (), & (mdata.front ()) + offset, (128 / 8));
    return true;
}

bool serializer::get256 (uint256& o, int offset) const
{
    if ((offset + (256 / 8)) > mdata.size ()) return false;

    memcpy (o.begin (), & (mdata.front ()) + offset, (256 / 8));
    return true;
}

uint256 serializer::get256 (int offset) const
{
    uint256 ret;

    if ((offset + (256 / 8)) > mdata.size ()) return ret;

    memcpy (ret.begin (), & (mdata.front ()) + offset, (256 / 8));
    return ret;
}

int serializer::addfieldid (int type, int name)
{
    int ret = mdata.size ();
    assert ((type > 0) && (type < 256) && (name > 0) && (name < 256));

    if (type < 16)
    {
        if (name < 16) // common type, common name
            mdata.push_back (static_cast<unsigned char> ((type << 4) | name));
        else
        {
            // common type, uncommon name
            mdata.push_back (static_cast<unsigned char> (type << 4));
            mdata.push_back (static_cast<unsigned char> (name));
        }
    }
    else if (name < 16)
    {
        // uncommon type, common name
        mdata.push_back (static_cast<unsigned char> (name));
        mdata.push_back (static_cast<unsigned char> (type));
    }
    else
    {
        // uncommon type, uncommon name
        mdata.push_back (static_cast<unsigned char> (0));
        mdata.push_back (static_cast<unsigned char> (type));
        mdata.push_back (static_cast<unsigned char> (name));
    }

    return ret;
}

bool serializer::getfieldid (int& type, int& name, int offset) const
{
    if (!get8 (type, offset))
    {
        writelog (lswarning, serializer) << "gfid: unable to get type";
        return false;
    }

    // writelog(lsdebug, serializer) << "gfid: type " << type;
    name = type & 15;
    type >>= 4;

    if (type == 0)
    {
        // uncommon type
        if (!get8 (type, ++offset))
            return false;

        if ((type == 0) || (type < 16))
        {
            writelog (lswarning, serializer) << "gfid: uncommon type out of range " << type;
            return false;
        }
    }

    if (name == 0)
    {
        // uncommon name
        if (!get8 (name, ++offset))
            return false;

        if ((name == 0) || (name < 16))
        {
            writelog (lswarning, serializer) << "gfid: uncommon name out of range " << name;
            return false;
        }
    }

    return true;
}

int serializer::add8 (unsigned char byte)
{
    int ret = mdata.size ();
    mdata.push_back (byte);
    return ret;
}

bool serializer::get8 (int& byte, int offset) const
{
    if (offset >= mdata.size ()) return false;

    byte = mdata[offset];
    return true;
}

bool serializer::chop (int bytes)
{
    if (bytes > mdata.size ()) return false;

    mdata.resize (mdata.size () - bytes);
    return true;
}

int serializer::removelastbyte ()
{
    int size = mdata.size () - 1;

    if (size < 0)
    {
        assert (false);
        return -1;
    }

    int ret = mdata[size];
    mdata.resize (size);
    return ret;
}

bool serializer::getraw (blob& o, int offset, int length) const
{
    if ((offset + length) > mdata.size ()) return false;

    o.assign (mdata.begin () + offset, mdata.begin () + offset + length);
    return true;
}

blob serializer::getraw (int offset, int length) const
{
    blob o;

    if ((offset + length) > mdata.size ()) return o;

    o.assign (mdata.begin () + offset, mdata.begin () + offset + length);
    return o;
}

uint160 serializer::getripemd160 (int size) const
{
    uint160 ret;

    if ((size < 0) || (size > mdata.size ())) size = mdata.size ();

    ripemd160 (& (mdata.front ()), size, (unsigned char*) &ret);
    return ret;
}

uint256 serializer::getsha256 (int size) const
{
    uint256 ret;

    if ((size < 0) || (size > mdata.size ())) size = mdata.size ();

    sha256 (& (mdata.front ()), size, (unsigned char*) &ret);
    return ret;
}

uint256 serializer::getsha512half (int size) const
{
    assert (size != 0);
    if (size == 0)
        return uint256();
    if (size < 0 || size > mdata.size())
        return getsha512half (mdata);

    return getsha512half (const_byte_view (
        mdata.data(), mdata.data() + size));
}

uint256 serializer::getsha512half (const_byte_view v)
{
    uint256 j[2];
#ifndef use_sha512_asm
    sha512(v.data(), v.size(),
           reinterpret_cast<unsigned char*> (j));
#else
    sha512asm(v.data(), v.size(),
            reinterpret_cast<unsigned char*> (j));
#endif
    return j[0];
}

uint256 serializer::getsha512half (const unsigned char* data, int len)
{
    uint256 j[2];
#ifndef use_sha512_asm
    sha512 (data, len, (unsigned char*) j);
#else
    sha512asm (data, len, (unsigned char*) j);
#endif
    return j[0];
}

uint256 serializer::getprefixhash (std::uint32_t prefix, const unsigned char* data, int len)
{
    char be_prefix[4];
    be_prefix[0] = static_cast<unsigned char> (prefix >> 24);
    be_prefix[1] = static_cast<unsigned char> ((prefix >> 16) & 0xff);
    be_prefix[2] = static_cast<unsigned char> ((prefix >> 8) & 0xff);
    be_prefix[3] = static_cast<unsigned char> (prefix & 0xff);

    uint256 j[2];
    sha512_ctx ctx;
    sha512_init (&ctx);
    sha512_update (&ctx, &be_prefix[0], 4);
    sha512_update (&ctx, data, len);
    sha512_final (reinterpret_cast<unsigned char*> (&j[0]), &ctx);

    return j[0];
}

int serializer::addvl (blob const& vector)
{
    int ret = addraw (encodevl (vector.size ()));
    addraw (vector);
    assert (mdata.size () == (ret + vector.size () + encodelengthlength (vector.size ())));
    return ret;
}

int serializer::addvl (const void* ptr, int len)
{
    int ret = addraw (encodevl (len));

    if (len)
        addraw (ptr, len);

    return ret;
}

int serializer::addvl (std::string const& string)
{
    int ret = addraw (string.size ());

    if (!string.empty ())
        addraw (string.data (), string.size ());

    return ret;
}

bool serializer::getvl (blob& objectvl, int offset, int& length) const
{
    int b1;

    if (!get8 (b1, offset++)) return false;

    int datlen, lenlen = decodelengthlength (b1);

    try
    {
        if (lenlen == 1)
            datlen = decodevllength (b1);
        else if (lenlen == 2)
        {
            int b2;

            if (!get8 (b2, offset++)) return false;

            datlen = decodevllength (b1, b2);
        }
        else if (lenlen == 3)
        {
            int b2, b3;

            if (!get8 (b2, offset++)) return false;

            if (!get8 (b3, offset++)) return false;

            datlen = decodevllength (b1, b2, b3);
        }
        else return false;
    }
    catch (...)
    {
        return false;
    }

    length = lenlen + datlen;
    return getraw (objectvl, offset, datlen);
}

bool serializer::getvllength (int& length, int offset) const
{
    int b1;

    if (!get8 (b1, offset++)) return false;

    int lenlen = decodelengthlength (b1);

    try
    {
        if (lenlen == 1)
            length = decodevllength (b1);
        else if (lenlen == 2)
        {
            int b2;

            if (!get8 (b2, offset++)) return false;

            length = decodevllength (b1, b2);
        }
        else if (lenlen == 3)
        {
            int b2, b3;

            if (!get8 (b2, offset++)) return false;

            if (!get8 (b3, offset++)) return false;

            length = decodevllength (b1, b2, b3);
        }
        else return false;
    }
    catch (...)
    {
        return false;
    }

    return true;
}

blob serializer::encodevl (int length)
{
    unsigned char lenbytes[4];

    if (length <= 192)
    {
        lenbytes[0] = static_cast<unsigned char> (length);
        return blob (&lenbytes[0], &lenbytes[1]);
    }
    else if (length <= 12480)
    {
        length -= 193;
        lenbytes[0] = 193 + static_cast<unsigned char> (length >> 8);
        lenbytes[1] = static_cast<unsigned char> (length & 0xff);
        return blob (&lenbytes[0], &lenbytes[2]);
    }
    else if (length <= 918744)
    {
        length -= 12481;
        lenbytes[0] = 241 + static_cast<unsigned char> (length >> 16);
        lenbytes[1] = static_cast<unsigned char> ((length >> 8) & 0xff);
        lenbytes[2] = static_cast<unsigned char> (length & 0xff);
        return blob (&lenbytes[0], &lenbytes[3]);
    }
    else throw std::overflow_error ("lenlen");
}

int serializer::encodelengthlength (int length)
{
    if (length < 0) throw std::overflow_error ("len<0");

    if (length <= 192) return 1;

    if (length <= 12480) return 2;

    if (length <= 918744) return 3;

    throw std::overflow_error ("len>918744");
}

int serializer::decodelengthlength (int b1)
{
    if (b1 < 0) throw std::overflow_error ("b1<0");

    if (b1 <= 192) return 1;

    if (b1 <= 240) return 2;

    if (b1 <= 254) return 3;

    throw std::overflow_error ("b1>254");
}

int serializer::decodevllength (int b1)
{
    if (b1 < 0) throw std::overflow_error ("b1<0");

    if (b1 > 254) throw std::overflow_error ("b1>254");

    return b1;
}

int serializer::decodevllength (int b1, int b2)
{
    if (b1 < 193) throw std::overflow_error ("b1<193");

    if (b1 > 240) throw std::overflow_error ("b1>240");

    return 193 + (b1 - 193) * 256 + b2;
}

int serializer::decodevllength (int b1, int b2, int b3)
{
    if (b1 < 241) throw std::overflow_error ("b1<241");

    if (b1 > 254) throw std::overflow_error ("b1>254");

    return 12481 + (b1 - 241) * 65536 + b2 * 256 + b3;
}

void serializer::testserializer ()
{
    serializer s (64);
}

int serializeriterator::getbytesleft ()
{
    return mserializer.size () - mpos;
}

void serializeriterator::getfieldid (int& type, int& field)
{
    if (!mserializer.getfieldid (type, field, mpos))
        throw std::runtime_error ("invalid serializer getfieldid");

    ++mpos;

    if (type >= 16)
        ++mpos;

    if (field >= 16)
        ++mpos;
}

unsigned char serializeriterator::get8 ()
{
    int val;

    if (!mserializer.get8 (val, mpos)) throw std::runtime_error ("invalid serializer get8");

    ++mpos;
    return val;
}

std::uint16_t serializeriterator::get16 ()
{
    std::uint16_t val;

    if (!mserializer.get16 (val, mpos)) throw std::runtime_error ("invalid serializer get16");

    mpos += 16 / 8;
    return val;
}

std::uint32_t serializeriterator::get32 ()
{
    std::uint32_t val;

    if (!mserializer.get32 (val, mpos)) throw std::runtime_error ("invalid serializer get32");

    mpos += 32 / 8;
    return val;
}

std::uint64_t serializeriterator::get64 ()
{
    std::uint64_t val;

    if (!mserializer.get64 (val, mpos)) throw std::runtime_error ("invalid serializer get64");

    mpos += 64 / 8;
    return val;
}

blob serializeriterator::getvl ()
{
    int length;
    blob vl;

    if (!mserializer.getvl (vl, mpos, length)) throw std::runtime_error ("invalid serializer getvl");

    mpos += length;
    return vl;
}

blob serializeriterator::getraw (int ilength)
{
    int ipos    = mpos;
    mpos        += ilength;

    return mserializer.getraw (ipos, ilength);
}

} // ripple
