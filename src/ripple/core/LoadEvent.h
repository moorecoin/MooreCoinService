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

#ifndef ripple_loadevent_h
#define ripple_loadevent_h

#include <beast/chrono/relativetime.h>
#include <memory>

namespace ripple {

class loadmonitor;

// vfalco note what is the difference between a loadevent and a loadmonitor?
// vfalco todo rename loadevent to scopedloadsample
//
//        this looks like a scoped elapsed time measuring class
//
class loadevent
{
public:
    // vfalco note why are these shared pointers? wouldn't there be a
    //             piece of lifetime-managed calling code that can simply own
    //             the object?
    //
    //             why both kinds of containers?
    //
    typedef std::shared_ptr <loadevent> pointer;
    typedef std::unique_ptr <loadevent>            autoptr;

public:
    // vfalco todo remove the dependency on loadmonitor. is that possible?
    loadevent (loadmonitor& monitor,
               std::string const& name,
               bool shouldstart);

    ~loadevent ();

    std::string const& name () const;
    double getsecondswaiting() const;
    double getsecondsrunning() const;
    double getsecondstotal() const;

    // vfalco todo rename this to setname () or setlabel ()
    void rename (std::string const& name);

    // start the measurement. the constructor calls this automatically if
    // shouldstart is true. if the operation is aborted, start() can be
    // called again later.
    //
    void start ();

    // stops the measurement and reports the results. the time reported is
    // measured from the last call to start.
    //
    void stop ();

private:
    loadmonitor& m_loadmonitor;
    bool m_isrunning;
    std::string m_name;
    // vfalco todo replace these with chrono
    beast::relativetime m_timestopped;
    beast::relativetime m_timestarted;
    double m_secondswaiting;
    double m_secondsrunning;
};

} // ripple

#endif
