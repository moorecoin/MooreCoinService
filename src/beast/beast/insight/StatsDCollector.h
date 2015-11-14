//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

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

#ifndef beast_insight_statsdcollector_h_included
#define beast_insight_statsdcollector_h_included

#include <beast/insight/collector.h>

#include <beast/utility/journal.h>
#include <beast/net/ipendpoint.h>

namespace beast {
namespace insight {

/** a collector that reports metrics to a statsd server.
    reference:
        https://github.com/b/statsd_spec
*/
class statsdcollector : public collector
{
public:
    /** create a statsd collector.
        @param address the ip address and port of the statsd server.
        @param prefix a string pre-pended before each metric name.
        @param journal destination for logging output.
    */
    static
    std::shared_ptr <statsdcollector>
    new (ip::endpoint const& address,
        std::string const& prefix, journal journal);
};

}
}

#endif
