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

#ifndef ripple_crypto_cbignum_h_included
#define ripple_crypto_cbignum_h_included

#include <ripple/basics/base_uint.h>
#include <openssl/bn.h>

namespace ripple {

// vfalco todo figure out a way to remove the dependency on openssl in the
//         header. maybe rewrite this to use cryptopp.

class cbignum : public bignum
{
public:
    cbignum ();
    cbignum (const cbignum& b);
    cbignum& operator= (const cbignum& b);
    cbignum (char n);
    cbignum (short n);
    cbignum (int n);
    cbignum (long n);
    cbignum (long long n);
    cbignum (unsigned char n);
    cbignum (unsigned short n);
    cbignum (unsigned int n);
    cbignum (unsigned long long n);
    explicit cbignum (uint256 n);
    explicit cbignum (blob const& vch);
    cbignum (unsigned char const* begin, unsigned char const* end);
    ~cbignum ();

    void setuint (unsigned int n);
    unsigned int getuint () const;
    int getint () const;
    void setint64 (std::int64_t n);
    std::uint64_t getuint64 () const;
    void setuint64 (std::uint64_t n);
    void setuint256 (uint256 const& n);
    uint256 getuint256 ();
    void setvch (unsigned char const* begin, unsigned char const* end);
    void setvch (blob const& vch);
    blob getvch () const;
    cbignum& setcompact (unsigned int ncompact);
    unsigned int getcompact () const;
    void sethex (std::string const& str);
    std::string tostring (int nbase = 10) const;
    std::string gethex () const;
    bool operator! () const;
    cbignum& operator+= (const cbignum& b);
    cbignum& operator-= (const cbignum& b);
    cbignum& operator*= (const cbignum& b);
    cbignum& operator/= (const cbignum& b);
    cbignum& operator%= (const cbignum& b);
    cbignum& operator<<= (unsigned int shift);
    cbignum& operator>>= (unsigned int shift);
    cbignum& operator++ ();
    cbignum& operator-- ();
    const cbignum operator++ (int);
    const cbignum operator-- (int);

    friend inline const cbignum operator- (const cbignum& a, const cbignum& b);
    friend inline const cbignum operator/ (const cbignum& a, const cbignum& b);
    friend inline const cbignum operator% (const cbignum& a, const cbignum& b);

private:
    // private because the size of an unsigned long varies by platform

    void setulong (unsigned long n);
    unsigned long getulong () const;
};

const cbignum operator+ (const cbignum& a, const cbignum& b);
const cbignum operator- (const cbignum& a, const cbignum& b);
const cbignum operator- (const cbignum& a);
const cbignum operator* (const cbignum& a, const cbignum& b);
const cbignum operator/ (const cbignum& a, const cbignum& b);
const cbignum operator% (const cbignum& a, const cbignum& b);
const cbignum operator<< (const cbignum& a, unsigned int shift);
const cbignum operator>> (const cbignum& a, unsigned int shift);

bool operator== (const cbignum& a, const cbignum& b);
bool operator!= (const cbignum& a, const cbignum& b);
bool operator<= (const cbignum& a, const cbignum& b);
bool operator>= (const cbignum& a, const cbignum& b);
bool operator< (const cbignum& a, const cbignum& b);
bool operator> (const cbignum& a, const cbignum& b);

//------------------------------------------------------------------------------

// vfalco i believe only stamount uses these
int bn_add_word64 (bignum* a, std::uint64_t w);
int bn_sub_word64 (bignum* a, std::uint64_t w);
int bn_mul_word64 (bignum* a, std::uint64_t w);
std::uint64_t bn_div_word64 (bignum* a, std::uint64_t w);

}

#endif
