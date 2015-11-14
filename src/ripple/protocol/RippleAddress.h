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

#ifndef ripple_protocol_rippleaddress_h_included
#define ripple_protocol_rippleaddress_h_included

#include <ripple/crypto/base58data.h>
#include <ripple/crypto/ecdsacanonical.h>
#include <ripple/protocol/ripplepublickey.h>
#include <ripple/protocol/uint160.h>
#include <ripple/protocol/uinttypes.h>

namespace ripple {

//
// used to hold addresses and parse and produce human formats.
//
// xxx this needs to be reworked to store data in uint160 and uint256.

class rippleaddress : private cbase58data
{
private:
    typedef enum
    {
        ver_none                = 1,
        ver_node_public         = 28,
        ver_node_private        = 32,
        ver_account_id          = 0,
        ver_account_public      = 35,
        ver_account_private     = 34,
        ver_family_generator    = 41,
        ver_family_seed         = 33,
    } versionencoding;

    bool    misvalid;

public:
    rippleaddress ();

    // for public and private key, checks if they are legal.
    bool isvalid () const
    {
        return misvalid;
    }

    void clear ();
    bool isset () const;

    static void clearcache ();

    /** returns the public key.
        precondition: version == ver_node_public
    */
    ripplepublickey
    topublickey() const;

    //
    // node public - also used for validators
    //
    nodeid getnodeid () const;
    blob const& getnodepublic () const;

    std::string humannodepublic () const;

    bool setnodepublic (std::string const& strpublic);
    void setnodepublic (blob const& vpublic);
    bool verifynodepublic (uint256 const& hash, blob const& vchsig,
                           ecdsa mustbefullycanonical) const;
    bool verifynodepublic (uint256 const& hash, std::string const& strsig,
                           ecdsa mustbefullycanonical) const;

    static rippleaddress createnodepublic (rippleaddress const& naseed);
    static rippleaddress createnodepublic (blob const& vpublic);
    static rippleaddress createnodepublic (std::string const& strpublic);

    //
    // node private
    //
    blob const& getnodeprivatedata () const;
    uint256 getnodeprivate () const;

    std::string humannodeprivate () const;

    bool setnodeprivate (std::string const& strprivate);
    void setnodeprivate (blob const& vprivate);
    void setnodeprivate (uint256 hash256);
    void signnodeprivate (uint256 const& hash, blob& vchsig) const;

    static rippleaddress createnodeprivate (rippleaddress const& naseed);

    //
    // accounts ids
    //
    account getaccountid () const;

    std::string humanaccountid () const;

    bool setaccountid (
        std::string const& straccountid,
        base58::alphabet const& alphabet = base58::getripplealphabet());
    void setaccountid (account const& hash160in);

    static rippleaddress createaccountid (account const& uiaccountid);

    //
    // accounts public
    //
    blob const& getaccountpublic () const;

    std::string humanaccountpublic () const;

    bool setaccountpublic (std::string const& strpublic);
    void setaccountpublic (blob const& vpublic);
    void setaccountpublic (rippleaddress const& generator, int seq);

    bool accountpublicverify (uint256 const& uhash, blob const& vucsig,
                              ecdsa mustbefullycanonical) const;

    static rippleaddress createaccountpublic (blob const& vpublic)
    {
        rippleaddress nanew;
        nanew.setaccountpublic (vpublic);
        return nanew;
    }

    static std::string createhumanaccountpublic (blob const& vpublic)
    {
        return createaccountpublic (vpublic).humanaccountpublic ();
    }

    // create a deterministic public key from a public generator.
    static rippleaddress createaccountpublic (
        rippleaddress const& nagenerator, int iseq);

    //
    // accounts private
    //
    uint256 getaccountprivate () const;

    bool setaccountprivate (std::string const& strprivate);
    void setaccountprivate (blob const& vprivate);
    void setaccountprivate (uint256 hash256);
    void setaccountprivate (rippleaddress const& nagenerator,
                            rippleaddress const& naseed, int seq);

    bool accountprivatesign (uint256 const& uhash, blob& vucsig) const;

    // encrypt a message.
    blob accountprivateencrypt (
        rippleaddress const& napublicto, blob const& vucplaintext) const;

    // decrypt a message.
    blob accountprivatedecrypt (
        rippleaddress const& napublicfrom, blob const& vucciphertext) const;

    static rippleaddress createaccountprivate (
        rippleaddress const& generator, rippleaddress const& seed, int iseq);

    static rippleaddress createaccountprivate (blob const& vprivate)
    {
        rippleaddress   nanew;

        nanew.setaccountprivate (vprivate);

        return nanew;
    }

    //
    // generators
    // use to generate a master or regular family.
    //
    blob const& getgenerator () const;

    std::string humangenerator () const;

    bool setgenerator (std::string const& strgenerator);
    void setgenerator (blob const& vpublic);
    // void setgenerator(rippleaddress const& seed);

    // create generator for making public deterministic keys.
    static rippleaddress creategeneratorpublic (rippleaddress const& naseed);

    //
    // seeds
    // clients must disallow reconizable entries from being seeds.
    uint128 getseed () const;

    std::string humanseed () const;
    std::string humanseed1751 () const;

    bool setseed (std::string const& strseed);
    int setseed1751 (std::string const& strhuman1751);
    bool setseedgeneric (std::string const& strtext);
    void setseed (uint128 hash128);
    void setseedrandom ();

    static rippleaddress createseedrandom ();
    static rippleaddress createseedgeneric (std::string const& strtext);

    std::string tostring () const
        {return static_cast<cbase58data const&>(*this).tostring();}

    template <class hasher>
    friend
    void
    hash_append(hasher& hasher, rippleaddress const& value)
    {
        using beast::hash_append;
        hash_append(hasher, static_cast<cbase58data const&>(value));
    }

    friend
    bool
    operator==(rippleaddress const& lhs, rippleaddress const& rhs)
    {
        return static_cast<cbase58data const&>(lhs) ==
               static_cast<cbase58data const&>(rhs);
    }

    friend
    bool
    operator <(rippleaddress const& lhs, rippleaddress const& rhs)
    {
        return static_cast<cbase58data const&>(lhs) <
               static_cast<cbase58data const&>(rhs);
    }
};

//------------------------------------------------------------------------------

inline
bool
operator!=(rippleaddress const& lhs, rippleaddress const& rhs)
{
    return !(lhs == rhs);
}

inline
bool
operator >(rippleaddress const& lhs, rippleaddress const& rhs)
{
    return rhs < lhs;
}

inline
bool
operator<=(rippleaddress const& lhs, rippleaddress const& rhs)
{
    return !(rhs < lhs);
}

inline
bool
operator>=(rippleaddress const& lhs, rippleaddress const& rhs)
{
    return !(lhs < rhs);
}

} // ripple

#endif
