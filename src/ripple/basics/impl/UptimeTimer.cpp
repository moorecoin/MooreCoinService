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
#include <ripple/basics/uptimetimer.h>
#include <beast/threads/thread.h>

#include <atomic>

namespace ripple {

uptimetimer::uptimetimer ()
    : m_elapsedtime (0)
    , m_starttime (::time (0))
    , m_isupdatingmanually (false)
{
}

uptimetimer::~uptimetimer ()
{
}

int uptimetimer::getelapsedseconds () const
{
    int result;

    if (m_isupdatingmanually)
    {
        std::atomic_thread_fence (std::memory_order_seq_cst);
        result = m_elapsedtime;
    }
    else
    {
        // vfalco todo use time_t instead of int return
        result = static_cast <int> (::time (0) - m_starttime);
    }

    return result;
}

void uptimetimer::beginmanualupdates ()
{
    //assert (!m_isupdatingmanually);

    m_isupdatingmanually = true;
}

void uptimetimer::endmanualupdates ()
{
    //assert (m_isupdatingmanually);

    m_isupdatingmanually = false;
}

void uptimetimer::incrementelapsedtime ()
{
    //assert (m_isupdatingmanually);
    ++m_elapsedtime;
}

uptimetimer& uptimetimer::getinstance ()
{
    static uptimetimer instance;

    return instance;
}

} // ripple
