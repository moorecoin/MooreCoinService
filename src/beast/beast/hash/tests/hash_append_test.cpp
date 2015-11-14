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
#include <beastconfig.h>
#endif

#include <beast/hash/tests/hash_metrics.h>
#include <beast/hash/hash_append.h>
#include <beast/chrono/chrono_io.h>
#include <beast/unit_test/suite.h>
#include <beast/utility/type_name.h>
#include <array>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <random>

namespace beast {

template <class block, class derived>
class block_stream
{
private:
    block m_block;
    std::size_t m_size;

    std::size_t
    needed() const noexcept
    {
        return sizeof(block) - m_size;
    }

    void*
    tail() noexcept
    {
        return ((char *)&m_block) + m_size;
    }

protected:
    void
    finish()
    {
        if (m_size > 0)
        {
            // zero-pad
            memset (tail(), 0, needed());
            static_cast <derived*> (this)->process_block (m_block);
        }
    }

public:
    block_stream ()
        : m_size(0)
    {
    }

    void
    operator() (void const* data, std::size_t bytes) noexcept
    {
        // handle leftovers
        if (m_size > 0)
        {
            std::size_t const n (std::min (needed(), bytes));
            std::memcpy (tail(), data, n);
            data = ((char const*)data) + n;
            bytes -= n;
            m_size += n;

            if (m_size < sizeof(block))
                return;

            static_cast <derived*> (this)->process_block (m_block);
        }

        // loop over complete blocks
        while (bytes >= sizeof(block))
        {
            m_block = *((block const*)data);
            static_cast <derived*> (this)->process_block (m_block);
            data = ((char const*)data) + sizeof(m_block);
            bytes -= sizeof(m_block);
        }

        // save leftovers
        if (bytes > 0)
        {
            memcpy (tail(), data, bytes);
            m_size += bytes;
        }
    }
};

//------------------------------------------------------------------------------

namespace hash_append_tests {

template <std::size_t> class fnv1a_imp;

template <>
class fnv1a_imp<64>
{
private:
    std::uint64_t state_ = 14695981039346656037u;

public:
    void
    append (void const* key, std::size_t len) noexcept
    {
        unsigned char const* p = static_cast<unsigned char const*>(key);
        unsigned char const* const e = p + len;
        for (; p < e; ++p)
            state_ = (state_ ^ *p) * 1099511628211u;
    }

    explicit
    operator std::size_t() noexcept
    {
        return state_;
    }
};

template <>
class fnv1a_imp<32>
{
private:
    std::uint32_t state_ = 2166136261;

public:
    void
    append (void const* key, std::size_t len) noexcept
    {
        unsigned char const* p = static_cast<unsigned char const*>(key);
        unsigned char const* const e = p + len;
        for (; p < e; ++p)
            state_ = (state_ ^ *p) * 16777619;
    }

    explicit
    operator std::size_t() noexcept
    {
        return state_;
    }
};

class fnv1a
    : public fnv1a_imp<char_bit*sizeof(std::size_t)>
{
public:
};

class jenkins1
{
private:
    std::size_t state_ = 0;

public:
    void
    append (void const* key, std::size_t len) noexcept
    {
        unsigned char const* p = static_cast <unsigned char const*>(key);
        unsigned char const* const e = p + len;
        for (; p < e; ++p)
        {
            state_ += *p;
            state_ += state_ << 10;
            state_ ^= state_ >> 6;
        }
    }

    explicit
    operator std::size_t() noexcept
    {
        state_ += state_ << 3;
        state_ ^= state_ >> 11;
        state_ += state_ << 15;
        return state_;
    }
};

class spooky
{
private:
    spookyhash state_;

public: 
    spooky(std::size_t seed1 = 1, std::size_t seed2 = 2) noexcept
    {
        state_.init(seed1, seed2);
    }

    void
    append(void const* key, std::size_t len) noexcept
    {
        state_.update(key, len);
    }

    explicit
    operator std::size_t() noexcept
    {
        std::uint64_t h1, h2;
        state_.final(&h1, &h2);
        return h1;
    }

};

template <
    class prng = std::conditional_t <
        sizeof(std::size_t)==sizeof(std::uint64_t),
        std::mt19937_64,
        std::mt19937
    >
>
class prng_hasher
    : public block_stream <std::size_t, prng_hasher <prng>>
{
private:
    std::size_t m_seed;
    prng m_prng;

    typedef block_stream <std::size_t, prng_hasher <prng>> base;
    friend base;

    // compress
    void
    process_block (std::size_t block)
    {
        m_prng.seed (m_seed + block);
        m_seed = m_prng();
    }

public:
    prng_hasher (std::size_t seed = 0)
        : m_seed (seed)
    {
    }

    void
    append (void const* data, std::size_t bytes) noexcept
    {
        base::operator() (data, bytes);
    }

    explicit
    operator std::size_t() noexcept
    {
        base::finish();
        return m_seed;
    }
};

class slowkey
{
private:
    std::tuple <short, unsigned char, unsigned char> date_;
    std::vector <std::pair <int, int>> data_;

public:
    slowkey()
    {
        static std::mt19937_64 eng;
        std::uniform_int_distribution<short> yeardata(1900, 2014);
        std::uniform_int_distribution<unsigned> monthdata(1, 12);
        std::uniform_int_distribution<unsigned> daydata(1, 28);
        std::uniform_int_distribution<std::size_t> veclen(0, 100);
        std::uniform_int_distribution<int> int1data(1, 10);
        std::uniform_int_distribution<int> int2data(-3, 5000);
        std::get<0>(date_) = yeardata(eng);
        std::get<1>(date_) = (unsigned char)monthdata(eng);
        std::get<2>(date_) = (unsigned char)daydata(eng);
        data_.resize(veclen(eng));
        for (auto& p : data_)
        {
            p.first = int1data(eng);
            p.second = int2data(eng);
        }
    }

    // hook into the system like this
    template <class hasher>
    friend
    void
    hash_append (hasher& h, slowkey const& x) noexcept
    {
        using beast::hash_append;
        hash_append (h, x.date_, x.data_);
    }

    friend
    bool operator< (slowkey const& x, slowkey const& y) noexcept
    {
        return std::tie(x.date_, x.data_) < std::tie(y.date_, y.data_);
    }

    // hook into the std::system like this
    friend struct std::hash<slowkey>;
    friend struct x_fnv1a;
};

struct fastkey
{
private:
    std::array <std::size_t, 4> m_values;

public:
    fastkey()
    {
        static std::conditional_t <sizeof(std::size_t)==sizeof(std::uint64_t),
            std::mt19937_64, std::mt19937> eng;
        for (auto& v : m_values)
            v = eng();
    }

    friend
    bool
    operator< (fastkey const& x, fastkey const& y) noexcept
    {
        return x.m_values < y.m_values;
    }
};

} // hash_append_tests

//------------------------------------------------------------------------------

template<>
struct is_contiguously_hashable <hash_append_tests::fastkey>
    : std::true_type
{
};

//------------------------------------------------------------------------------

class hash_append_test : public unit_test::suite
{
public:
    typedef hash_append_tests::slowkey slowkey;
    typedef hash_append_tests::fastkey fastkey;

    struct results_t
    {
        results_t()
            : collision_factor (0)
            , distribution_factor (0)
            , elapsed (0)
        {
        }

        float collision_factor;
        float distribution_factor;
        float windowed_score;
        std::chrono::milliseconds elapsed;
    };

    // generate a set of keys
    template <class key>
    std::set <key>
    make_keys (std::size_t count)
    {
        std::set <key> keys;
        while (count--)
            keys.emplace();
        return keys;
    }

    // generate a set of hashes from a container
    template <class hasher, class keys>
    std::vector <std::size_t>
    make_hashes (keys const& keys)
    {
        std::vector <std::size_t> hashes;
        hashes.reserve (keys.size());
        for (auto const& key : keys)
        {
            hasher h;
            hash_append (h, key);
            hashes.push_back (static_cast <std::size_t> (h));
        }
        return hashes;
    }

    template <class hasher, class hashes>
    void
    measure_hashes (results_t& results, hashes const& hashes)
    {
        results.collision_factor =
            hash_metrics::collision_factor (
                hashes.begin(), hashes.end());

        results.distribution_factor =
            hash_metrics::distribution_factor (
                hashes.begin(), hashes.end());

        results.windowed_score =
            hash_metrics::windowed_score (
                hashes.begin(), hashes.end());
    }

    template <class hasher, class keys>
    void
    measure_keys (results_t& results, keys const& keys)
    {
        auto const start (
            std::chrono::high_resolution_clock::now());
        
        auto const hashes (make_hashes <hasher> (keys));
        
        results.elapsed = std::chrono::duration_cast <std::chrono::milliseconds> (
            std::chrono::high_resolution_clock::now() - start);

        measure_hashes <hasher> (results, hashes);
    }

    template <class hasher, class key>
    void
    test_hasher (std::string const& name, std::size_t n)
    {
        results_t results;
        auto const keys (make_keys <key> (n));
        measure_keys <hasher> (results, keys);
        report (name, results);
    }

    void
    report (std::string const& name, results_t const& results)
    {
        log <<
            std::left <<
            std::setw (39) << name << " | " <<
            std::right <<
            std::setw (13) << std::setprecision (5) <<
                results.collision_factor << " | " <<
            std::setw (13) << std::setprecision (5) <<
                results.distribution_factor << " | " <<
            std::setw (13) << std::setprecision (5) <<
                results.windowed_score << " | " <<
            std::left <<
            results.elapsed.count();
        pass ();
    }

    void
    run()
    {
        log <<
            "name                                    |     collision |  distribution |   windowed    | time (milliseconds)" << std::endl <<
            "----------------------------------------+---------------+---------------+---------------+--------------------";

        //test_hasher <hash_append_tests::prng_hasher<>, slowkey> ("prng_hasher <slowkey>", 10000);
        //test_hasher <hash_append_tests::prng_hasher<>, fastkey> ("prng_hasher <fastkey>", 100000);

        test_hasher <hash_append_tests::jenkins1, slowkey>      ("jenkins1 <slowkey>",  1000000);
        test_hasher <hash_append_tests::spooky, slowkey>        ("spooky <slowkey>",    1000000);
        test_hasher <hash_append_tests::fnv1a, slowkey>         ("fnv1a <slowkey>",     1000000);

        test_hasher <hash_append_tests::jenkins1, fastkey>      ("jenkins1 <fastkey>",  1000000);
        test_hasher <hash_append_tests::spooky, fastkey>        ("spooky <fastkey>",    1000000);
        test_hasher <hash_append_tests::fnv1a, fastkey>         ("fnv1a <fastkey>",     1000000);
    }
};

beast_define_testsuite_manual(hash_append,container,beast);

}
