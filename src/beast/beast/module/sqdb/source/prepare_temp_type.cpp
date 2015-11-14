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

prepare_temp_type::prepare_temp_type(session& s)
    : m_rcpi(new ref_counted_prepare_info(s))
{
    // new query
    s.get_query_stream().str("");
}

prepare_temp_type::prepare_temp_type(prepare_temp_type const& o)
    : m_rcpi(o.m_rcpi)
{
    m_rcpi->addref();
}

prepare_temp_type& prepare_temp_type::operator=(prepare_temp_type const& o)
{
    o.m_rcpi->addref();
    m_rcpi->release();
    m_rcpi = o.m_rcpi;
    return *this;
}

prepare_temp_type::~prepare_temp_type()
{
    m_rcpi->release();
}

prepare_temp_type& prepare_temp_type::operator, (into_type_ptr const& i)
{
    m_rcpi->exchange(i);
    return *this;
}

prepare_temp_type& prepare_temp_type::operator, (use_type_ptr const& u)
{
    m_rcpi->exchange(u);
    return *this;
}

} // detail
} // sqdb
} // beast
