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

#ifndef ripple_core_jobtypeinfo_h_included
#define ripple_core_jobtypeinfo_h_included

namespace ripple
{

/** holds all the 'static' information about a job, which does not change */
class jobtypeinfo
{
private:
    jobtype const m_type;
    std::string const m_name;

    /** the limit on the number of running jobs for this job type. */
    int const m_limit;

    /** can be skipped */
    bool const m_skip;

    /** special jobs are not dispatched via the job queue */
    bool const m_special;

    /** average and peak latencies for this job type. 0 is none specified */
    std::uint64_t const m_avglatency;
    std::uint64_t const m_peaklatency;

public:
    // not default constructible
    jobtypeinfo () = delete;

    jobtypeinfo (jobtype type, std::string name, int limit,
            bool skip, bool special, std::uint64_t avglatency, std::uint64_t peaklatency)
        : m_type (type)
        , m_name (name)
        , m_limit (limit)
        , m_skip (skip)
        , m_special (special)
        , m_avglatency (avglatency)
        , m_peaklatency (peaklatency)
    {

    }

    jobtype type () const
    {
        return m_type;
    }

    std::string name () const
    {
        return m_name;
    }

    int limit () const
    {
        return m_limit;
    }

    bool skip () const
    {
        return m_skip;
    }

    bool special () const
    {
        return m_special;
    }

    std::uint64_t getaveragelatency () const
    {
        return m_avglatency;
    }

    std::uint64_t getpeaklatency () const
    {
        return m_peaklatency;
    }
};

}

#endif
