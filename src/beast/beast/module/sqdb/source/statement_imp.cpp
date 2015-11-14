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

namespace beast {
namespace sqdb {
namespace detail {

/*

running a new prepared statement:

sqlite3_prepare()   // create prepared statement
sqlite3_bind()      // define input variables
sqlite3_step()      // retrieve next row
sqlite3_column()    // extract output value
sqlite3_finalize()  // destroy prepared statement

re-using an existing prepared statement

sqlite3_reset()     // reset the statement (input bindings are not cleared)
sqlite3_clear_bindings()  // set all input variables to null
  and/or
sqlite3_bind()      // define input variables
sqlite3_step()      // retrieve next row
sqlite3_column()    // extract output value

*/

statement_imp::statement_imp(session& s)
    : m_session(s)
    , m_stmt(0)
    , m_bready(false)
    , m_bgotdata(false)
    , m_last_insert_rowid(0)
{
}

statement_imp::statement_imp(prepare_temp_type const& prep)
    : m_session(prep.get_prepare_info().m_session)
    , m_stmt(0)
    , m_bready(false)
    , m_bgotdata(false)
{
    ref_counted_prepare_info& rcpi = prep.get_prepare_info();

    m_intos.swap(rcpi.m_intos);
    m_uses.swap(rcpi.m_uses);

    prepare(rcpi.get_query());
}

statement_imp::~statement_imp()
{
    clean_up();
}

void statement_imp::erase_bindings()
{
    // delete intos
    for (std::size_t i = m_intos.size(); i != 0; --i)
    {
        into_type_base* p = m_intos[i - 1];
        delete p;
        m_intos.resize(i - 1);
    }

    // delete uses
    for (std::size_t i = m_uses.size(); i != 0; --i)
    {
        use_type_base* p = m_uses[i - 1];
        p->clean_up();
        delete p;
        m_uses.resize(i - 1);
    }
}

void statement_imp::exchange(detail::into_type_ptr const& i)
{
    m_intos.push_back(i.get());
    i.release();
}

void statement_imp::exchange(detail::use_type_ptr const& u)
{
    m_uses.push_back(u.get());
    u.release();
}

void statement_imp::clean_up()
{
    erase_bindings();
    release_resources();
}

void statement_imp::prepare(std::string const& query, bool brepeatable)
{
    m_query = query;
    m_session.log_query(query);
    m_last_insert_rowid = 0;

    release_resources();

    char const* tail = 0;
    int result = sqlite3_prepare_v2(
                     m_session.get_connection(),
                     query.c_str(),
                     static_cast<int>(query.size()),
                     &m_stmt,
                     &tail);

    if (result == sqlite_ok)
    {
        m_bready = true;
    }
    else
    {
        throw detail::sqliteerror(__file__, __line__, result);
    }
}

error statement_imp::execute()
{
    error error;

    assert (m_stmt != nullptr);

    // ???
    m_bgotdata = false;
    m_session.set_got_data(m_bgotdata);

    // binds

    int icol = 0;

    for (intos_t::iterator iter = m_intos.begin(); iter != m_intos.end(); ++iter)
        (*iter)->bind(*this, icol);

    int iparam = 1;

    for (uses_t::iterator iter = m_uses.begin(); iter != m_uses.end(); ++iter)
        (*iter)->bind(*this, iparam);

    // reset
    error = detail::sqliteerror(__file__, __line__, sqlite3_reset(m_stmt));

    if (!error)
    {
        // set input variables
        do_uses();

        m_bready = true;
        m_bfirsttime = true;
    }

    return error;
}

bool statement_imp::fetch(error& error)
{
    int result = sqlite3_step(m_stmt);

    if (result == sqlite_row ||
            result == sqlite_done)
    {
        if (m_bfirsttime)
        {
            m_last_insert_rowid = m_session.last_insert_rowid();
            m_bfirsttime = false;
        }

        if (result == sqlite_row)
        {
            m_bgotdata = true;
            m_session.set_got_data(m_bgotdata);

            do_intos();
        }
        else
        {
            m_bgotdata = false;
            m_session.set_got_data(m_bgotdata);

            if (result == sqlite_done)
            {
                m_bready = false;
            }
        }
    }
    else if (result != sqlite_ok)
    {
        m_bgotdata = false;
        error = detail::sqliteerror(__file__, __line__, result);
    }
    else
    {
        // should never get sqlite_ok here
        throw std::invalid_argument ("invalid result");
    }

    return m_bgotdata;
}

bool statement_imp::got_data() const
{
    return m_bgotdata;
}

void statement_imp::do_intos()
{
    for (intos_t::iterator iter = m_intos.begin(); iter != m_intos.end(); ++iter)
        (*iter)->do_into();
}

void statement_imp::do_uses()
{
    error error = detail::sqliteerror(__file__, __line__,
                                      sqlite3_clear_bindings(m_stmt));

    if (error)
        throw error;

    for (uses_t::iterator iter = m_uses.begin(); iter != m_uses.end(); ++iter)
        (*iter)->do_use();
}

void statement_imp::post_use()
{
    // reverse order
    for (uses_t::reverse_iterator iter = m_uses.rbegin(); iter != m_uses.rend(); ++iter)
        (*iter)->post_use();
}

void statement_imp::release_resources()
{
    if (m_stmt)
    {
        sqlite3_finalize(m_stmt);
        m_stmt = 0;
    }

    m_bready = false;
    m_bgotdata = false;
}

rowid statement_imp::last_insert_rowid()
{
    return m_last_insert_rowid;
}

} // detail
} // sqdb
} // beast