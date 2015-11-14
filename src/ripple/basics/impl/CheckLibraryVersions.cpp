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
#include <ripple/basics/impl/checklibraryversionsimpl.h>
#include <beast/unit_test/suite.h>
#include <beast/module/core/diagnostic/semanticversion.h>
#include <boost/version.hpp>
#include <openssl/opensslv.h>
#include <sstream>
#include <vector>

namespace ripple {
namespace version {

typedef unsigned long long versionnumber;

const char boostminimal[] = "1.55.0";
const char opensslminimal[] = "1.0.1-g";

std::string boostversion(versionnumber boostversion)
{
    std::stringstream ss;
    ss << (boostversion / 100000) << "."
       << (boostversion / 100 % 1000) << "."
       << (boostversion % 100);
    return ss.str();
}

std::string opensslversion(versionnumber opensslversion)
{
    std::stringstream ss;
    ss << (opensslversion / 0x10000000l) << "."
       << (opensslversion / 0x100000 % 0x100) << "."
       << (opensslversion / 0x1000 % 0x100);
    auto patchno = opensslversion % 0x10;
    if (patchno)
        ss << '-' << char('a' + patchno - 1);
    return ss.str();
}

void checkversion(std::string name, std::string required, std::string actual)
{
    beast::semanticversion r, a;
    if (!r.parse(required)) {
        throw std::runtime_error("didn't understand required version of " +
                                 name + ": " + required);
    }
    if (!a.parse(actual)) {
        throw std::runtime_error("didn't understand actual version of " +
                                 name + ": " + required);
    }

    if (a < r) {
        throw std::runtime_error("your " + name + " library is out of date.\n" +
                                 "your version: " + actual + "\n" +
                                 "required version: " +  "\n");
    }
}

void checkboost(std::string version)
{
    checkversion("boost", boostminimal, version);
}

void checkopenssl(std::string version)
{
    checkversion("openssl", opensslminimal, version);
}

void checklibraryversions()
{
    checkboost();
    checkopenssl();
}

}  // namespace version
}  // namespace ripple
