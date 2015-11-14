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
#include <ripple/app/paths/pathrequests.h>
#include <ripple/app/ledger/ledgermaster.h>
#include <ripple/app/main/application.h>
#include <ripple/core/jobqueue.h>
#include <ripple/resource/fees.h>
#include <boost/foreach.hpp>

namespace ripple {

/** get the current ripplelinecache, updating it if necessary.
    get the correct ledger to use.
*/
ripplelinecache::pointer pathrequests::getlinecache (ledger::pointer& ledger, bool authoritative)
{
    scopedlocktype sl (mlock);

    std::uint32_t lineseq = mlinecache ? mlinecache->getledger()->getledgerseq() : 0;
    std::uint32_t lgrseq = ledger->getledgerseq();

    if ( (lineseq == 0) ||                                 // no ledger
         (authoritative && (lgrseq > lineseq)) ||          // newer authoritative ledger
         (authoritative && ((lgrseq + 8)  < lineseq)) ||   // we jumped way back for some reason
         (lgrseq > (lineseq + 8)))                         // we jumped way forward for some reason
    {
        ledger = std::make_shared<ledger>(*ledger, false); // take a snapshot of the ledger
        mlinecache = std::make_shared<ripplelinecache> (ledger);
    }
    else
    {
        ledger = mlinecache->getledger();
    }
    return mlinecache;
}

void pathrequests::updateall (ledger::ref inledger,
                              job::cancelcallback shouldcancel)
{
    std::vector<pathrequest::wptr> requests;

    loadevent::autoptr event (getapp().getjobqueue().getloadeventap(jtpath_find, "pathrequest::updateall"));

    // get the ledger and cache we should be using
    ledger::pointer ledger = inledger;
    ripplelinecache::pointer cache;
    {
        scopedlocktype sl (mlock);
        requests = mrequests;
        cache = getlinecache (ledger, true);
    }

    bool newrequests = getapp().getledgermaster().isnewpathrequest();
    bool mustbreak = false;

    mjournal.trace << "updateall seq=" << ledger->getledgerseq() << ", " <<
        requests.size() << " requests";
    int processed = 0, removed = 0;

    do
    {
        boost_foreach (pathrequest::wref wrequest, requests)
        {
            if (shouldcancel())
                break;

            bool remove = true;
            pathrequest::pointer prequest = wrequest.lock ();

            if (prequest)
            {
                if (!prequest->needsupdate (newrequests, ledger->getledgerseq ()))
                    remove = false;
                else
                {
                    infosub::pointer ipsub = prequest->getsubscriber ();
                    if (ipsub)
                    {
                        ipsub->getconsumer ().charge (resource::feepathfindupdate);
                        if (!ipsub->getconsumer ().warn ())
                        {
                            json::value update = prequest->doupdate (cache, false);
                            prequest->updatecomplete ();
                            update["type"] = "path_find";
                            ipsub->send (update, false);
                            remove = false;
                            ++processed;
                        }
                    }
                }
            }

            if (remove)
            {
                pathrequest::pointer prequest = wrequest.lock ();

                scopedlocktype sl (mlock);

                // remove any dangling weak pointers or weak pointers that refer to this path request.
                std::vector<pathrequest::wptr>::iterator it = mrequests.begin();
                while (it != mrequests.end())
                {
                    pathrequest::pointer itrequest = it->lock ();
                    if (!itrequest || (itrequest == prequest))
                    {
                        ++removed;
                        it = mrequests.erase (it);
                    }
                    else
                        ++it;
                }
            }

            mustbreak = !newrequests && getapp().getledgermaster().isnewpathrequest();
            if (mustbreak) // we weren't handling new requests and then there was a new request
                break;

        }

        if (mustbreak)
        { // a new request came in while we were working
            newrequests = true;
        }
        else if (newrequests)
        { // we only did new requests, so we always need a last pass
            newrequests = getapp().getledgermaster().isnewpathrequest();
        }
        else
        { // check if there are any new requests, otherwise we are done
            newrequests = getapp().getledgermaster().isnewpathrequest();
            if (!newrequests) // we did a full pass and there are no new requests
                return;
        }

        {
            // get the latest requests, cache, and ledger for next pass
            scopedlocktype sl (mlock);

            if (mrequests.empty())
                break;
            requests = mrequests;

            cache = getlinecache (ledger, false);
        }

    }
    while (!shouldcancel ());

    mjournal.debug << "updateall complete " << processed << " process and " <<
        removed << " removed";
}

json::value pathrequests::makepathrequest(
    std::shared_ptr <infosub> const& subscriber,
    const std::shared_ptr<ledger>& inledger,
    json::value const& requestjson)
{
    pathrequest::pointer req = std::make_shared<pathrequest> (
        subscriber, ++mlastidentifier, *this, mjournal);

    ledger::pointer ledger = inledger;
    ripplelinecache::pointer cache;

    {
        scopedlocktype sl (mlock);
        cache = getlinecache (ledger, false);
    }

    bool valid = false;
    json::value result = req->docreate (ledger, cache, requestjson, valid);

    if (valid)
    {
        {
            scopedlocktype sl (mlock);

            // insert after any older unserviced requests but before any serviced requests
            std::vector<pathrequest::wptr>::iterator it = mrequests.begin ();
            while (it != mrequests.end ())
            {
                pathrequest::pointer req = it->lock ();
                if (req && !req->isnew ())
                    break; // this request has been handled, we come before it

                // this is a newer request, we come after it
                ++it;
            }
            mrequests.insert (it, pathrequest::wptr (req));

        }
        subscriber->setpathrequest (req);
        getapp().getledgermaster().newpathrequest();
    }
    return result;
}

} // ripple
