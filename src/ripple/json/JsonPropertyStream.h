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

#ifndef ripple_jsonpropertystream_h_included
#define ripple_jsonpropertystream_h_included

#include <ripple/json/json_config.h>
#include <beast/utility/propertystream.h>

namespace ripple {

/** a propertystream::sink which produces a json::value of type objectvalue. */
class jsonpropertystream : public beast::propertystream
{
public:
    json::value m_top;
    std::vector <json::value*> m_stack;

public:
    jsonpropertystream ();
    json::value const& top() const;

protected:

    void map_begin ();
    void map_begin (std::string const& key);
    void map_end ();
    void add (std::string const& key, short value);
    void add (std::string const& key, unsigned short value);
    void add (std::string const& key, int value);
    void add (std::string const& key, unsigned int value);
    void add (std::string const& key, long value);
    void add (std::string const& key, float v);
    void add (std::string const& key, double v);
    void add (std::string const& key, std::string const& v);
    void array_begin ();
    void array_begin (std::string const& key);
    void array_end ();

    void add (short value);
    void add (unsigned short value);
    void add (int value);
    void add (unsigned int value);
    void add (long value);
    void add (float v);
    void add (double v);
    void add (std::string const& v);
};

}

#endif
