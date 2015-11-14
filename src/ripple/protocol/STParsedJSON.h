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

#ifndef ripple_protocol_stparsedjson_h_included
#define ripple_protocol_stparsedjson_h_included

#include <ripple/protocol/starray.h>

namespace ripple {

/** holds the serialized result of parsing an input json object.
    this does validation and checking on the provided json.
*/
class stparsedjsonobject
{
public:
    /** parses and creates an stparsedjson object.
        the result of the parsing is stored in object and error.
        exceptions:
            does not throw.
        @param name the name of the json field, used in diagnostics.
        @param json the json-rpc to parse.
    */
    stparsedjsonobject (std::string const& name, json::value const& json);

    stparsedjsonobject () = delete;
    stparsedjsonobject (stparsedjsonobject const&) = delete;
    stparsedjsonobject& operator= (stparsedjsonobject const&) = delete;
    ~stparsedjsonobject () = default;

    /** the stobject if the parse was successful. */
    std::unique_ptr <stobject> object;

    /** on failure, an appropriate set of error values. */
    json::value error;
};

/** holds the serialized result of parsing an input json array.
    this does validation and checking on the provided json.
*/
class stparsedjsonarray
{
public:
    /** parses and creates an stparsedjson array.
        the result of the parsing is stored in array and error.
        exceptions:
            does not throw.
        @param name the name of the json field, used in diagnostics.
        @param json the json-rpc to parse.
    */
    stparsedjsonarray (std::string const& name, json::value const& json);

    stparsedjsonarray () = delete;
    stparsedjsonarray (stparsedjsonarray const&) = delete;
    stparsedjsonarray& operator= (stparsedjsonarray const&) = delete;
    ~stparsedjsonarray () = default;

    /** the starray if the parse was successful. */
    std::unique_ptr <starray> array;

    /** on failure, an appropriate set of error values. */
    json::value error;
};



} // ripple

#endif
