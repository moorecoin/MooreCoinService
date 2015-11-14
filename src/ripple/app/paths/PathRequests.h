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

#ifndef ripple_pathrequests_h
#define ripple_pathrequests_h

#include <ripple/app/paths/pathrequest.h>
#include <ripple/app/paths/ripplelinecache.h>
#include <ripple/core/job.h>
#include <atomic>

namespace ripple {

class pathrequests
{
public:
    pathrequests (beast::journal journal, beast::insight::collector::ptr const& collector)
        : mjournal (journal)
        , mlastidentifier (0)
    {
        mfast = collector->make_event ("pathfind_fast");
        mfull = collector->make_event ("pathfind_full");
    }

    void updateall (const std::shared_ptr<ledger>& ledger,
                    job::cancelcallback shouldcancel);

    ripplelinecache::pointer getlinecache (
        ledger::pointer& ledger, bool authoritative);

    json::value makepathrequest (
        std::shared_ptr <infosub> const& subscriber,
        const std::shared_ptr<ledger>& ledger,
        json::value const& request);

    void reportfast (int milliseconds)
    {
        mfast.notify (static_cast < beast::insight::event::value_type> (milliseconds));
    }

    void reportfull (int milliseconds)
    {
        mfull.notify (static_cast < beast::insight::event::value_type> (milliseconds));
    }

private:
    beast::journal                   mjournal;

    beast::insight::event            mfast;
    beast::insight::event            mfull;

    // track all requests
    std::vector<pathrequest::wptr>   mrequests;

    // use a ripplelinecache
    ripplelinecache::pointer         mlinecache;

    std::atomic<int>                 mlastidentifier;

    typedef ripplerecursivemutex     locktype;
    typedef std::lock_guard <locktype> scopedlocktype;
    locktype                         mlock;

};

} // ripple

#endif
