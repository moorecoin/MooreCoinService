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

#ifndef ripple_nodestore_databaseimp_h_included
#define ripple_nodestore_databaseimp_h_included

#include <ripple/nodestore/database.h>
#include <ripple/nodestore/scheduler.h>
#include <ripple/nodestore/impl/tuning.h>
#include <ripple/basics/taggedcache.h>
#include <ripple/basics/keycache.h>
#include <ripple/basics/log.h>
#include <ripple/basics/seconds_clock.h>
#include <beast/threads/thread.h>
#include <chrono>
#include <condition_variable>
#include <set>
#include <thread>

namespace ripple {
namespace nodestore {

class databaseimp
    : public database
    , public beast::leakchecked <databaseimp>
{
public:
    beast::journal m_journal;
    scheduler& m_scheduler;
    // persistent key/value storage.
    std::unique_ptr <backend> m_backend;
    // larger key/value storage, but not necessarily persistent.
    std::unique_ptr <backend> m_fastbackend;

    // positive cache
    taggedcache <uint256, nodeobject> m_cache;

    // negative cache
    keycache <uint256> m_negcache;

    std::mutex                m_readlock;
    std::condition_variable   m_readcondvar;
    std::condition_variable   m_readgencondvar;
    std::set <uint256>        m_readset;        // set of reads to do
    uint256                   m_readlast;       // last hash read
    std::vector <std::thread> m_readthreads;
    bool                      m_readshut;
    uint64_t                  m_readgen;        // current read generation

    databaseimp (std::string const& name,
                 scheduler& scheduler,
                 int readthreads,
                 std::unique_ptr <backend> backend,
                 std::unique_ptr <backend> fastbackend,
                 beast::journal journal)
        : m_journal (journal)
        , m_scheduler (scheduler)
        , m_backend (std::move (backend))
        , m_fastbackend (std::move (fastbackend))
        , m_cache ("nodestore", cachetargetsize, cachetargetseconds,
            get_seconds_clock (), deprecatedlogs().journal("taggedcache"))
        , m_negcache ("nodestore", get_seconds_clock (),
            cachetargetsize, cachetargetseconds)
        , m_readshut (false)
        , m_readgen (0)
        , m_storecount (0)
        , m_fetchtotalcount (0)
        , m_fetchhitcount (0)
        , m_storesize (0)
        , m_fetchsize (0)
    {
        for (int i = 0; i < readthreads; ++i)
            m_readthreads.push_back (std::thread (&databaseimp::threadentry,
                    this));
    }

    ~databaseimp ()
    {
        {
            std::unique_lock <std::mutex> lock (m_readlock);
            m_readshut = true;
            m_readcondvar.notify_all ();
            m_readgencondvar.notify_all ();
        }

        for (auto& e : m_readthreads)
            e.join();
    }

    std::string
    getname () const override
    {
        return m_backend->getname ();
    }

    void
    close() override
    {
        if (m_backend)
        {
            m_backend->close();
            m_backend = nullptr;
        }
        if (m_fastbackend)
        {
            m_fastbackend->close();
            m_fastbackend = nullptr;
        }
    }

    //------------------------------------------------------------------------------

    bool asyncfetch (uint256 const& hash, nodeobject::pointer& object)
    {
        // see if the object is in cache
        object = m_cache.fetch (hash);
        if (object || m_negcache.touch_if_exists (hash))
            return true;

        {
            // no. post a read
            std::unique_lock <std::mutex> lock (m_readlock);
            if (m_readset.insert (hash).second)
                m_readcondvar.notify_one ();
        }

        return false;
    }

    void waitreads ()
    {
        {
            std::unique_lock <std::mutex> lock (m_readlock);

            // wake in two generations
            std::uint64_t const wakegeneration = m_readgen + 2;

            while (!m_readshut && !m_readset.empty () && (m_readgen < wakegeneration))
                m_readgencondvar.wait (lock);
        }

    }

    int getdesiredasyncreadcount ()
    {
        // we prefer a client not fill our cache
        // we don't want to push data out of the cache
        // before it's retrieved
        return m_cache.gettargetsize() / asyncdivider;
    }

    nodeobject::ptr fetch (uint256 const& hash) override
    {
        return dotimedfetch (hash, false);
    }

    /** perform a fetch and report the time it took */
    nodeobject::ptr dotimedfetch (uint256 const& hash, bool isasync)
    {
        fetchreport report;
        report.isasync = isasync;
        report.wenttodisk = false;

        auto const before = std::chrono::steady_clock::now();
        nodeobject::ptr ret = dofetch (hash, report);
        report.elapsed = std::chrono::duration_cast <std::chrono::milliseconds>
            (std::chrono::steady_clock::now() - before);

        report.wasfound = (ret != nullptr);
        m_scheduler.onfetch (report);

        return ret;
    }

    nodeobject::ptr dofetch (uint256 const& hash, fetchreport &report)
    {
        // see if the object already exists in the cache
        //
        nodeobject::ptr obj = m_cache.fetch (hash);

        if (obj != nullptr)
            return obj;

        if (m_negcache.touch_if_exists (hash))
            return obj;

        // check the database(s).

        bool foundinfastbackend = false;
        report.wenttodisk = true;

        // check the fast backend database if we have one
        //
        if (m_fastbackend != nullptr)
        {
            obj = fetchinternal (*m_fastbackend, hash);

            // if we found the object, avoid storing it again later.
            if (obj != nullptr)
                foundinfastbackend = true;
        }

        // are we still without an object?
        //
        if (obj == nullptr)
        {
            // yes so at last we will try the main database.
            //
            obj = fetchfrom (hash);
            ++m_fetchtotalcount;
        }

        if (obj == nullptr)
        {

            // just in case a write occurred
            obj = m_cache.fetch (hash);

            if (obj == nullptr)
            {
                // we give up
                m_negcache.insert (hash);
            }
        }
        else
        {
            // ensure all threads get the same object
            //
            m_cache.canonicalize (hash, obj);

            if (! foundinfastbackend)
            {
                // if we have a fast back end, store it there for later.
                //
                if (m_fastbackend != nullptr)
                {
                    m_fastbackend->store (obj);
                    ++m_storecount;
                    if (obj)
                        m_storesize += obj->getdata().size();
                }

                // since this was a 'hard' fetch, we will log it.
                //
                if (m_journal.trace) m_journal.trace <<
                    "hos: " << hash << " fetch: in db";
            }
        }

        return obj;
    }

    virtual nodeobject::ptr fetchfrom (uint256 const& hash)
    {
        return fetchinternal (*m_backend, hash);
    }

    nodeobject::ptr fetchinternal (backend& backend,
        uint256 const& hash)
    {
        nodeobject::ptr object;

        status const status = backend.fetch (hash.begin (), &object);

        switch (status)
        {
        case ok:
            ++m_fetchhitcount;
            if (object)
                m_fetchsize += object->getdata().size();
        case notfound:
            break;

        case datacorrupt:
            // vfalco todo deal with encountering corrupt data!
            //
            if (m_journal.fatal) m_journal.fatal <<
                "corrupt nodeobject #" << hash;
            break;

        default:
            if (m_journal.warning) m_journal.warning <<
                "unknown status=" << status;
            break;
        }

        return object;
    }

    //------------------------------------------------------------------------------

    void store (nodeobjecttype type,
                blob&& data,
                uint256 const& hash) override
    {
        storeinternal (type, std::move(data), hash, *m_backend.get());
    }

    void storeinternal (nodeobjecttype type,
                        blob&& data,
                        uint256 const& hash,
                        backend& backend)
    {
        nodeobject::ptr object = nodeobject::createobject(
            type, std::move(data), hash);

        #if ripple_verify_nodeobject_keys
        assert (hash == serializer::getsha512half (data));
        #endif

        m_cache.canonicalize (hash, object, true);

        backend.store (object);
        ++m_storecount;
        if (object)
            m_storesize += object->getdata().size();

        m_negcache.erase (hash);

        if (m_fastbackend)
        {
            m_fastbackend->store (object);
            ++m_storecount;
            if (object)
                m_storesize += object->getdata().size();
        }
    }

    //------------------------------------------------------------------------------

    float getcachehitrate ()
    {
        return m_cache.gethitrate ();
    }

    void tune (int size, int age)
    {
        m_cache.settargetsize (size);
        m_cache.settargetage (age);
        m_negcache.settargetsize (size);
        m_negcache.settargetage (age);
    }

    void sweep ()
    {
        m_cache.sweep ();
        m_negcache.sweep ();
    }

    std::int32_t getwriteload() const override
    {
        return m_backend->getwriteload();
    }

    //------------------------------------------------------------------------------

    // entry point for async read threads
    void threadentry ()
    {
        beast::thread::setcurrentthreadname ("prefetch");
        while (1)
        {
            uint256 hash;

            {
                std::unique_lock <std::mutex> lock (m_readlock);

                while (!m_readshut && m_readset.empty ())
                {
                    // all work is done
                    m_readgencondvar.notify_all ();
                    m_readcondvar.wait (lock);
                }

                if (m_readshut)
                    break;

                // read in key order to make the back end more efficient
                std::set <uint256>::iterator it = m_readset.lower_bound (m_readlast);
                if (it == m_readset.end ())
                {
                    it = m_readset.begin ();

                    // a generation has completed
                    ++m_readgen;
                    m_readgencondvar.notify_all ();
                }

                hash = *it;
                m_readset.erase (it);
                m_readlast = hash;
            }

            // perform the read
            dotimedfetch (hash, true);
         }
     }

    //------------------------------------------------------------------------------

    void for_each (std::function <void(nodeobject::ptr)> f) override
    {
        m_backend->for_each (f);
    }

    void import (database& source)
    {
        importinternal (source, *m_backend.get());
    }

    void importinternal (database& source, backend& dest)
    {
        batch b;
        b.reserve (batchwritepreallocationsize);

        source.for_each ([&](nodeobject::ptr object)
        {
            if (b.size() >= batchwritepreallocationsize)
            {
                dest.storebatch (b);
                b.clear();
                b.reserve (batchwritepreallocationsize);
            }

            b.push_back (object);
            ++m_storecount;
            if (object)
                m_storesize += object->getdata().size();
        });

        if (! b.empty())
            dest.storebatch (b);
    }

    std::uint32_t getstorecount () const override
    {
        return m_storecount;
    }

    std::uint32_t getfetchtotalcount () const override
    {
        return m_fetchtotalcount;
    }

    std::uint32_t getfetchhitcount () const override
    {
        return m_fetchhitcount;
    }

    std::uint32_t getstoresize () const override
    {
        return m_storesize;
    }

    std::uint32_t getfetchsize () const override
    {
        return m_fetchsize;
    }

private:
    std::atomic <std::uint32_t> m_storecount;
    std::atomic <std::uint32_t> m_fetchtotalcount;
    std::atomic <std::uint32_t> m_fetchhitcount;
    std::atomic <std::uint32_t> m_storesize;
    std::atomic <std::uint32_t> m_fetchsize;
};

}
}

#endif
