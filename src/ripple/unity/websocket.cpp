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

#include <beastconfig.h>

#ifndef __stdc_limit_macros
#define __stdc_limit_macros 1
#endif
#include <stdint.h>

#include <ripple/unity/websocket.h>

// unity build file for websocket
//

#include <websocketpp_02/src/sha1/sha1.h>

// must come first to prevent compile errors
#include <websocketpp_02/src/uri.cpp>

#include <websocketpp_02/src/base64/base64.cpp>
#include <websocketpp_02/src/messages/data.cpp>
#include <websocketpp_02/src/processors/hybi_header.cpp>
#include <websocketpp_02/src/processors/hybi_util.cpp>
#include <websocketpp_02/src/md5/md5.c>
#include <websocketpp_02/src/network_utilities.cpp>
#include <websocketpp_02/src/sha1/sha1.cpp>

#include <ripple/websocket/autosocket/autosocket.cpp>
#include <ripple/websocket/autosocket/logwebsockets.cpp>
