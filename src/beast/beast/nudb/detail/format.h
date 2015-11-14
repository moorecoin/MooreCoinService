//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2014, vinnie falco <vinnie.falco@gmail.com>

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

#ifndef beast_nudb_format_h_included
#define beast_nudb_format_h_included

#include <beast/nudb/common.h>
#include <beast/nudb/detail/buffer.h>
#include <beast/nudb/detail/field.h>
#include <beast/nudb/detail/stream.h>
#include <beast/config/compilerconfig.h> // for beast_constexpr
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <beast/cxx14/type_traits.h> // <type_traits>

namespace beast {
namespace nudb {
namespace detail {

// format of the nudb files:

static std::size_t beast_constexpr currentversion = 2;

struct dat_file_header
{
    static std::size_t beast_constexpr size =
        8 +     // type
        2 +     // version
        8 +     // uid
        8 +     // appnum
        2 +     // keysize

        64;     // (reserved)

    char type[8];
    std::size_t version;
    std::uint64_t uid;
    std::uint64_t appnum;
    std::size_t key_size;
};

struct key_file_header
{
    static std::size_t beast_constexpr size =
        8 +     // type
        2 +     // version
        8 +     // uid
        8 +     // appnum
        2 +     // keysize

        8 +     // salt
        8 +     // pepper
        2 +     // blocksize
        2 +     // loadfactor

        56;     // (reserved)

    char type[8];
    std::size_t version;
    std::uint64_t uid;
    std::uint64_t appnum;
    std::size_t key_size;

    std::uint64_t salt;
    std::uint64_t pepper;
    std::size_t block_size;
    std::size_t load_factor;

    // computed values
    std::size_t capacity;
    std::size_t bucket_size;
    std::size_t buckets;
    std::size_t modulus;
};

struct log_file_header
{
    static std::size_t beast_constexpr size =
        8 +     // type
        2 +     // version
        8 +     // uid
        8 +     // appnum
        2 +     // keysize

        8 +     // salt
        8 +     // pepper
        2 +     // blocksize

        8 +     // keyfilesize
        8;      // datafilesize

    char type[8];
    std::size_t version;
    std::uint64_t uid;
    std::uint64_t appnum;
    std::size_t key_size;
    std::uint64_t salt;
    std::uint64_t pepper;
    std::size_t block_size;
    std::size_t key_file_size;
    std::size_t dat_file_size;
};

// type used to store hashes in buckets.
// this can be smaller than the output
// of the hash function.
//
using hash_t = uint48_t;

static_assert(field<hash_t>::size <=
    sizeof(std::size_t), "");

template <class t>
std::size_t
make_hash (std::size_t h);

template<>
inline
std::size_t
make_hash<uint48_t>(std::size_t h)
{
    return (h>>16)&0xffffffffffff;
}

// returns the hash of a key given the salt.
// note: the hash is expressed in hash_t units
//
template <class hasher>
inline
std::size_t
hash (void const* key,
    std::size_t key_size, std::size_t salt)
{
    hasher h (salt);
    h.append (key, key_size);
    return make_hash<hash_t>(static_cast<
        typename hasher::result_type>(h));
}

// computes pepper from salt
//
template <class hasher>
std::size_t
pepper (std::size_t salt)
{
    hasher h (salt);
    h.append (&salt, sizeof(salt));
    return static_cast<std::size_t>(h);
}

// returns the actual size of a bucket.
// this can be smaller than the block size.
//
template <class = void>
std::size_t
bucket_size (std::size_t capacity)
{
    // bucket record
    return
        field<std::uint16_t>::size +    // count
        field<uint48_t>::size +         // spill
        capacity * (
            field<uint48_t>::size +     // offset
            field<uint48_t>::size +     // size
            field<hash_t>::size);       // hash
}

// returns the number of entries that fit in a bucket
//
template <class = void>
std::size_t
bucket_capacity (std::size_t block_size)
{
    // bucket record
    auto const size =
        field<std::uint16_t>::size +    // count
        field<uint48_t>::size;          // spill
    auto const entry_size =
        field<uint48_t>::size +         // offset
        field<uint48_t>::size +         // size
        field<hash_t>::size;            // hash
    if (block_size < key_file_header::size ||
        block_size < size)
        return 0;
    return (block_size - size) / entry_size;
}

// returns the number of bytes occupied by a value record
inline
std::size_t
value_size (std::size_t size,
    std::size_t key_size)
{
    // data record
    return
        field<uint48_t>::size + // size
        key_size +              // key
        size;                   // data
}

// returns the closest power of 2 not less than x
template <class = void>
std::size_t
ceil_pow2 (unsigned long long x)
{
    static const unsigned long long t[6] = {
        0xffffffff00000000ull,
        0x00000000ffff0000ull,
        0x000000000000ff00ull,
        0x00000000000000f0ull,
        0x000000000000000cull,
        0x0000000000000002ull
    };

    int y = (((x & (x - 1)) == 0) ? 0 : 1);
    int j = 32;
    int i;

    for(i = 0; i < 6; i++) {
        int k = (((x & t[i]) == 0) ? 0 : j);
        y += k;
        x >>= k;
        j >>= 1;
    }

    return std::size_t(1)<<y;
}

//------------------------------------------------------------------------------

// read data file header from stream
template <class = void>
void
read (istream& is, dat_file_header& dh)
{
    read (is, dh.type, sizeof(dh.type));
    read<std::uint16_t>(is, dh.version);
    read<std::uint64_t>(is, dh.uid);
    read<std::uint64_t>(is, dh.appnum);
    read<std::uint16_t>(is, dh.key_size);
    std::array <std::uint8_t, 64> reserved;
    read (is,
        reserved.data(), reserved.size());
}

// read data file header from file
template <class file>
void
read (file& f, dat_file_header& dh)
{
    std::array<std::uint8_t,
        dat_file_header::size> buf;
    try
    {
        f.read(0, buf.data(), buf.size());
    }
    catch (file_short_read_error const&)
    {
        throw store_corrupt_error(
            "short data file header");
    }
    istream is(buf);
    read (is, dh);
}

// write data file header to stream
template <class = void>
void
write (ostream& os, dat_file_header const& dh)
{
    write (os, "nudb.dat", 8);
    write<std::uint16_t>(os, dh.version);
    write<std::uint64_t>(os, dh.uid);
    write<std::uint64_t>(os, dh.appnum);
    write<std::uint16_t>(os, dh.key_size);
    std::array <std::uint8_t, 64> reserved;
    reserved.fill(0);
    write (os,
        reserved.data(), reserved.size());
}

// write data file header to file
template <class file>
void
write (file& f, dat_file_header const& dh)
{
    std::array <std::uint8_t,
        dat_file_header::size> buf;
    ostream os(buf);
    write(os, dh);
    f.write (0, buf.data(), buf.size());
}

// read key file header from stream
template <class = void>
void
read (istream& is, std::size_t file_size,
    key_file_header& kh)
{
    read(is, kh.type, sizeof(kh.type));
    read<std::uint16_t>(is, kh.version);
    read<std::uint64_t>(is, kh.uid);
    read<std::uint64_t>(is, kh.appnum);
    read<std::uint16_t>(is, kh.key_size);
    read<std::uint64_t>(is, kh.salt);
    read<std::uint64_t>(is, kh.pepper);
    read<std::uint16_t>(is, kh.block_size);
    read<std::uint16_t>(is, kh.load_factor);
    std::array <std::uint8_t, 56> reserved;
    read (is,
        reserved.data(), reserved.size());

    // vfalco these need to be checked to handle
    //        when the file size is too small
    kh.capacity = bucket_capacity(kh.block_size);
    kh.bucket_size = bucket_size(kh.capacity);
    if (file_size > kh.block_size)
    {
        // vfalco this should be handled elsewhere.
        //        we shouldn't put the computed fields
        //        in this header.
        if (kh.block_size > 0)
            kh.buckets = (file_size - kh.bucket_size)
                / kh.block_size;
        else
            // vfalco corruption or logic error
            kh.buckets = 0;
    }
    else
    {
        kh.buckets = 0;
    }
    kh.modulus = ceil_pow2(kh.buckets);
}

// read key file header from file
template <class file>
void
read (file& f, key_file_header& kh)
{
    std::array <std::uint8_t,
        key_file_header::size> buf;
    try
    {
        f.read(0, buf.data(), buf.size());
    }
    catch (file_short_read_error const&)
    {
        throw store_corrupt_error(
            "short key file header");
    }
    istream is(buf);
    read (is, f.actual_size(), kh);
}

// write key file header to stream
template <class = void>
void
write (ostream& os, key_file_header const& kh)
{
    write (os, "nudb.key", 8);
    write<std::uint16_t>(os, kh.version);
    write<std::uint64_t>(os, kh.uid);
    write<std::uint64_t>(os, kh.appnum);
    write<std::uint16_t>(os, kh.key_size);
    write<std::uint64_t>(os, kh.salt);
    write<std::uint64_t>(os, kh.pepper);
    write<std::uint16_t>(os, kh.block_size);
    write<std::uint16_t>(os, kh.load_factor);
    std::array <std::uint8_t, 56> reserved;
    reserved.fill (0);
    write (os,
        reserved.data(), reserved.size());
}

// write key file header to file
template <class file>
void
write (file& f, key_file_header const& kh)
{
    if (kh.block_size < key_file_header::size)
        throw std::logic_error(
            "nudb: block size too small");
    buffer buf(kh.block_size);
    std::fill(buf.get(),
        buf.get() + buf.size(), 0);
    ostream os (buf.get(), buf.size());
    write (os, kh);
    f.write (0, buf.get(), buf.size());
}

// read log file header from stream
template <class = void>
void
read (istream& is, log_file_header& lh)
{
    read (is, lh.type, sizeof(lh.type));
    read<std::uint16_t>(is, lh.version);
    read<std::uint64_t>(is, lh.uid);
    read<std::uint64_t>(is, lh.appnum);
    read<std::uint16_t>(is, lh.key_size);
    read<std::uint64_t>(is, lh.salt);
    read<std::uint64_t>(is, lh.pepper);
    read<std::uint16_t>(is, lh.block_size);
    read<std::uint64_t>(is, lh.key_file_size);
    read<std::uint64_t>(is, lh.dat_file_size);
}

// read log file header from file
template <class file>
void
read (file& f, log_file_header& lh)
{
    std::array <std::uint8_t,
        log_file_header::size> buf;
    // can throw file_short_read_error to callers
    f.read (0, buf.data(), buf.size());
    istream is(buf);
    read (is, lh);
}

// write log file header to stream
template <class = void>
void
write (ostream& os, log_file_header const& lh)
{
    write (os, "nudb.log", 8);
    write<std::uint16_t>(os, lh.version);
    write<std::uint64_t>(os, lh.uid);
    write<std::uint64_t>(os, lh.appnum);
    write<std::uint16_t>(os, lh.key_size);
    write<std::uint64_t>(os, lh.salt);
    write<std::uint64_t>(os, lh.pepper);
    write<std::uint16_t>(os, lh.block_size);
    write<std::uint64_t>(os, lh.key_file_size);
    write<std::uint64_t>(os, lh.dat_file_size);
}

// write log file header to file
template <class file>
void
write (file& f, log_file_header const& lh)
{
    std::array <std::uint8_t,
        log_file_header::size> buf;
    ostream os (buf);
    write (os, lh);
    f.write (0, buf.data(), buf.size());
}

template <class = void>
void
verify (dat_file_header const& dh)
{
    std::string const type (dh.type, 8);
    if (type != "nudb.dat")
        throw store_corrupt_error (
            "bad type in data file");
    if (dh.version != currentversion)
        throw store_corrupt_error (
            "bad version in data file");
    if (dh.key_size < 1)
        throw store_corrupt_error (
            "bad key size in data file");
}

template <class hasher>
void
verify (key_file_header const& kh)
{
    std::string const type (kh.type, 8);
    if (type != "nudb.key")
        throw store_corrupt_error (
            "bad type in key file");
    if (kh.version != currentversion)
        throw store_corrupt_error (
            "bad version in key file");
    if (kh.key_size < 1)
        throw store_corrupt_error (
            "bad key size in key file");
    if (kh.pepper != pepper<hasher>(kh.salt))
        throw store_corrupt_error(
            "wrong hash function for key file");
    if (kh.load_factor < 1)
        throw store_corrupt_error (
            "bad load factor in key file");
    if (kh.capacity < 1)
        throw store_corrupt_error (
            "bad capacity in key file");
    if (kh.buckets < 1)
        throw store_corrupt_error (
            "bad key file size");
}

template <class hasher>
void
verify (log_file_header const& lh)
{
    std::string const type (lh.type, 8);
    if (type != "nudb.log")
        throw store_corrupt_error (
            "bad type in log file");
    if (lh.version != currentversion)
        throw store_corrupt_error (
            "bad version in log file");
    if (lh.pepper != pepper<hasher>(lh.salt))
        throw store_corrupt_error(
            "wrong hash function for log file");
    if (lh.key_size < 1)
        throw store_corrupt_error (
            "bad key size in log file");
}

// make sure key file and value file headers match
template <class hasher>
void
verify (dat_file_header const& dh,
    key_file_header const& kh)
{
    verify<hasher> (kh);
    if (kh.uid != dh.uid)
        throw store_corrupt_error(
            "uid mismatch");
    if (kh.appnum != dh.appnum)
        throw store_corrupt_error(
            "appnum mismatch");
    if (kh.key_size != dh.key_size)
        throw store_corrupt_error(
            "key size mismatch");
}

template <class hasher>
void
verify (key_file_header const& kh,
    log_file_header const& lh)
{
    verify<hasher>(lh);
    if (kh.uid != lh.uid)
        throw store_corrupt_error (
            "uid mismatch in log file");
    if (kh.appnum != lh.appnum)
        throw store_corrupt_error(
            "appnum mismatch in log file");
    if (kh.key_size != lh.key_size)
        throw store_corrupt_error (
            "key size mismatch in log file");
    if (kh.salt != lh.salt)
        throw store_corrupt_error (
            "salt mismatch in log file");
    if (kh.pepper != lh.pepper)
        throw store_corrupt_error (
            "pepper mismatch in log file");
    if (kh.block_size != lh.block_size)
        throw store_corrupt_error (
            "block size mismatch in log file");
}

} // detail
} // nudb
} // beast

#endif
