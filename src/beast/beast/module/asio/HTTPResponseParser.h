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

#ifndef beast_http_responseparser_h_included
#define beast_http_responseparser_h_included

#include <beast/module/asio/httpparser.h>

namespace beast {

/** a parser for httpresponse objects. */
class httpresponseparser : public httpparser
{
public:
    /** construct a new parser for the specified httpmessage type. */
    httpresponseparser ();

    /** destroy the parser. */
    ~httpresponseparser ();

    /** return the httpresponse object produced from the parsing. */
    sharedptr <httpresponse> const& response ();

private:
    //sharedptr <httpresponse> m_response;
};

}

#endif
