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

#ifndef beast_asio_httpparser_h_included
#define beast_asio_httpparser_h_included

#include <beast/module/asio/httprequest.h>
#include <beast/module/asio/httpresponse.h>

namespace beast {

class httpparserimpl;

/** a parser for httprequest and httpresponse objects. */
class httpparser
{
public:
    enum type
    {
        typerequest,
        typeresponse
    };

    /** construct a new parser for the specified httpmessage type. */
    explicit httpparser (type type);

    /** destroy the parser. */
    ~httpparser ();

    /** returns a non zero error code if parsing fails. */
    unsigned char error () const;

    /** returns the error message text when error is non zero. */
    string message () const;

    /** parse the buffer and return the amount used.
        typically it is an error when this returns less than
        the amount passed in.
    */
    std::size_t process (void const* buf, std::size_t bytes);

    /** notify the parser that eof was received.
    */
    void process_eof ();

    /** returns `true` when parsing is successful and complete. */
    bool finished () const;

    /** peek at the header fields as they are being built.
        only complete pairs will show up, never partial strings.
    */
    stringpairarray const& fields () const;

    /** returns `true` if all the http headers have been received. */
    bool headerscomplete () const;

    /** return the httprequest object produced from the parsiing.
        only valid after finished returns `true`.
    */
    sharedptr <httprequest> const& request ();

    /** return the httpresponse object produced from the parsing.
        only valid after finished returns `true`.
    */
    sharedptr <httpresponse> const& response ();

protected:
    type m_type;
    std::unique_ptr <httpparserimpl> m_impl;
    sharedptr <httprequest> m_request;
    sharedptr <httpresponse> m_response;
};

}

#endif
