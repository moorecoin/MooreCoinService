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

#include <ripple/unity/rocksdb.h>

#if ripple_rocksdb_available

#include <ripple/core/config.h> // vfalco bad dependency
#include <ripple/nodestore/factory.h>
#include <ripple/nodestore/manager.h>
#include <ripple/nodestore/impl/batchwriter.h>
#include <ripple/nodestore/impl/decodedblob.h>
#include <ripple/nodestore/impl/encodedblob.h>
#include <beast/threads/thread.h>
#include <atomic>
#include <beast/cxx14/memory.h> // <memory>

namespace ripple {
namespace nodestore {

class rocksdbenv : public rocksdb::envwrapper
{
public:
    rocksdbenv ()
        : envwrapper (rocksdb::env::default())
    {
    }

    struct threadparams
    {
        threadparams (void (*f_)(void*), void* a_)
            : f (f_)
            , a (a_)
        {
        }

        void (*f)(void*);
        void* a;
    };

    static
    void
    thread_entry (void* ptr)
    {
        threadparams* const p (reinterpret_cast <threadparams*> (ptr));
        void (*f)(void*) = p->f;
        void* a (p->a);
        delete p;

        static std::atomic <std::size_t> n;
        std::size_t const id (++n);
        std::stringstream ss;
        ss << "rocksdb #" << id;
        beast::thread::setcurrentthreadname (ss.str());

        (*f)(a);
    }

    void
    startthread (void (*f)(void*), void* a)
    {
        threadparams* const p (new threadparams (f, a));
        envwrapper::startthread (&rocksdbenv::thread_entry, p);
    }
};

//------------------------------------------------------------------------------

class rocksdbbackend
    : public backend
    , public batchwriter::callback
{
private:
    std::atomic <bool> m_deletepath;

public:
    beast::journal m_journal;
    size_t const m_keybytes;
    scheduler& m_scheduler;
    batchwriter m_batch;
    std::string m_name;
    std::unique_ptr <rocksdb::db> m_db;

    rocksdbbackend (int keybytes, parameters const& keyvalues,
        scheduler& scheduler, beast::journal journal, rocksdbenv* env)
        : m_deletepath (false)
        , m_journal (journal)
        , m_keybytes (keybytes)
        , m_scheduler (scheduler)
        , m_batch (*this, scheduler)
        , m_name (keyvalues ["path"].tostdstring ())
    {
        if (m_name.empty())
            throw std::runtime_error ("missing path in rocksdbfactory backend");

        rocksdb::options options;
        rocksdb::blockbasedtableoptions table_options;
        options.create_if_missing = true;
        options.env = env;

        if (keyvalues["cache_mb"].isempty())
        {
            table_options.block_cache = rocksdb::newlrucache (getconfig ().getsize (sihashnodedbcache) * 1024 * 1024);
        }
        else
        {
            table_options.block_cache = rocksdb::newlrucache (keyvalues["cache_mb"].getintvalue() * 1024l * 1024l);
        }

        if (keyvalues["filter_bits"].isempty())
        {
            if (getconfig ().node_size >= 2)
                table_options.filter_policy.reset (rocksdb::newbloomfilterpolicy (10));
        }
        else if (keyvalues["filter_bits"].getintvalue() != 0)
        {
            table_options.filter_policy.reset (rocksdb::newbloomfilterpolicy (keyvalues["filter_bits"].getintvalue()));
        }

        if (! keyvalues["open_files"].isempty())
        {
            options.max_open_files = keyvalues["open_files"].getintvalue();
        }

        if (! keyvalues["file_size_mb"].isempty())
        {
            options.target_file_size_base = 1024 * 1024 * keyvalues["file_size_mb"].getintvalue();
            options.max_bytes_for_level_base = 5 * options.target_file_size_base;
            options.write_buffer_size = 2 * options.target_file_size_base;
        }

        if (! keyvalues["file_size_mult"].isempty())
        {
            options.target_file_size_multiplier = keyvalues["file_size_mult"].getintvalue();
        }

        if (! keyvalues["bg_threads"].isempty())
        {
            options.env->setbackgroundthreads
                (keyvalues["bg_threads"].getintvalue(), rocksdb::env::low);
        }

        if (! keyvalues["high_threads"].isempty())
        {
            auto const highthreads = keyvalues["high_threads"].getintvalue();
            options.env->setbackgroundthreads (highthreads, rocksdb::env::high);

            // if we have high-priority threads, presumably we want to
            // use them for background flushes
            if (highthreads > 0)
                options.max_background_flushes = highthreads;
        }

        if (! keyvalues["compression"].isempty ())
        {
            if (keyvalues["compression"].getintvalue () == 0)
            {
                options.compression = rocksdb::knocompression;
            }
        }

        if (! keyvalues["block_size"].isempty ())
        {
            table_options.block_size = keyvalues["block_size"].getintvalue ();
        }

        if (! keyvalues["universal_compaction"].isempty ())
        {
            if (keyvalues["universal_compaction"].getintvalue () != 0)
            {
                options.compaction_style = rocksdb:: kcompactionstyleuniversal;
                options.min_write_buffer_number_to_merge = 2;
                options.max_write_buffer_number = 6;
                options.write_buffer_size = 6 * options.target_file_size_base;
            }
        }

        options.table_factory.reset(newblockbasedtablefactory(table_options));

        rocksdb::db* db = nullptr;
        rocksdb::status status = rocksdb::db::open (options, m_name, &db);
        if (!status.ok () || !db)
            throw std::runtime_error (std::string("unable to open/create rocksdb: ") + status.tostring());

        m_db.reset (db);
    }

    ~rocksdbbackend ()
    {
        close();
    }

    void
    close() override
    {
        if (m_db)
        {
            m_db.reset();
            if (m_deletepath)
            {
                boost::filesystem::path dir = m_name;
                boost::filesystem::remove_all (dir);
            }
        }
    }

    std::string
    getname()
    {
        return m_name;
    }

    //--------------------------------------------------------------------------

    status
    fetch (void const* key, nodeobject::ptr* pobject)
    {
        pobject->reset ();

        status status (ok);

        rocksdb::readoptions const options;
        rocksdb::slice const slice (static_cast <char const*> (key), m_keybytes);

        std::string string;

        rocksdb::status getstatus = m_db->get (options, slice, &string);

        if (getstatus.ok ())
        {
            decodedblob decoded (key, string.data (), string.size ());

            if (decoded.wasok ())
            {
                *pobject = decoded.createobject ();
            }
            else
            {
                // decoding failed, probably corrupted!
                //
                status = datacorrupt;
            }
        }
        else
        {
            if (getstatus.iscorruption ())
            {
                status = datacorrupt;
            }
            else if (getstatus.isnotfound ())
            {
                status = notfound;
            }
            else
            {
                status = status (customcode + getstatus.code());

                m_journal.error << getstatus.tostring ();
            }
        }

        return status;
    }

    void
    store (nodeobject::ref object)
    {
        m_batch.store (object);
    }

    void
    storebatch (batch const& batch)
    {
        rocksdb::writebatch wb;

        encodedblob encoded;

        for (auto const& e : batch)
        {
            encoded.prepare (e);

            wb.put (
                rocksdb::slice (reinterpret_cast <char const*> (
                    encoded.getkey ()), m_keybytes),
                rocksdb::slice (reinterpret_cast <char const*> (
                    encoded.getdata ()), encoded.getsize ()));
        }

        rocksdb::writeoptions const options;

        auto ret = m_db->write (options, &wb);

        if (!ret.ok ())
            throw std::runtime_error ("storebatch failed: " + ret.tostring());
    }

    void
    for_each (std::function <void(nodeobject::ptr)> f)
    {
        rocksdb::readoptions const options;

        std::unique_ptr <rocksdb::iterator> it (m_db->newiterator (options));

        for (it->seektofirst (); it->valid (); it->next ())
        {
            if (it->key ().size () == m_keybytes)
            {
                decodedblob decoded (it->key ().data (),
                                                it->value ().data (),
                                                it->value ().size ());

                if (decoded.wasok ())
                {
                    f (decoded.createobject ());
                }
                else
                {
                    // uh oh, corrupted data!
                    if (m_journal.fatal) m_journal.fatal <<
                        "corrupt nodeobject #" << uint256 (it->key ().data ());
                }
            }
            else
            {
                // vfalco note what does it mean to find an
                //             incorrectly sized key? corruption?
                if (m_journal.fatal) m_journal.fatal <<
                    "bad key size = " << it->key ().size ();
            }
        }
    }

    int
    getwriteload ()
    {
        return m_batch.getwriteload ();
    }

    void
    setdeletepath() override
    {
        m_deletepath = true;
    }

    //--------------------------------------------------------------------------

    void
    writebatch (batch const& batch)
    {
        storebatch (batch);
    }

    void
    verify() override
    {
    }
};

//------------------------------------------------------------------------------

class rocksdbfactory : public factory
{
public:
    rocksdbenv m_env;

    rocksdbfactory ()
    {
        manager::instance().insert(*this);
    }

    ~rocksdbfactory ()
    {
        manager::instance().erase(*this);
    }

    std::string
    getname () const
    {
        return "rocksdb";
    }

    std::unique_ptr <backend>
    createinstance (
        size_t keybytes,
        parameters const& keyvalues,
        scheduler& scheduler,
        beast::journal journal)
    {
        return std::make_unique <rocksdbbackend> (
            keybytes, keyvalues, scheduler, journal, &m_env);
    }
};

static rocksdbfactory rocksdbfactory;

}
}

#endif
