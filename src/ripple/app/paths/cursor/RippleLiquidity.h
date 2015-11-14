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

#ifndef ripple_paths_calculators_h
#define ripple_paths_calculators_h

#include <boost/log/trivial.hpp>

#include <ripple/app/paths/cursor/pathcursor.h>
#include <ripple/app/paths/ripplecalc.h>
#include <ripple/app/paths/tuning.h>

namespace ripple {
namespace path {

void rippleliquidity (
    ripplecalc&,
    const std::uint32_t uqualityin,
    const std::uint32_t uqualityout,
    stamount const& saprvreq,
    stamount const& sacurreq,
    stamount& saprvact,
    stamount& sacuract,
    std::uint64_t& uratemax);

std::uint32_t
quality_in (
    ledgerentryset& ledger,
    account const& utoaccountid,
    account const& ufromaccountid,
    currency const& currency);

std::uint32_t
quality_out (
    ledgerentryset& ledger,
    account const& utoaccountid,
    account const& ufromaccountid,
    currency const& currency);

} // path
} // ripple

#endif
