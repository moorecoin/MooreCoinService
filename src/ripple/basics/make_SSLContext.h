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

#ifndef ripple_basics_make_sslcontext_h_included
#define ripple_basics_make_sslcontext_h_included

#include <boost/asio/ssl/context.hpp>
#include <beast/utility/noexcept.h>
#include <string>

namespace ripple {

/** retrieve raw dh parameters.
    this is in the format expected by the openssl function d2i_dhparams.
    the vector is binary. an empty vector means the key size is unsupported.
    @note the string may contain nulls in the middle. use size() to
            determine the actual size.
*/
std::string
getrawdhparams(int keysize);

/** create a self-signed ssl context that allows anonymous diffie hellman. */
std::shared_ptr<boost::asio::ssl::context>
make_sslcontext();

/** create an authenticated ssl context using the specified files. */
std::shared_ptr<boost::asio::ssl::context>
make_sslcontextauthed (std::string const& key_file,
    std::string const& cert_file, std::string const& chain_file);

}

#endif
