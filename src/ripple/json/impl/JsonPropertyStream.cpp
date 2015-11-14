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
#include <ripple/json/json_value.h>
#include <ripple/json/jsonpropertystream.h>

namespace ripple {

jsonpropertystream::jsonpropertystream ()
    : m_top (json::objectvalue)
{
    m_stack.reserve (64);
    m_stack.push_back (&m_top);
}

json::value const& jsonpropertystream::top() const
{
    return m_top;
}

void jsonpropertystream::map_begin ()
{
    // top is array
    json::value& top (*m_stack.back());
    json::value& map (top.append (json::objectvalue));
    m_stack.push_back (&map);
}

void jsonpropertystream::map_begin (std::string const& key)
{
    // top is a map
    json::value& top (*m_stack.back());
    json::value& map (top [key] = json::objectvalue);
    m_stack.push_back (&map);
}

void jsonpropertystream::map_end ()
{
    m_stack.pop_back ();
}

void jsonpropertystream::add (std::string const& key, short v)
{
    (*m_stack.back())[key] = v;
}

void jsonpropertystream::add (std::string const& key, unsigned short v)
{
    (*m_stack.back())[key] = v;
}

void jsonpropertystream::add (std::string const& key, int v)
{
    (*m_stack.back())[key] = v;
}

void jsonpropertystream::add (std::string const& key, unsigned int v)
{
    (*m_stack.back())[key] = v;
}

void jsonpropertystream::add (std::string const& key, long v)
{
    (*m_stack.back())[key] = int(v);
}

void jsonpropertystream::add (std::string const& key, float v)
{
    (*m_stack.back())[key] = v;
}

void jsonpropertystream::add (std::string const& key, double v)
{
    (*m_stack.back())[key] = v;
}

void jsonpropertystream::add (std::string const& key, std::string const& v)
{
    (*m_stack.back())[key] = v;
}

void jsonpropertystream::array_begin ()
{
    // top is array
    json::value& top (*m_stack.back());
    json::value& vec (top.append (json::arrayvalue));
    m_stack.push_back (&vec);
}

void jsonpropertystream::array_begin (std::string const& key)
{
    // top is a map
    json::value& top (*m_stack.back());
    json::value& vec (top [key] = json::arrayvalue);
    m_stack.push_back (&vec);
}

void jsonpropertystream::array_end ()
{
    m_stack.pop_back ();
}

void jsonpropertystream::add (short v)
{
    m_stack.back()->append (v);
}

void jsonpropertystream::add (unsigned short v)
{
    m_stack.back()->append (v);
}

void jsonpropertystream::add (int v)
{
    m_stack.back()->append (v);
}

void jsonpropertystream::add (unsigned int v)
{
    m_stack.back()->append (v);
}

void jsonpropertystream::add (long v)
{
    m_stack.back()->append (int (v));
}

void jsonpropertystream::add (float v)
{
    m_stack.back()->append (v);
}

void jsonpropertystream::add (double v)
{
    m_stack.back()->append (v);
}

void jsonpropertystream::add (std::string const& v)
{
    m_stack.back()->append (v);
}

}

