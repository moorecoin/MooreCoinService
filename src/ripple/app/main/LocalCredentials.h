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

#ifndef ripple_localcredentials_h
#define ripple_localcredentials_h

#include <ripple/protocol/rippleaddress.h>
#include <mutex>
#include <string>

namespace ripple {

/** holds the cryptographic credentials identifying this instance of the server. */
class localcredentials
{
public:
    localcredentials () = default;
    localcredentials (localcredentials const&) = delete;
    localcredentials& operator= (localcredentials const&) = delete;

    // begin processing.
    // - maintain peer connectivity through validation and peer management.
    void start ();

    rippleaddress const& getnodepublic () const
    {
        return mnodepublickey;
    }

    rippleaddress const& getnodeprivate () const
    {
        return mnodeprivatekey;
    }

    // local persistence of rpc clients
    bool datadelete (std::string const& strkey);

    // vfalco note why is strvalue non-const?
    bool datafetch (std::string const& strkey, std::string& strvalue);
    bool datastore (std::string const& strkey, std::string const& strvalue);

private:
    bool nodeidentityload ();
    bool nodeidentitycreate ();

private:
    std::recursive_mutex mlock;

    rippleaddress mnodepublickey;
    rippleaddress mnodeprivatekey;
};

} // ripple

#endif
