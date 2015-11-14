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

#ifndef ripple_core_jobqueue_h_included
#define ripple_core_jobqueue_h_included

#include <ripple/core/jobtypes.h>
#include <ripple/json/json_value.h>
#include <beast/insight/collector.h>
#include <beast/threads/stoppable.h>
#include <boost/function.hpp>

namespace ripple {

class jobqueue : public beast::stoppable
{
protected:
    jobqueue (char const* name, stoppable& parent);

public:
    virtual ~jobqueue () { }

    // vfalco note using boost::function here because visual studio 2012
    //             std::function doesn't swallow return types.
    //
    //        todo replace with std::function
    //
    virtual void addjob (jobtype type,
        std::string const& name, boost::function <void (job&)> const& job) = 0;

    // jobs waiting at this priority
    virtual int getjobcount (jobtype t) = 0;

    // jobs waiting plus running at this priority
    virtual int getjobcounttotal (jobtype t) = 0;

    // all waiting jobs at or greater than this priority
    virtual int getjobcountge (jobtype t) = 0;

    virtual void shutdown () = 0;

    virtual void setthreadcount (int c, bool const standalonemode) = 0;

    // vfalco todo rename these to newloadeventmeasurement or something similar
    //             since they create the object.
    //
    virtual loadevent::pointer getloadevent (jobtype t, std::string const& name) = 0;

    // vfalco todo why do we need two versions, one which returns a shared
    //             pointer and the other which returns an autoptr?
    //
    virtual loadevent::autoptr getloadeventap (jobtype t, std::string const& name) = 0;

    // add multiple load events
    virtual void addloadevents (jobtype t,
        int count, std::chrono::milliseconds elapsed) = 0;

    virtual bool isoverloaded () = 0;

    virtual json::value getjson (int c = 0) = 0;
};

std::unique_ptr <jobqueue>
make_jobqueue (beast::insight::collector::ptr const& collector,
    beast::stoppable& parent, beast::journal journal);

}

#endif
