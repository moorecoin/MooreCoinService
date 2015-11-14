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
#include <ripple/resource/consumer.h>
#include <ripple/resource/impl/entry.h>
#include <ripple/resource/impl/logic.h>

namespace ripple {
namespace resource {

consumer::consumer (logic& logic, entry& entry)
    : m_logic (&logic)
    , m_entry (&entry)
{
}

consumer::consumer ()
    : m_logic (nullptr)
    , m_entry (nullptr)
{
}

consumer::consumer (consumer const& other)
    : m_logic (other.m_logic)
    , m_entry (nullptr)
{
    if (m_logic && other.m_entry)
    {
        m_entry = other.m_entry;
        m_logic->acquire (*m_entry);
    }
}

consumer::~consumer()
{
    if (m_logic && m_entry)
        m_logic->release (*m_entry);
}

consumer& consumer::operator= (consumer const& other)
{
    // remove old ref
    if (m_logic && m_entry)
        m_logic->release (*m_entry);

    m_logic = other.m_logic;
    m_entry = other.m_entry;

    // add new ref
    if (m_logic && m_entry)
        m_logic->acquire (*m_entry);

    return *this;
}

std::string consumer::to_string () const
{
    if (m_logic == nullptr)
        return "(none)";

    return m_entry->to_string();
}

bool consumer::admin () const
{
    if (m_entry)
        return m_entry->admin();

    return false;
}

void consumer::elevate (std::string const& name)
{
    bassert (m_entry != nullptr);
    m_entry = &m_logic->elevatetoadminendpoint (*m_entry, name);
}

disposition consumer::disposition() const
{
    disposition d = ok;
    if (m_logic && m_entry)
        d =  m_logic->charge(*m_entry, charge(0));

    return d;
}

disposition consumer::charge (charge const& what)
{
    bassert (m_entry != nullptr);
    return m_logic->charge (*m_entry, what);
}

bool consumer::warn ()
{
    bassert (m_entry != nullptr);
    return m_logic->warn (*m_entry);
}

bool consumer::disconnect ()
{
    bassert (m_entry != nullptr);
    return m_logic->disconnect (*m_entry);
}

int consumer::balance()
{
    bassert (m_entry != nullptr);
    return m_logic->balance (*m_entry);
}

entry& consumer::entry()
{
    bassert (m_entry != nullptr);
    return *m_entry;
}

std::ostream& operator<< (std::ostream& os, consumer const& v)
{
    os << v.to_string();
    return os;
}

}
}
