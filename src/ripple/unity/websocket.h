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

#ifndef ripple_websocket_h_included
#define ripple_websocket_h_included

// needed before inclusion of stdint.h for int32_min/int32_max macros
#ifndef __stdc_limit_macros
  #define __stdc_limit_macros
#endif

#include <beast/module/core/text/lexicalcast.h>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <websocketpp_02/src/common.hpp>
#include <websocketpp_02/src/sockets/socket_base.hpp>
#include <websocketpp_02/src/sockets/autotls.hpp>
#include <websocketpp_02/src/websocketpp.hpp>
#include <websocketpp_02/src/logger/logger.hpp>

#endif
