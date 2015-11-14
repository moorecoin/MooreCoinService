//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012-2014 ripple labs inc.

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
#include <ripple/rpc/impl/tuning.h>
#include <ripple/rpc/impl/legacypathfind.h>

namespace ripple {
namespace rpc {

legacypathfind::legacypathfind (bool isadmin) : m_isok (false)
{
    if (isadmin)
    {
        ++inprogress;
        m_isok = true;
        return;
    }

    auto& app = getapp();
    auto const& jobcount = app.getjobqueue ().getjobcountge (jtclient);
    if (jobcount > tuning::maxpathfindjobcount || app.getfeetrack().isloadedlocal ())
        return;

    while (true)
    {
        int prevval = inprogress.load();
        if (prevval >= tuning::maxpathfindsinprogress)
            return;

        if (inprogress.compare_exchange_strong (
                prevval,
                prevval + 1,
                std::memory_order_release,
                std::memory_order_relaxed))
        {
            m_isok = true;
            return;
        }
    }
}

legacypathfind::~legacypathfind ()
{
    if (m_isok)
        --inprogress;
}

std::atomic <int> legacypathfind::inprogress (0);

} // rpc
} // ripple
