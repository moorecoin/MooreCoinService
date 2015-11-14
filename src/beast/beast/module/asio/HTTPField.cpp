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

#include <beast/module/asio/httpfield.h>

namespace beast {

httpfield::httpfield ()
{
}

httpfield::httpfield (string name_, string value_)
    : m_name (name_)
    , m_value (value_)
{
}

httpfield::httpfield (httpfield const& other)
    : m_name (other.m_name)
    , m_value (other.m_value)
{
}

httpfield& httpfield::operator= (httpfield const& other)
{
    m_name = other.m_name;
    m_value = other.m_value;
    return *this;
}

string httpfield::name () const
{
    return m_name;
}

string httpfield::value () const
{
    return m_value;
}

}
