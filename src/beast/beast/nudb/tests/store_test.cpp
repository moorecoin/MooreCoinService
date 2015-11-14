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

#include <beastconfig.h>
#include <beast/nudb/tests/common.h>
#include <beast/module/core/diagnostic/unittestutilities.h>
#include <beast/module/core/files/file.h>
#include <beast/random/xor_shift_engine.h>
#include <beast/unit_test/suite.h>
#include <cmath>
#include <iomanip>
#include <memory>
#include <random>
#include <utility>

namespace beast {
namespace nudb {
namespace test {

// basic, single threaded test that verifies the
// correct operation of the store. load factor is
// set high to ensure that spill records are created,
// exercised, and split.
//
class store_test : public unit_test::suite
{
public:
    void
    do_test (std::size_t n,
        std::size_t block_size, float load_factor)
    {
        testcase (abort_on_fail);
        std::string const path =
            beast::unittestutilities::tempdirectory(
                "test_db").getfullpathname().tostdstring();
        auto const dp = path + ".dat";
        auto const kp = path + ".key";
        auto const lp = path + ".log";
        sequence seq;
        test_api::store db;
        try
        {
            expect (test_api::create (dp, kp, lp, appnum,
                salt, sizeof(key_type), block_size,
                    load_factor), "create");
            expect (db.open(dp, kp, lp,
                arena_alloc_size), "open");
            storage s;
            // insert
            for (std::size_t i = 0; i < n; ++i)
            {
                auto const v = seq[i];
                expect (db.insert(
                    &v.key, v.data, v.size), "insert 1");
            }
            // fetch
            for (std::size_t i = 0; i < n; ++i)
            {
                auto const v = seq[i];
                bool const found = db.fetch (&v.key, s);
                expect (found, "not found");
                expect (s.size() == v.size, "wrong size");
                expect (std::memcmp(s.get(),
                    v.data, v.size) == 0, "not equal");
            }
            // insert duplicates
            for (std::size_t i = 0; i < n; ++i)
            {
                auto const v = seq[i];
                expect (! db.insert(&v.key,
                    v.data, v.size), "insert duplicate");
            }
            // insert/fetch
            for (std::size_t i = 0; i < n; ++i)
            {
                auto v = seq[i];
                bool const found = db.fetch (&v.key, s);
                expect (found, "missing");
                expect (s.size() == v.size, "wrong size");
                expect (memcmp(s.get(),
                    v.data, v.size) == 0, "wrong data");
                v = seq[i + n];
                expect (db.insert(&v.key, v.data, v.size),
                    "insert 2");
            }
            db.close();
            //auto const stats = test_api::verify(dp, kp);
            auto const stats = verify<test_api::hash_type>(
                dp, kp, 1 * 1024 * 1024);
            expect (stats.hist[1] > 0, "no splits");
            print (log, stats);
        }
        catch (nudb::store_error const& e)
        {
            fail (e.what());
        }
        catch (std::exception const& e)
        {
            fail (e.what());
        }
        expect (test_api::file_type::erase(dp));
        expect (test_api::file_type::erase(kp));
        expect (! test_api::file_type::erase(lp));
    }

    void
    run() override
    {
        enum
        {
            n =             50000
            ,block_size =   256
        };

        float const load_factor = 0.95f;

        do_test (n, block_size, load_factor);
    }
};

beast_define_testsuite(store,nudb,beast);

} // test
} // nudb
} // beast

