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
#include <ripple/peerfinder/manager.h>
#include <ripple/peerfinder/impl/tuning.h>

namespace ripple {
namespace peerfinder {

config::config()
    : maxpeers (tuning::defaultmaxpeers)
    , outpeers (calcoutpeers ())
    , wantincoming (true)
    , autoconnect (true)
    , listeningport (0)
{
}

double config::calcoutpeers () const
{
    return std::max (
        maxpeers * tuning::outpercent * 0.01,
            double (tuning::minoutcount));
}

void config::applytuning ()
{
    if (maxpeers < tuning::minoutcount)
        maxpeers = tuning::minoutcount;
    outpeers = calcoutpeers ();
}

void config::onwrite (beast::propertystream::map &map)
{
    map ["max_peers"]       = maxpeers;
    map ["out_peers"]       = outpeers;
    map ["want_incoming"]   = wantincoming;
    map ["auto_connect"]    = autoconnect;
    map ["port"]            = listeningport;
    map ["features"]        = features;
}

}
}
