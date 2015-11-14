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

// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2011 the bitcoin developers
// distributed under the mit/x11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.

#ifndef ripple_crypto_cautobn_ctx_h_included
#define ripple_crypto_cautobn_ctx_h_included

#include <openssl/bn.h>

#include <stdexcept>
#include <string>

namespace ripple {

class cautobn_ctx
{
protected:
    bn_ctx* pctx;

public:
    cautobn_ctx ()
    {
        pctx = bn_ctx_new ();

        if (pctx == nullptr)
            throw std::runtime_error ("cautobn_ctx : bn_ctx_new() returned nullptr");
    }

    ~cautobn_ctx ()
    {
        if (pctx != nullptr)
            bn_ctx_free (pctx);
    }

    cautobn_ctx (cautobn_ctx const&) = delete;
    cautobn_ctx& operator= (cautobn_ctx const&) = delete;

    operator bn_ctx* ()
    {
        return pctx;
    }
    bn_ctx& operator* ()
    {
        return *pctx;
    }
    bn_ctx** operator& ()
    {
        return &pctx;
    }
    bool operator! ()
    {
        return (pctx == nullptr);
    }
};

}

#endif
