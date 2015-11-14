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
#include <ripple/basics/checklibraryversions.h>
#include <ripple/basics/impl/checklibraryversionsimpl.h>
#include <beast/unit_test/suite.h>

namespace ripple {
namespace version {

struct checklibraryversions_test : beast::unit_test::suite
{
    void print_message()
    {
        log << "ssl minimal: " << opensslminimal << "\n"
            << "ssl actual:  " << opensslversion() << "\n"
            << "boost minimal: " << boostminimal << "\n"
            << "boost actual:  " << boostversion() << "\n"
            << std::flush;
    }

    void test_bad_ssl()
    {
        std::string error;
        try {
            checkopenssl(opensslversion(0x0090819fl));
        } catch (std::runtime_error& e) {
            error = e.what();
        }
        auto expectederror = "your openssl library is out of date.\n"
          "your version: 0.9.8-o\n"
          "required version: ";
        unexpected(error.find(expectederror) != 0, error);
    }

    void test_bad_boost()
    {
        std::string error;
        try {
            checkboost(boostversion(105400));
        } catch (std::runtime_error& e) {
            error = e.what();
        }
        auto expectederror = "your boost library is out of date.\n"
          "your version: 1.54.0\n"
          "required version: ";
        unexpected(error.find(expectederror) != 0, error);
    }


    void run()
    {
        print_message();
        checklibraryversions();

        test_bad_ssl();
        test_bad_boost();
    }
};

beast_define_testsuite(checklibraryversions, ripple_basics, ripple);

}  // namespace version
}  // namespace ripple
