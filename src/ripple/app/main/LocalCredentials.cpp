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
#include <ripple/app/data/databasecon.h>
#include <ripple/app/main/application.h>
#include <ripple/app/main/localcredentials.h>
#include <ripple/app/peers/uniquenodelist.h>
#include <ripple/basics/log.h>
#include <ripple/basics/stringutilities.h>
#include <ripple/basics/make_sslcontext.h>
#include <ripple/core/config.h>
#include <iostream>

namespace ripple {

void localcredentials::start ()
{
    // we need our node identity before we begin networking.
    // - allows others to identify if they have connected multiple times.
    // - determines our cas routing and responsibilities.
    // - this is not our validation identity.
    if (!nodeidentityload ())
    {
        nodeidentitycreate ();

        if (!nodeidentityload ())
            throw std::runtime_error ("unable to retrieve new node identity.");
    }

    if (!getconfig ().quiet)
        std::cerr << "nodeidentity: " << mnodepublickey.humannodepublic () << std::endl;

    getapp().getunl ().start ();
}

// retrieve network identity.
bool localcredentials::nodeidentityload ()
{
    auto db = getapp().getwalletdb ().getdb ();
    auto sl (getapp().getwalletdb ().lock ());
    bool        bsuccess    = false;

    if (db->executesql ("select * from nodeidentity;") && db->startiterrows ())
    {
        std::string strpublickey, strprivatekey;

        db->getstr ("publickey", strpublickey);
        db->getstr ("privatekey", strprivatekey);

        mnodepublickey.setnodepublic (strpublickey);
        mnodeprivatekey.setnodeprivate (strprivatekey);

        db->enditerrows ();
        bsuccess    = true;
    }

    if (getconfig ().node_pub.isvalid () && getconfig ().node_priv.isvalid ())
    {
        mnodepublickey = getconfig ().node_pub;
        mnodeprivatekey = getconfig ().node_priv;
    }

    return bsuccess;
}

// create and store a network identity.
bool localcredentials::nodeidentitycreate ()
{
    if (!getconfig ().quiet)
        std::cerr << "nodeidentity: creating." << std::endl;

    //
    // generate the public and private key
    //
    rippleaddress   naseed          = rippleaddress::createseedrandom ();
    rippleaddress   nanodepublic    = rippleaddress::createnodepublic (naseed);
    rippleaddress   nanodeprivate   = rippleaddress::createnodeprivate (naseed);

    // make new key.
    std::string strdh512 (getrawdhparams (512));

    std::string strdh1024 = strdh512;

    //
    // store the node information
    //
    auto db = getapp().getwalletdb ().getdb ();

    auto sl (getapp().getwalletdb ().lock ());
    db->executesql (str (boost::format ("insert into nodeidentity (publickey,privatekey,dh512,dh1024) values ('%s','%s',%s,%s);")
                         % nanodepublic.humannodepublic ()
                         % nanodeprivate.humannodeprivate ()
                         % sqlescape (strdh512)
                         % sqlescape (strdh1024)));
    // xxx check error result.

    if (!getconfig ().quiet)
        std::cerr << "nodeidentity: created." << std::endl;

    return true;
}

bool localcredentials::datadelete (std::string const& strkey)
{
    auto db = getapp().getrpcdb ().getdb ();

    auto sl (getapp().getrpcdb ().lock ());

    return db->executesql (str (boost::format ("delete from rpcdata where key=%s;")
                                % sqlescape (strkey)));
}

bool localcredentials::datafetch (std::string const& strkey, std::string& strvalue)
{
    auto db = getapp().getrpcdb ().getdb ();

    auto sl (getapp().getrpcdb ().lock ());

    bool        bsuccess    = false;

    if (db->executesql (str (boost::format ("select value from rpcdata where key=%s;")
                             % sqlescape (strkey))) && db->startiterrows ())
    {
        blob vucdata    = db->getbinary ("value");
        strvalue.assign (vucdata.begin (), vucdata.end ());

        db->enditerrows ();

        bsuccess    = true;
    }

    return bsuccess;
}

bool localcredentials::datastore (std::string const& strkey, std::string const& strvalue)
{
    auto db = getapp().getrpcdb ().getdb ();

    auto sl (getapp().getrpcdb ().lock ());

    return (db->executesql (str (boost::format ("replace into rpcdata (key, value) values (%s,%s);")
                                 % sqlescape (strkey)
                                 % sqlescape (strvalue)
                                )));
}

} // ripple
