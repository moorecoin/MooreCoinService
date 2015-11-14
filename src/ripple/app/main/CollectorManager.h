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

#ifndef ripple_ripplecollector_h_included
#define ripple_ripplecollector_h_included

#include <beast/module/core/text/stringpairarray.h>
#include <beast/insight.h>

namespace ripple {

/** provides the beast::insight::collector service. */
class collectormanager
{
public:
    static collectormanager* new (beast::stringpairarray const& params,
        beast::journal journal);
    virtual ~collectormanager () = 0;
    virtual beast::insight::collector::ptr const& collector () = 0;
    virtual beast::insight::group::ptr const& group (std::string const& name) = 0;
};

}

#endif
