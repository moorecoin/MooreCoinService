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

#ifndef ripple_basics_checklibraryversions_impl_h_included
#define ripple_basics_checklibraryversions_impl_h_included

#include <ripple/basics/checklibraryversions.h>
#include <boost/version.hpp>
#include <openssl/opensslv.h>

namespace ripple {
namespace version {

/** both boost and openssl have integral version numbers. */
typedef unsigned long long versionnumber;

/** minimal required boost version. */
extern const char boostminimal[];

/** minimal required openssl version. */
extern const char opensslminimal[];

std::string boostversion(versionnumber boostversion = boost_version);
std::string opensslversion(
    versionnumber opensslversion = openssl_version_number);

void checkversion(std::string name, std::string required, std::string actual);
void checkboost(std::string version = boostversion());
void checkopenssl(std::string version = opensslversion());

}  // namespace version
}  // namespace ripple

#endif
