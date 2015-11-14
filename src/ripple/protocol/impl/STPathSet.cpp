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
#include <ripple/protocol/stpathset.h>
#include <ripple/protocol/jsonfields.h>
#include <ripple/basics/log.h>
#include <ripple/basics/strhex.h>
#include <ripple/basics/stringutilities.h>
#include <cstddef>

namespace ripple {

std::size_t
stpathelement::get_hash (stpathelement const& element)
{
    std::size_t hash_account  = 2654435761;
    std::size_t hash_currency = 2654435761;
    std::size_t hash_issuer   = 2654435761;

    // nikb note: this doesn't have to be a secure hash as speed is more
    //            important. we don't even really need to fully hash the whole
    //            base_uint here, as a few bytes would do for our use.

    for (auto const x : element.getaccountid ())
        hash_account += (hash_account * 257) ^ x;

    for (auto const x : element.getcurrency ())
        hash_currency += (hash_currency * 509) ^ x;

    for (auto const x : element.getissuerid ())
        hash_issuer += (hash_issuer * 911) ^ x;

    return (hash_account ^ hash_currency ^ hash_issuer);
}

stpathset* stpathset::construct (serializeriterator& s, sfield::ref name)
{
    std::vector<stpath> paths;
    std::vector<stpathelement> path;

    do
    {
        int itype   = s.get8 ();

        if (itype == stpathelement::typenone ||
            itype == stpathelement::typeboundary)
        {
            if (path.empty ())
            {
                writelog (lsinfo, stbase) << "stpathset: empty path.";

                throw std::runtime_error ("empty path");
            }

            paths.push_back (path);
            path.clear ();

            if (itype == stpathelement::typenone)
            {
                return new stpathset (name, paths);
            }
        }
        else if (itype & ~stpathelement::typeall)
        {
            writelog (lsinfo, stbase)
                    << "stpathset: bad path element: " << itype;

            throw std::runtime_error ("bad path element");
        }
        else
        {
            auto hasaccount = itype & stpathelement::typeaccount;
            auto hascurrency = itype & stpathelement::typecurrency;
            auto hasissuer = itype & stpathelement::typeissuer;

            account account;
            currency currency;
            account issuer;

            if (hasaccount)
                account.copyfrom (s.get160 ());

            if (hascurrency)
                currency.copyfrom (s.get160 ());

            if (hasissuer)
                issuer.copyfrom (s.get160 ());

            path.emplace_back (account, currency, issuer, hascurrency);
        }
    }
    while (1);
}

bool stpathset::isequivalent (const stbase& t) const
{
    const stpathset* v = dynamic_cast<const stpathset*> (&t);
    return v && (value == v->value);
}

bool stpath::hasseen (
    account const& account, currency const& currency,
    account const& issuer) const
{
    for (auto& p: mpath)
    {
        if (p.getaccountid () == account
            && p.getcurrency () == currency
            && p.getissuerid () == issuer)
            return true;
    }

    return false;
}

json::value stpath::getjson (int) const
{
    json::value ret (json::arrayvalue);

    for (auto it: mpath)
    {
        json::value elem (json::objectvalue);
        int         itype   = it.getnodetype ();

        elem[jss::type]      = itype;
        elem[jss::type_hex]  = strhex (itype);

        if (itype & stpathelement::typeaccount)
            elem[jss::account]  = to_string (it.getaccountid ());

        if (itype & stpathelement::typecurrency)
            elem[jss::currency] = to_string (it.getcurrency ());

        if (itype & stpathelement::typeissuer)
            elem[jss::issuer]   = to_string (it.getissuerid ());

        ret.append (elem);
    }

    return ret;
}

json::value stpathset::getjson (int options) const
{
    json::value ret (json::arrayvalue);
    for (auto it: value)
        ret.append (it.getjson (options));

    return ret;
}

void stpathset::add (serializer& s) const
{
    assert (fname->isbinary ());
    assert (fname->fieldtype == sti_pathset);
    bool first = true;

    for (auto const& sppath : value)
    {
        if (!first)
            s.add8 (stpathelement::typeboundary);

        for (auto const& speelement : sppath)
        {
            int itype = speelement.getnodetype ();

            s.add8 (itype);

            if (itype & stpathelement::typeaccount)
                s.add160 (speelement.getaccountid ());

            if (itype & stpathelement::typecurrency)
                s.add160 (speelement.getcurrency ());

            if (itype & stpathelement::typeissuer)
                s.add160 (speelement.getissuerid ());
        }

        first = false;
    }

    s.add8 (stpathelement::typenone);
}

} // ripple
