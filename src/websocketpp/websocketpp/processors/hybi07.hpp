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

#ifndef websocketpp_processor_hybi07_hpp
#define websocketpp_processor_hybi07_hpp

#include <websocketpp/processors/hybi08.hpp>

namespace websocketpp {
namespace processor {

/// processor for hybi draft version 07
/**
 * the primary difference between 07 and 08 is a version number.
 */
template <typename config>
class hybi07 : public hybi08<config> {
public:
    typedef typename config::request_type request_type;

    typedef typename config::con_msg_manager_type::ptr msg_manager_ptr;
    typedef typename config::rng_type rng_type;

    explicit hybi07(bool secure, bool p_is_server, msg_manager_ptr manager, rng_type& rng)
      : hybi08<config>(secure, p_is_server, manager, rng) {}

    /// fill in a set of request headers for a client connection request
    /**
     * the hybi 07 processor only implements incoming connections so this will
     * always return an error.
     *
     * @param [out] req  set of headers to fill in
     * @param [in] uri the uri being connected to
     * @param [in] subprotocols the list of subprotocols to request
     */
    lib::error_code client_handshake_request(request_type &, uri_ptr,
        std::vector<std::string> const &) const
    {
        return error::make_error_code(error::no_protocol_support);
    }

    int get_version() const {
        return 7;
    }
private:
};

} // namespace processor
} // namespace websocketpp

#endif //websocketpp_processor_hybi07_hpp
