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

#ifndef beast_nudb_tests_common_h_included
#define beast_nudb_tests_common_h_included

#include <beast/nudb.h>
#include <beast/nudb/identity_codec.h>
#include <beast/nudb/tests/fail_file.h>
#include <beast/hash/xxhasher.h>
#include <beast/random/xor_shift_engine.h>
#include <cstdint>
#include <iomanip>
#include <memory>

namespace beast {
namespace nudb {
namespace test {

using key_type = std::size_t;

// xxhasher is fast and produces good results
using test_api_base =
    nudb::api<xxhasher, identity_codec, native_file>;

struct test_api : test_api_base
{
    using fail_store = nudb::store<
        typename test_api_base::hash_type,
            typename test_api_base::codec_type,
            nudb::fail_file <typename test_api_base::file_type>>;
};

static std::size_t beast_constexpr arena_alloc_size = 16 * 1024 * 1024;

static std::uint64_t beast_constexpr appnum = 1337;

static std::uint64_t beast_constexpr salt = 42;

//------------------------------------------------------------------------------

// meets the requirements of handler
class storage
{
private:
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
    std::unique_ptr<std::uint8_t[]> buf_;

public:
    storage() = default;
    storage (storage const&) = delete;
    storage& operator= (storage const&) = delete;

    std::size_t
    size() const
    {
        return size_;
    }

    std::uint8_t*
    get() const
    {
        return buf_.get();
    }

    std::uint8_t*
    reserve (std::size_t size)
    {
        if (capacity_ < size)
        {
            capacity_ = detail::ceil_pow2(size);
            buf_.reset (
                new std::uint8_t[capacity_]);
        }
        size_ = size;
        return buf_.get();
    }

    std::uint8_t*
    operator()(void const* data, std::size_t size)
    {
        reserve (size);
        std::memcpy(buf_.get(), data, size);
        return buf_.get();
    }
};

struct value_type
{
    value_type() = default;
    value_type (value_type const&) = default;
    value_type& operator= (value_type const&) = default;

    key_type key;
    std::size_t size;
    uint8_t* data;
};

//------------------------------------------------------------------------------

template <class generator>
static
void
rngcpy (void* buffer, std::size_t bytes,
    generator& g)
{
    using result_type =
        typename generator::result_type;
    while (bytes >= sizeof(result_type))
    {
        auto const v = g();
        memcpy(buffer, &v, sizeof(v));
        buffer = reinterpret_cast<
            std::uint8_t*>(buffer) + sizeof(v);
        bytes -= sizeof(v);
    }
    if (bytes > 0)
    {
        auto const v = g();
        memcpy(buffer, &v, bytes);
    }
}

//------------------------------------------------------------------------------

class sequence
{
public:
    using key_type = test::key_type;

private:
    enum
    {
        minsize = 250,
        maxsize = 1250
    };

    storage s_;
    beast::xor_shift_engine gen_;
    std::uniform_int_distribution<std::uint32_t> d_size_;

public:
    sequence()
        : d_size_ (minsize, maxsize)
    {
    }

    // returns the n-th key
    key_type
    key (std::size_t n)
    {
        gen_.seed(n+1);
        key_type result;
        rngcpy (&result, sizeof(result), gen_);
        return result;
    }

    // returns the n-th value
    value_type
    operator[] (std::size_t n)
    {
        gen_.seed(n+1);
        value_type v;
        rngcpy (&v.key, sizeof(v.key), gen_);
        v.size = d_size_(gen_);
        v.data = s_.reserve(v.size);
        rngcpy (v.data, v.size, gen_);
        return v;
    }
};

template <class t>
static
std::string
num (t t)
{
    std::string s = std::to_string(t);
    std::reverse(s.begin(), s.end());
    std::string s2;
    s2.reserve(s.size() + (s.size()+2)/3);
    int n = 0;
    for (auto c : s)
    {
        if (n == 3)
        {
            n = 0;
            s2.insert (s2.begin(), ',');
        }
        ++n;
        s2.insert(s2.begin(), c);
    }
    return s2;
}

template <class log>
void
print (log log,
    beast::nudb::verify_info const& info)
{
    log << "avg_fetch:       " << std::fixed << std::setprecision(3) <<
                                    info.avg_fetch;
    log << "waste:           " << std::fixed << std::setprecision(3) <<
                                    info.waste * 100 << "%";
    log << "overhead:        " << std::fixed << std::setprecision(1) <<
                                    info.overhead * 100 << "%";
    log << "actual_load:     " << std::fixed << std::setprecision(0) <<
                                    info.actual_load * 100 << "%";
    log << "version:         " << num(info.version);
    log << "uid:             " << std::showbase << std::hex << info.uid;
    log << "appnum:          " << info.appnum;
    log << "key_size:        " << num(info.key_size);
    log << "salt:            " << std::showbase << std::hex << info.salt;
    log << "pepper:          " << std::showbase << std::hex << info.pepper;
    log << "block_size:      " << num(info.block_size);
    log << "bucket_size:     " << num(info.bucket_size);
    log << "load_factor:     " << std::fixed << std::setprecision(0) <<
                                    info.load_factor * 100 << "%";
    log << "capacity:        " << num(info.capacity);
    log << "buckets:         " << num(info.buckets);
    log << "key_count:       " << num(info.key_count);
    log << "value_count:     " << num(info.value_count);
    log << "value_bytes:     " << num(info.value_bytes);
    log << "spill_count:     " << num(info.spill_count);
    log << "spill_count_tot: " << num(info.spill_count_tot);
    log << "spill_bytes:     " << num(info.spill_bytes);
    log << "spill_bytes_tot: " << num(info.spill_bytes_tot);
    log << "key_file_size:   " << num(info.key_file_size);
    log << "dat_file_size:   " << num(info.dat_file_size);

    std::string s;
    for (int i = 0; i < info.hist.size(); ++i)
        s += (i==0) ?
            std::to_string(info.hist[i]) :
            (", " + std::to_string(info.hist[i]));
    log << "hist:            " << s;
}

} // test
} // nudb
} // beast

#endif
