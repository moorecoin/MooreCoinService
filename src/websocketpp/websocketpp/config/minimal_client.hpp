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

#ifndef websocketpp_config_minimal_client_hpp
#define websocketpp_config_minimal_client_hpp

#include <websocketpp/config/minimal_server.hpp>

namespace websocketpp {
namespace config {

/// client config with minimal dependencies
/**
 * this config strips out as many dependencies as possible. it is suitable for
 * use as a base class for custom configs that want to implement or choose their
 * own policies for components that even the core config includes.
 *
 * note: this config stubs out enough that it cannot be used directly. you must
 * supply at least a transport policy and a cryptographically secure random 
 * number generation policy for a config based on `minimal_client` to do 
 * anything useful.
 *
 * present dependency list for minimal_server config:
 *
 * c++98 stl:
 * <algorithm>
 * <map>
 * <sstream>
 * <string>
 * <vector>
 *
 * c++11 stl or boost
 * <memory>
 * <functional>
 * <system_error>
 *
 * operating system:
 * <stdint.h> or <boost/cstdint.hpp>
 * <netinet/in.h> or <winsock2.h> (for ntohl.. could potentially bundle this)
 *
 * @since 0.4.0-dev
 */
typedef minimal_server minimal_client;

} // namespace config
} // namespace websocketpp

#endif // websocketpp_config_minimal_client_hpp
