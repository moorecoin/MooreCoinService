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

#ifndef ripple_resource_manager_h_included
#define ripple_resource_manager_h_included

#include <ripple/json/json_value.h>
#include <ripple/resource/consumer.h>
#include <ripple/resource/gossip.h>
#include <beast/insight/collector.h>
#include <beast/net/ipendpoint.h>
#include <beast/utility/journal.h>
#include <beast/utility/propertystream.h>

namespace ripple {
namespace resource {

/** tracks load and resource consumption. */
class manager : public beast::propertystream::source
{
protected:
    manager ();

public:
    virtual ~manager() = 0;

    /** create a new endpoint keyed by inbound ip address. */
    virtual consumer newinboundendpoint (beast::ip::endpoint const& address) = 0;

    /** create a new endpoint keyed by outbound ip address and port. */
    virtual consumer newoutboundendpoint (beast::ip::endpoint const& address) = 0;

    /** create a new endpoint keyed by name. */
    virtual consumer newadminendpoint (std::string const& name) = 0;

    /** extract packaged consumer information for export. */
    virtual gossip exportconsumers () = 0;

    /** extract consumer information for reporting. */
    virtual json::value getjson () = 0;
    virtual json::value getjson (int threshold) = 0;

    /** import packaged consumer information.
        @param origin an identifier that unique labels the origin.
    */
    virtual void importconsumers (std::string const& origin, gossip const& gossip) = 0;
};

//------------------------------------------------------------------------------

std::unique_ptr <manager> make_manager (
    beast::insight::collector::ptr const& collector,
        beast::journal journal);

}
}

#endif
