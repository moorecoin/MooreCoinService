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

#ifndef beast_http_method_h_included
#define beast_http_method_h_included

#include <memory>
#include <string>

namespace beast {
namespace http {

enum class method_t
{
    http_delete,
    http_get,
    http_head,
    http_post,
    http_put,

    // pathological
    http_connect,
    http_options,
    http_trace,

    // webdav
    http_copy,
    http_lock,
    http_mkcol,
    http_move,
    http_propfind,
    http_proppatch,
    http_search,
    http_unlock,

    // subversion
    http_report,
    http_mkactivity,
    http_checkout,
    http_merge,

    // upnp
    http_msearch,
    http_notify,
    http_subscribe,
    http_unsubscribe,

    // rfc-5789
    http_patch,
    http_purge
};

std::string
to_string (method_t m);

template <class stream>
stream&
operator<< (stream& s, method_t m)
{
    return s << to_string(m);
}

/** returns the string corresponding to the numeric http status code. */
std::string
status_text (int status);

}
}

#endif
