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

#ifndef beast_insight_collector_h_included
#define beast_insight_collector_h_included

#include <beast/insight/counter.h>
#include <beast/insight/event.h>
#include <beast/insight/gauge.h>
#include <beast/insight/hook.h>
#include <beast/insight/meter.h>

#include <string>

namespace beast {
namespace insight {

/** interface for a manager that allows collection of metrics.

    to export metrics from a class, pass and save a shared_ptr to this
    interface in the class constructor. create the metric objects
    as desired (counters, events, gauges, meters, and an optional hook)
    using the interface.

    @see counter, event, gauge, hook, meter
    @see nullcollector, statsdcollector
*/
class collector
{
public:
    typedef std::shared_ptr <collector> ptr;

    virtual ~collector() = 0;

    /** create a hook.
        
        a hook is called at each collection interval, on an implementation
        defined thread. this is a convenience facility for gathering metrics
        in the polling style. the typical usage is to update all the metrics
        of interest in the handler.

        handler will be called with this signature:
            void handler (void)

        @see hook
    */
    /** @{ */
    template <class handler>
    hook make_hook (handler handler)
    {
        return make_hook (hookimpl::handlertype (handler));
    }

    virtual hook make_hook (hookimpl::handlertype const& handler) = 0;
    /** @} */

    /** create a counter with the specified name.
        @see counter
    */
    /** @{ */
    virtual counter make_counter (std::string const& name) = 0;
    
    counter make_counter (std::string const& prefix, std::string const& name)
    {
        if (prefix.empty ())
            return make_counter (name);
        return make_counter (prefix + "." + name);
    }
    /** @} */

    /** create an event with the specified name.
        @see event
    */
    /** @{ */
    virtual event make_event (std::string const& name) = 0;

    event make_event (std::string const& prefix, std::string const& name)
    {
        if (prefix.empty ())
            return make_event (name);
        return make_event (prefix + "." + name);
    }
    /** @} */

    /** create a gauge with the specified name. 
        @see gauge
    */
    /** @{ */
    virtual gauge make_gauge (std::string const& name) = 0;

    gauge make_gauge (std::string const& prefix, std::string const& name)
    {
        if (prefix.empty ())
            return make_gauge (name);
        return make_gauge (prefix + "." + name);
    }
    /** @} */

    /** create a meter with the specified name.
        @see meter
    */
    /** @{ */
    virtual meter make_meter (std::string const& name) = 0;

    meter make_meter (std::string const& prefix, std::string const& name)
    {
        if (prefix.empty ())
            return make_meter (name);
        return make_meter (prefix + "." + name);
    }
    /** @} */
};

}
}

#endif
