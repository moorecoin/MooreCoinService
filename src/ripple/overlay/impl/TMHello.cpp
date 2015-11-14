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
#include <ripple/app/ledger/ledgermaster.h>
#include <ripple/app/main/application.h>
#include <ripple/app/main/localcredentials.h>
#include <ripple/app/misc/networkops.h>
#include <ripple/protocol/buildinfo.h>
#include <ripple/overlay/impl/tmhello.h>
#include <beast/crypto/base64.h>
#include <beast/http/rfc2616.h>
#include <beast/module/core/text/lexicalcast.h>
#include <boost/regex.hpp>
#include <algorithm>

// vfalco shouldn't we have to include the openssl
// headers or something for ssl_get_finished?

namespace ripple {

/** hashes the latest finished message from an ssl stream
    @param sslsession the session to get the message from.
    @param hash       the buffer into which the hash of the retrieved
                        message will be saved. the buffer must be at least
                        64 bytes long.
    @param getmessage a pointer to the function to call to retrieve the
                        finished message. this be either:
                        `ssl_get_finished` or
                        `ssl_get_peer_finished`.
    @return `true` if successful, `false` otherwise.
*/
static
bool
hashlastmessage (ssl const* ssl, unsigned char* hash,
    size_t (*get)(const ssl *, void *buf, size_t))
{
    enum
    {
        sslminimumfinishedlength = 12
    };
    unsigned char buf[1024];
    std::memset(hash, 0, 64);
    size_t len = get (ssl, buf, sizeof (buf));
    if(len < sslminimumfinishedlength)
        return false;
    sha512 (buf, len, hash);
    return true;
}

std::pair<uint256, bool>
makesharedvalue (ssl* ssl, beast::journal journal)
{
    std::pair<uint256, bool> result = { {}, false };

    unsigned char sha1[64];
    unsigned char sha2[64];

    if (!hashlastmessage(ssl, sha1, ssl_get_finished))
    {
        journal.error << "cookie generation: local setup not complete";
        return result;
    }

    if (!hashlastmessage(ssl, sha2, ssl_get_peer_finished))
    {
        journal.error << "cookie generation: peer setup not complete";
        return result;
    }

    // if both messages hash to the same value (i.e. match) something is
    // wrong. this would cause the resulting cookie to be 0.
    if (memcmp (sha1, sha2, sizeof (sha1)) == 0)
    {
        journal.error << "cookie generation: identical finished messages";
        return result;
    }

    for (size_t i = 0; i < sizeof (sha1); ++i)
        sha1[i] ^= sha2[i];

    // finally, derive the actual cookie for the values that
    // we have calculated.
    result.first = serializer::getsha512half (sha1, sizeof(sha1));
    result.second = true;
    return result;
}

protocol::tmhello
buildhello (uint256 const& sharedvalue, application& app)
{
    protocol::tmhello h;

    blob vchsig;
    app.getlocalcredentials ().getnodeprivate ().signnodeprivate (
        sharedvalue, vchsig);

    h.set_protoversion (to_packed (buildinfo::getcurrentprotocol()));
    h.set_protoversionmin (to_packed (buildinfo::getminimumprotocol()));
    h.set_fullversion (buildinfo::getfullversionstring ());
    h.set_nettime (app.getops ().getnetworktimenc ());
    h.set_nodepublic (app.getlocalcredentials ().getnodepublic (
        ).humannodepublic ());
    h.set_nodeproof (&vchsig[0], vchsig.size ());
    // h.set_ipv4port (portnumber); // ignored now
    h.set_testnet (false);

    // we always advertise ourselves as private in the hello message. this
    // suppresses the old peer advertising code and allows peerfinder to
    // take over the functionality.
    h.set_nodeprivate (true);

    auto const closedledger = app.getledgermaster().getclosedledger();

    if (closedledger && closedledger->isclosed ())
    {
        uint256 hash = closedledger->gethash ();
        h.set_ledgerclosed (hash.begin (), hash.size ());
        hash = closedledger->getparenthash ();
        h.set_ledgerprevious (hash.begin (), hash.size ());
    }

    return h;
}

void
appendhello (beast::http::message& m,
    protocol::tmhello const& hello)
{
    auto& h = m.headers;

    //h.append ("protocol-versions",...

    h.append ("public-key", hello.nodepublic());

    h.append ("session-signature", beast::base64_encode (
        hello.nodeproof()));

    if (hello.has_nettime())
        h.append ("network-time", std::to_string (hello.nettime()));

    if (hello.has_ledgerindex())
        h.append ("ledger", std::to_string (hello.ledgerindex()));

    if (hello.has_ledgerclosed())
        h.append ("closed-ledger", beast::base64_encode (
            hello.ledgerclosed()));

    if (hello.has_ledgerprevious())
        h.append ("previous-ledger", beast::base64_encode (
            hello.ledgerprevious()));
}

std::vector<protocolversion>
parse_protocolversions (std::string const& s)
{
    static boost::regex const re (
        "^"                  // start of line
        "rtxp/"              // the string "rtxp/"
        "([1-9][0-9]*)"      // a number (non-zero and with no leading zeroes)
        "\\."                // a period
        "(0|[1-9][0-9]*)"    // a number (no leading zeroes unless exactly zero)
        "$"                  // the end of the string
        , boost::regex_constants::optimize);

    auto const list = beast::rfc2616::split_commas (s);
    std::vector<protocolversion> result;
    for (auto const& s : list)
    {
        boost::smatch m;
        if (! boost::regex_match (s, m, re))
            continue;
        int major;
        int minor;
        if (! beast::lexicalcastchecked (
                major, std::string (m[1])))
            continue;
        if (! beast::lexicalcastchecked (
                minor, std::string (m[2])))
            continue;
        result.push_back (std::make_pair (major, minor));
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique (result.begin(), result.end()), result.end());
    return result;
}

std::pair<protocol::tmhello, bool>
parsehello (beast::http::message const& m, beast::journal journal)
{
    auto const& h = m.headers;
    std::pair<protocol::tmhello, bool> result = { {}, false };
    protocol::tmhello& hello = result.first;

    // protocol version in tmhello is obsolete,
    // it is supplanted by the values in the headers.

    {
        // required
        auto const iter = h.find ("upgrade");
        if (iter == h.end())
            return result;
        auto const versions = parse_protocolversions(iter->second);
        if (versions.empty())
            return result;
        hello.set_protoversion(
            (static_cast<std::uint32_t>(versions.back().first) << 16) |
            (static_cast<std::uint32_t>(versions.back().second)));
        hello.set_protoversionmin(
            (static_cast<std::uint32_t>(versions.front().first) << 16) |
            (static_cast<std::uint32_t>(versions.front().second)));
    }

    {
        // required
        auto const iter = h.find ("public-key");
        if (iter == h.end())
            return result;
        rippleaddress addr;
        addr.setnodepublic (iter->second);
        if (! addr.isvalid())
            return result;
        hello.set_nodepublic (iter->second);
    }

    {
        // required
        auto const iter = h.find ("session-signature");
        if (iter == h.end())
            return result;
        // todo security review
        hello.set_nodeproof (beast::base64_decode (iter->second));
    }

    {
        auto const iter = h.find (m.request() ?
            "user-agent" : "server");
        if (iter != h.end())
            hello.set_fullversion (iter->second);
    }

    {
        auto const iter = h.find ("network-time");
        if (iter != h.end())
        {
            std::uint64_t nettime;
            if (! beast::lexicalcastchecked(nettime, iter->second))
                return result;
            hello.set_nettime (nettime);
        }
    }

    {
        auto const iter = h.find ("ledger");
        if (iter != h.end())
        {
            ledgerindex ledgerindex;
            if (! beast::lexicalcastchecked(ledgerindex, iter->second))
                return result;
            hello.set_ledgerindex (ledgerindex);
        }
    }

    {
        auto const iter = h.find ("closed-ledger");
        if (iter != h.end())
            hello.set_ledgerclosed (beast::base64_decode (iter->second));
    }

    {
        auto const iter = h.find ("previous-ledger");
        if (iter != h.end())
            hello.set_ledgerprevious (beast::base64_decode (iter->second));
    }

    result.second = true;
    return result;
}

std::pair<rippleaddress, bool>
verifyhello (protocol::tmhello const& h, uint256 const& sharedvalue,
    beast::journal journal, application& app)
{
    std::pair<rippleaddress, bool> result = { {}, false };
    std::uint32_t const ourtime = app.getops().getnetworktimenc();
    std::uint32_t const mintime = ourtime - clocktolerancedeltaseconds;
    std::uint32_t const maxtime = ourtime + clocktolerancedeltaseconds;

#ifdef beast_debug
    if (h.has_nettime ())
    {
        std::int64_t to = ourtime;
        to -= h.nettime ();
        journal.debug <<
            "connect: time offset " << to;
    }
#endif

    auto const protocol = buildinfo::make_protocol(h.protoversion());

    if (h.has_nettime () &&
        ((h.nettime () < mintime) || (h.nettime () > maxtime)))
    {
        if (h.nettime () > maxtime)
        {
            journal.info <<
                "clock for is off by +" << h.nettime() - ourtime;
        }
        else if (h.nettime () < mintime)
        {
            journal.info <<
                "clock is off by -" << ourtime - h.nettime();
        }
    }
    else if (h.protoversionmin () > to_packed (
        buildinfo::getcurrentprotocol()))
    {
        journal.info <<
            "hello: disconnect: protocol mismatch [" <<
            "peer expects " << to_string (protocol) <<
            " and we run " << to_string (buildinfo::getcurrentprotocol()) << "]";
    }
    else if (! result.first.setnodepublic (h.nodepublic()))
    {
        journal.info <<
            "hello: disconnect: bad node public key.";
    }
    else if (! result.first.verifynodepublic (
        sharedvalue, h.nodeproof (), ecdsa::not_strict))
    {
        // unable to verify they have private key for claimed public key.
        journal.info <<
            "hello: disconnect: failed to verify session.";
    }
    else
    {
        // successful connection.
        result.second = true;
    }
    return result;
}

}
