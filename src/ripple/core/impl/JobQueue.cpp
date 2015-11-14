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
#include <ripple/core/jobqueue.h>
#include <ripple/core/jobtypes.h>
#include <ripple/core/jobtypeinfo.h>
#include <ripple/core/jobtypedata.h>
#include <beast/cxx14/memory.h>
#include <beast/chrono/chrono_util.h>
#include <beast/module/core/thread/workers.h>
#include <chrono>
#include <mutex>
#include <set>
#include <thread>

namespace ripple {

class jobqueueimp
    : public jobqueue
    , private beast::workers::callback
{
public:
    typedef std::set <job> jobset;
    typedef std::map <jobtype, jobtypedata> jobdatamap;
    typedef std::lock_guard <std::mutex> scopedlock;

    beast::journal m_journal;
    std::mutex m_mutex;
    std::uint64_t m_lastjob;
    jobset m_jobset;
    jobdatamap m_jobdata;
    jobtypedata m_invalidjobdata;

    // the number of jobs currently in processtask()
    int m_processcount;

    beast::workers m_workers;
    job::cancelcallback m_cancelcallback;

    // statistics tracking
    beast::insight::collector::ptr m_collector;
    beast::insight::gauge job_count;
    beast::insight::hook hook;

    //--------------------------------------------------------------------------
    static jobtypes const& getjobtypes ()
    {
        static jobtypes types;

        return types;
    }

    //--------------------------------------------------------------------------
    jobqueueimp (beast::insight::collector::ptr const& collector,
        stoppable& parent, beast::journal journal)
        : jobqueue ("jobqueue", parent)
        , m_journal (journal)
        , m_lastjob (0)
        , m_invalidjobdata (getjobtypes ().getinvalid (), collector)
        , m_processcount (0)
        , m_workers (*this, "jobqueue", 0)
        , m_cancelcallback (std::bind (&stoppable::isstopping, this))
        , m_collector (collector)
    {
        hook = m_collector->make_hook (std::bind (
            &jobqueueimp::collect, this));
        job_count = m_collector->make_gauge ("job_count");

        {
            scopedlock lock (m_mutex);

            for (auto const& x : getjobtypes ())
            {
                jobtypeinfo const& jt = x.second;

                // and create dynamic information for all jobs
                auto const result (m_jobdata.emplace (std::piecewise_construct,
                    std::forward_as_tuple (jt.type ()),
                    std::forward_as_tuple (jt, m_collector)));
                assert (result.second == true);
                (void) result.second;
            }
        }
    }

    ~jobqueueimp ()
    {
        // must unhook before destroying
        hook = beast::insight::hook ();
    }

    void collect ()
    {
        scopedlock lock (m_mutex);
        job_count = m_jobset.size ();
    }

    void addjob (jobtype type, std::string const& name,
        boost::function <void (job&)> const& jobfunc)
    {
        assert (type != jtinvalid);

        jobdatamap::iterator iter (m_jobdata.find (type));
        assert (iter != m_jobdata.end ());

        if (iter == m_jobdata.end ())
            return;

        jobtypedata& data (iter->second);

        // fixme: workaround incorrect client shutdown ordering
        // do not add jobs to a queue with no threads
        assert (type == jtclient || m_workers.getnumberofthreads () > 0);

        {
            // if this goes off it means that a child didn't follow
            // the stoppable api rules. a job may only be added if:
            //
            //  - the jobqueue has not stopped
            //          and
            //      * we are currently processing jobs
            //          or
            //      * we have have pending jobs
            //          or
            //      * not all children are stopped
            //
            scopedlock lock (m_mutex);
            assert (! isstopped() && (
                m_processcount>0 ||
                ! m_jobset.empty () ||
                ! arechildrenstopped()));
        }

        // don't even add it to the queue if we're stopping
        // and the job type is marked for skiponstop.
        //
        if (isstopping() && skiponstop (type))
        {
            m_journal.debug <<
                "skipping addjob ('" << name << "')";
            return;
        }

        {
            scopedlock lock (m_mutex);

            std::pair <std::set <job>::iterator, bool> result (
                m_jobset.insert (job (type, name, ++m_lastjob,
                    data.load (), jobfunc, m_cancelcallback)));
            queuejob (*result.first, lock);
        }
    }

    int getjobcount (jobtype t)
    {
        scopedlock lock (m_mutex);

        jobdatamap::const_iterator c = m_jobdata.find (t);

        return (c == m_jobdata.end ())
            ? 0
            : c->second.waiting;
    }

    int getjobcounttotal (jobtype t)
    {
        scopedlock lock (m_mutex);

        jobdatamap::const_iterator c = m_jobdata.find (t);

        return (c == m_jobdata.end ())
            ? 0
            : (c->second.waiting + c->second.running);
    }

    int getjobcountge (jobtype t)
    {
        // return the number of jobs at this priority level or greater
        int ret = 0;

        scopedlock lock (m_mutex);

        for (auto const& x : m_jobdata)
        {
            if (x.first >= t)
                ret += x.second.waiting;
        }

        return ret;
    }

    // shut down the job queue without completing pending jobs
    //
    void shutdown ()
    {
        m_journal.info <<  "job queue shutting down";

        m_workers.pauseallthreadsandwait ();
    }

    // set the number of thread serving the job queue to precisely this number
    void setthreadcount (int c, bool const standalonemode)
    {
        if (standalonemode)
        {
            c = 1;
        }
        else if (c == 0)
        {
            c = static_cast<int>(std::thread::hardware_concurrency());
            c = 2 + std::min (c, 4); // i/o will bottleneck

            m_journal.info << "auto-tuning to " << c <<
                              " validation/transaction/proposal threads";
        }

        m_workers.setnumberofthreads (c);
    }


    loadevent::pointer getloadevent (jobtype t, std::string const& name)
    {
        jobdatamap::iterator iter (m_jobdata.find (t));
        assert (iter != m_jobdata.end ());

        if (iter == m_jobdata.end ())
            return std::shared_ptr<loadevent> ();

        return std::make_shared<loadevent> (
            std::ref (iter-> second.load ()), name, true);
    }

    loadevent::autoptr getloadeventap (jobtype t, std::string const& name)
    {
        jobdatamap::iterator iter (m_jobdata.find (t));
        assert (iter != m_jobdata.end ());

        if (iter == m_jobdata.end ())
            return loadevent::autoptr ();

        return loadevent::autoptr (
            new loadevent (iter-> second.load (), name, true));
    }

    void addloadevents (jobtype t,
        int count, std::chrono::milliseconds elapsed)
    {
        jobdatamap::iterator iter (m_jobdata.find (t));
        assert (iter != m_jobdata.end ());
        iter->second.load().addsamples (count, elapsed);
    }

    bool isoverloaded ()
    {
        int count = 0;

        for (auto& x : m_jobdata)
        {
            if (x.second.load ().isover ())
                ++count;
        }

        return count > 0;
    }

    json::value getjson (int)
    {
        json::value ret (json::objectvalue);

        ret["threads"] = m_workers.getnumberofthreads ();

        json::value priorities = json::arrayvalue;

        scopedlock lock (m_mutex);

        for (auto& x : m_jobdata)
        {
            assert (x.first != jtinvalid);

            if (x.first == jtgeneric)
                continue;

            jobtypedata& data (x.second);

            loadmonitor::stats stats (data.stats ());

            int waiting (data.waiting);
            int running (data.running);

            if ((stats.count != 0) || (waiting != 0) ||
                (stats.latencypeak != 0) || (running != 0))
            {
                json::value& pri = priorities.append (json::objectvalue);

                pri["job_type"] = data.name ();

                if (stats.isoverloaded)
                    pri["over_target"] = true;

                if (waiting != 0)
                    pri["waiting"] = waiting;

                if (stats.count != 0)
                    pri["per_second"] = static_cast<int> (stats.count);

                if (stats.latencypeak != 0)
                    pri["peak_time"] = static_cast<int> (stats.latencypeak);

                if (stats.latencyavg != 0)
                    pri["avg_time"] = static_cast<int> (stats.latencyavg);

                if (running != 0)
                    pri["in_progress"] = running;
            }
        }

        ret["job_types"] = priorities;

        return ret;
    }

private:
    //--------------------------------------------------------------------------
    jobtypedata& getjobtypedata (jobtype type)
    {
        jobdatamap::iterator c (m_jobdata.find (type));
        assert (c != m_jobdata.end ());

        // nikb: this is ugly and i hate it. we must remove jtinvalid completely
        //       and use something sane.
        if (c == m_jobdata.end ())
            return m_invalidjobdata;

        return c->second;
    }

    //--------------------------------------------------------------------------

    // signals the service stopped if the stopped condition is met.
    //
    void checkstopped (scopedlock const& lock)
    {
        // we are stopped when all of the following are true:
        //
        //  1. a stop notification was received
        //  2. all stoppable children have stopped
        //  3. there are no executing calls to processtask
        //  4. there are no remaining jobs in the job set
        //
        if (isstopping() &&
            arechildrenstopped() &&
            (m_processcount == 0) &&
            m_jobset.empty())
        {
            stopped();
        }
    }

    //--------------------------------------------------------------------------
    //
    // signals an added job for processing.
    //
    // pre-conditions:
    //  the jobtype must be valid.
    //  the job must exist in mjobset.
    //  the job must not have previously been queued.
    //
    // post-conditions:
    //  count of waiting jobs of that type will be incremented.
    //  if jobqueue exists, and has at least one thread, job will eventually run.
    //
    // invariants:
    //  the calling thread owns the joblock
    //
    void queuejob (job const& job, scopedlock const& lock)
    {
        jobtype const type (job.gettype ());
        assert (type != jtinvalid);
        assert (m_jobset.find (job) != m_jobset.end ());

        jobtypedata& data (getjobtypedata (type));

        if (data.waiting + data.running < getjoblimit (type))
        {
            m_workers.addtask ();
        }
        else
        {
            // defer the task until we go below the limit
            //
            ++data.deferred;
        }
        ++data.waiting;
    }

    //------------------------------------------------------------------------------
    //
    // returns the next job we should run now.
    //
    // runnablejob:
    //  a job in the jobset whose slots count for its type is greater than zero.
    //
    // pre-conditions:
    //  mjobset must not be empty.
    //  mjobset holds at least one runnablejob
    //
    // post-conditions:
    //  job is a valid job object.
    //  job is removed from mjobqueue.
    //  waiting job count of it's type is decremented
    //  running job count of it's type is incremented
    //
    // invariants:
    //  the calling thread owns the joblock
    //
    void getnextjob (job& job, scopedlock const& lock)
    {
        assert (! m_jobset.empty ());

        jobset::const_iterator iter;
        for (iter = m_jobset.begin (); iter != m_jobset.end (); ++iter)
        {
            jobtypedata& data (getjobtypedata (iter->gettype ()));

            assert (data.running <= getjoblimit (data.type ()));

            // run this job if we're running below the limit.
            if (data.running < getjoblimit (data.type ()))
            {
                assert (data.waiting > 0);
                break;
            }
        }

        assert (iter != m_jobset.end ());

        jobtype const type = iter->gettype ();
        jobtypedata& data (getjobtypedata (type));

        assert (type != jtinvalid);

        job = *iter;
        m_jobset.erase (iter);

        --data.waiting;
        ++data.running;
    }

    //------------------------------------------------------------------------------
    //
    // indicates that a running job has completed its task.
    //
    // pre-conditions:
    //  job must not exist in mjobset.
    //  the jobtype must not be invalid.
    //
    // post-conditions:
    //  the running count of that jobtype is decremented
    //  a new task is signaled if there are more waiting jobs than the limit, if any.
    //
    // invariants:
    //  <none>
    //
    void finishjob (job const& job, scopedlock const& lock)
    {
        jobtype const type = job.gettype ();

        assert (m_jobset.find (job) == m_jobset.end ());
        assert (type != jtinvalid);

        jobtypedata& data (getjobtypedata (type));

        // queue a deferred task if possible
        if (data.deferred > 0)
        {
            assert (data.running + data.waiting >= getjoblimit (type));

            --data.deferred;
            m_workers.addtask ();
        }

        --data.running;
    }

    //--------------------------------------------------------------------------
    template <class rep, class period>
    void on_dequeue (jobtype type,
        std::chrono::duration <rep, period> const& value)
    {
        auto const ms (ceil <std::chrono::milliseconds> (value));

        if (ms.count() >= 10)
            getjobtypedata (type).dequeue.notify (ms);
    }

    template <class rep, class period>
    void on_execute (jobtype type,
        std::chrono::duration <rep, period> const& value)
    {
        auto const ms (ceil <std::chrono::milliseconds> (value));

        if (ms.count() >= 10)
            getjobtypedata (type).execute.notify (ms);
    }

    //--------------------------------------------------------------------------
    //
    // runs the next appropriate waiting job.
    //
    // pre-conditions:
    //  a runnablejob must exist in the jobset
    //
    // post-conditions:
    //  the chosen runnablejob will have job::dojob() called.
    //
    // invariants:
    //  <none>
    //
    void processtask ()
    {
        job job;

        {
            scopedlock lock (m_mutex);
            getnextjob (job, lock);
            ++m_processcount;
        }

        jobtypedata& data (getjobtypedata (job.gettype ()));

        // skip the job if we are stopping and the
        // skiponstop flag is set for the job type
        //
        if (!isstopping() || !data.info.skip ())
        {
            beast::thread::setcurrentthreadname (data.name ());
            m_journal.trace << "doing " << data.name () << " job";

            job::clock_type::time_point const start_time (
                job::clock_type::now());

            on_dequeue (job.gettype (), start_time - job.queue_time ());
            job.dojob ();
            on_execute (job.gettype (), job::clock_type::now() - start_time);
        }
        else
        {
            m_journal.trace << "skipping processtask ('" << data.name () << "')";
        }

        {
            scopedlock lock (m_mutex);
            finishjob (job, lock);
            --m_processcount;
            checkstopped (lock);
        }

        // note that when job::~job is called, the last reference
        // to the associated loadevent object (in the job) may be destroyed.
    }

    //------------------------------------------------------------------------------

    // returns `true` if all jobs of this type should be skipped when
    // the jobqueue receives a stop notification. if the job type isn't
    // skipped, the job will be called and the job must call job::shouldcancel
    // to determine if a long running or non-mandatory operation should be canceled.
    bool skiponstop (jobtype type)
    {
        jobtypeinfo const& j (getjobtypes ().get (type));
        assert (j.type () != jtinvalid);

        return j.skip ();
    }

    // returns the limit of running jobs for the given job type.
    // for jobs with no limit, we return the largest int. hopefully that
    // will be enough.
    //
    int getjoblimit (jobtype type)
    {
        jobtypeinfo const& j (getjobtypes ().get (type));
        assert (j.type () != jtinvalid);

        return j.limit ();
    }

    //--------------------------------------------------------------------------

    void onstop ()
    {
        // vfalco note i wanted to remove all the jobs that are skippable
        //             but then the workers count of tasks to process
        //             goes wrong.

        /*
        {
            scopedlock lock (m_mutex);

            // remove all jobs whose type is skiponstop
            typedef hash_map <jobtype, std::size_t> jobdatamap;
            jobdatamap counts;
            bool const report (m_journal.debug.active());

            for (jobset::const_iterator iter (m_jobset.begin());
                iter != m_jobset.end();)
            {
                if (skiponstop (iter->gettype()))
                {
                    if (report)
                    {
                        std::pair <jobdatamap::iterator, bool> result (
                            counts.insert (std::make_pair (iter->gettype(), 1)));
                        if (! result.second)
                            ++(result.first->second);
                    }

                    iter = m_jobset.erase (iter);
                }
                else
                {
                    ++iter;
                }
            }

            if (report)
            {
                beast::journal::scopedstream s (m_journal.debug);

                for (jobdatamap::const_iterator iter (counts.begin());
                    iter != counts.end(); ++iter)
                {
                    s << std::endl <<
                        "removed " << iter->second <<
                        " skiponstop jobs of type " << job::tostring (iter->first);
                }
            }
        }
        */
    }

    void onchildrenstopped ()
    {
        scopedlock lock (m_mutex);

        checkstopped (lock);
    }
};

//------------------------------------------------------------------------------

jobqueue::jobqueue (char const* name, stoppable& parent)
    : stoppable (name, parent)
{
}

//------------------------------------------------------------------------------

std::unique_ptr <jobqueue> make_jobqueue (
    beast::insight::collector::ptr const& collector,
        beast::stoppable& parent, beast::journal journal)
{
    return std::make_unique <jobqueueimp> (collector, parent, journal);
}

}
