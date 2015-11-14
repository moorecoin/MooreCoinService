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

#ifndef ripple_core_systemparameters_h_included
#define ripple_core_systemparameters_h_included

#include <cstdint>
#include <string>

namespace ripple {

// various protocol and system specific constant globals.

/* the name of the system. */
static inline
std::string const&
systemname ()
{
    static std::string const name = "moorecoin";
    return name;
}

static
std::uint64_t const
system_currency_gift = 2;

static
std::uint64_t const
system_currency_users = 1000000;

static
std::uint64_t const
system_currency_parts = 1000000;      // 10^system_currency_precision

static
std::uint64_t const
system_currency_start = system_currency_gift*system_currency_users*system_currency_parts;

static
std::uint64_t const
system_currency_gift_vbc = 10;

static
std::uint64_t const
system_currency_users_vbc = 1000000;

static
std::uint64_t const
system_currency_parts_vbc = 1000000;      // 10^system_currency_precision

static
std::uint64_t const
min_vspd_to_get_fee_share = 10000000000;
    

/** calculate the amount of native currency created at genesis. */
static
std::uint64_t const
system_currency_start_vbc = system_currency_gift_vbc*system_currency_users_vbc*system_currency_parts_vbc;

static
std::uint64_t const
vbc_dividend_min = 1000;

static
std::uint64_t const
vbc_dividend_period_1 = 473904000ull; // 2015-1-7

static
std::uint64_t const
vbc_dividend_period_2 = 568598400ull; // 2018-1-7

static
std::uint64_t const
vbc_dividend_period_3 = 663292800ull; // 2021-1-7

static
std::uint64_t const
vbc_increase_rate_1 = 3;

static
std::uint64_t const
vbc_increase_rate_1_parts= 1000;

static
std::uint64_t const
vbc_increase_rate_2 = 15;

static
std::uint64_t const
vbc_increase_rate_2_parts = 10000;

static
std::uint64_t const
vbc_increase_rate_3 = 1;

static
std::uint64_t const
vbc_increase_rate_3_parts = 1000;

static
std::uint64_t const
vbc_increase_rate_4 = 3;

static
std::uint64_t const
vbc_increase_rate_4_parts = 10000;

static
std::uint64_t const
vbc_increase_max = 1000000000000000ull;

static
std::uint64_t const
vrp_increase_rate = 1;

static
std::uint64_t const
vrp_increase_rate_parts = 1000;

static
std::uint64_t const
vrp_increase_max = 10000000000000ull;

/* the currency code for the native currency. */
static inline
std::string const&
systemcurrencycode ()
{
    static std::string const code = "vrp";
    return code;
}
    
/* the currency code for the native currency. */
static inline
std::string const&
systemcurrencycodevbc ()
{
    static std::string const code = "vbc";
    return code;
}
} // ripple

#endif
