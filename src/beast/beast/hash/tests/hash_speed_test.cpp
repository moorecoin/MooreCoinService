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
#include <beast/hash/fnv1a.h>
#include <beast/hash/siphash.h>
#include <beast/hash/xxhasher.h>
#include <beast/chrono/chrono_io.h>
#include <beast/random/rngfill.h>
#include <beast/random/xor_shift_engine.h>
#include <beast/unit_test/suite.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <random>

namespace beast {

class hash_speed_test : public beast::unit_test::suite
{
public:
    using clock_type =
        std::chrono::high_resolution_clock;
    template <class hasher, std::size_t keysize>
    void
    test (std::string const& what, std::size_t n)
    {
        using namespace std;
        using namespace std::chrono;
        xor_shift_engine g(1);
        array<std::uint8_t, keysize> key;
        auto const start = clock_type::now();
        while(n--)
        {
            rngfill (key, g);
            hasher h;
            h.append(key.data(), keysize);
            volatile size_t temp =
                static_cast<std::size_t>(h);
            (void)temp;
        }
        auto const elapsed = clock_type::now() - start;
        log << setw(12) << what << " " <<
            duration<double>(elapsed) << "s";
    }

    void
    run()
    {
        enum
        {
            n = 100000000
        };

    #if ! beast_no_xxhash
        test<xxhasher,32>   ("xxhash", n);
    #endif
        test<fnv1a,32>      ("fnv1a", n);
        test<siphash,32>    ("siphash", n);
        pass();
    }
};

beast_define_testsuite_manual(hash_speed,container,beast);

} // beast
