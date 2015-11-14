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

#ifndef shared_const_buffer_hpp
#define shared_const_buffer_hpp


#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>

#include <string>
#include <vector>

namespace websocketpp_02 {

class shared_const_buffer {
public:
    explicit shared_const_buffer(const std::string &data) : m_data(new std::vector<char>(data.begin(), data.end())),
    m_buffer(boost::asio::buffer(*m_data)) {}
public:
    typedef boost::asio::const_buffer value_type;
    typedef const boost::asio::const_buffer *const_iterator;
    const boost::asio::const_buffer *begin() const { return &m_buffer; }
    const boost::asio::const_buffer *end() const { return &m_buffer + 1; }
private:
    boost::shared_ptr< std::vector<char> > m_data;
    boost::asio::const_buffer m_buffer;
};

} // namespace websocketpp_02

#endif // shared_const_buffer_hpp
