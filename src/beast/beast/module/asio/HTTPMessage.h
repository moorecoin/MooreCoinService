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

#ifndef beast_asio_httpmessage_h_included
#define beast_asio_httpmessage_h_included

#include <beast/module/asio/httpheaders.h>
#include <beast/module/asio/httpversion.h>

#include <beast/smart_ptr/sharedobject.h>
#include <beast/net/dynamicbuffer.h>
#include <beast/module/core/text/stringpairarray.h>

namespace beast {

/** a complete http message.

    this provides the information common to all http messages, including
    the version, content body, and headers.
    derived classes provide the request or response specific data.

    because a single http message can be a fairly expensive object to
    make copies of, this is a sharedobject.

    @see httprequest, httpresponse
*/
class httpmessage : public sharedobject
{
public:
    /** construct the common http message parts from values.
        ownership of the fields and body parameters are
        transferred from the caller.
    */
    httpmessage (httpversion const& version_,
                 stringpairarray& fields,
                 dynamicbuffer& body);

    /** returns the http version of this message. */
    httpversion const& version () const;

    /** returns the set of http headers associated with this message. */
    httpheaders const& headers () const;

    /** returns the content-body. */
    dynamicbuffer const& body () const;

    /** outputs all the httpmessage data excluding the body into a string. */
    string tostring () const;
        
private:
    httpversion m_version;
    httpheaders m_headers;
    dynamicbuffer m_body;
};

}

#endif
