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
#include <ripple/core/loadevent.h>
#include <ripple/core/loadmonitor.h>

namespace ripple {

loadevent::loadevent (loadmonitor& monitor, std::string const& name, bool shouldstart)
    : m_loadmonitor (monitor)
    , m_isrunning (false)
    , m_name (name)
    , m_timestopped (beast::relativetime::fromstartup())
    , m_secondswaiting (0)
    , m_secondsrunning (0)
{
    if (shouldstart)
        start ();
}

loadevent::~loadevent ()
{
    if (m_isrunning)
        stop ();
}

std::string const& loadevent::name () const
{
    return m_name;
}

double loadevent::getsecondswaiting() const
{
    return m_secondswaiting;
}

double loadevent::getsecondsrunning() const
{
    return m_secondsrunning;
}

double loadevent::getsecondstotal() const
{
    return m_secondswaiting + m_secondsrunning;
}

void loadevent::rename (std::string const& name)
{
    m_name = name;
}

void loadevent::start ()
{
    beast::relativetime const currenttime (beast::relativetime::fromstartup());

    // if we already called start, this call will replace the previous one.
    if (m_isrunning)
    {
        m_secondswaiting += (currenttime - m_timestarted).inseconds();
    }
    else
    {
        m_secondswaiting += (currenttime - m_timestopped).inseconds();
        m_isrunning = true;
    }

    m_timestarted = currenttime;
}

void loadevent::stop ()
{
    bassert (m_isrunning);

    m_timestopped = beast::relativetime::fromstartup();
    m_secondsrunning += (m_timestopped - m_timestarted).inseconds();

    m_isrunning = false;
    m_loadmonitor.addloadsample (*this);
}

} // ripple
