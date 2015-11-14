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
#include <ripple/app/misc/feevote.h>
#include <ripple/app/main/application.h>
#include <ripple/app/misc/validations.h>
#include <ripple/basics/basicconfig.h>
#include <beast/utility/journal.h>

namespace ripple {

namespace detail {

template <typename integer>
class votableinteger
{
private:
    typedef std::map <integer, int> map_type;
    integer mcurrent;   // the current setting
    integer mtarget;    // the setting we want
    map_type mvotemap;

public:
    votableinteger (integer current, integer target)
        : mcurrent (current)
        , mtarget (target)
    {
        // add our vote
        ++mvotemap[mtarget];
    }

    void
    addvote(integer vote)
    {
        ++mvotemap[vote];
    }

    void
    novote()
    {
        addvote (mcurrent);
    }

    integer
    getvotes() const;
};

template <class integer>
integer
votableinteger <integer>::getvotes() const
{
    integer ourvote = mcurrent;
    int weight = 0;
    for (auto const& e : mvotemap)
    {
        // take most voted value between current and target, inclusive
        if ((e.first <= std::max (mtarget, mcurrent)) &&
                (e.first >= std::min (mtarget, mcurrent)) &&
                (e.second > weight))
        {
            ourvote = e.first;
            weight = e.second;
        }
    }

    return ourvote;
}

}

//------------------------------------------------------------------------------

class feevoteimpl : public feevote
{
private:
    setup target_;
    beast::journal journal_;

public:
    feevoteimpl (setup const& setup, beast::journal journal);

    void
    dovalidation (ledger::ref lastclosedledger,
        stobject& basevalidation) override;

    void
    dovoting (ledger::ref lastclosedledger,
        shamap::ref initialposition) override;
};

//--------------------------------------------------------------------------

feevoteimpl::feevoteimpl (setup const& setup, beast::journal journal)
    : target_ (setup)
    , journal_ (journal)
{
}

void
feevoteimpl::dovalidation (ledger::ref lastclosedledger,
    stobject& basevalidation)
{
    if (lastclosedledger->getbasefee () != target_.reference_fee)
    {
        if (journal_.info) journal_.info <<
            "voting for base fee of " << target_.reference_fee;

        basevalidation.setfieldu64 (sfbasefee, target_.reference_fee);
    }

    if (lastclosedledger->getreserve (0) != target_.account_reserve)
    {
        if (journal_.info) journal_.info <<
            "voting for base resrve of " << target_.account_reserve;

        basevalidation.setfieldu32(sfreservebase, target_.account_reserve);
    }

    if (lastclosedledger->getreserveinc () != target_.owner_reserve)
    {
        if (journal_.info) journal_.info <<
            "voting for reserve increment of " << target_.owner_reserve;

        basevalidation.setfieldu32 (sfreserveincrement,
            target_.owner_reserve);
    }
}

void
feevoteimpl::dovoting (ledger::ref lastclosedledger,
    shamap::ref initialposition)
{
    // lcl must be flag ledger
    assert ((lastclosedledger->getledgerseq () % 256) == 0);

    detail::votableinteger<std::uint64_t> basefeevote (
        lastclosedledger->getbasefee (), target_.reference_fee);

    detail::votableinteger<std::uint32_t> basereservevote (
        lastclosedledger->getreserve (0), target_.account_reserve);

    detail::votableinteger<std::uint32_t> increservevote (
        lastclosedledger->getreserveinc (), target_.owner_reserve);

    // get validations for ledger before flag
    validationset const set =
        getapp().getvalidations ().getvalidations (
            lastclosedledger->getparenthash ());
    for (auto const& e : set)
    {
        stvalidation const& val = *e.second;

        if (val.istrusted ())
        {
            if (val.isfieldpresent (sfbasefee))
            {
                basefeevote.addvote (val.getfieldu64 (sfbasefee));
            }
            else
            {
                basefeevote.novote ();
            }

            if (val.isfieldpresent (sfreservebase))
            {
                basereservevote.addvote (val.getfieldu32 (sfreservebase));
            }
            else
            {
                basereservevote.novote ();
            }

            if (val.isfieldpresent (sfreserveincrement))
            {
                increservevote.addvote (val.getfieldu32 (sfreserveincrement));
            }
            else
            {
                increservevote.novote ();
            }
        }
    }

    // choose our positions
    std::uint64_t const basefee = basefeevote.getvotes ();
    std::uint32_t const basereserve = basereservevote.getvotes ();
    std::uint32_t const increserve = increservevote.getvotes ();

    // add transactions to our position
    if ((basefee != lastclosedledger->getbasefee ()) ||
            (basereserve != lastclosedledger->getreserve (0)) ||
            (increserve != lastclosedledger->getreserveinc ()))
    {
        if (journal_.warning) journal_.warning <<
            "we are voting for a fee change: " << basefee <<
            "/" << basereserve <<
            "/" << increserve;

        sttx trans (ttfee);
        trans.setfieldaccount (sfaccount, account ());
        trans.setfieldu64 (sfbasefee, basefee);
        trans.setfieldu32 (sfreferencefeeunits, 10);
        trans.setfieldu32 (sfreservebase, basereserve);
        trans.setfieldu32 (sfreserveincrement, increserve);

        uint256 txid = trans.gettransactionid ();

        if (journal_.warning)
            journal_.warning << "vote: " << txid;

        serializer s;
        trans.add (s, true);

        shamapitem::pointer titem = std::make_shared<shamapitem> (
            txid, s.peekdata ());

        if (!initialposition->addgiveitem (titem, true, false))
        {
            if (journal_.warning) journal_.warning <<
                "ledger already had fee change";
        }
    }
}

//------------------------------------------------------------------------------

feevote::setup
setup_feevote (section const& section)
{
    feevote::setup setup;
    set (setup.reference_fee, "reference_fee", section);
    set (setup.account_reserve, "account_reserve", section);
    set (setup.owner_reserve, "owner_reserve", section);
    return setup;
}

std::unique_ptr<feevote>
make_feevote (feevote::setup const& setup, beast::journal journal)
{
    return std::make_unique<feevoteimpl> (setup, journal);
}

} // ripple
