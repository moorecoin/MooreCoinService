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

#ifndef websocketpp_error_message_hpp
#define websocketpp_error_message_hpp

namespace websocketpp {

/**
 * the transport::security::* classes are a set of security/socket related
 * policies and support code for the asio transport types.
 */
class error_msg {
public:
    const std::string& get_msg() const {
        return m_error_msg;
    }

    void set_msg(const std::string& msg) {
        m_error_msg = msg;
    }

    void append_msg(const std::string& msg) {
        m_error_msg.append(msg);
    }

    template <typename t>
    void set_msg(const t& thing) {
        std::stringsteam val;
        val << thing;
        this->set_msg(val.str());
    }

    template <typename t>
    void append_msg(const t& thing) {
        std::stringsteam val;
        val << thing;
        this->append_msg(val.str());
    }
private:
    // error resources
    std::string     m_error_msg;
};

} // namespace websocketpp

#endif // websocketpp_error_message_hpp
