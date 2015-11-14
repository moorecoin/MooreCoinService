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

#ifndef ripple_peerfinder_storesqdb_h_included
#define ripple_peerfinder_storesqdb_h_included

#include <beast/module/sqdb/sqdb.h>
#include <beast/utility/debug.h>

namespace ripple {
namespace peerfinder {

/** database persistence for peerfinder using sqlite */
class storesqdb
    : public store
    , public beast::leakchecked <storesqdb>
{
private:
    beast::journal m_journal;
    beast::sqdb::session m_session;

public:
    enum
    {
        // this determines the on-database format of the data
        currentschemaversion = 4
    };

    explicit storesqdb (beast::journal journal = beast::journal())
        : m_journal (journal)
    {
    }

    ~storesqdb ()
    {
    }

    beast::error open (beast::file const& file)
    {
        beast::error error (m_session.open (file.getfullpathname ()));

        m_journal.info << "opening database at '" << file.getfullpathname() << "'";

        if (! error)
            error = init ();

        if (! error)
            error = update ();

        return error;
    }

    // loads the bootstrap cache, calling the callback for each entry
    //
    std::size_t load (load_callback const& cb)
    {
        std::size_t n (0);
        beast::error error;
        std::string s;
        int valence;
        beast::sqdb::statement st = (m_session.prepare <<
            "select "
            " address, "
            " valence "
            "from peerfinder_bootstrapcache "
            , beast::sqdb::into (s)
            , beast::sqdb::into (valence)
            );

        if (st.execute_and_fetch (error))
        {
            do
            {
                beast::ip::endpoint const endpoint (
                    beast::ip::endpoint::from_string (s));

                if (! is_unspecified (endpoint))
                {
                    cb (endpoint, valence);
                    ++n;
                }
                else
                {
                    m_journal.error <<
                        "bad address string '" << s << "' in bootcache table";
                }
            }
            while (st.fetch (error));
        }

        if (error)
            report (error, __file__, __line__);

        return n;
    }

    // overwrites the stored bootstrap cache with the specified array.
    //
    void save (std::vector <entry> const& v)
    {
        beast::error error;
        beast::sqdb::transaction tr (m_session);
        m_session.once (error) <<
            "delete from peerfinder_bootstrapcache";
        if (! error)
        {
            std::string s;
            int valence;

            beast::sqdb::statement st = (m_session.prepare <<
                "insert into peerfinder_bootstrapcache ( "
                "  address, "
                "  valence "
                ") values ( "
                "  ?, ? "
                ");"
                , beast::sqdb::use (s)
                , beast::sqdb::use (valence)
                );

            for (auto const& e : v)
            {
                s = to_string (e.endpoint);
                valence = e.valence;
                st.execute_and_fetch (error);
                if (error)
                    break;
            }
        }

        if (! error)
            error = tr.commit();

        if (error)
        {
            tr.rollback ();
            report (error, __file__, __line__);
        }
    }

    // convert any existing entries from an older schema to the
    // current one, if appropriate.
    //
    beast::error update ()
    {
        beast::error error;

        beast::sqdb::transaction tr (m_session);

        // get version
        int version (0);
        if (! error)
        {
            m_session.once (error) <<
                "select "
                "  version "
                "from schemaversion where "
                "  name = 'peerfinder'"
                ,beast::sqdb::into (version)
                ;

            if (! error)
            {
                if (!m_session.got_data())
                    version = 0;

                m_journal.info <<
                    "opened version " << version << " database";
            }
        }

        if (!error)
        {
            if (version < currentschemaversion)
                m_journal.info <<
                    "updating database to version " << currentschemaversion;
            else if (version > currentschemaversion)
                error.fail (__file__, __line__,
                    "the peerfinder database version is higher than expected");
        }

        if (! error && version < 4)
        {
            //
            // remove the "uptime" column from the bootstrap table
            //

            if (! error)
                m_session.once (error) <<
                    "create table if not exists peerfinder_bootstrapcache_next ( "
                    "  id       integer primary key autoincrement, "
                    "  address  text unique not null, "
                    "  valence  integer"
                    ");"
                    ;

            if (! error)
                m_session.once (error) <<
                    "create index if not exists "
                    "  peerfinder_bootstrapcache_next_index on "
                    "    peerfinder_bootstrapcache_next "
                    "  ( address ); "
                    ;

            std::size_t count;
            if (! error)
                m_session.once (error) <<
                    "select count(*) from peerfinder_bootstrapcache "
                    ,beast::sqdb::into (count)
                    ;

            std::vector <store::entry> list;

            if (! error)
            {
                list.reserve (count);
                std::string s;
                int valence;
                beast::sqdb::statement st = (m_session.prepare <<
                    "select "
                    " address, "
                    " valence "
                    "from peerfinder_bootstrapcache "
                    , beast::sqdb::into (s)
                    , beast::sqdb::into (valence)
                    );

                if (st.execute_and_fetch (error))
                {
                    do
                    {
                        store::entry entry;
                        entry.endpoint = beast::ip::endpoint::from_string (s);
                        if (! is_unspecified (entry.endpoint))
                        {
                            entry.valence = valence;
                            list.push_back (entry);
                        }
                        else
                        {
                            m_journal.error <<
                                "bad address string '" << s << "' in bootcache table";
                        }
                    }
                    while (st.fetch (error));
                }
            }

            if (! error)
            {
                std::string s;
                int valence;
                beast::sqdb::statement st = (m_session.prepare <<
                    "insert into peerfinder_bootstrapcache_next ( "
                    "  address, "
                    "  valence "
                    ") values ( "
                    "  ?, ?"
                    ");"
                    , beast::sqdb::use (s)
                    , beast::sqdb::use (valence)
                    );

                for (auto iter (list.cbegin());
                    !error && iter != list.cend(); ++iter)
                {
                    s = to_string (iter->endpoint);
                    valence = iter->valence;
                    st.execute_and_fetch (error);
                }

            }

            if (! error)
                m_session.once (error) <<
                    "drop table if exists peerfinder_bootstrapcache";

            if (! error)
                m_session.once (error) <<
                    "drop index if exists peerfinder_bootstrapcache_index";

            if (! error)
                m_session.once (error) <<
                    "alter table peerfinder_bootstrapcache_next "
                    "  rename to peerfinder_bootstrapcache";

            if (! error)
                m_session.once (error) <<
                    "create index if not exists "
                    "  peerfinder_bootstrapcache_index on peerfinder_bootstrapcache "
                    "  (  "
                    "    address "
                    "  ); "
                    ;
        }

        if (! error && version < 3)
        {
            //
            // remove legacy endpoints from the schema
            //

            if (! error)
                m_session.once (error) <<
                    "drop table if exists legacyendpoints";

            if (! error)
                m_session.once (error) <<
                    "drop table if exists peerfinderlegacyendpoints";

            if (! error)
                m_session.once (error) <<
                    "drop table if exists peerfinder_legacyendpoints";

            if (! error)
                m_session.once (error) <<
                    "drop table if exists peerfinder_legacyendpoints_index";
        }

        if (! error)
        {
            int const version (currentschemaversion);
            m_session.once (error) <<
                "insert or replace into schemaversion ("
                "   name "
                "  ,version "
                ") values ( "
                "  'peerfinder', ? "
                ")"
                ,beast::sqdb::use(version);
        }

        if (! error)
            error = tr.commit();

        if (error)
        {
            tr.rollback();
            report (error, __file__, __line__);
        }

        return error;
    }

private:
    beast::error init ()
    {
        beast::error error;
        beast::sqdb::transaction tr (m_session);

        if (! error)
            m_session.once (error) <<
                "pragma encoding=\"utf-8\"";

        if (! error)
            m_session.once (error) <<
                "create table if not exists schemaversion ( "
                "  name             text primary key, "
                "  version          integer"
                ");"
                ;

        if (! error)
            m_session.once (error) <<
                "create table if not exists peerfinder_bootstrapcache ( "
                "  id       integer primary key autoincrement, "
                "  address  text unique not null, "
                "  valence  integer"
                ");"
                ;

        if (! error)
            m_session.once (error) <<
                "create index if not exists "
                "  peerfinder_bootstrapcache_index on peerfinder_bootstrapcache "
                "  (  "
                "    address "
                "  ); "
                ;

        if (! error)
            error = tr.commit();

        if (error)
        {
            tr.rollback ();
            report (error, __file__, __line__);
        }

        return error;
    }

    void report (beast::error const& error, char const* filename, int linenumber)
    {
        if (error)
        {
            m_journal.error <<
                "failure: '"<< error.getreasontext() << "' " <<
                " at " << beast::debug::getsourcelocation (filename, linenumber);
        }
    }
};

}
}

#endif
