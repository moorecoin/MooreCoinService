/*
 * copyright (c) 2011, peter thorson. all rights reserved.
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

#ifndef websocketpp_socket_base_hpp
#define websocketpp_socket_base_hpp

/*

 websocketpp_02::socket api
 endpoint policy that provides:


 nested connection policy called `connection` that provides:
 - constructor that takes a reference to the endpoint_policy
 - async_init
 - async_read_some
 - async_write_some
 - get_raw_socket

 */

#include <boost/system/error_code.hpp>
#include <boost/function.hpp>

namespace websocketpp_02 {
namespace socket {

typedef boost::function<void(const boost::system::error_code&)> socket_init_callback;


} // namespace socket
} // namespace websocketpp_02



#endif // websocketpp_socket_base_hpp
