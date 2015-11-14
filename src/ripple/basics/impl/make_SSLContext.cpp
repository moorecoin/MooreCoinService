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
#include <ripple/basics/make_sslcontext.h>
#include <ripple/basics/seconds_clock.h>
#include <beast/container/aged_unordered_set.h>
#include <beast/module/core/diagnostic/fatalerror.h>
#include <beast/utility/static_initializer.h>
#include <cstdint>
#include <sstream>
#include <stdexcept>

namespace ripple {
namespace openssl {
namespace detail {

template <class>
struct custom_delete;

template <>
struct custom_delete <rsa>
{
    void operator() (rsa* rsa) const
    {
        rsa_free (rsa);
    }
};

template <>
struct custom_delete <evp_pkey>
{
    void operator() (evp_pkey* evp_pkey) const
    {
        evp_pkey_free (evp_pkey);
    }
};

template <>
struct custom_delete <x509>
{
    void operator() (x509* x509) const
    {
        x509_free (x509);
    }
};

template <>
struct custom_delete <dh>
{
    void operator() (dh* dh) const
    {
        dh_free(dh);
    }
};

template <class t>
using custom_delete_unique_ptr = std::unique_ptr <t, custom_delete <t>>;

// rsa

typedef custom_delete_unique_ptr <rsa> rsa_ptr;

static rsa_ptr rsa_generate_key (int n_bits)
{
    rsa* rsa = rsa_generate_key (n_bits, rsa_f4, nullptr, nullptr);

    if (rsa == nullptr)
    {
        throw std::runtime_error ("rsa_generate_key failed");
    }

    return rsa_ptr (rsa);
}

// evp_pkey

typedef custom_delete_unique_ptr <evp_pkey> evp_pkey_ptr;

static evp_pkey_ptr evp_pkey_new()
{
    evp_pkey* evp_pkey = evp_pkey_new();

    if (evp_pkey == nullptr)
    {
        throw std::runtime_error ("evp_pkey_new failed");
    }

    return evp_pkey_ptr (evp_pkey);
}

static void evp_pkey_assign_rsa (evp_pkey* evp_pkey, rsa_ptr&& rsa)
{
    if (! evp_pkey_assign_rsa (evp_pkey, rsa.get()))
    {
        throw std::runtime_error ("evp_pkey_assign_rsa failed");
    }

    rsa.release();
}

// x509

typedef custom_delete_unique_ptr <x509> x509_ptr;

static x509_ptr x509_new()
{
    x509* x509 = x509_new();

    if (x509 == nullptr)
    {
        throw std::runtime_error ("x509_new failed");
    }

    x509_set_version (x509, nid_x509);

    int const margin =                    60 * 60;  //      3600, one hour
    int const length = 10 * 365.25 * 24 * 60 * 60;  // 315576000, ten years
    
    x509_gmtime_adj (x509_get_notbefore (x509), -margin);
    x509_gmtime_adj (x509_get_notafter  (x509),  length);

    return x509_ptr (x509);
}

static void x509_set_pubkey (x509* x509, evp_pkey* evp_pkey)
{
    x509_set_pubkey (x509, evp_pkey);
}

static void x509_sign (x509* x509, evp_pkey* evp_pkey)
{
    if (! x509_sign (x509, evp_pkey, evp_sha1()))
    {
        throw std::runtime_error ("x509_sign failed");
    }
}

static void ssl_ctx_use_certificate (ssl_ctx* const ctx, x509_ptr& cert)
{
    if (ssl_ctx_use_certificate (ctx, cert.release()) <= 0)
    {
        throw std::runtime_error ("ssl_ctx_use_certificate failed");
    }
}

static void ssl_ctx_use_privatekey (ssl_ctx* const ctx, evp_pkey_ptr& key)
{
    if (ssl_ctx_use_privatekey (ctx, key.release()) <= 0)
    {
        throw std::runtime_error ("ssl_ctx_use_privatekey failed");
    }
}

// track when ssl connections have last negotiated
struct staticdata
{
    std::mutex lock;
    beast::aged_unordered_set <ssl const*> set;

    staticdata()
        : set (ripple::get_seconds_clock ())
        { }
};

using dh_ptr = custom_delete_unique_ptr<dh>;

static
dh_ptr
make_dh(std::string const& params)
{
    auto const* p (
        reinterpret_cast <std::uint8_t const*>(&params [0]));
    dh* const dh = d2i_dhparams (nullptr, &p, params.size ());
    if (p == nullptr)
        beast::fatalerror ("d2i_dhparams returned nullptr.",
            __file__, __line__);
    return dh_ptr(dh);
}

static
dh*
getdh (int keylength)
{
    if (keylength == 512 || keylength == 1024)
    {
        static dh_ptr dh512 = make_dh(getrawdhparams (keylength));
        return dh512.get ();
    }
    else
    {
        beast::fatalerror ("unsupported key length", __file__, __line__);
    }

    return nullptr;
}

static
dh*
tmp_dh_handler (ssl*, int, int key_length)
{
    return dhparams_dup (getdh (key_length));
}

static
bool
disallowrenegotiation (ssl const* ssl, bool isnew)
{
    // do not allow a connection to renegotiate
    // more than once every 4 minutes

    static beast::static_initializer <staticdata> static_data;

    auto& sd (static_data.get ());
    std::lock_guard <std::mutex> lock (sd.lock);
    auto const expired (sd.set.clock().now() - std::chrono::minutes(4));

    // remove expired entries
    for (auto iter (sd.set.chronological.begin ());
        (iter != sd.set.chronological.end ()) && (iter.when () <= expired);
        iter = sd.set.chronological.begin ())
    {
        sd.set.erase (iter);
    }

    auto iter = sd.set.find (ssl);
    if (iter != sd.set.end ())
    {
        if (! isnew)
        {
            // this is a renegotiation and the last negotiation was recent
            return true;
        }

        sd.set.touch (iter);
    }
    else
    {
        sd.set.emplace (ssl);
    }

    return false;
}

static
void
info_handler (ssl const* ssl, int event, int)
{
    if ((ssl->s3) && (event & ssl_cb_handshake_start))
    {
        if (disallowrenegotiation (ssl, ssl_in_before (ssl)))
            ssl->s3->flags |= ssl3_flags_no_renegotiate_ciphers;
    }
}

static
std::string
error_message (std::string const& what,
    boost::system::error_code const& ec)
{
    std::stringstream ss;
    ss <<
        what << ": " <<
        ec.message() <<
        " (" << ec.value() << ")";
    return ss.str();
}

static
void
initcommon (boost::asio::ssl::context& context)
{
    context.set_options (
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::no_sslv2 |
        boost::asio::ssl::context::no_sslv3 |
        boost::asio::ssl::context::single_dh_use);

    ssl_ctx_set_tmp_dh_callback (
        context.native_handle (),
        tmp_dh_handler);

    ssl_ctx_set_info_callback (
        context.native_handle (),
        info_handler);
}

static
void
initanonymous (
    boost::asio::ssl::context& context, std::string const& cipherlist)
{
    initcommon(context);
    int const result = ssl_ctx_set_cipher_list (
        context.native_handle (),
        cipherlist.c_str ());
    if (result != 1)
        throw std::invalid_argument("ssl_ctx_set_cipher_list failed");

    using namespace openssl;

    evp_pkey_ptr pkey = evp_pkey_new();
    evp_pkey_assign_rsa (pkey.get(), rsa_generate_key (2048));

    x509_ptr cert = x509_new();
    x509_set_pubkey (cert.get(), pkey.get());
    x509_sign       (cert.get(), pkey.get());

    ssl_ctx* const ctx = context.native_handle();
    ssl_ctx_use_certificate (ctx, cert);
    ssl_ctx_use_privatekey  (ctx, pkey);
}

static
void
initauthenticated (boost::asio::ssl::context& context,
    std::string key_file, std::string cert_file, std::string chain_file)
{
    initcommon (context);

    ssl_ctx* const ssl = context.native_handle ();

    bool cert_set = false;

    if (! cert_file.empty ())
    {
        boost::system::error_code ec;

        context.use_certificate_file (
            cert_file, boost::asio::ssl::context::pem, ec);

        if (ec)
        {
            beast::fatalerror (error_message (
                "problem with ssl certificate file.", ec).c_str(),
                __file__, __line__);
        }

        cert_set = true;
    }

    if (! chain_file.empty ())
    {
        // vfalco replace fopen() with raii
        file* f = fopen (chain_file.c_str (), "r");

        if (!f)
        {
            beast::fatalerror (error_message (
                "problem opening ssl chain file.", boost::system::error_code (errno,
                boost::system::generic_category())).c_str(),
                __file__, __line__);
        }

        try
        {
            for (;;)
            {
                x509* const x = pem_read_x509 (f, nullptr, nullptr, nullptr);

                if (x == nullptr)
                    break;

                if (! cert_set)
                {
                    if (ssl_ctx_use_certificate (ssl, x) != 1)
                        beast::fatalerror ("problem retrieving ssl certificate from chain file.",
                            __file__, __line__);

                    cert_set = true;
                }
                else if (ssl_ctx_add_extra_chain_cert (ssl, x) != 1)
                {
                    x509_free (x);
                    beast::fatalerror ("problem adding ssl chain certificate.",
                        __file__, __line__);
                }
            }

            fclose (f);
        }
        catch (...)
        {
            fclose (f);
            beast::fatalerror ("reading the ssl chain file generated an exception.",
                __file__, __line__);
        }
    }

    if (! key_file.empty ())
    {
        boost::system::error_code ec;

        context.use_private_key_file (key_file,
            boost::asio::ssl::context::pem, ec);

        if (ec)
        {
            beast::fatalerror (error_message (
                "problem using the ssl private key file.", ec).c_str(),
                __file__, __line__);
        }
    }

    if (ssl_ctx_check_private_key (ssl) != 1)
    {
        beast::fatalerror ("invalid key in ssl private key file.",
            __file__, __line__);
    }
}

} // detail
} // openssl

//------------------------------------------------------------------------------

std::string
getrawdhparams (int keysize)
{
    std::string params;

    // original code provided the 512-bit keysize parameters
    // when 1024 bits were requested so we will do the same.
    if (keysize == 1024)
        keysize = 512;

    switch (keysize)
    {
    case 512:
        {
            // these are the dh parameters that opencoin has chosen for ripple
            //
            std::uint8_t const raw [] = {
                0x30, 0x46, 0x02, 0x41, 0x00, 0x98, 0x15, 0xd2, 0xd0, 0x08, 0x32, 0xda,
                0xaa, 0xac, 0xc4, 0x71, 0xa3, 0x1b, 0x11, 0xf0, 0x6c, 0x62, 0xb2, 0x35,
                0x8a, 0x10, 0x92, 0xc6, 0x0a, 0xa3, 0x84, 0x7e, 0xaf, 0x17, 0x29, 0x0b,
                0x70, 0xef, 0x07, 0x4f, 0xfc, 0x9d, 0x6d, 0x87, 0x99, 0x19, 0x09, 0x5b,
                0x6e, 0xdb, 0x57, 0x72, 0x4a, 0x7e, 0xcd, 0xaf, 0xbd, 0x3a, 0x97, 0x55,
                0x51, 0x77, 0x5a, 0x34, 0x7c, 0xe8, 0xc5, 0x71, 0x63, 0x02, 0x01, 0x02
            };

            params.resize (sizeof (raw));
            std::copy (raw, raw + sizeof (raw), params.begin ());
        }
        break;
    };

    return params;
}

std::shared_ptr<boost::asio::ssl::context>
make_sslcontext()
{
    std::shared_ptr<boost::asio::ssl::context> context =
        std::make_shared<boost::asio::ssl::context> (
            boost::asio::ssl::context::sslv23);
    // by default, allow anonymous dh.
    openssl::detail::initanonymous (
        *context, "all:!low:!exp:!md5:@strength");
    // vfalco note, it seems the websocket context never has
    // set_verify_mode called, for either setting of websocket_secure
    context->set_verify_mode (boost::asio::ssl::verify_none);
    return context;
}

std::shared_ptr<boost::asio::ssl::context>
make_sslcontextauthed (std::string const& key_file,
    std::string const& cert_file, std::string const& chain_file)
{
    std::shared_ptr<boost::asio::ssl::context> context =
        std::make_shared<boost::asio::ssl::context> (
            boost::asio::ssl::context::sslv23);
    openssl::detail::initauthenticated(*context,
        key_file, cert_file, chain_file);
    return context;
}

} // ripple

