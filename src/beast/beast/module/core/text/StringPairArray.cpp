//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

    portions of this file are from juce.
    copyright (c) 2013 - raw material software ltd.
    please visit http://www.juce.com

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

namespace beast
{

stringpairarray::stringpairarray (const bool ignorecase_)
    : ignorecase (ignorecase_)
{
}

stringpairarray::stringpairarray (const stringpairarray& other)
    : keys (other.keys),
      values (other.values),
      ignorecase (other.ignorecase)
{
}

stringpairarray::~stringpairarray()
{
}

stringpairarray& stringpairarray::operator= (const stringpairarray& other)
{
    keys = other.keys;
    values = other.values;
    return *this;
}

void stringpairarray::swapwith (stringpairarray& other)
{
    std::swap (ignorecase, other.ignorecase);
    keys.swapwith (other.keys);
    values.swapwith (other.values);
}

bool stringpairarray::operator== (const stringpairarray& other) const
{
    for (int i = keys.size(); --i >= 0;)
        if (other [keys[i]] != values[i])
            return false;

    return true;
}

bool stringpairarray::operator!= (const stringpairarray& other) const
{
    return ! operator== (other);
}

const string& stringpairarray::operator[] (const string& key) const
{
    return values [keys.indexof (key, ignorecase)];
}

string stringpairarray::getvalue (const string& key, const string& defaultreturnvalue) const
{
    const int i = keys.indexof (key, ignorecase);

    if (i >= 0)
        return values[i];

    return defaultreturnvalue;
}

void stringpairarray::set (const string& key, const string& value)
{
    const int i = keys.indexof (key, ignorecase);

    if (i >= 0)
    {
        values.set (i, value);
    }
    else
    {
        keys.add (key);
        values.add (value);
    }
}

void stringpairarray::addarray (const stringpairarray& other)
{
    for (int i = 0; i < other.size(); ++i)
        set (other.keys[i], other.values[i]);
}

void stringpairarray::clear()
{
    keys.clear();
    values.clear();
}

void stringpairarray::remove (const string& key)
{
    remove (keys.indexof (key, ignorecase));
}

void stringpairarray::remove (const int index)
{
    keys.remove (index);
    values.remove (index);
}

void stringpairarray::setignorescase (const bool shouldignorecase)
{
    ignorecase = shouldignorecase;
}

string stringpairarray::getdescription() const
{
    string s;

    for (int i = 0; i < keys.size(); ++i)
    {
        s << keys[i] << " = " << values[i];
        if (i < keys.size())
            s << ", ";
    }

    return s;
}

void stringpairarray::minimisestorageoverheads()
{
    keys.minimisestorageoverheads();
    values.minimisestorageoverheads();
}

} // beast
