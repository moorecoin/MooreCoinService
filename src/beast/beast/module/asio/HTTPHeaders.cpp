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
*/
//==============================================================================

#include <beast/module/asio/httpheaders.h>
#include <algorithm>

namespace beast {

httpheaders::httpheaders ()
{
}

httpheaders::httpheaders (stringpairarray& fields)
{
    m_fields.swapwith (fields);
}

httpheaders::httpheaders (stringpairarray const& fields)
    : m_fields (fields)
{
}

httpheaders::httpheaders (httpheaders const& other)
    : m_fields (other.m_fields)
{
}

httpheaders& httpheaders::operator= (httpheaders const& other)
{
    m_fields = other.m_fields;
    return *this;
}

bool httpheaders::empty () const
{
    return m_fields.size () == 0;
}

std::size_t httpheaders::size () const
{
    return m_fields.size ();
}

httpfield httpheaders::at (int index) const
{
    return httpfield (m_fields.getallkeys () [index],
                      m_fields.getallvalues () [index]);
}

httpfield httpheaders::operator[] (int index) const
{
    return at (index);
}

string httpheaders::get (string const& field) const
{
    return m_fields [field];
}

string httpheaders::operator[] (string const& field) const
{
    return get (field);
}

string httpheaders::tostring () const
{
    string s;
    for (int i = 0; i < m_fields.size (); ++i)
    {
        httpfield const field (at(i));
        s << field.name() << ": " << field.value() << newline;
    }
    return s;
}

std::map <std::string, std::string>
httpheaders::build_map() const
{
    std::map <std::string, std::string> c;
    auto const& k (m_fields.getallkeys());
    auto const& v (m_fields.getallvalues());
    for (std::size_t i = 0; i < m_fields.size(); ++i)
    {
        auto key (k[i].tostdstring());
        auto const value (v[i].tostdstring());
        std::transform (key.begin(), key.end(), key.begin(), ::tolower);
        c[key] = value;
    }
    return c;
}

}
