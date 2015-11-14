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

#ifndef ripple_overlay_tmhello_h_included
#define ripple_overlay_tmhello_h_included

#include <ripple/app/main/application.h>
#include <ripple/protocol/buildinfo.h>
#include <ripple/protocol/rippleaddress.h>
#include <ripple/protocol/uinttypes.h>
#include <beast/http/message.h>
#include <beast/utility/journal.h>
#include <utility>

#include <boost/asio/ssl.hpp>

#include "ripple.pb.h"

namespace ripple {

enum
{
    // the clock drift we allow a remote peer to have
    clocktolerancedeltaseconds = 20
};

/** computes a shared value based on the ssl connection state.
    when there is no man in the middle, both sides will compute the same
    value. in the presence of an attacker, the computed values will be
    different.
    if the shared value generation fails, the link must be dropped.
    @return a pair. second will be false if shared value generation failed.
*/
std::pair<uint256, bool>
makesharedvalue (ssl* ssl, beast::journal journal);

/** build a tmhello protocol message. */
protocol::tmhello
buildhello (uint256 const& sharedvalue, application& app);

/** insert http headers based on the tmhello protocol message. */
void
appendhello (beast::http::message& m, protocol::tmhello const& hello);

/** parse http headers into tmhello protocol message.
    @return a pair. second will be false if the parsing failed.
*/
std::pair<protocol::tmhello, bool>
parsehello (beast::http::message const& m, beast::journal journal);

/** validate and store the public key in the tmhello.
    this includes signature verification on the shared value.
    @return a pair. second will be false if the check failed.
*/
std::pair<rippleaddress, bool>
verifyhello (protocol::tmhello const& h, uint256 const& sharedvalue,
    beast::journal journal, application& app);

/** parse a set of protocol versions.
    the returned list contains no duplicates and is sorted ascending.
    any strings that are not parseable as rtxp protocol strings are
    excluded from the result set.
*/
std::vector<protocolversion>
parse_protocolversions (std::string const& s);

}

#endif
