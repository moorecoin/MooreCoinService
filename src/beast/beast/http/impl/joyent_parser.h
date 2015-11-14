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

#ifndef beast_http_joyent_parser_h_included
#define beast_http_joyent_parser_h_included

#include <beast/http/method.h>

// todo use <system_error>
#include <boost/system/error_code.hpp>

// wraps the c-language joyent http parser header in a namespace 

namespace beast {
namespace joyent {

#include <beast/http/impl/http-parser/http_parser.h>

http::method_t
convert_http_method (joyent::http_method m);

boost::system::error_code
convert_http_errno (joyent::http_errno err);

}
}

#endif
