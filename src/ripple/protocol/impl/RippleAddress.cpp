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
#include <ripple/basics/log.h>
#include <ripple/basics/stringutilities.h>
#include <ripple/crypto/ecdsa.h>
#include <ripple/crypto/ecies.h>
#include <ripple/crypto/generatedeterministickey.h>
#include <ripple/crypto/randomnumbers.h>
#include <ripple/crypto/rfc1751.h>
#include <ripple/protocol/rippleaddress.h>
#include <ripple/protocol/serializer.h>
#include <ripple/protocol/ripplepublickey.h>
#include <beast/unit_test/suite.h>
#include <openssl/ripemd.h>
#include <openssl/bn.h>
#include <openssl/pem.h>
#include <mutex>

namespace ripple {

static bignum* getsecretbn (const openssl::ec_key& keypair)
{
	// deprecated
	return bn_dup (ec_key_get0_private_key ((ec_key*) keypair.get()));
}

// <-- seed
static uint128 passphrasetokey (std::string const& passphrase)
{
    serializer s;

    s.addraw (passphrase);
    // nikb todo this caling sequence is a bit ugly; this should be improved.
    uint256 hash256 = s.getsha512half ();
    uint128 ret (uint128::fromvoid (hash256.data()));

    s.secureerase ();

    return ret;
}

static blob getpublickey (openssl::ec_key const& key)
{
	blob result (33);
	
	key.get_public_key (&result[0]);
	
	return result;
}

static bool verifysignature (blob const& pubkey, uint256 const& hash, blob const& sig, ecdsa fullycanonical)
{
	if (! iscanonicalecdsasig (sig, fullycanonical))
	{
		return false;
	}
	
	openssl::ec_key key = ecdsapublickey (pubkey);
	
	return key.valid() && ecdsaverify (hash, sig, key);
}

rippleaddress::rippleaddress ()
    : misvalid (false)
{
    nversion = ver_none;
}

void rippleaddress::clear ()
{
    nversion = ver_none;
    vchdata.clear ();
    misvalid = false;
}

bool rippleaddress::isset () const
{
    return nversion != ver_none;
}

//
// nodepublic
//

static uint160 hash160 (blob const& vch)
{
    uint256 hash1;
    sha256 (vch.data (), vch.size (), hash1.data ());

    uint160 hash2;
    ripemd160 (hash1.data (), hash1.size (), hash2.data ());

    return hash2;
}

rippleaddress rippleaddress::createnodepublic (rippleaddress const& naseed)
{
    rippleaddress   nanew;

    // yyy should there be a getpubkey() equiv that returns a uint256?
    nanew.setnodepublic (getpublickey (generaterootdeterministickey (naseed.getseed())));

    return nanew;
}

rippleaddress rippleaddress::createnodepublic (blob const& vpublic)
{
    rippleaddress   nanew;

    nanew.setnodepublic (vpublic);

    return nanew;
}

rippleaddress rippleaddress::createnodepublic (std::string const& strpublic)
{
    rippleaddress   nanew;

    nanew.setnodepublic (strpublic);

    return nanew;
}

ripplepublickey
rippleaddress::topublickey() const
{
    assert (nversion == ver_node_public);
    return ripplepublickey (vchdata.begin(), vchdata.end());
}

nodeid rippleaddress::getnodeid () const
{
    switch (nversion)
    {
    case ver_none:
        throw std::runtime_error ("unset source - getnodeid");

    case ver_node_public: {
        // note, we are encoding the left.
        nodeid node;
        node.copyfrom(hash160 (vchdata));
        return node;
    }

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nversion)));
    }
}

blob const& rippleaddress::getnodepublic () const
{
    switch (nversion)
    {
    case ver_none:
        throw std::runtime_error ("unset source - getnodepublic");

    case ver_node_public:
        return vchdata;

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nversion)));
    }
}

std::string rippleaddress::humannodepublic () const
{
    switch (nversion)
    {
    case ver_none:
        throw std::runtime_error ("unset source - humannodepublic");

    case ver_node_public:
        return tostring ();

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nversion)));
    }
}

bool rippleaddress::setnodepublic (std::string const& strpublic)
{
    misvalid = setstring (strpublic, ver_node_public, base58::getripplealphabet ());

    return misvalid;
}

void rippleaddress::setnodepublic (blob const& vpublic)
{
    misvalid        = true;

    setdata (ver_node_public, vpublic);
}

bool rippleaddress::verifynodepublic (uint256 const& hash, blob const& vchsig, ecdsa fullycanonical) const
{
    return verifysignature (getnodepublic(), hash, vchsig, fullycanonical);
}

bool rippleaddress::verifynodepublic (uint256 const& hash, std::string const& strsig, ecdsa fullycanonical) const
{
    blob vchsig (strsig.begin (), strsig.end ());

    return verifynodepublic (hash, vchsig, fullycanonical);
}

//
// nodeprivate
//

rippleaddress rippleaddress::createnodeprivate (rippleaddress const& naseed)
{
    rippleaddress   nanew;

    nanew.setnodeprivate (generaterootdeterministickey (naseed.getseed()).get_private_key());

    return nanew;
}

blob const& rippleaddress::getnodeprivatedata () const
{
    switch (nversion)
    {
    case ver_none:
        throw std::runtime_error ("unset source - getnodeprivatedata");

    case ver_node_private:
        return vchdata;

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nversion)));
    }
}

uint256 rippleaddress::getnodeprivate () const
{
    switch (nversion)
    {
    case ver_none:
        throw std::runtime_error ("unset source = getnodeprivate");

    case ver_node_private:
        return uint256 (vchdata);

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nversion)));
    }
}

std::string rippleaddress::humannodeprivate () const
{
    switch (nversion)
    {
    case ver_none:
        throw std::runtime_error ("unset source - humannodeprivate");

    case ver_node_private:
        return tostring ();

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nversion)));
    }
}

bool rippleaddress::setnodeprivate (std::string const& strprivate)
{
    misvalid = setstring (strprivate, ver_node_private, base58::getripplealphabet ());

    return misvalid;
}

void rippleaddress::setnodeprivate (blob const& vprivate)
{
    misvalid = true;

    setdata (ver_node_private, vprivate);
}

void rippleaddress::setnodeprivate (uint256 hash256)
{
    misvalid = true;

    setdata (ver_node_private, hash256);
}

void rippleaddress::signnodeprivate (uint256 const& hash, blob& vchsig) const
{
    openssl::ec_key key = ecdsaprivatekey (getnodeprivate());
    
    vchsig = ecdsasign (hash, key);

    if (vchsig.empty())
        throw std::runtime_error ("signing failed.");
}

//
// accountid
//

account rippleaddress::getaccountid () const
{
    switch (nversion)
    {
    case ver_none:
        throw std::runtime_error ("unset source - getaccountid");

    case ver_account_id:
        return account(vchdata);

    case ver_account_public: {
        // note, we are encoding the left.
        // todo(tom): decipher this comment.
        account account;
        account.copyfrom (hash160 (vchdata));
        return account;
    }

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nversion)));
    }
}

typedef std::mutex staticlocktype;
typedef std::lock_guard <staticlocktype> staticscopedlocktype;

static staticlocktype s_lock;
static hash_map <blob, std::string> rncmapold, rncmapnew;

void rippleaddress::clearcache ()
{
    staticscopedlocktype sl (s_lock);

    rncmapold.clear ();
    rncmapnew.clear ();
}

std::string rippleaddress::humanaccountid () const
{
    switch (nversion)
    {
    case ver_none:
        throw std::runtime_error ("unset source - humanaccountid");

    case ver_account_id:
    {
        std::string ret;

        {
            staticscopedlocktype sl (s_lock);

            auto it = rncmapnew.find (vchdata);

            if (it != rncmapnew.end ())
            {
                // found in new map, nothing to do
                ret = it->second;
            }
            else
            {
                it = rncmapold.find (vchdata);

                if (it != rncmapold.end ())
                {
                    ret = it->second;
                    rncmapold.erase (it);
                }
                else
                    ret = tostring ();

                if (rncmapnew.size () >= 128000)
                {
                    rncmapold = std::move (rncmapnew);
                    rncmapnew.clear ();
                    rncmapnew.reserve (128000);
                }

                rncmapnew[vchdata] = ret;
            }
        }

        return ret;
    }

    case ver_account_public:
    {
        rippleaddress   accountid;

        (void) accountid.setaccountid (getaccountid ());

        return accountid.tostring ();
    }

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nversion)));
    }
}

bool rippleaddress::setaccountid (
    std::string const& straccountid, base58::alphabet const& alphabet)
{
    if (straccountid.empty ())
    {
        setaccountid (account ());

        misvalid    = true;
    }
    else
    {
        misvalid = setstring (straccountid, ver_account_id, alphabet);
    }

    return misvalid;
}

void rippleaddress::setaccountid (account const& hash160)
{
    misvalid        = true;
    setdata (ver_account_id, hash160);
}

//
// accountpublic
//

rippleaddress rippleaddress::createaccountpublic (
    rippleaddress const& generator, int iseq)
{
    rippleaddress   nanew;

    nanew.setaccountpublic (generator, iseq);

    return nanew;
}

blob const& rippleaddress::getaccountpublic () const
{
    switch (nversion)
    {
    case ver_none:
        throw std::runtime_error ("unset source - getaccountpublic");

    case ver_account_id:
        throw std::runtime_error ("public not available from account id");
        break;

    case ver_account_public:
        return vchdata;

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nversion)));
    }
}

std::string rippleaddress::humanaccountpublic () const
{
    switch (nversion)
    {
    case ver_none:
        throw std::runtime_error ("unset source - humanaccountpublic");

    case ver_account_id:
        throw std::runtime_error ("public not available from account id");

    case ver_account_public:
        return tostring ();

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nversion)));
    }
}

bool rippleaddress::setaccountpublic (std::string const& strpublic)
{
    misvalid = setstring (strpublic, ver_account_public, base58::getripplealphabet ());

    return misvalid;
}

void rippleaddress::setaccountpublic (blob const& vpublic)
{
    misvalid = true;

    setdata (ver_account_public, vpublic);
}

void rippleaddress::setaccountpublic (rippleaddress const& generator, int seq)
{
    setaccountpublic (getpublickey (generatepublicdeterministickey (generator.getgenerator(), seq)));
}

bool rippleaddress::accountpublicverify (
    uint256 const& uhash, blob const& vucsig, ecdsa fullycanonical) const
{
    return verifysignature (getaccountpublic(), uhash, vucsig, fullycanonical);
}

rippleaddress rippleaddress::createaccountid (account const& account)
{
    rippleaddress   na;
    na.setaccountid (account);

    return na;
}

//
// accountprivate
//

rippleaddress rippleaddress::createaccountprivate (
    rippleaddress const& generator, rippleaddress const& naseed, int iseq)
{
    rippleaddress   nanew;

    nanew.setaccountprivate (generator, naseed, iseq);

    return nanew;
}

uint256 rippleaddress::getaccountprivate () const
{
    switch (nversion)
    {
    case ver_none:
        throw std::runtime_error ("unset source - getaccountprivate");

    case ver_account_private:
        return uint256 (vchdata);

    default:
        throw std::runtime_error ("bad source: " + std::to_string(nversion));
    }
}

bool rippleaddress::setaccountprivate (std::string const& strprivate)
{
    misvalid = setstring (
        strprivate, ver_account_private, base58::getripplealphabet ());

    return misvalid;
}

void rippleaddress::setaccountprivate (blob const& vprivate)
{
    misvalid = true;
    setdata (ver_account_private, vprivate);
}

void rippleaddress::setaccountprivate (uint256 hash256)
{
    misvalid = true;
    setdata (ver_account_private, hash256);
}

void rippleaddress::setaccountprivate (
    rippleaddress const& generator, rippleaddress const& naseed, int seq)
{
    openssl::ec_key publickey = generaterootdeterministickey (naseed.getseed());
    openssl::ec_key secretkey = generateprivatedeterministickey (generator.getgenerator(), getsecretbn (publickey), seq);
    
    setaccountprivate (secretkey.get_private_key());
}

bool rippleaddress::accountprivatesign (uint256 const& uhash, blob& vucsig) const
{
    openssl::ec_key key = ecdsaprivatekey (getaccountprivate());
    
    if (!key.valid())
    {
        // bad private key.
        writelog (lswarning, rippleaddress)
                << "accountprivatesign: bad private key.";
        
        return false;
    }
    
    vucsig = ecdsasign (uhash, key);
    const bool ok = !vucsig.empty();
    
	condlog (!ok, lswarning, rippleaddress)
			<< "accountprivatesign: signing failed.";

    return ok;
}

blob rippleaddress::accountprivateencrypt (
    rippleaddress const& napublicto, blob const& vucplaintext) const
{
    openssl::ec_key secretkey = ecdsaprivatekey (getaccountprivate());
    openssl::ec_key publickey = ecdsapublickey (napublicto.getaccountpublic());
    
    blob vucciphertext;

    if (! publickey.valid())
    {
        writelog (lswarning, rippleaddress)
                << "accountprivateencrypt: bad public key.";
    }
    
    if (! secretkey.valid())
    {
        writelog (lswarning, rippleaddress)
                << "accountprivateencrypt: bad private key.";
    }

    {
        try
        {
            vucciphertext = encryptecies (secretkey, publickey, vucplaintext);
        }
        catch (...)
        {
        }
    }

    return vucciphertext;
}

blob rippleaddress::accountprivatedecrypt (
    rippleaddress const& napublicfrom, blob const& vucciphertext) const
{
    openssl::ec_key secretkey = ecdsaprivatekey (getaccountprivate());
    openssl::ec_key publickey = ecdsapublickey (napublicfrom.getaccountpublic());
    
    blob    vucplaintext;

    if (! publickey.valid())
    {
        writelog (lswarning, rippleaddress)
                << "accountprivatedecrypt: bad public key.";
    }
    
    if (! secretkey.valid())
    {
        writelog (lswarning, rippleaddress)
                << "accountprivatedecrypt: bad private key.";
    }

    {
        try
        {
            vucplaintext = decryptecies (secretkey, publickey, vucciphertext);
        }
        catch (...)
        {
        }
    }

    return vucplaintext;
}

//
// generators
//

blob const& rippleaddress::getgenerator () const
{
    // returns the public generator
    switch (nversion)
    {
    case ver_none:
        throw std::runtime_error ("unset source - getgenerator");

    case ver_family_generator:
        // do nothing.
        return vchdata;

    default:
        throw std::runtime_error ("bad source: " + std::to_string(nversion));
    }
}

std::string rippleaddress::humangenerator () const
{
    switch (nversion)
    {
    case ver_none:
        throw std::runtime_error ("unset source - humangenerator");

    case ver_family_generator:
        return tostring ();

    default:
        throw std::runtime_error ("bad source: " + std::to_string(nversion));
    }
}

void rippleaddress::setgenerator (blob const& vpublic)
{
    misvalid        = true;
    setdata (ver_family_generator, vpublic);
}

rippleaddress rippleaddress::creategeneratorpublic (rippleaddress const& naseed)
{
    rippleaddress   nanew;
    nanew.setgenerator (getpublickey (generaterootdeterministickey (naseed.getseed())));
    return nanew;
}

//
// seed
//

uint128 rippleaddress::getseed () const
{
    switch (nversion)
    {
    case ver_none:
        throw std::runtime_error ("unset source - getseed");

    case ver_family_seed:
        return uint128 (vchdata);

    default:
        throw std::runtime_error ("bad source: " + std::to_string(nversion));
    }
}

std::string rippleaddress::humanseed1751 () const
{
    switch (nversion)
    {
    case ver_none:
        throw std::runtime_error ("unset source - humanseed1751");

    case ver_family_seed:
    {
        std::string strhuman;
        std::string strlittle;
        std::string strbig;
        uint128 useed   = getseed ();

        strlittle.assign (useed.begin (), useed.end ());

        strbig.assign (strlittle.rbegin (), strlittle.rend ());

        rfc1751::getenglishfromkey (strhuman, strbig);

        return strhuman;
    }

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nversion)));
    }
}

std::string rippleaddress::humanseed () const
{
    switch (nversion)
    {
    case ver_none:
        throw std::runtime_error ("unset source - humanseed");

    case ver_family_seed:
        return tostring ();

    default:
        throw std::runtime_error (str (boost::format ("bad source: %d") % int (nversion)));
    }
}

int rippleaddress::setseed1751 (std::string const& strhuman1751)
{
    std::string strkey;
    int         iresult = rfc1751::getkeyfromenglish (strkey, strhuman1751);

    if (1 == iresult)
    {
        blob    vchlittle (strkey.rbegin (), strkey.rend ());
        uint128     useed (vchlittle);

        setseed (useed);
    }

    return iresult;
}

bool rippleaddress::setseed (std::string const& strseed)
{
    misvalid = setstring (strseed, ver_family_seed, base58::getripplealphabet ());

    return misvalid;
}

bool rippleaddress::setseedgeneric (std::string const& strtext)
{
    rippleaddress   natemp;
    bool            bresult = true;
    uint128         useed;

    if (strtext.empty ()
            || natemp.setaccountid (strtext)
            || natemp.setaccountpublic (strtext)
            || natemp.setaccountprivate (strtext)
            || natemp.setnodepublic (strtext)
            || natemp.setnodeprivate (strtext))
    {
        bresult = false;
    }
    else if (strtext.length () == 32 && useed.sethex (strtext, true))
    {
        setseed (useed);
    }
    else if (setseed (strtext))
    {
        // log::out() << "recognized seed.";
    }
    else if (1 == setseed1751 (strtext))
    {
        // log::out() << "recognized 1751 seed.";
    }
    else
    {
        setseed (passphrasetokey (strtext));
    }

    return bresult;
}

void rippleaddress::setseed (uint128 hash128)
{
    misvalid = true;

    setdata (ver_family_seed, hash128);
}

void rippleaddress::setseedrandom ()
{
    // xxx maybe we should call makenewkey
    uint128 key;

    random_fill (key.begin (), key.size ());

    rippleaddress::setseed (key);
}

rippleaddress rippleaddress::createseedrandom ()
{
    rippleaddress   nanew;

    nanew.setseedrandom ();

    return nanew;
}

rippleaddress rippleaddress::createseedgeneric (std::string const& strtext)
{
    rippleaddress   nanew;

    nanew.setseedgeneric (strtext);

    return nanew;
}

} // ripple
