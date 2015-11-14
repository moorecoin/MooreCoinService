/*
 * copyright (c) 2014, peter thorson. all rights reserved.
 *
 * redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * neither the name of the websocket++ project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * this software is provided by the copyright holders and contributors "as is"
 * and any express or implied warranties, including, but not limited to, the
 * implied warranties of merchantability and fitness for a particular purpose
 * are disclaimed. in no event shall peter thorson be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential damages
 * (including, but not limited to, procurement of substitute goods or services;
 * loss of use, data, or profits; or business interruption) however caused and
 * on any theory of liability, whether in contract, strict liability, or tort
 * (including negligence or otherwise) arising in any way out of the use of this
 * software, even if advised of the possibility of such damage.
 *
 */

#ifndef websocketpp_common_connection_hdl_hpp
#define websocketpp_common_connection_hdl_hpp

#include <websocketpp/common/memory.hpp>

namespace websocketpp {

/// a handle to uniquely identify a connection.
/**
 * this type uniquely identifies a connection. it is implemented as a weak
 * pointer to the connection in question. this provides uniqueness across
 * multiple endpoints and ensures that ids never conflict or run out.
 *
 * it is safe to make copies of this handle, store those copies in containers,
 * and use them from other threads.
 *
 * this handle can be upgraded to a full shared_ptr using
 * `endpoint::get_con_from_hdl()` from within a handler fired by the connection
 * that owns the handler.
 */
typedef lib::weak_ptr<void> connection_hdl;

} // namespace websocketpp

#endif // websocketpp_common_connection_hdl_hpp
