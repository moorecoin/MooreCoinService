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

#include <beastconfig.h>
#include <ripple/basics/hardened_hash.h>
#include <beast/unit_test/suite.h>
#include <beast/crypto/sha256.h>
#include <boost/functional/hash.hpp>
#include <array>
#include <cstdint>
#include <iomanip>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace ripple {
namespace detail {

template <class t>
class test_user_type_member
{
private:
    t t;

public:
    explicit test_user_type_member (t const& t_ = t())
        : t (t_)
    {
    }

    template <class hasher>
    friend void hash_append (hasher& h, test_user_type_member const& a) noexcept
    {
        using beast::hash_append;
        hash_append (h, a.t);
    }
};

template <class t>
class test_user_type_free
{
private:
    t t;

public:
    explicit test_user_type_free (t const& t_ = t())
        : t (t_)
    {
    }

    template <class hasher>
    friend void hash_append (hasher& h, test_user_type_free const& a) noexcept
    {
        using beast::hash_append;
        hash_append (h, a.t);
    }
};

} // detail
} // ripple

//------------------------------------------------------------------------------

namespace ripple {

namespace detail {

template <class t>
using test_hardened_unordered_set =
    std::unordered_set <t, hardened_hash <>>;

template <class t>
using test_hardened_unordered_map =
    std::unordered_map <t, int, hardened_hash <>>;

template <class t>
using test_hardened_unordered_multiset =
    std::unordered_multiset <t, hardened_hash <>>;

template <class t>
using test_hardened_unordered_multimap =
    std::unordered_multimap <t, int, hardened_hash <>>;

} // detail

template <std::size_t bits, class uint = std::uint64_t>
class unsigned_integer
{
private:
    static_assert (std::is_integral<uint>::value &&
        std::is_unsigned <uint>::value,
            "uint must be an unsigned integral type");

    static_assert (bits%(8*sizeof(uint))==0,
        "bits must be a multiple of 8*sizeof(uint)");

    static_assert (bits >= (8*sizeof(uint)),
        "bits must be at least 8*sizeof(uint)");

    static std::size_t const size = bits/(8*sizeof(uint));

    std::array <uint, size> m_vec;

public:
    typedef uint value_type;

    static std::size_t const bits = bits;
    static std::size_t const bytes = bits / 8;

    template <class int>
    static
    unsigned_integer
    from_number (int v)
    {
        unsigned_integer result;
        for (std::size_t i (1); i < size; ++i)
            result.m_vec [i] = 0;
        result.m_vec[0] = v;
        return result;
    }

    void*
    data() noexcept
    {
        return &m_vec[0];
    }

    void const*
    data() const noexcept
    {
        return &m_vec[0];
    }

    template <class hasher>
    friend void hash_append(hasher& h, unsigned_integer const& a) noexcept
    {
        using beast::hash_append;
        hash_append (h, a.m_vec);
    }

    friend
    std::ostream&
    operator<< (std::ostream& s, unsigned_integer const& v)
    {
        for (std::size_t i (0); i < size; ++i)
            s <<
                std::hex <<
                std::setfill ('0') <<
                std::setw (2*sizeof(uint)) <<
                v.m_vec[i]
                ;
        return s;
    }
};

typedef unsigned_integer <256, std::size_t> sha256_t;

static_assert (sha256_t::bits == 256,
    "sha256_t must have 256 bits");

} // ripple

//------------------------------------------------------------------------------

namespace ripple {

class hardened_hash_test
    : public beast::unit_test::suite
{
public:
    template <class t>
    void
    check ()
    {
        t t{};
        hardened_hash <>() (t);
        pass();
    }

    template <template <class t> class u>
    void
    check_user_type()
    {
        check <u <bool>> ();
        check <u <char>> ();
        check <u <signed char>> ();
        check <u <unsigned char>> ();
        // these cause trouble for boost
        //check <u <char16_t>> ();
        //check <u <char32_t>> ();
        check <u <wchar_t>> ();
        check <u <short>> ();
        check <u <unsigned short>> ();
        check <u <int>> ();
        check <u <unsigned int>> ();
        check <u <long>> ();
        check <u <long long>> ();
        check <u <unsigned long>> ();
        check <u <unsigned long long>> ();
        check <u <float>> ();
        check <u <double>> ();
        check <u <long double>> ();
    }

    template <template <class t> class c >
    void
    check_container()
    {
        {
            c <detail::test_user_type_member <std::string>> c;
        }

        pass();

        {
            c <detail::test_user_type_free <std::string>> c;
        }

        pass();
    }

    void
    test_user_types()
    {
        testcase ("user types");
        check_user_type <detail::test_user_type_member> ();
        check_user_type <detail::test_user_type_free> ();
    }

    void
    test_containers()
    {
        testcase ("containers");
        check_container <detail::test_hardened_unordered_set>();
        check_container <detail::test_hardened_unordered_map>();
        check_container <detail::test_hardened_unordered_multiset>();
        check_container <detail::test_hardened_unordered_multimap>();
    }

    void
    run ()
    {
        test_user_types();
        test_containers();
    }
};

class hardened_hash_sha256_test
    : public beast::unit_test::suite
{
public:
    void
    testsha256()
    {
        testcase ("sha256");

        log <<
            "sizeof(std::size_t) == " << sizeof(std::size_t);

        hardened_hash <> h;
        for (int i = 0; i < 100; ++i)
        {
            sha256_t v (sha256_t::from_number (i));
            beast::sha256::digest_type d;
            beast::sha256::hash (v.data(), sha256_t::bytes, d);
            sha256_t d_;
            memcpy (d_.data(), d.data(), sha256_t::bytes);
            std::size_t result (h (d_));
            log <<
                "i=" << std::setw(2) << i << " " <<
                "sha256=0x" << d_ << " " <<
                "hash=0x" <<
                    std::setfill ('0') <<
                    std::setw (2*sizeof(std::size_t)) << result
                ;
            pass();
        }
    }

    void
    run ()
    {
        testsha256();
    }
};

beast_define_testsuite(hardened_hash,basics,ripple);
beast_define_testsuite_manual(hardened_hash_sha256,basics,ripple);

} // ripple
