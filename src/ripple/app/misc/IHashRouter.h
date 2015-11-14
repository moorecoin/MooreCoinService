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

#ifndef ripple_hashrouter_h_included
#define ripple_hashrouter_h_included

#include <ripple/basics/base_uint.h>
#include <cstdint>
#include <set>

namespace ripple {

// vfalco note are these the flags?? why aren't we using a packed struct?
// vfalco todo convert these macros to int constants
#define sf_relayed      0x01    // has already been relayed to other nodes
// vfalco note how can both bad and good be set on a hash?
#define sf_bad          0x02    // signature/format is bad
#define sf_siggood      0x04    // signature is good
#define sf_saved        0x08
#define sf_retry        0x10    // transaction can be retried
#define sf_trusted      0x20    // comes from trusted source

/** routing table for objects identified by hash.

    this table keeps track of which hashes have been received by which peers.
    it is used to manage the routing and broadcasting of messages in the peer
    to peer overlay.
*/
class ihashrouter
{
public:
    // the type here *must* match the type of peer::id_t
    typedef std::uint32_t peershortid;

    // vfalco note this preferred alternative to default parameters makes
    //         behavior clear.
    //
    static inline int getdefaultholdtime ()
    {
        return 300;
    }

    // vfalco todo rename the parameter to entryholdtimeinseconds
    static ihashrouter* new (int holdtime);

    virtual ~ihashrouter () { }

    // vfalco todo replace "supression" terminology with something more semantically meaningful.
    virtual bool addsuppression (uint256 const& index) = 0;

    virtual bool addsuppressionpeer (uint256 const& index, peershortid peer) = 0;

    virtual bool addsuppressionpeer (uint256 const& index, peershortid peer, int& flags) = 0;

    virtual bool addsuppressionflags (uint256 const& index, int flag) = 0;

    /** set the flags on a hash.

        @return `true` if the flags were changed.
    */
    // vfalco todo rename to setflags since it works with multiple flags.
    virtual bool setflag (uint256 const& index, int mask) = 0;

    virtual int getflags (uint256 const& index) = 0;

    virtual bool swapset (uint256 const& index, std::set<peershortid>& peers, int flag) = 0;

    // vfalco todo this appears to be unused!
    //
//    virtual entry getentry (uint256 const&) = 0;
};

} // ripple

#endif
