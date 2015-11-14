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

#ifndef ripple_peerfinder_source_h_included
#define ripple_peerfinder_source_h_included

#include <ripple/peerfinder/manager.h>
#include <beast/smart_ptr/sharedobject.h>
#include <boost/system/error_code.hpp>

namespace ripple {
namespace peerfinder {

/** a static or dynamic source of peer addresses.
    these are used as fallbacks when we are bootstrapping and don't have
    a local cache, or when none of our addresses are functioning. typically
    sources will represent things like static text in the config file, a
    separate local file with addresses, or a remote https url that can
    be updated automatically. another solution is to use a custom dns server
    that hands out peer ip addresses when name lookups are performed.
*/
class source : public beast::sharedobject
{
public:
    /** the results of a fetch. */
    struct results
    {
        // error_code on a failure
        boost::system::error_code error;

        // list of fetched endpoints
        ipaddresses addresses;
    };

    virtual ~source () { }
    virtual std::string const& name () = 0;
    virtual void cancel () { }
    virtual void fetch (results& results, beast::journal journal) = 0;
};

}
}

#endif
