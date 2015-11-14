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

#include <beast/nudb/tests/common.h>
#include <beast/module/core/diagnostic/unittestutilities.h>
#include <beast/module/core/files/file.h>
#include <beast/random/xor_shift_engine.h>
#include <beast/unit_test/suite.h>
#include <cmath>
#include <cstring>
#include <memory>
#include <random>
#include <utility>

namespace beast {
namespace nudb {
namespace test {

// this test is designed for callgrind runs to find hotspots
class callgrind_test : public unit_test::suite
{
public:
    // creates and opens a database, performs a bunch
    // of inserts, then alternates fetching all the keys
    // with keys not present.
    void
    do_test (std::size_t count,
        path_type const& path)
    {
        auto const dp = path + ".dat";
        auto const kp = path + ".key";
        auto const lp = path + ".log";
        test_api::create (dp, kp, lp,
            appnum,
            salt,
            sizeof(nudb::test::key_type),
            nudb::block_size(path),
            0.50);
        test_api::store db;
        if (! expect (db.open(dp, kp, lp,
                arena_alloc_size), "open"))
            return;
        expect (db.appnum() == appnum, "appnum");
        sequence seq;
        for (std::size_t i = 0; i < count; ++i)
        {
            auto const v = seq[i];
            expect (db.insert(&v.key, v.data, v.size),
                "insert");
        }
        storage s;
        for (std::size_t i = 0; i < count * 2; ++i)
        {
            if (! (i%2))
            {
                auto const v = seq[i/2];
                expect (db.fetch (&v.key, s), "fetch");
                expect (s.size() == v.size, "size");
                expect (std::memcmp(s.get(),
                    v.data, v.size) == 0, "data");
            }
            else
            {
                auto const v = seq[count + i/2];
                expect (! db.fetch (&v.key, s),
                    "fetch missing");
            }
        }
        db.close();
        nudb::native_file::erase (dp);
        nudb::native_file::erase (kp);
        nudb::native_file::erase (lp);
    }

    void
    run() override
    {
        enum
        {
            // higher numbers, more pain
            n = 100000
        };

        testcase (abort_on_fail);
        path_type const path =
            beast::unittestutilities::tempdirectory(
                "nudb").getfullpathname().tostdstring();
        do_test (n, path);
    }
};

beast_define_testsuite_manual(callgrind,nudb,beast);

} // test
} // nudb
} // beast

