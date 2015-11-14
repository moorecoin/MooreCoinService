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
#include <ripple/nodestore/impl/decodedblob.h>
#include <ripple/nodestore/impl/encodedblob.h>
#include <beast/threads/thread.h>
#include <atomic>
#include <beast/cxx14/memory.h> // <memory>

namespace ripple {
namespace nodestore {

class rocksdbquickenv : public rocksdb::envwrapper
{
public:
    rocksdbquickenv ()
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
        envwrapper::startthread (&rocksdbquickenv::thread_entry, p);
    }
};

//------------------------------------------------------------------------------

class rocksdbquickbackend
    : public backend
{
private:
    std::atomic <bool> m_deletepath;

public:
    beast::journal m_journal;
    size_t const m_keybytes;
    std::string m_name;
    std::unique_ptr <rocksdb::db> m_db;

    rocksdbquickbackend (int keybytes, parameters const& keyvalues,
        scheduler& scheduler, beast::journal journal, rocksdbquickenv* env)
        : m_deletepath (false)
        , m_journal (journal)
        , m_keybytes (keybytes)
        , m_name (keyvalues ["path"].tostdstring ())
    {
        if (m_name.empty())
            throw std::runtime_error ("missing path in rocksdbquickfactory backend");

        // defaults
        std::uint64_t budget = 512 * 1024 * 1024;  // 512mb
        std::string style("level");
        std::uint64_t threads=4;

        if (!keyvalues["budget"].isempty())
            budget = keyvalues["budget"].getintvalue();

        if (!keyvalues["style"].isempty())
            style = keyvalues["style"].tostdstring();

        if (!keyvalues["threads"].isempty())
            threads = keyvalues["threads"].getintvalue();


        // set options
        rocksdb::options options;
        options.create_if_missing = true;
        options.env = env;

        if (style == "level")
            options.optimizelevelstylecompaction(budget);

        if (style == "universal")
            options.optimizeuniversalstylecompaction(budget);

        if (style == "point")
            options.optimizeforpointlookup(budget / 1024 / 1024);  // in mb

        options.increaseparallelism(threads);

        // allows hash indexes in blocks
        options.prefix_extractor.reset(rocksdb::newnooptransform());

        // overrride optimizelevelstylecompaction
        options.min_write_buffer_number_to_merge = 1;
        
        rocksdb::blockbasedtableoptions table_options;
        // use hash index
        table_options.index_type =
            rocksdb::blockbasedtableoptions::khashsearch;
        table_options.filter_policy.reset(
            rocksdb::newbloomfilterpolicy(10));
        options.table_factory.reset(
            newblockbasedtablefactory(table_options));
        
        // higher values make reads slower
        // table_options.block_size = 4096;

        // no point when databaseimp has a cache
        // table_options.block_cache =
        //     rocksdb::newlrucache(64 * 1024 * 1024);

        options.memtable_factory.reset(rocksdb::newhashskiplistrepfactory());
        // alternative:
        // options.memtable_factory.reset(
        //     rocksdb::newhashcuckoorepfactory(options.write_buffer_size));

        if (! keyvalues["open_files"].isempty())
        {
            options.max_open_files = keyvalues["open_files"].getintvalue();
        }
 
        if (! keyvalues["compression"].isempty ())  
        {
            if (keyvalues["compression"].getintvalue () == 0)
            {
                options.compression = rocksdb::knocompression;
            }
        }

        rocksdb::db* db = nullptr;

        rocksdb::status status = rocksdb::db::open (options, m_name, &db);
        if (!status.ok () || !db)
            throw std::runtime_error (std::string("unable to open/create rocksdbquick: ") + status.tostring());

        m_db.reset (db);
    }

    ~rocksdbquickbackend ()
    {
        close();
    }

    std::string
    getname()
    {
        return m_name;
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
        storebatch(batch{object});
    }

    void
    storebatch (batch const& batch)
    {
        rocksdb::writebatch wb;
 
        encodedblob encoded;

        for (auto const& e : batch)
        {
            encoded.prepare (e);

            wb.put(
                rocksdb::slice(reinterpret_cast<char const*>(encoded.getkey()),
                               m_keybytes),
                rocksdb::slice(reinterpret_cast<char const*>(encoded.getdata()),
                               encoded.getsize()));
        }

        rocksdb::writeoptions options;

        // crucial to ensure good write speed and non-blocking writes to memtable
        options.disablewal = true;
        
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
        return 0;
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

class rocksdbquickfactory : public factory
{
public:
    rocksdbquickenv m_env;

    rocksdbquickfactory()
    {
        manager::instance().insert(*this);
    }

    ~rocksdbquickfactory()
    {
        manager::instance().erase(*this);
    }

    std::string
    getname () const
    {
        return "rocksdbquick";
    }

    std::unique_ptr <backend>
    createinstance (
        size_t keybytes,
        parameters const& keyvalues,
        scheduler& scheduler,
        beast::journal journal)
    {
        return std::make_unique <rocksdbquickbackend> (
            keybytes, keyvalues, scheduler, journal, &m_env);
    }
};

static rocksdbquickfactory rocksdbquickfactory;

}
}

#endif
