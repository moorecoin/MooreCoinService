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

#ifndef ripple_protocol_buildinfo_h_included
#define ripple_protocol_buildinfo_h_included

#include <cstdint>
#include <string>

namespace ripple {

/** describes a ripple/rtxp protocol version. */
using protocolversion = std::pair<std::uint16_t, std::uint16_t>;

/** versioning information for this build. */
// vfalco the namespace is deprecated
namespace buildinfo {

/** server version.
    follows the semantic versioning specification:
    http://semver.org/
*/
std::string const&
getversionstring();

/** full server version string.
    this includes the name of the server. it is used in the peer
    protocol hello message and also the headers of some http replies.
*/
std::string const&
getfullversionstring();

/** construct a protocol version from a packed 32-bit protocol identifier */
protocolversion
make_protocol (std::uint32_t version);

/** the protocol version we speak and prefer. */
protocolversion const&
getcurrentprotocol();

/** the oldest protocol version we will accept. */
protocolversion const& getminimumprotocol ();

char const*
getrawversionstring();

} // buildinfo (deprecated)

std::string
to_string (protocolversion const& p);

std::uint32_t
to_packed (protocolversion const& p);

} // ripple

#endif
