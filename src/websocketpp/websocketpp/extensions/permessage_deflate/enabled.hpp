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

#ifndef websocketpp_processor_extension_permessagedeflate_hpp
#define websocketpp_processor_extension_permessagedeflate_hpp


#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/platforms.hpp>
#include <websocketpp/common/system_error.hpp>
#include <websocketpp/error.hpp>

#include <websocketpp/extensions/extension.hpp>

#include "zlib.h"

#include <algorithm>
#include <string>
#include <vector>

namespace websocketpp {
namespace extensions {

/// implimentation of the draft permessage-deflate websocket extension
/**
 * ### permessage-deflate interface
 *
 * **is_implemented**\n
 * `bool is_implemented()`\n
 * returns whether or not the object impliments the extension or not
 *
 * **is_enabled**\n
 * `bool is_enabled()`\n
 * returns whether or not the extension was negotiated for the current
 * connection
 *
 * **generate_offer**\n
 * `std::string generate_offer() const`\n
 * create an extension offer string based on local policy
 *
 * **validate_response**\n
 * `lib::error_code validate_response(http::attribute_list const & response)`\n
 * negotiate the parameters of extension use
 *
 * **negotiate**\n
 * `err_str_pair negotiate(http::attribute_list const & attributes)`\n
 * negotiate the parameters of extension use
 *
 * **compress**\n
 * `lib::error_code compress(std::string const & in, std::string & out)`\n
 * compress the bytes in `in` and append them to `out`
 *
 * **decompress**\n
 * `lib::error_code decompress(uint8_t const * buf, size_t len, std::string &
 * out)`\n
 * decompress `len` bytes from `buf` and append them to string `out`
 */
namespace permessage_deflate {

/// permessage deflate error values
namespace error {
enum value {
    /// catch all
    general = 1,

    /// invalid extension attributes
    invalid_attributes,

    /// invalid extension attribute value
    invalid_attribute_value,

    /// invalid megotiation mode
    invalid_mode,

    /// unsupported extension attributes
    unsupported_attributes,

    /// invalid value for max_window_bits
    invalid_max_window_bits,

    /// zlib error
    zlib_error,

    /// uninitialized
    uninitialized,
};

/// permessage-deflate error category
class category : public lib::error_category {
public:
    category() {}

    char const * name() const _websocketpp_noexcept_token_ {
        return "websocketpp.extension.permessage-deflate";
    }

    std::string message(int value) const {
        switch(value) {
            case general:
                return "generic permessage-compress error";
            case invalid_attributes:
                return "invalid extension attributes";
            case invalid_attribute_value:
                return "invalid extension attribute value";
            case invalid_mode:
                return "invalid permessage-deflate negotiation mode";
            case unsupported_attributes:
                return "unsupported extension attributes";
            case invalid_max_window_bits:
                return "invalid value for max_window_bits";
            case zlib_error:
                return "a zlib function returned an error";
            case uninitialized:
                return "object must be initialized before use";
            default:
                return "unknown permessage-compress error";
        }
    }
};

/// get a reference to a static copy of the permessage-deflate error category
lib::error_category const & get_category() {
    static category instance;
    return instance;
}

/// create an error code in the permessage-deflate category
lib::error_code make_error_code(error::value e) {
    return lib::error_code(static_cast<int>(e), get_category());
}

} // namespace error
} // namespace permessage_deflate
} // namespace extensions
} // namespace websocketpp

_websocketpp_error_code_enum_ns_start_
template<> struct is_error_code_enum
    <websocketpp::extensions::permessage_deflate::error::value>
{
    static bool const value = true;
};
_websocketpp_error_code_enum_ns_end_
namespace websocketpp {
namespace extensions {
namespace permessage_deflate {

/// default value for s2c_max_window_bits as defined by rfc6455
static uint8_t const default_s2c_max_window_bits = 15;
/// minimum value for s2c_max_window_bits as defined by rfc6455
static uint8_t const min_s2c_max_window_bits = 8;
/// maximum value for s2c_max_window_bits as defined by rfc6455
static uint8_t const max_s2c_max_window_bits = 15;

/// default value for c2s_max_window_bits as defined by rfc6455
static uint8_t const default_c2s_max_window_bits = 15;
/// minimum value for c2s_max_window_bits as defined by rfc6455
static uint8_t const min_c2s_max_window_bits = 8;
/// maximum value for c2s_max_window_bits as defined by rfc6455
static uint8_t const max_c2s_max_window_bits = 15;

namespace mode {
enum value {
    /// accept any value the remote endpoint offers
    accept = 1,
    /// decline any value the remote endpoint offers. insist on defaults.
    decline,
    /// use the largest value common to both offers
    largest,
    /// use the smallest value common to both offers
    smallest
};
} // namespace mode

template <typename config>
class enabled {
public:
    enabled()
      : m_enabled(false)
      , m_s2c_no_context_takeover(false)
      , m_c2s_no_context_takeover(false)
      , m_s2c_max_window_bits(15)
      , m_c2s_max_window_bits(15)
      , m_s2c_max_window_bits_mode(mode::accept)
      , m_c2s_max_window_bits_mode(mode::accept)
      , m_initialized(false)
      , m_compress_buffer_size(16384)
    {
        m_dstate.zalloc = z_null;
        m_dstate.zfree = z_null;
        m_dstate.opaque = z_null;

        m_istate.zalloc = z_null;
        m_istate.zfree = z_null;
        m_istate.opaque = z_null;
        m_istate.avail_in = 0;
        m_istate.next_in = z_null;
    }

    ~enabled() {
        if (!m_initialized) {
            return;
        }

        int ret = deflateend(&m_dstate);

        if (ret != z_ok) {
            //std::cout << "error cleaning up zlib compression state"
            //          << std::endl;
        }

        ret = inflateend(&m_istate);

        if (ret != z_ok) {
            //std::cout << "error cleaning up zlib decompression state"
            //          << std::endl;
        }
    }

    /// initialize zlib state
    /**
     *
     * @todo memory level, strategy, etc are hardcoded
     * @todo server detection is hardcoded
     */
    lib::error_code init() {
        uint8_t deflate_bits;
        uint8_t inflate_bits;

        if (true /*is_server*/) {
            deflate_bits = m_s2c_max_window_bits;
            inflate_bits = m_c2s_max_window_bits;
        } else {
            deflate_bits = m_c2s_max_window_bits;
            inflate_bits = m_s2c_max_window_bits;
        }

        int ret = deflateinit2(
            &m_dstate,
            z_default_compression,
            z_deflated,
            -1*deflate_bits,
            8, // memory level 1-9
            /*z_default_strategy*/z_fixed
        );

        if (ret != z_ok) {
            return make_error_code(error::zlib_error);
        }

        ret = inflateinit2(
            &m_istate,
            -1*inflate_bits
        );

        if (ret != z_ok) {
            return make_error_code(error::zlib_error);
        }

        m_compress_buffer.reset(new unsigned char[m_compress_buffer_size]);
        m_initialized = true;
        return lib::error_code();
    }

    /// test if this object impliments the permessage-deflate specification
    /**
     * because this object does impliment it, it will always return true.
     *
     * @return whether or not this object impliments permessage-deflate
     */
    bool is_implemented() const {
        return true;
    }

    /// test if the extension was negotiated for this connection
    /**
     * retrieves whether or not this extension is in use based on the initial
     * handshake extension negotiations.
     *
     * @return whether or not the extension is in use
     */
    bool is_enabled() const {
        return m_enabled;
    }

    /// reset server's outgoing lz77 sliding window for each new message
    /**
     * enabling this setting will cause the server's compressor to reset the
     * compression state (the lz77 sliding window) for every message. this
     * means that the compressor will not look back to patterns in previous
     * messages to improve compression. this will reduce the compression
     * efficiency for large messages somewhat and small messages drastically.
     *
     * this option may reduce server compressor memory usage and client
     * decompressor memory usage.
     * @todo document to what extent memory usage will be reduced
     *
     * for clients, this option is dependent on server support. enabling it
     * via this method does not guarantee that it will be successfully
     * negotiated, only that it will be requested.
     *
     * for servers, no client support is required. enabling this option on a
     * server will result in its use. the server will signal to clients that
     * the option will be in use so they can optimize resource usage if they
     * are able.
     */
    void enable_s2c_no_context_takeover() {
        m_s2c_no_context_takeover = true;
    }

    /// reset client's outgoing lz77 sliding window for each new message
    /**
     * enabling this setting will cause the client's compressor to reset the
     * compression state (the lz77 sliding window) for every message. this
     * means that the compressor will not look back to patterns in previous
     * messages to improve compression. this will reduce the compression
     * efficiency for large messages somewhat and small messages drastically.
     *
     * this option may reduce client compressor memory usage and server
     * decompressor memory usage.
     * @todo document to what extent memory usage will be reduced
     *
     * this option is supported by all compliant clients and servers. enabling
     * it via either endpoint should be sufficient to ensure it is used.
     */
    void enable_c2s_no_context_takeover() {
        m_c2s_no_context_takeover = true;
    }

    /// limit server lz77 sliding window size
    /**
     * the bits setting is the base 2 logarithm of the maximum window size that
     * the server must use to compress outgoing messages. the permitted range
     * is 8 to 15 inclusive. 8 represents a 256 byte window and 15 a 32kib
     * window. the default setting is 15.
     *
     * mode options:
     * - accept: accept whatever the remote endpoint offers.
     * - decline: decline any offers to deviate from the defaults
     * - largest: accept largest window size acceptable to both endpoints
     * - smallest: accept smallest window size acceptiable to both endpoints
     *
     * this setting is dependent on server support. a client requesting this
     * setting may be rejected by the server or have the exact value used
     * adjusted by the server. a server may unilaterally set this value without
     * client support.
     *
     * @param bits the size to request for the outgoing window size
     * @param mode the mode to use for negotiating this parameter
     * @return a status code
     */
    lib::error_code set_s2c_max_window_bits(uint8_t bits, mode::value mode) {
        if (bits < min_s2c_max_window_bits || bits > max_s2c_max_window_bits) {
            return error::make_error_code(error::invalid_max_window_bits);
        }
        m_s2c_max_window_bits = bits;
        m_s2c_max_window_bits_mode = mode;

        return lib::error_code();
    }

    /// limit client lz77 sliding window size
    /**
     * the bits setting is the base 2 logarithm of the window size that the
     * client must use to compress outgoing messages. the permitted range is 8
     * to 15 inclusive. 8 represents a 256 byte window and 15 a 32kib window.
     * the default setting is 15.
     *
     * mode options:
     * - accept: accept whatever the remote endpoint offers.
     * - decline: decline any offers to deviate from the defaults
     * - largest: accept largest window size acceptable to both endpoints
     * - smallest: accept smallest window size acceptiable to both endpoints
     *
     * this setting is dependent on client support. a client may limit its own
     * outgoing window size unilaterally. a server may only limit the client's
     * window size if the remote client supports that feature.
     *
     * @param bits the size to request for the outgoing window size
     * @param mode the mode to use for negotiating this parameter
     * @return a status code
     */
    lib::error_code set_c2s_max_window_bits(uint8_t bits, mode::value mode) {
        if (bits < min_c2s_max_window_bits || bits > max_c2s_max_window_bits) {
            return error::make_error_code(error::invalid_max_window_bits);
        }
        m_c2s_max_window_bits = bits;
        m_c2s_max_window_bits_mode = mode;

        return lib::error_code();
    }

    /// generate extension offer
    /**
     * creates an offer string to include in the sec-websocket-extensions
     * header of outgoing client requests.
     *
     * @return a websocket extension offer string for this extension
     */
    std::string generate_offer() const {
        return "";
    }

    /// validate extension response
    /**
     * confirm that the server has negotiated settings compatible with our
     * original offer and apply those settings to the extension state.
     *
     * @param response the server response attribute list to validate
     * @return validation error or 0 on success
     */
    lib::error_code validate_offer(http::attribute_list const &) {
        return make_error_code(error::general);
    }

    /// negotiate extension
    /**
     * confirm that the client's extension negotiation offer has settings
     * compatible with local policy. if so, generate a reply and apply those
     * settings to the extension state.
     *
     * @param offer attribute from client's offer
     * @return status code and value to return to remote endpoint
     */
    err_str_pair negotiate(http::attribute_list const & offer) {
        err_str_pair ret;

        http::attribute_list::const_iterator it;
        for (it = offer.begin(); it != offer.end(); ++it) {
            if (it->first == "s2c_no_context_takeover") {
                negotiate_s2c_no_context_takeover(it->second,ret.first);
            } else if (it->first == "c2s_no_context_takeover") {
                negotiate_c2s_no_context_takeover(it->second,ret.first);
            } else if (it->first == "s2c_max_window_bits") {
                negotiate_s2c_max_window_bits(it->second,ret.first);
            } else if (it->first == "c2s_max_window_bits") {
                negotiate_c2s_max_window_bits(it->second,ret.first);
            } else {
                ret.first = make_error_code(error::invalid_attributes);
            }

            if (ret.first) {
                break;
            }
        }

        if (ret.first == lib::error_code()) {
            m_enabled = true;
            ret.second = generate_response();
        }

        return ret;
    }

    /// compress bytes
    /**
     * @param [in] in string to compress
     * @param [out] out string to append compressed bytes to
     * @return error or status code
     */
    lib::error_code compress(std::string const & in, std::string & out) {
        if (!m_initialized) {
            return make_error_code(error::uninitialized);
        }

        size_t output;

        m_dstate.avail_out = m_compress_buffer_size;
        m_dstate.next_in = (unsigned char *)(const_cast<char *>(in.data()));

        do {
            // output to local buffer
            m_dstate.avail_out = m_compress_buffer_size;
            m_dstate.next_out = m_compress_buffer.get();

            deflate(&m_dstate, z_sync_flush);

            output = m_compress_buffer_size - m_dstate.avail_out;

            out.append((char *)(m_compress_buffer.get()),output);
        } while (m_dstate.avail_out == 0);

        return lib::error_code();
    }

    /// decompress bytes
    /**
     * @param buf byte buffer to decompress
     * @param len length of buf
     * @param out string to append decompressed bytes to
     * @return error or status code
     */
    lib::error_code decompress(uint8_t const * buf, size_t len, std::string &
        out)
    {
        if (!m_initialized) {
            return make_error_code(error::uninitialized);
        }

        int ret;

        m_istate.avail_in = len;
        m_istate.next_in = const_cast<unsigned char *>(buf);

        do {
            m_istate.avail_out = m_compress_buffer_size;
            m_istate.next_out = m_compress_buffer.get();

            ret = inflate(&m_istate, z_sync_flush);

            if (ret == z_need_dict || ret == z_data_error || ret == z_mem_error) {
                return make_error_code(error::zlib_error);
            }

            out.append(
                reinterpret_cast<char *>(m_compress_buffer.get()),
                m_compress_buffer_size - m_istate.avail_out
            );
        } while (m_istate.avail_out == 0);

        return lib::error_code();
    }
private:
    /// generate negotiation response
    /**
     * @return generate extension negotiation reponse string to send to client
     */
    std::string generate_response() {
        std::string ret = "permessage-deflate";

        if (m_s2c_no_context_takeover) {
            ret += "; s2c_no_context_takeover";
        }

        if (m_c2s_no_context_takeover) {
            ret += "; c2s_no_context_takeover";
        }

        if (m_s2c_max_window_bits < default_s2c_max_window_bits) {
            std::stringstream s;
            s << int(m_s2c_max_window_bits);
            ret += "; s2c_max_window_bits="+s.str();
        }

        if (m_c2s_max_window_bits < default_c2s_max_window_bits) {
            std::stringstream s;
            s << int(m_c2s_max_window_bits);
            ret += "; c2s_max_window_bits="+s.str();
        }

        return ret;
    }

    /// negotiate s2c_no_context_takeover attribute
    /**
     * @param [in] value the value of the attribute from the offer
     * @param [out] ec a reference to the error code to return errors via
     */
    void negotiate_s2c_no_context_takeover(std::string const & value,
        lib::error_code & ec)
    {
        if (!value.empty()) {
            ec = make_error_code(error::invalid_attribute_value);
            return;
        }

        m_s2c_no_context_takeover = true;
    }

    /// negotiate c2s_no_context_takeover attribute
    /**
     * @param [in] value the value of the attribute from the offer
     * @param [out] ec a reference to the error code to return errors via
     */
    void negotiate_c2s_no_context_takeover(std::string const & value,
        lib::error_code & ec)
    {
        if (!value.empty()) {
            ec = make_error_code(error::invalid_attribute_value);
            return;
        }

        m_c2s_no_context_takeover = true;
    }

    /// negotiate s2c_max_window_bits attribute
    /**
     * when this method starts, m_s2c_max_window_bits will contain the server's
     * preferred value and m_s2c_max_window_bits_mode will contain the mode the
     * server wants to use to for negotiation. `value` contains the value the
     * client requested that we use.
     *
     * options:
     * - decline (refuse to use the attribute)
     * - accept (use whatever the client says)
     * - largest (use largest possible value)
     * - smallest (use smallest possible value)
     *
     * @param [in] value the value of the attribute from the offer
     * @param [out] ec a reference to the error code to return errors via
     */
    void negotiate_s2c_max_window_bits(std::string const & value,
        lib::error_code & ec)
    {
        uint8_t bits = uint8_t(atoi(value.c_str()));

        if (bits < min_s2c_max_window_bits || bits > max_s2c_max_window_bits) {
            ec = make_error_code(error::invalid_attribute_value);
            m_s2c_max_window_bits = default_s2c_max_window_bits;
            return;
        }

        switch (m_s2c_max_window_bits_mode) {
            case mode::decline:
                m_s2c_max_window_bits = default_s2c_max_window_bits;
                break;
            case mode::accept:
                m_s2c_max_window_bits = bits;
                break;
            case mode::largest:
                m_s2c_max_window_bits = (std::min)(bits,m_s2c_max_window_bits);
                break;
            case mode::smallest:
                m_s2c_max_window_bits = min_s2c_max_window_bits;
                break;
            default:
                ec = make_error_code(error::invalid_mode);
                m_s2c_max_window_bits = default_s2c_max_window_bits;
        }
    }

    /// negotiate c2s_max_window_bits attribute
    /**
     * when this method starts, m_c2s_max_window_bits and m_c2s_max_window_mode
     * will contain the server's preferred values for window size and
     * negotiation mode.
     *
     * options:
     * - decline (refuse to use the attribute)
     * - accept (use whatever the client says)
     * - largest (use largest possible value)
     * - smallest (use smallest possible value)
     *
     * @param [in] value the value of the attribute from the offer
     * @param [out] ec a reference to the error code to return errors via
     */
    void negotiate_c2s_max_window_bits(std::string const & value,
            lib::error_code & ec)
    {
        uint8_t bits = uint8_t(atoi(value.c_str()));

        if (value.empty()) {
            bits = default_c2s_max_window_bits;
        } else if (bits < min_c2s_max_window_bits ||
                   bits > max_c2s_max_window_bits)
        {
            ec = make_error_code(error::invalid_attribute_value);
            m_c2s_max_window_bits = default_c2s_max_window_bits;
            return;
        }

        switch (m_c2s_max_window_bits_mode) {
            case mode::decline:
                m_c2s_max_window_bits = default_c2s_max_window_bits;
                break;
            case mode::accept:
                m_c2s_max_window_bits = bits;
                break;
            case mode::largest:
                m_c2s_max_window_bits = std::min(bits,m_c2s_max_window_bits);
                break;
            case mode::smallest:
                m_c2s_max_window_bits = min_c2s_max_window_bits;
                break;
            default:
                ec = make_error_code(error::invalid_mode);
                m_c2s_max_window_bits = default_c2s_max_window_bits;
        }
    }

    bool m_enabled;
    bool m_s2c_no_context_takeover;
    bool m_c2s_no_context_takeover;
    uint8_t m_s2c_max_window_bits;
    uint8_t m_c2s_max_window_bits;
    mode::value m_s2c_max_window_bits_mode;
    mode::value m_c2s_max_window_bits_mode;

    bool m_initialized;
    size_t m_compress_buffer_size;
    lib::unique_ptr_uchar_array m_compress_buffer;
    z_stream m_dstate;
    z_stream m_istate;
};

} // namespace permessage_deflate
} // namespace extensions
} // namespace websocketpp

#endif // websocketpp_processor_extension_permessagedeflate_hpp
