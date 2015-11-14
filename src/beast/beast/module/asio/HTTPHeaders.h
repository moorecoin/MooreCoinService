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

#ifndef beast_asio_httpheaders_h_included
#define beast_asio_httpheaders_h_included

#include <beast/module/asio/httpfield.h>
#include <beast/module/core/text/stringpairarray.h>
#include <map>

namespace beast {

/** a set of http headers. */
class httpheaders
{
public:
    /** construct an empty set of headers. */
    httpheaders ();

    /** construct headers taking ownership of a field array.
        the callers value is overwritten.
    */
    httpheaders (stringpairarray& fields);

    /** construct a copy of headers from an array.*/
    httpheaders (stringpairarray const& fields);

    /** construct a copy of headers. */
    httpheaders (httpheaders const& other);

    /** assign a copy of headers. */
    httpheaders& operator= (httpheaders const& other);

    /** returns `true` if the container is empty. */
    bool empty () const;

    /** returns the number of fields in the container. */
    std::size_t size () const;

    /** random access to fields by index. */
    /** @{ */
    httpfield at (int index) const;
    httpfield operator[] (int index) const;
    /** @} */

    /** associative access to fields by name.
        if the field is not present, an empty string is returned.
    */
    /** @{ */
    string get (string const& field) const;
    string operator[] (string const& field) const;
    /** @} */

    /** outputs all the headers into one string. */
    string tostring () const;

    // vfalco hack to present the headers in a useful format
    std::map <std::string, std::string>
    build_map() const;

private:
    stringpairarray m_fields;
};

}

#endif
