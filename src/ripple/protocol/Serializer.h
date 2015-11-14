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

#ifndef ripple_protocol_serializer_h_included
#define ripple_protocol_serializer_h_included

#include <ripple/basics/byte_view.h>
#include <ripple/protocol/sfield.h>
#include <ripple/basics/base_uint.h>
#include <iomanip>
#include <sstream>

namespace ripple {

class ckey; // forward declaration

class serializer
{
public:
    typedef std::shared_ptr<serializer> pointer;

protected:
    blob mdata;

public:
    serializer (int n = 2048)
    {
        mdata.reserve (n);
    }
    serializer (blob const& data) : mdata (data)
    {
        ;
    }
    serializer (std::string const& data) : mdata (data.data (), (data.data ()) + data.size ())
    {
        ;
    }
    serializer (blob ::iterator begin, blob ::iterator end) :
        mdata (begin, end)
    {
        ;
    }
    serializer (blob ::const_iterator begin, blob ::const_iterator end) :
        mdata (begin, end)
    {
        ;
    }

    // assemble functions
    int add8 (unsigned char byte);
    int add16 (std::uint16_t);
    int add32 (std::uint32_t);      // ledger indexes, account sequence, timestamps
    int add64 (std::uint64_t);      // native currency amounts
    int add128 (const uint128&);    // private key generators
    int add256 (uint256 const& );       // transaction and ledger hashes

    template <typename integer>
    int addinteger(integer);

    template <int bits, class tag>
    int addbitstring(base_uint<bits, tag> const& v) {
        int ret = mdata.size ();
        mdata.insert (mdata.end (), v.begin (), v.end ());
        return ret;
    }

    // todo(tom): merge with add128 and add256.
    template <class tag>
    int add160 (base_uint<160, tag> const& i)
    {
        return addbitstring<160, tag>(i);
    }

    int addraw (blob const& vector);
    int addraw (const void* ptr, int len);
    int addraw (const serializer& s);
    int addzeros (size_t ubytes);

    int addvl (blob const& vector);
    int addvl (std::string const& string);
    int addvl (const void* ptr, int len);

    // disassemble functions
    bool get8 (int&, int offset) const;
    bool get8 (unsigned char&, int offset) const;
    bool get16 (std::uint16_t&, int offset) const;
    bool get32 (std::uint32_t&, int offset) const;
    bool get64 (std::uint64_t&, int offset) const;
    bool get128 (uint128&, int offset) const;
    bool get256 (uint256&, int offset) const;

    template <typename integer>
    bool getinteger(integer& number, int offset) {
        static const auto bytes = sizeof(integer);
        if ((offset + bytes) > mdata.size ())
            return false;
        number = 0;

        auto ptr = &mdata[offset];
        for (auto i = 0; i < bytes; ++i)
        {
            if (i)
                number <<= 8;
            number |= *ptr++;
        }
        return true;
    }

    template <int bits, typename tag = void>
    bool getbitstring(base_uint<bits, tag>& data, int offset) const {
        auto success = (offset + (bits / 8)) <= mdata.size ();
        if (success)
            memcpy (data.begin (), & (mdata.front ()) + offset, (bits / 8));
        return success;
    }

    uint256 get256 (int offset) const;

    // todo(tom): merge with get128 and get256.
    template <class tag>
    bool get160 (base_uint<160, tag>& o, int offset) const
    {
        return getbitstring<160, tag>(o, offset);
    }

    bool getraw (blob&, int offset, int length) const;
    blob getraw (int offset, int length) const;

    bool getvl (blob& objectvl, int offset, int& length) const;
    bool getvllength (int& length, int offset) const;

    bool getfieldid (int& type, int& name, int offset) const;
    int addfieldid (int type, int name);
    int addfieldid (serializedtypeid type, int name)
    {
        return addfieldid (static_cast<int> (type), name);
    }

    // normal hash functions
    uint160 getripemd160 (int size = -1) const;
    uint256 getsha256 (int size = -1) const;
    uint256 getsha512half (int size = -1) const;
    static uint256 getsha512half (const_byte_view v);

    static uint256 getsha512half (const unsigned char* data, int len);

    // prefix hash functions
    static uint256 getprefixhash (std::uint32_t prefix, const unsigned char* data, int len);
    uint256 getprefixhash (std::uint32_t prefix) const
    {
        return getprefixhash (prefix, & (mdata.front ()), mdata.size ());
    }
    static uint256 getprefixhash (std::uint32_t prefix, blob const& data)
    {
        return getprefixhash (prefix, & (data.front ()), data.size ());
    }
    static uint256 getprefixhash (std::uint32_t prefix, std::string const& strdata)
    {
        return getprefixhash (prefix, reinterpret_cast<const unsigned char*> (strdata.data ()), strdata.size ());
    }

    // totality functions
    blob const& peekdata () const
    {
        return mdata;
    }
    blob getdata () const
    {
        return mdata;
    }
    blob& moddata ()
    {
        return mdata;
    }
    int getcapacity () const
    {
        return mdata.capacity ();
    }
    int getdatalength () const
    {
        return mdata.size ();
    }
    const void* getdataptr () const
    {
        return &mdata.front ();
    }
    void* getdataptr ()
    {
        return &mdata.front ();
    }
    int getlength () const
    {
        return mdata.size ();
    }
    std::string getstring () const
    {
        return std::string (static_cast<const char*> (getdataptr ()), size ());
    }
    void secureerase ()
    {
        memset (& (mdata.front ()), 0, mdata.size ());
        erase ();
    }
    void erase ()
    {
        mdata.clear ();
    }
    int removelastbyte ();
    bool chop (int num);

    // vector-like functions
    blob ::iterator begin ()
    {
        return mdata.begin ();
    }
    blob ::iterator end ()
    {
        return mdata.end ();
    }
    blob ::const_iterator begin () const
    {
        return mdata.begin ();
    }
    blob ::const_iterator end () const
    {
        return mdata.end ();
    }
    blob ::size_type size () const
    {
        return mdata.size ();
    }
    void reserve (size_t n)
    {
        mdata.reserve (n);
    }
    void resize (size_t n)
    {
        mdata.resize (n);
    }
    size_t capacity () const
    {
        return mdata.capacity ();
    }

    bool operator== (blob const& v)
    {
        return v == mdata;
    }
    bool operator!= (blob const& v)
    {
        return v != mdata;
    }
    bool operator== (const serializer& v)
    {
        return v.mdata == mdata;
    }
    bool operator!= (const serializer& v)
    {
        return v.mdata != mdata;
    }

    std::string gethex () const
    {
        std::stringstream h;

        for (unsigned char const& element : mdata)
        {
            h <<
                std::setw (2) <<
                std::hex <<
                std::setfill ('0') <<
                static_cast<unsigned int>(element);
        }
        return h.str ();
    }

    // low-level vl length encode/decode functions
    static blob encodevl (int length);
    static int lengthvl (int length)
    {
        return length + encodelengthlength (length);
    }
    static int encodelengthlength (int length); // length to encode length
    static int decodelengthlength (int b1);
    static int decodevllength (int b1);
    static int decodevllength (int b1, int b2);
    static int decodevllength (int b1, int b2, int b3);

    static void testserializer ();
};

class serializeriterator
{
protected:
    const serializer& mserializer;
    int mpos;

public:

    // reference is not const because we don't want to bind to a temporary
    serializeriterator (serializer& s) : mserializer (s), mpos (0)
    {
        ;
    }

    const serializer& operator* (void)
    {
        return mserializer;
    }
    void reset (void)
    {
        mpos = 0;
    }
    void setpos (int p)
    {
        mpos = p;
    }

    int getpos (void)
    {
        return mpos;
    }
    bool empty ()
    {
        return mpos == mserializer.getlength ();
    }
    int getbytesleft ();

    // get functions throw on error
    unsigned char get8 ();
    std::uint16_t get16 ();
    std::uint32_t get32 ();
    std::uint64_t get64 ();

    uint128 get128 () { return getbitstring<128>(); }
    uint160 get160 () { return getbitstring<160>(); }
    uint256 get256 () { return getbitstring<256>(); }

    template <std::size_t bits, typename tag = void>
    void getbitstring (base_uint<bits, tag>& bits) {
        if (!mserializer.getbitstring<bits> (bits, mpos))
            throw std::runtime_error ("invalid serializer getbitstring");

        mpos += bits / 8;
    }

    template <std::size_t bits, typename tag = void>
    base_uint<bits, tag> getbitstring () {
        base_uint<bits, tag> bits;
        getbitstring(bits);
        return bits;
    }

    void getfieldid (int& type, int& field);

    blob getraw (int ilength);

    blob getvl ();
};

} // ripple

#endif
