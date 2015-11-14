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
#include <ripple/app/paths/node.h>
#include <ripple/app/paths/pathstate.h>

namespace ripple {
namespace path {

// compare the non-calculated fields.
bool node::operator== (const node& other) const
{
    return other.uflags == uflags
            && other.account_ == account_
            && other.issue_ == issue_;
}

// this is for debugging not end users. output names can be changed without
// warning.
json::value node::getjson () const
{
    json::value jvnode (json::objectvalue);
    json::value jvflags (json::arrayvalue);

    jvnode["type"]  = uflags;

    bool const hascurrency = !isnative(issue_.currency);
    bool const hasaccount = !isnative(account_);
    bool const hasissuer = !isnative(issue_.account);

    if (isaccount() || hasaccount)
        jvflags.append (!isaccount() == hasaccount ? "account" : "-account");

    if (uflags & stpathelement::typecurrency || hascurrency)
    {
        jvflags.append ((uflags & stpathelement::typecurrency) && hascurrency
            ? "currency"
            : "-currency");
    }

    if (uflags & stpathelement::typeissuer || hasissuer)
    {
        jvflags.append ((uflags & stpathelement::typeissuer) && hasissuer
            ? "issuer"
            : "-issuer");
    }

    jvnode["flags"] = jvflags;

	if (!isnative(account_))
        jvnode["account"] = to_string (account_);

	if (!isnative(issue_.currency))
        jvnode["currency"] = to_string (issue_.currency);

	if (!isnative(issue_.account))
        jvnode["issuer"] = to_string (issue_.account);

    if (sarevredeem)
        jvnode["rev_redeem"] = sarevredeem.getfulltext ();

    if (sarevissue)
        jvnode["rev_issue"] = sarevissue.getfulltext ();

    if (sarevdeliver)
        jvnode["rev_deliver"] = sarevdeliver.getfulltext ();

    if (safwdredeem)
        jvnode["fwd_redeem"] = safwdredeem.getfulltext ();

    if (safwdissue)
        jvnode["fwd_issue"] = safwdissue.getfulltext ();

    if (safwddeliver)
        jvnode["fwd_deliver"] = safwddeliver.getfulltext ();

    return jvnode;
}

} // path
} // ripple
