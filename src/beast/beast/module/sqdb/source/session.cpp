//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

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

    portions based on soci - the c++ database access library:

    soci: http://soci.sourceforge.net/

    this file incorporates work covered by the following copyright
    and permission notice:

    copyright (c) 2004 maciej sobczak, stephen hutton, mateusz loskot,
    pawel aleksander fedorynski, david courtney, rafal bobrowski,
    julian taylor, henning basold, ilia barahovski, denis arnaud,
    daniel lidstr鰉, matthieu kermagoret, artyom beilis, cory bennett,
    chris weed, michael davidsaver, jakub stachowski, alex ott, rainer bauer,
    martin muenstermann, philip pemberton, eli green, frederic chateau,
    artyom tonkikh, roger orr, robert massaioli, sergey nikulov,
    shridhar daithankar, s鰎en meyer-eppler, mario valesco.

    boost software license - version 1.0, august 17th, 2003

    permission is hereby granted, free of charge, to any person or organization
    obtaining a copy of the software and accompanying documentation covered by
    this license (the "software") to use, reproduce, display, distribute,
    execute, and transmit the software, and to prepare derivative works of the
    software, and to permit third-parties to whom the software is furnished to
    do so, all subject to the following:

    the copyright notices in the software and this entire statement, including
    the above license grant, this restriction and the following disclaimer,
    must be included in all copies of the software, in whole or in part, and
    all derivative works of the software, unless such copies or derivative
    works are solely in the form of machine-executable object code generated by
    a source language processor.

    the software is provided "as is", without warranty of any kind, express or
    implied, including but not limited to the warranties of merchantability,
    fitness for a particular purpose, title and non-infringement. in no event
    shall the copyright holders or anyone distributing the software be liable
    for any damages or other liability, whether in contract, tort or otherwise,
    arising from, out of or in connection with the software or the use or other
    dealings in the software.
*/
//==============================================================================

#include <cassert>

namespace beast {
namespace sqdb {

class session::sqlite3
{
public:
    sqlite3()
    {
        assert (sqlite3_threadsafe() != 0);
        sqlite3_initialize();
    }

    ~sqlite3()
    {
        sqlite3_shutdown();
    }
};

//------------------------------------------------------------------------------

session::session()
    : prepare (this)
    , m_instance (sharedsingleton <sqlite3>::getinstance ())
    , m_bintransaction (false)
    , m_connection (nullptr)
{
}

session::session(const session& deferredclone)
    : prepare (this)
    , m_instance (sharedsingleton <sqlite3>::getinstance ())
    , m_bintransaction (false)
    , m_connection (nullptr)
    , m_filename (deferredclone.m_filename)
    , m_connectstring (deferredclone.m_connectstring)
{
    // shouldn't be needed since deferredclone did it
    //sqlite::initialize();
}

session::~session()
{
    close();
}

error session::clone()
{
    assert (! m_connection);

    return open(m_filename, m_connectstring);
}

error session::open(string filename, std::string options)
{
    error err;

    // can't open twice
    assert (! m_connection);

    int mode = 0;
    int flags = 0;
    int timeout = 0;

    std::stringstream ssconn(options);

    while (!err && !ssconn.eof() &&
           ssconn.str().find('=') != std::string::npos)
    {
        std::string key, val;
        std::getline(ssconn, key, '=');
        std::getline(ssconn, val, '|');

        if ("timeout" == key)
        {
            if ("infinite" == val)
            {
                //timeout = -1;
                timeout = 0x7fffffff;
            }
            else
            {
                std::istringstream converter(val);
                converter >> timeout;

                if (timeout < 1)
                    timeout = 1;
            }
        }
        else if ("mode" == key)
        {
            if (!(mode & (sqlite_open_readonly | sqlite_open_readwrite | sqlite_open_create)))
            {
                if ("read" == val)
                {
                    mode = sqlite_open_readonly;
                }
                else if ("write" == val)
                {
                    mode = sqlite_open_readwrite;
                }
                else if ("create" == val)
                {
                    mode = sqlite_open_readwrite | sqlite_open_create;
                }
                else
                {
                    throw std::invalid_argument ("bad parameter");
                }
            }
            else
            {
                throw std::invalid_argument ("duplicate parameter");
            }
        }

        /* most native sqlite libraries don't have these experimental features.
        */
#if ! vf_use_native_sqlite
        else if ("cache" == key)
        {
            if (!(flags & (sqlite_open_sharedcache | sqlite_open_privatecache)))
            {
                if ("shared" == val)
                {
                    flags |= sqlite_open_sharedcache;
                }
                else if ("private" == val)
                {
                    flags |= sqlite_open_privatecache;
                }
                else
                {
                    throw std::invalid_argument ("bad parameter");
                }
            }
            else
            {
                throw std::invalid_argument ("duplicate parameter");
            }
        }

#endif
        else if ("threads" == key)
        {
            if (!(flags & (sqlite_open_nomutex | sqlite_open_fullmutex)))
            {
                if ("single" == val)
                {
                    flags |= sqlite_open_fullmutex;
                }
                else if ("multi" == val)
                {
                    flags |= sqlite_open_nomutex;
                }
                else
                {
                    throw std::invalid_argument ("bad parameter");
                }
            }
            else
            {
                throw std::invalid_argument ("duplicate parameter");
            }
        }
        else
        {
            throw std::invalid_argument ("unknown parameter");
        }
    }

    if (!err)
    {
        if (! mode)
            mode = sqlite_open_readwrite | sqlite_open_create;

        flags |= mode;

        err = detail::sqliteerror(__file__, __line__,
                                  sqlite3_open_v2(filename.toutf8(), &m_connection, flags, 0));

        if (!err)
        {
            if (timeout > 0)
            {
                err = detail::sqliteerror(__file__, __line__,
                                          sqlite3_busy_timeout(m_connection, timeout));
            }

            /*
            else
            {
              err = detail::sqliteerror (__file__, __line__,
                sqlite3_busy_handler(m_connection, infinitebusyhandler, 0));
            }
            */
        }

        if (!err)
        {
            m_filename = filename;
            m_connectstring = options;
        }

        if (err)
        {
            close();
        }
    }

    return err;
}

void session::close()
{
    if (m_connection)
    {
        sqlite3_close(m_connection);
        m_connection = 0;
        m_filename = string::empty;
        m_connectstring = "";
    }
}

void session::begin()
{
    bassert(!m_bintransaction);
    m_bintransaction = true;

    //error error = hard_exec("begin exclusive");
    error error = hard_exec("begin");

    if (error)
        throw error;
}

error session::commit()
{
    bassert(m_bintransaction);
    m_bintransaction = false;
    return hard_exec("commit");
}

void session::rollback()
{
    bassert(m_bintransaction);
    m_bintransaction = false;
    error error = hard_exec("rollback");

    if (error)
        throw error;
}

detail::once_type session::once(error& error)
{
    return detail::once_type(this, error);
}

rowid session::last_insert_rowid()
{
    return sqlite3_last_insert_rowid(m_connection);
}

std::ostringstream& session::get_query_stream()
{
    return m_query_stream;
}

void session::log_query(std::string const& query)
{
    // todo
}

void session::set_got_data(bool bgotdata)
{
    m_bgotdata = bgotdata;
}

bool session::got_data() const
{
    return m_bgotdata;
}

error session::hard_exec(std::string const& query)
{
    error error;
    sqlite3_stmt* stmt;
    char const* tail = 0;

    int result = sqlite3_prepare_v2(
                     m_connection,
                     query.c_str(),
                     static_cast<int>(query.size()),
                     &stmt,
                     &tail);

    if (result == sqlite_ok)
    {
        result = sqlite3_step(stmt);

        sqlite3_finalize(stmt);
    }

    if (result != sqlite_done)
        error = detail::sqliteerror(__file__, __line__, result);

    return error;
}

} // sqdb
} // beast