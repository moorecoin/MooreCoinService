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
#include <ripple/net/infosub.h>
#include <atomic>

namespace ripple {

// this is the primary interface into the "client" portion of the program.
// code that wants to do normal operations on the network such as
// creating and monitoring accounts, creating transactions, and so on
// should use this interface. the rpc code will primarily be a light wrapper
// over this code.

// eventually, it will check the node's operating mode (synched, unsynched,
// etectera) and defer to the correct means of processing. the current
// code assumes this node is synched (and will continue to do so until
// there's a functional network.

//------------------------------------------------------------------------------

infosub::source::source (char const* name, stoppable& parent)
    : stoppable (name, parent)
{
}

//------------------------------------------------------------------------------

infosub::infosub (source& source, consumer consumer)
    : m_consumer (consumer)
    , m_source (source)
{
    static std::atomic <int> s_seq_id (0);
    mseq = ++s_seq_id;
}

infosub::~infosub ()
{
    m_source.unsubtransactions (mseq);
    m_source.unsubrttransactions (mseq);
    m_source.unsubledger (mseq);
    m_source.unsubserver (mseq);
    m_source.unsubaccount (mseq, msubaccountinfo, true);
    m_source.unsubaccount (mseq, msubaccountinfo, false);
}

resource::consumer& infosub::getconsumer()
{
    return m_consumer;
}

void infosub::send (
    json::value const& jvobj, std::string const& sobj, bool broadcast)
{
    send (jvobj, broadcast);
}

std::uint64_t infosub::getseq ()
{
    return mseq;
}

void infosub::onsendempty ()
{
}

void infosub::insertsubaccountinfo (
    rippleaddress addr, std::uint32_t uledgerindex)
{
    scopedlocktype sl (mlock);

    msubaccountinfo.insert (addr);
}

void infosub::clearpathrequest ()
{
    mpathrequest.reset ();
}

void infosub::setpathrequest (const std::shared_ptr<pathrequest>& req)
{
    mpathrequest = req;
}

const std::shared_ptr<pathrequest>& infosub::getpathrequest ()
{
    return mpathrequest;
}

} // ripple
