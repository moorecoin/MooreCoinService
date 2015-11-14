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

#include <beast/http/impl/joyent_parser.h>
#include <beast/http/method.h>
#include <boost/system/error_code.hpp>

namespace beast {
namespace joyent {

#ifdef _msc_ver
# pragma warning (push)
# pragma warning (disable: 4127) // conditional expression is constant
# pragma warning (disable: 4244) // integer conversion, possible loss of data
#endif
#include <beast/http/impl/http-parser/http_parser.c>
#ifdef _msc_ver
# pragma warning (pop)
#endif

}
}

namespace boost {
namespace system {

template <>
struct is_error_code_enum <beast::joyent::http_errno>
    : std::true_type
{
};

template <>
struct is_error_condition_enum <beast::joyent::http_errno>
    : std::true_type
{
};

}
}

namespace beast {
namespace joyent {

http::method_t
convert_http_method (joyent::http_method m)
{
    switch (m)
    {
    case http_delete:      return http::method_t::http_delete;
    case http_get:         return http::method_t::http_get;
    case http_head:        return http::method_t::http_head;
    case http_post:        return http::method_t::http_post;
    case http_put:         return http::method_t::http_put;

    // pathological
    case http_connect:     return http::method_t::http_connect;
    case http_options:     return http::method_t::http_options;
    case http_trace:       return http::method_t::http_trace;

    // webdav
    case http_copy:        return http::method_t::http_copy;
    case http_lock:        return http::method_t::http_lock;
    case http_mkcol:       return http::method_t::http_mkcol;
    case http_move:        return http::method_t::http_move;
    case http_propfind:    return http::method_t::http_propfind;
    case http_proppatch:   return http::method_t::http_proppatch;
    case http_search:      return http::method_t::http_search;
    case http_unlock:      return http::method_t::http_unlock;

    // subversion
    case http_report:      return http::method_t::http_report;
    case http_mkactivity:  return http::method_t::http_mkactivity;
    case http_checkout:    return http::method_t::http_checkout;
    case http_merge:       return http::method_t::http_merge;

    // upnp
    case http_msearch:     return http::method_t::http_msearch;
    case http_notify:      return http::method_t::http_notify;
    case http_subscribe:   return http::method_t::http_subscribe;
    case http_unsubscribe: return http::method_t::http_unsubscribe;

    // rfc-5789
    case http_patch:       return http::method_t::http_patch;
    case http_purge:       return http::method_t::http_purge;
    };

    return http::method_t::http_get;
}

boost::system::error_code
convert_http_errno (joyent::http_errno err)
{
    class http_error_category_t
        : public boost::system::error_category
    {
    private:
        typedef boost::system::error_code error_code;
        typedef boost::system::error_condition error_condition;

    public:
        char const*
        name() const noexcept override
        {
            return "http_errno";
        }

        std::string
        message (int ev) const override
        {
            return joyent::http_errno_name (
                joyent::http_errno (ev));
        }

        error_condition
        default_error_condition (int ev) const noexcept override
        {
            return error_condition (ev, *this);
        }

        bool
        equivalent (int code, error_condition const& condition
            ) const noexcept override
        {
            return default_error_condition (code) == condition;
        }
        
        bool
        equivalent (error_code const& code, int condition
            ) const noexcept override
        {
            return *this == code.category() &&
                code.value() == condition;
        }
    };

    static http_error_category_t http_error_category;
    
    return boost::system::error_code (
        err, http_error_category);
}

}
}
