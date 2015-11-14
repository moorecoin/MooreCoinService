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
#include <ripple/app/ledger/directoryentryiterator.h>
#include <ripple/basics/log.h>

namespace ripple {

/** get the current ledger entry */
sle::pointer directoryentryiterator::getentry (ledgerentryset& les, ledgerentrytype type)
{
    return les.entrycache (type, mentryindex);
}

/** position the iterator at the first entry
*/
bool directoryentryiterator::firstentry (ledgerentryset& les)
{
    writelog (lstrace, ledger) << "directoryentryiterator::firstentry(" <<
        to_string (mrootindex) << ")";
    mentry = 0;
    mdirnode.reset ();

    return nextentry (les);
}

/** advance the iterator to the next entry
*/
bool directoryentryiterator::nextentry (ledgerentryset& les)
{
    if (!mdirnode)
    {
        writelog (lstrace, ledger) << "directoryentryiterator::nextentry(" <<
            to_string (mrootindex) << ") need dir node";
        // are we already at the end
        if (mdirindex.iszero())
        {
            writelog (lstrace, ledger) << "directoryentryiterator::nextentry(" <<
                to_string (mrootindex) << ") at end";
            return false;
        }

        // fetch the current directory
        mdirnode = les.entrycache (ltdir_node, mrootindex);
        if (!mdirnode)
        {
            writelog (lstrace, ledger) << "directoryentryiterator::nextentry("
                << to_string (mrootindex) << ") no dir node";
            mentryindex.zero();
            return false;
        }
    }

    if (!les.dirnext (mrootindex, mdirnode, mentry, mentryindex))
    {
        mdirindex.zero();
        mdirnode.reset();
        writelog (lstrace, ledger) << "directoryentryiterator::nextentry(" <<
            to_string (mrootindex) << ") now at end";
        return false;
    }

    writelog (lstrace, ledger) << "directoryentryiterator::nextentry(" <<
        to_string (mrootindex) << ") now at " << mentry;
    return true;
}

bool directoryentryiterator::addjson (json::value& j) const
{
    if (mdirnode && (mentry != 0))
    {
        j["dir_root"] = to_string (mrootindex);
        j["dir_entry"] = static_cast<json::uint> (mentry);

        if (mdirnode)
            j["dir_index"] = to_string (mdirindex);

        return true;
    }
    return false;
}

bool directoryentryiterator::setjson (json::value const& j, ledgerentryset& les)
{
    if (!j.ismember("dir_root") || !j.ismember("dir_index") || !j.ismember("dir_entry"))
        return false;
#if 0 // writeme
    json::value const& dirroot = j["dir_root"];
    json::value const& dirindex = j["dir_index"];
    json::value const& direntry = j["dir_entry"];

    assert(false); // caution: this function is incomplete

    mentry = j["dir_entry"].asuint ();

    if (!mdirindex.sethex(j["dir_index"].asstring()))
        return false;
#endif

    return true;
}

} // ripple
