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

#ifndef ripple_inboundledgers_h
#define ripple_inboundledgers_h

#include <ripple/app/ledger/inboundledger.h>
#include <ripple/protocol/rippleledgerhash.h>
#include <beast/threads/stoppable.h>
#include <beast/cxx14/memory.h> // <memory>

namespace ripple {

/** manages the lifetime of inbound ledgers.

    @see inboundledger
*/
class inboundledgers
{
public:
    typedef beast::abstract_clock <std::chrono::steady_clock> clock_type;

    virtual ~inboundledgers() = 0;

    // vfalco todo should this be called findoradd ?
    //
    virtual inboundledger::pointer findcreate (uint256 const& hash,
        std::uint32_t seq, inboundledger::fcreason) = 0;

    virtual inboundledger::pointer find (ledgerhash const& hash) = 0;

    virtual bool hasledger (ledgerhash const& ledgerhash) = 0;

    virtual void dropledger (ledgerhash const& ledgerhash) = 0;

    // vfalco todo why is hash passed by value?
    // vfalco todo remove the dependency on the peer object.
    //
    virtual bool gotledgerdata (ledgerhash const& ledgerhash,
        std::shared_ptr<peer>,
        std::shared_ptr <protocol::tmledgerdata>) = 0;

    virtual void doledgerdata (job&, ledgerhash hash) = 0;

    virtual void gotstaledata (
        std::shared_ptr <protocol::tmledgerdata> packet) = 0;

    virtual int getfetchcount (int& timeoutcount) = 0;

    virtual void logfailure (uint256 const& h) = 0;

    virtual bool isfailure (uint256 const& h) = 0;

    virtual void clearfailures() = 0;

    virtual json::value getinfo() = 0;

    virtual void gotfetchpack (job&) = 0;
    virtual void sweep () = 0;

    virtual void onstop() = 0;
};

std::unique_ptr<inboundledgers>
make_inboundledgers (inboundledgers::clock_type& clock, beast::stoppable& parent,
                     beast::insight::collector::ptr const& collector);


} // ripple

#endif
