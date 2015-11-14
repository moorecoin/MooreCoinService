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

#ifndef ripple_protocol_stamount_h_included
#define ripple_protocol_stamount_h_included

#include <ripple/protocol/sfield.h>
#include <ripple/protocol/serializer.h>
#include <ripple/protocol/stbase.h>
#include <ripple/protocol/issue.h>
#include <beast/cxx14/memory.h> // <memory>

namespace ripple {

// internal form:
// 1: if amount is zero, then value is zero and offset is -100
// 2: otherwise:
//   legal offset range is -96 to +80 inclusive
//   value range is 10^15 to (10^16 - 1) inclusive
//  amount = value * [10 ^ offset]

// wire form:
// high 8 bits are (offset+142), legal range is, 80 to 22 inclusive
// low 56 bits are value, legal range is 10^15 to (10^16 - 1) inclusive
class stamount : public stbase
{
public:
    typedef std::uint64_t mantissa_type;
    typedef int exponent_type;
    typedef std::pair <mantissa_type, exponent_type> rep;

private:
    issue missue;
    mantissa_type mvalue;
    exponent_type moffset;
    bool misnative;      // a shorthand for isxrp(missue).
    bool misnegative;

public:
    static const int cminoffset = -96;
    static const int cmaxoffset = 80;

    static const std::uint64_t cminvalue   = 1000000000000000ull;
    static const std::uint64_t cmaxvalue   = 9999999999999999ull;
    static const std::uint64_t cmaxnative  = 9000000000000000000ull;

    // max native value on network.
    static const std::uint64_t cmaxnativen = 100000000000000000ull;
    static const std::uint64_t cnotnative  = 0x8000000000000000ull;
    static const std::uint64_t cposnative  = 0x4000000000000000ull;
    static const std::uint64_t cvbcnative  = 0x2000000000000000ull;

    static std::uint64_t const urateone;

    //--------------------------------------------------------------------------

    struct unchecked { };

    // calls canonicalize
    stamount (sfield::ref name, issue const& issue,
        mantissa_type mantissa, exponent_type exponent,
            bool native, bool negative);

    // does not call canonicalize
    stamount (sfield::ref name, issue const& issue,
        mantissa_type mantissa, exponent_type exponent,
            bool native, bool negative, unchecked);

    stamount (sfield::ref name, bool isvbc, std::int64_t mantissa);

    stamount (sfield::ref name, bool isvbc = false,
        std::uint64_t mantissa = 0, bool negative = false);

    stamount (sfield::ref name, issue const& issue,
        std::uint64_t mantissa = 0, int exponent = 0, bool negative = false);

    stamount (std::uint64_t mantissa = 0, bool negative = false);

    stamount (issue const& issue,
        std::uint64_t mantissa = 0, int exponent = 0, bool negative = false);

    // vfalco is this needed when we have the previous signature?
    stamount (issue const& issue,
        std::uint32_t mantissa, int exponent = 0, bool negative = false);

    stamount (issue const& issue, std::int64_t mantissa, int exponent = 0);

    stamount (issue const& issue, int mantissa, int exponent = 0);

    //--------------------------------------------------------------------------

private:
    static
    std::unique_ptr<stamount>
    construct (serializeriterator&, sfield::ref name);

public:
    static
    stamount
    createfromint64(sfield::ref n, bool isvbc, std::int64_t v);

    static
    std::unique_ptr <stbase>
    deserialize (
        serializeriterator& sit, sfield::ref name)
    {
        return construct (sit, name);
    }

    static
    stamount
    deserialize (serializeriterator&);

    //--------------------------------------------------------------------------
    //
    // observers
    //
    //--------------------------------------------------------------------------

    int exponent() const noexcept { return moffset; }
    bool native() const noexcept { return misnative; }
    bool negative() const noexcept { return misnegative; }
    std::uint64_t mantissa() const noexcept { return mvalue; }
    issue const& issue() const { return missue; }

    // these three are deprecated
    currency const& getcurrency() const { return missue.currency; }
    account const& getissuer() const { return missue.account; }
    bool isnative() const { return misnative; }

    int
    signum() const noexcept
    {
        return mvalue ? (misnegative ? -1 : 1) : 0;
    }

    /** returns a zero value with the same issuer and currency. */
    stamount
    zeroed() const
    {
        // todo(tom): what does this next comment mean here?
        // see https://ripplelabs.atlassian.net/browse/wc-1847?jql=
        return stamount (missue);
    }

    // when the currency is xrp, the value in raw unsigned units.
    std::uint64_t
    getnvalue() const;

    // when the currency is xrp, the value in raw signed units.
    std::int64_t
    getsnvalue() const;

    // vfalco todo this can be a free function or just call the
    //             member on the issue.
    std::string
    gethumancurrency() const;

    void
    setjson (json::value&) const;

    //--------------------------------------------------------------------------
    //
    // operators
    //
    //--------------------------------------------------------------------------

    explicit operator bool() const noexcept
    {
        return *this != zero;
    }

    bool iscomparable (stamount const&) const;
    void throwcomparable (stamount const&) const;

    stamount& operator+= (stamount const&);
    stamount& operator-= (stamount const&);
    stamount& operator+= (std::uint64_t);
    stamount& operator-= (std::uint64_t);

    stamount& operator= (std::uint64_t);
    stamount& operator= (beast::zero)
    {
        clear();
        return *this;
    }

    friend stamount operator+ (stamount const& v1, stamount const& v2);
    friend stamount operator- (stamount const& v1, stamount const& v2);

    //--------------------------------------------------------------------------
    //
    // modification
    //
    //--------------------------------------------------------------------------

    // vfalco todo remove this, it is only called from the unit test
    void roundself();

    void setnvalue (std::uint64_t v);
    void setsnvalue (std::int64_t);

    void negate()
    {
        if (*this != zero)
            misnegative = !misnegative;
    }

    void clear()
    {
        // vfalco: why -100?
        moffset = misnative ? 0 : -100;
        mvalue = 0;
        misnegative = false;
    }

    // zero while copying currency and issuer.
    void clear (stamount const& satmpl)
    {
        clear (satmpl.missue);
    }

    void clear (issue const& issue)
    {
        setissue(issue);
        clear();
    }

    void setissuer (account const& uissuer)
    {
        missue.account = uissuer;
        setissue(missue);
    }

    /** set the issue for this amount and update misnative. */
    void setissue (issue const& issue);

    // vfalco todo rename to setvalueonly (it only sets mantissa and exponent)
    //             make this private
    bool setvalue (std::string const& samount);

    //--------------------------------------------------------------------------
    //
    // stbase
    //
    //--------------------------------------------------------------------------

    serializedtypeid
    getstype() const override
    {
        return sti_amount;
    }

    std::string
    getfulltext() const override;

    std::string
    gettext() const override;

    json::value
    getjson (int) const override;

    void
    add (serializer& s) const override;

    bool
    isequivalent (const stbase& t) const override;

    bool
    isdefault() const override
    {
        return (mvalue == 0) && misnative;
    }
    
    bool
    ismathematicalinteger () const;

    void floor(int offset = 0);

private:
    stamount*
    duplicate() const override;

    void canonicalize();
    void set (std::int64_t v);
};

//------------------------------------------------------------------------------
//
// creation
//
//------------------------------------------------------------------------------

// vfalco todo the parameter type should be quality not uint64_t
stamount
amountfromquality (std::uint64_t rate);

stamount
amountfromjson (sfield::ref name, json::value const& v);

stamount
amountfromrate (std::uint64_t urate);

bool
amountfromjsonnothrow (stamount& result, json::value const& jvsource);

//------------------------------------------------------------------------------
//
// observers
//
//------------------------------------------------------------------------------

inline
bool
islegalnet (stamount const& value)
{
    return ! value.native() || (value.mantissa() <= stamount::cmaxnativen);
}

//------------------------------------------------------------------------------
//
// operators
//
//------------------------------------------------------------------------------

bool operator== (stamount const& lhs, stamount const& rhs);
bool operator!= (stamount const& lhs, stamount const& rhs);
bool operator<  (stamount const& lhs, stamount const& rhs);
bool operator>  (stamount const& lhs, stamount const& rhs);
bool operator<= (stamount const& lhs, stamount const& rhs);
bool operator>= (stamount const& lhs, stamount const& rhs);

// native currency only
bool operator<  (stamount const& lhs, std::uint64_t rhs);
bool operator>  (stamount const& lhs, std::uint64_t rhs);
bool operator<= (stamount const& lhs, std::uint64_t rhs);
bool operator>= (stamount const& lhs, std::uint64_t rhs);

stamount operator+ (stamount const& lhs, std::uint64_t rhs);
stamount operator- (stamount const& lhs, std::uint64_t rhs);
stamount operator- (stamount const& value);

//------------------------------------------------------------------------------
//
// arithmetic
//
//------------------------------------------------------------------------------

stamount
divide (stamount const& v1, stamount const& v2, issue const& issue);

inline
stamount
divide (stamount const& v1, stamount const& v2, stamount const& saunit)
{
    return divide (v1, v2, saunit.issue());
}

inline
stamount
divide (stamount const& v1, stamount const& v2)
{
    return divide (v1, v2, v1);
}

stamount
multiply (stamount const& v1, stamount const& v2, issue const& issue);

inline
stamount
multiply (stamount const& v1, stamount const& v2, stamount const& saunit)
{
    return multiply (v1, v2, saunit.issue());
}

inline
stamount
multiply (stamount const& v1, stamount const& v2)
{
    return multiply (v1, v2, v1);
}

void
canonicalizeround (bool native, std::uint64_t& mantissa,
    int& exponent, bool roundup);

/* addround, subround can end up rounding if the amount subtracted is too small
    to make a change. consder (x-d) where d is very small relative to x.
    if you ask to round down, then (x-d) should not be x unless d is zero.
    if you ask to round up, (x+d) should never be x unless d is zero. (assuming x and d are positive).
*/
// add, subtract, multiply, or divide rounding result in specified direction
stamount
addround (stamount const& v1, stamount const& v2, bool roundup);

stamount
subround (stamount const& v1, stamount const& v2, bool roundup);

stamount
mulround (stamount const& v1, stamount const& v2,
    issue const& issue, bool roundup);

inline
stamount
mulround (stamount const& v1, stamount const& v2,
    stamount const& saunit, bool roundup)
{
    return mulround (v1, v2, saunit.issue(), roundup);
}

inline
stamount
mulround (stamount const& v1, stamount const& v2, bool roundup)
{
    return mulround (v1, v2, v1.issue(), roundup);
}

stamount
divround (stamount const& v1, stamount const& v2,
    issue const& issue, bool roundup);

inline
stamount
divround (stamount const& v1, stamount const& v2,
    stamount const& saunit, bool roundup)
{
    return divround (v1, v2, saunit.issue(), roundup);
}

inline
stamount
divround (stamount const& v1, stamount const& v2, bool roundup)
{
    return divround (v1, v2, v1.issue(), roundup);
}

// someone is offering x for y, what is the rate?
// rate: smaller is better, the taker wants the most out: in/out
// vfalco todo return a quality object
std::uint64_t
getrate (stamount const& offerout, stamount const& offerin);

//------------------------------------------------------------------------------

inline bool isxrp(stamount const& amount)
{
    return isxrp (amount.issue().currency);
}
inline bool isvbc(stamount const& amount)
{
	return isvbc(amount.issue().currency);
}
inline bool isnative(stamount const& amount)
{
    return isxrp(amount) || isvbc(amount);
}

// vfalco todo make static member accessors for these in stamount
extern const stamount sazero;
extern const stamount saone;

} // ripple

#endif
