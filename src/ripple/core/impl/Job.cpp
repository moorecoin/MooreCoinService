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
#include <ripple/core/job.h>

namespace ripple {

job::job ()
    : mtype (jtinvalid)
    , mjobindex (0)
{
}

job::job (jobtype type, std::uint64_t index)
    : mtype (type)
    , mjobindex (index)
{
}

job::job (jobtype type,
          std::string const& name,
          std::uint64_t index,
          loadmonitor& lm,
          std::function <void (job&)> const& job,
          cancelcallback cancelcallback)
    : m_cancelcallback (cancelcallback)
    , mtype (type)
    , mjobindex (index)
    , mjob (job)
    , mname (name)
    , m_queue_time (clock_type::now ())
{
    m_loadevent = std::make_shared <loadevent> (std::ref (lm), name, false);
}

jobtype job::gettype () const
{
    return mtype;
}

job::cancelcallback job::getcancelcallback () const
{
    bassert (m_cancelcallback);
    return m_cancelcallback;
}

job::clock_type::time_point const& job::queue_time () const
{
    return m_queue_time;
}

bool job::shouldcancel () const
{
    if (m_cancelcallback)
        return m_cancelcallback ();
    return false;
}

void job::dojob ()
{
    m_loadevent->start ();
    m_loadevent->rename (mname);

    mjob (*this);
}

void job::rename (std::string const& newname)
{
    mname = newname;
}

bool job::operator> (const job& j) const
{
    if (mtype < j.mtype)
        return true;

    if (mtype > j.mtype)
        return false;

    return mjobindex > j.mjobindex;
}

bool job::operator>= (const job& j) const
{
    if (mtype < j.mtype)
        return true;

    if (mtype > j.mtype)
        return false;

    return mjobindex >= j.mjobindex;
}

bool job::operator< (const job& j) const
{
    if (mtype < j.mtype)
        return false;

    if (mtype > j.mtype)
        return true;

    return mjobindex < j.mjobindex;
}

bool job::operator<= (const job& j) const
{
    if (mtype < j.mtype)
        return false;

    if (mtype > j.mtype)
        return true;

    return mjobindex <= j.mjobindex;
}

}
