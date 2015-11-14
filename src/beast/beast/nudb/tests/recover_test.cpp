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

class basic_recover_test : public unit_test::suite
{
public:
    // creates and opens a database, performs a bunch
    // of inserts, then fetches all of them to make sure
    // they are there. uses a fail_file that causes the n-th
    // i/o to fail, causing an exception.
    void
    do_work (std::size_t count, float load_factor,
        nudb::path_type const& path, fail_counter& c)
    {
        auto const dp = path + ".dat";
        auto const kp = path + ".key";
        auto const lp = path + ".log";
        test_api::file_type::erase (dp);
        test_api::file_type::erase (kp);
        test_api::file_type::erase (lp);
        expect(test_api::create (
            dp, kp, lp, appnum, salt, sizeof(key_type),
                block_size(path), load_factor), "create");
        test_api::fail_store db;
        if (! expect(db.open(dp, kp, lp,
            arena_alloc_size, c), "open"))
        {
            // vfalco open should never fail here, we need
            //        to report this and terminate the test.
        }
        expect (db.appnum() == appnum, "appnum");
        sequence seq;
        for (std::size_t i = 0; i < count; ++i)
        {
            auto const v = seq[i];
            expect(db.insert(&v.key, v.data, v.size),
                "insert");
        }
        storage s;
        for (std::size_t i = 0; i < count; ++i)
        {
            auto const v = seq[i];
            if (! expect(db.fetch (&v.key, s),
                    "fetch"))
                break;
            if (! expect(s.size() == v.size, "size"))
                break;
            if (! expect(std::memcmp(s.get(),
                    v.data, v.size) == 0, "data"))
                break;
        }
        db.close();
        verify_info info;
        try
        {
            info = test_api::verify(dp, kp);
        }
        catch(...)
        {
            print(log, info);
            throw;
        }
        test_api::file_type::erase (dp);
        test_api::file_type::erase (kp);
        test_api::file_type::erase (lp);
    }

    void
    do_recover (path_type const& path,
        fail_counter& c)
    {
        auto const dp = path + ".dat";
        auto const kp = path + ".key";
        auto const lp = path + ".log";
        recover<test_api::hash_type,
            test_api::codec_type, fail_file<
                test_api::file_type>>(dp, kp, lp,
                    test_api::buffer_size, c);
        test_api::verify(dp, kp);
        test_api::file_type::erase (dp);
        test_api::file_type::erase (kp);
        test_api::file_type::erase (lp);
    }

    void
    test_recover (float load_factor, std::size_t count)
    {
        testcase << count << " inserts";
        path_type const path =
            beast::unittestutilities::tempdirectory(
                "nudb").getfullpathname().tostdstring();
        for (std::size_t n = 1;;++n)
        {
            try
            {
                fail_counter c(n);
                do_work (count, load_factor, path, c);
                break;
            }
            catch (nudb::fail_error const&)
            {
            }
            for (std::size_t m = 1;;++m)
            {
                fail_counter c(m);
                try
                {
                    do_recover (path, c);
                    break;
                }
                catch (nudb::fail_error const&)
                {
                }
            }
        }
    }
};

class recover_test : public basic_recover_test
{
public:
    void
    run() override
    {
        float lf = 0.55f;
        test_recover (lf, 0);
        test_recover (lf, 10);
        test_recover (lf, 100);
    }
};

beast_define_testsuite(recover,nudb,beast);

class recover_big_test : public basic_recover_test
{
public:
    void
    run() override
    {
        float lf = 0.90f;
        test_recover (lf, 1000);
        test_recover (lf, 10000);
    }
};

beast_define_testsuite_manual(recover_big,nudb,beast);

} // test
} // nudb
} // beast

