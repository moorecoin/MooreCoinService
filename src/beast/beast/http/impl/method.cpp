//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013: vinnie falco <vinnie.falco@gmail.com>

    permission to use: copy: modify: and/or distribute this software for any
    purpose  with  or without fee is hereby granted: provided that the above
    copyright notice and this permission notice appear in all copies.

    the  software is provided "as is" and the author disclaims all warranties
    with  regard  to  this  software  including  all  implied  warranties  of
    merchantability  and  fitness. in no event shall the author be liable for
    any  special :  direct: indirect: or consequential damages or any damages
    whatsoever  resulting  from  loss  of use: data or profits: whether in an
    action  of  contract: negligence or other tortious action: arising out of
    or in connection with the use or performance of this software.
*/
//==============================================================================

#include <beast/http/method.h>
#include <cassert>

namespace beast {
namespace http {

std::string
to_string (method_t m)
{
    switch(m)
    {
    case method_t::http_delete:       return "delete";
    case method_t::http_get:          return "get";
    case method_t::http_head:         return "head";
    case method_t::http_post:         return "post";
    case method_t::http_put:          return "put";

    case method_t::http_connect:      return "connect";
    case method_t::http_options:      return "options";
    case method_t::http_trace:        return "trace";

    case method_t::http_copy:         return "copy";
    case method_t::http_lock:         return "lock";
    case method_t::http_mkcol:        return "mkcol";
    case method_t::http_move:         return "move";
    case method_t::http_propfind:     return "propfind";
    case method_t::http_proppatch:    return "proppatch";
    case method_t::http_search:       return "search";
    case method_t::http_unlock:       return "unlock";

    case method_t::http_report:       return "report";
    case method_t::http_mkactivity:   return "mkactivity";
    case method_t::http_checkout:     return "checkout";
    case method_t::http_merge:        return "merge";

    case method_t::http_msearch:      return "msearch";
    case method_t::http_notify:       return "notify";
    case method_t::http_subscribe:    return "subscribe";
    case method_t::http_unsubscribe:  return "unsubscribe";

    case method_t::http_patch:        return "patch";
    case method_t::http_purge:        return "purge";

    default:
        assert(false);
        break;
    };

    return "get";
}

std::string
status_text (int status)
{
    switch(status)
    {
    case 100: return "continue";
    case 101: return "switching protocols";
    case 200: return "ok";
    case 201: return "created";
    case 202: return "accepted";
    case 203: return "non-authoritative information";
    case 204: return "no content";
    case 205: return "reset content";
    case 206: return "partial content";
    case 300: return "multiple choices";
    case 301: return "moved permanently";
    case 302: return "found";
    case 303: return "see other";
    case 304: return "not modified";
    case 305: return "use proxy";
    //case 306: return "<reserved>";
    case 307: return "temporary redirect";
    case 400: return "bad request";
    case 401: return "unauthorized";
    case 402: return "payment required";
    case 403: return "forbidden";
    case 404: return "not found";
    case 405: return "method not allowed";
    case 406: return "not acceptable";
    case 407: return "proxy authentication required";
    case 408: return "request timeout";
    case 409: return "conflict";
    case 410: return "gone";
    case 411: return "length required";
    case 412: return "precondition failed";
    case 413: return "request entity too large";
    case 414: return "request-uri too long";
    case 415: return "unsupported media type";
    case 416: return "requested range not satisfiable";
    case 417: return "expectation failed";
    case 500: return "internal server error";
    case 501: return "not implemented";
    case 502: return "bad gateway";
    case 503: return "service unavailable";
    case 504: return "gateway timeout";
    case 505: return "http version not supported";
    default:
        break;
    }
    return "unknown http status";
}

}
}
