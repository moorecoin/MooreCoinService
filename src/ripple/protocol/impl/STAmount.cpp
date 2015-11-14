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
#include <ripple/protocol/jsonfields.h>
#include <ripple/crypto/cbignum.h>
#include <ripple/protocol/systemparameters.h>
#include <ripple/protocol/stamount.h>
#include <ripple/protocol/uinttypes.h>
#include <beast/module/core/text/lexicalcast.h>
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <beast/cxx14/iterator.h> // <iterator>

namespace ripple {

static const std::uint64_t tento14 = 100000000000000ull;
static const std::uint64_t tento14m1 = tento14 - 1;
static const std::uint64_t tento17 = tento14 * 1000;

//------------------------------------------------------------------------------

stamount::stamount (sfield::ref name, issue const& issue,
        mantissa_type mantissa, exponent_type exponent,
            bool native, bool negative)
    : stbase (name)
    , missue (issue)
    , mvalue (mantissa)
    , moffset (exponent)
    , misnative (native)
    , misnegative (negative)
{
    canonicalize();
}

stamount::stamount (sfield::ref name, issue const& issue,
        mantissa_type mantissa, exponent_type exponent,
            bool native, bool negative, unchecked)
    : stbase (name)
    , missue (issue)
    , mvalue (mantissa)
    , moffset (exponent)
    , misnative (native)
    , misnegative (negative)
{
}

stamount::stamount(sfield::ref name, bool isvbc, std::int64_t mantissa)
    : stbase (name)
    , missue(isvbc?vbcissue():xrpissue())
    , moffset (0)
    , misnative (true)
{
    set (mantissa);
}

stamount::stamount (sfield::ref name, bool isvbc,
        std::uint64_t mantissa, bool negative)
    : stbase (name)
    , missue(isvbc?vbcissue():xrpissue())
    , mvalue (mantissa)
    , moffset (0)
    , misnative (true)
    , misnegative (negative)
{
}

stamount::stamount (sfield::ref name, issue const& issue,
        std::uint64_t mantissa, int exponent, bool negative)
    : stbase (name)
    , missue (issue)
    , mvalue (mantissa)
    , moffset (exponent)
    , misnegative (negative)
{
    canonicalize ();
}

//------------------------------------------------------------------------------

stamount::stamount(std::uint64_t mantissa, bool negative)
    : mvalue (mantissa)
    , moffset (0)
    , misnative (true)
    , misnegative (mantissa != 0 && negative)
{
}

stamount::stamount (issue const& issue,
    std::uint64_t mantissa, int exponent, bool negative)
    : missue (issue)
    , mvalue (mantissa)
    , moffset (exponent)
    , misnegative (negative)
{
    canonicalize ();
}

stamount::stamount (issue const& issue,
        std::int64_t mantissa, int exponent)
    : missue (issue)
    , moffset (exponent)
{
    set (mantissa);
    canonicalize ();
}

stamount::stamount (issue const& issue,
        std::uint32_t mantissa, int exponent, bool negative)
    : stamount (issue, static_cast<std::uint64_t>(mantissa), exponent, negative)
{
}

stamount::stamount (issue const& issue,
        int mantissa, int exponent)
    : stamount (issue, static_cast<std::int64_t>(mantissa), exponent)
{
}

std::unique_ptr<stamount>
stamount::construct (serializeriterator& sit, sfield::ref name)
{
    std::uint64_t value = sit.get64 ();

    // native
    if ((value & cnotnative) == 0)
    {
        bool isvbc = false;
        if (value & cvbcnative) {
            isvbc = true;
            value &= ~cvbcnative;
        }
        
        // positive
        if ((value & cposnative) != 0)
            return std::make_unique<stamount> (name, isvbc, value & ~cposnative, false);

        // negative
        if (value == 0)
            throw std::runtime_error ("negative zero is not canonical");

        return std::make_unique<stamount> (name, isvbc, value, true);
    }

    issue issue;
    issue.currency.copyfrom (sit.get160 ());

    if (::ripple::isnative (issue.currency))
        throw std::runtime_error ("invalid native currency");

    issue.account.copyfrom (sit.get160 ());

    if (::ripple::isnative (issue.account))
		throw std::runtime_error ("invalid native account");

    // 10 bits for the offset, sign and "not native" flag
    int offset = static_cast<int> (value >> (64 - 10));

    value &= ~ (1023ull << (64 - 10));

    if (value)
    {
        bool isnegative = (offset & 256) == 0;
        offset = (offset & 255) - 97; // center the range

        if (value < cminvalue ||
            value > cmaxvalue ||
            offset < cminoffset ||
            offset > cmaxoffset)
        {
            throw std::runtime_error ("invalid currency value");
        }

        return std::make_unique<stamount> (name, issue, value, offset, isnegative);
    }

    if (offset != 512)
        throw std::runtime_error ("invalid currency value");

    return std::make_unique<stamount> (name, issue);
}

stamount
stamount::createfromint64 (sfield::ref name, bool isvbc, std::int64_t value)
{
    return value >= 0
           ? stamount (name, isvbc, static_cast<std::uint64_t> (value), false)
           : stamount (name, isvbc, static_cast<std::uint64_t> (-value), true);
}

stamount stamount::deserialize (serializeriterator& it)
{
    auto s = construct (it, sfgeneric);

    if (!s)
        throw std::runtime_error("deserialization error");

    return stamount (*s);
}

//------------------------------------------------------------------------------
//
// operators
//
//------------------------------------------------------------------------------

bool stamount::iscomparable (stamount const& t) const
{
    // are these two stamount instances in the same currency
    if (misnative) return t.misnative;

    if (t.misnative) return false;

    return missue.currency == t.missue.currency;
}

void stamount::throwcomparable (stamount const& t) const
{
    // throw an exception if these two stamount instances are incomparable
    if (!iscomparable (t))
        throw std::runtime_error ("amounts are not comparable");
}

stamount& stamount::operator+= (stamount const& a)
{
    *this = *this + a;
    return *this;
}

stamount& stamount::operator-= (stamount const& a)
{
    *this = *this - a;
    return *this;
}

stamount& stamount::operator+= (std::uint64_t v)
{
    assert (misnative);
    if (!misnative)
        throw std::runtime_error ("not native");
    // vfalco todo the cast looks dangerous, is it needed?
    setsnvalue (getsnvalue () + static_cast<std::int64_t> (v));
    return *this;
}

stamount& stamount::operator-= (std::uint64_t v)
{
    assert (misnative);

    if (!misnative)
        throw std::runtime_error ("not native");

    // vfalco todo the cast looks dangerous, is it needed?
    setsnvalue (getsnvalue () - static_cast<std::int64_t> (v));
    return *this;
}

stamount& stamount::operator= (std::uint64_t v)
{
    // does not copy name, does not change currency type.
    moffset = 0;
    mvalue = v;
    misnegative = false;
    if (!misnative)
        canonicalize ();
    return *this;
}



stamount operator+ (stamount const& v1, stamount const& v2)
{
    v1.throwcomparable (v2);

    if (v2 == zero)
        return v1;

    if (v1 == zero)
    {
        // result must be in terms of v1 currency and issuer.
        return stamount (v1.getfname (), v1.missue,
                         v2.mvalue, v2.moffset, v2.misnegative);
    }

    if (v1.misnative)
        return stamount (v1.getfname (), isvbc(v1), v1.getsnvalue () + v2.getsnvalue ());

    int ov1 = v1.moffset, ov2 = v2.moffset;
    std::int64_t vv1 = static_cast<std::int64_t> (v1.mvalue);
    std::int64_t vv2 = static_cast<std::int64_t> (v2.mvalue);

    if (v1.misnegative)
        vv1 = -vv1;

    if (v2.misnegative)
        vv2 = -vv2;

    while (ov1 < ov2)
    {
        vv1 /= 10;
        ++ov1;
    }

    while (ov2 < ov1)
    {
        vv2 /= 10;
        ++ov2;
    }

    // this addition cannot overflow an std::int64_t. it can overflow an
    // stamount and the constructor will throw.

    std::int64_t fv = vv1 + vv2;

    if ((fv >= -10) && (fv <= 10))
        return stamount (v1.getfname (), v1.missue);
    if (fv >= 0)
        return stamount (v1.getfname (), v1.missue, fv, ov1, false);
    return stamount (v1.getfname (), v1.missue, -fv, ov1, true);
}

stamount operator- (stamount const& v1, stamount const& v2)
{
    v1.throwcomparable (v2);

    if (v2 == zero)
        return v1;

    if (v2.misnative)
    {
        // xxx this could be better, check for overflow and that maximum range
        // is covered.
        return stamount::createfromint64 (
                v1.getfname (), isvbc(v1), v1.getsnvalue () - v2.getsnvalue ());
    }

    int ov1 = v1.moffset, ov2 = v2.moffset;
    auto vv1 = static_cast<std::int64_t> (v1.mvalue);
    auto vv2 = static_cast<std::int64_t> (v2.mvalue);

    if (v1.misnegative)
        vv1 = -vv1;

    if (v2.misnegative)
        vv2 = -vv2;

    while (ov1 < ov2)
    {
        vv1 /= 10;
        ++ov1;
    }

    while (ov2 < ov1)
    {
        vv2 /= 10;
        ++ov2;
    }

    // this subtraction cannot overflow an std::int64_t, it can overflow an stamount and the constructor will throw

    std::int64_t fv = vv1 - vv2;

    if ((fv >= -10) && (fv <= 10))
        return stamount (v1.getfname (), v1.missue);
    if (fv >= 0)
        return stamount (v1.getfname (), v1.missue, fv, ov1, false);
    return stamount (v1.getfname (), v1.missue, -fv, ov1, true);
}

//------------------------------------------------------------------------------

std::uint64_t const stamount::urateone = getrate (stamount (1), stamount (1));

// note: misnative and missue.currency must be set already!
bool
stamount::setvalue (std::string const& amount)
{
    static boost::regex const renumber (
        "^"                       // the beginning of the string
        "([-+]?)"                 // (optional) + or - character
        "(0|[1-9][0-9]*)"         // a number (no leading zeroes, unless 0)
        "(\\.([0-9]+))?"          // (optional) period followed by any number
        "([ee]([+-]?)([0-9]+))?"  // (optional) e, optional + or -, any number
        "$",
        boost::regex_constants::optimize);

    boost::smatch match;

    if (!boost::regex_match (amount, match, renumber))
    {
        writelog (lswarning, stamount) <<
            "number not valid: \"" << amount << "\"";
        return false;
    }

    // match fields:
    //   0 = whole input
    //   1 = sign
    //   2 = integer portion
    //   3 = whole fraction (with '.')
    //   4 = fraction (without '.')
    //   5 = whole exponent (with 'e')
    //   6 = exponent sign
    //   7 = exponent number

    try
    {
        // checkme: why 32? shouldn't this be 16?
        if ((match[2].length () + match[4].length ()) > 32)
        {
            writelog (lswarning, stamount) << "overlong number: " << amount;
            return false;
        }

        misnegative = (match[1].matched && (match[1] == "-"));

        // can't specify xrp using fractional representation
        if (misnative && match[3].matched)
            return false;

        if (!match[4].matched) // integer only
        {
            mvalue = beast::lexicalcastthrow <std::uint64_t> (std::string (match[2]));
            moffset = 0;
        }
        else
        {
            // integer and fraction
            mvalue = beast::lexicalcastthrow <std::uint64_t> (match[2] + match[4]);
            moffset = -(match[4].length ());
        }

        if (match[5].matched)
        {
            // we have an exponent
            if (match[6].matched && (match[6] == "-"))
                moffset -= beast::lexicalcastthrow <int> (std::string (match[7]));
            else
                moffset += beast::lexicalcastthrow <int> (std::string (match[7]));
        }

        canonicalize ();

        writelog (lstrace, stamount) <<
            "canonicalized \"" << amount << "\" to " << mvalue << " : " << moffset;
    }
    catch (...)
    {
        return false;
    }

    return true;
}

void
stamount::setissue (issue const& issue)
{
    missue = std::move(issue);
    misnative = ::ripple::isnative(*this);
}

std::uint64_t
stamount::getnvalue () const
{
    if (!misnative)
        throw std::runtime_error ("not native");
    return mvalue;
}

std::int64_t
stamount::getsnvalue () const
{
    // signed native value
    if (!misnative)
        throw std::runtime_error ("not native");

    if (misnegative)
        return - static_cast<std::int64_t> (mvalue);

    return static_cast<std::int64_t> (mvalue);
}

std::string stamount::gethumancurrency () const
{
    return to_string (missue.currency);
}

void
stamount::setnvalue (std::uint64_t v)
{
    if (!misnative)
        throw std::runtime_error ("not native");
    mvalue = v;
}

void
stamount::setsnvalue (std::int64_t v)
{
    if (!misnative) throw std::runtime_error ("not native");

    if (v > 0)
    {
        misnegative = false;
        mvalue = static_cast<std::uint64_t> (v);
    }
    else
    {
        misnegative = true;
        mvalue = static_cast<std::uint64_t> (-v);
    }
}

// convert an offer into an index amount so they sort by rate.
// a taker will take the best, lowest, rate first.
// (e.g. a taker will prefer pay 1 get 3 over pay 1 get 2.
// --> offerout: takergets: how much the offerer is selling to the taker.
// -->  offerin: takerpays: how much the offerer is receiving from the taker.
// <--    urate: normalize(offerin/offerout)
//             a lower rate is better for the person taking the order.
//             the taker gets more for less with a lower rate.
// zero is returned if the offer is worthless.
std::uint64_t
getrate (stamount const& offerout, stamount const& offerin)
{
    if (offerout == zero)
        return 0;
    try
    {
        stamount r = divide (offerin, offerout, noissue());
        if (r == zero) // offer is too good
            return 0;
        assert ((r.exponent() >= -100) && (r.exponent() <= 155));
        std::uint64_t ret = r.exponent() + 100;
        return (ret << (64 - 8)) | r.mantissa();
    }
    catch (...)
    {
    }

    // overflow -- very bad offer
    return 0;
}

void stamount::setjson (json::value& elem) const
{
    elem = json::objectvalue;

    if (!misnative || isvbc(*this))
    {
        // it is an error for currency or issuer not to be specified for valid
        // json.
        elem[jss::value]      = gettext ();
        elem[jss::currency]   = gethumancurrency ();
        elem[jss::issuer]     = to_string (missue.account);
    }
    else
    {
        elem = gettext ();
    }
}

// vfalco what does this perplexing function do?
void stamount::roundself ()
{
    if (misnative)
        return;

    std::uint64_t valuedigits = mvalue % 1000000000ull;

    if (valuedigits == 1)
    {
        mvalue -= 1;

        if (mvalue < cminvalue)
            canonicalize ();
    }
    else if (valuedigits == 999999999ull)
    {
        mvalue += 1;

        if (mvalue > cmaxvalue)
            canonicalize ();
    }
}

//------------------------------------------------------------------------------
//
// stbase
//
//------------------------------------------------------------------------------

std::string
stamount::getfulltext () const
{
    std::string ret;

    ret.reserve(64);
    ret = gettext () + "/" + gethumancurrency ();

    if (!misnative)
    {
        ret += "/";

        if (isxrp (*this))
            ret += "0";
		else if (isvbc(*this))
			ret += "0xff";
        else if (missue.account == noaccount())
            ret += "1";
        else
            ret += to_string (missue.account);
    }

    return ret;
}

std::string
stamount::gettext () const
{
    // keep full internal accuracy, but make more human friendly if posible
    if (*this == zero)
        return "0";

    std::string const raw_value (std::to_string (mvalue));
    std::string ret;

    if (misnegative)
        ret.append (1, '-');

    bool const scientific ((moffset != 0) && ((moffset < -25) || (moffset > -5)));

    if (misnative || scientific)
    {
        ret.append (raw_value);

        if (scientific)
        {
            ret.append (1, 'e');
            ret.append (std::to_string (moffset));
        }

        return ret;
    }

    assert (moffset + 43 > 0);

    size_t const pad_prefix = 27;
    size_t const pad_suffix = 23;

    std::string val;
    val.reserve (raw_value.length () + pad_prefix + pad_suffix);
    val.append (pad_prefix, '0');
    val.append (raw_value);
    val.append (pad_suffix, '0');

    size_t const offset (moffset + 43);

    auto pre_from (val.begin ());
    auto const pre_to (val.begin () + offset);

    auto const post_from (val.begin () + offset);
    auto post_to (val.end ());

    // crop leading zeroes. take advantage of the fact that there's always a
    // fixed amount of leading zeroes and skip them.
    if (std::distance (pre_from, pre_to) > pad_prefix)
        pre_from += pad_prefix;

    assert (post_to >= post_from);

    pre_from = std::find_if (pre_from, pre_to,
        [](char c)
        {
            return c != '0';
        });

    // crop trailing zeroes. take advantage of the fact that there's always a
    // fixed amount of trailing zeroes and skip them.
    if (std::distance (post_from, post_to) > pad_suffix)
        post_to -= pad_suffix;

    assert (post_to >= post_from);

    post_to = std::find_if(
        std::make_reverse_iterator (post_to),
        std::make_reverse_iterator (post_from),
        [](char c)
        {
            return c != '0';
        }).base();

    // assemble the output:
    if (pre_from == pre_to)
        ret.append (1, '0');
    else
        ret.append(pre_from, pre_to);

    if (post_to != post_from)
    {
        ret.append (1, '.');
        ret.append (post_from, post_to);
    }

    return ret;
}

json::value
stamount::getjson (int) const
{
    json::value elem;
    setjson (elem);
    return elem;
}

void
stamount::add (serializer& s) const
{
    if (misnative)
    {
        assert (moffset == 0);

        if (!misnegative)
            if (isvbc(*this))
                s.add64 (mvalue | cvbcnative | cposnative);
            else
                s.add64 (mvalue | cposnative);
        else
            if (isvbc(*this))
                s.add64 (mvalue | cvbcnative);
            else
                s.add64 (mvalue);
    }
    else
    {
        if (*this == zero)
            s.add64 (cnotnative);
        else if (misnegative) // 512 = not native
            s.add64 (mvalue | (static_cast<std::uint64_t> (moffset + 512 + 97) << (64 - 10)));
        else // 256 = positive
            s.add64 (mvalue | (static_cast<std::uint64_t> (moffset + 512 + 256 + 97) << (64 - 10)));

        s.add160 (missue.currency);
        s.add160 (missue.account);
    }
}

bool
stamount::isequivalent (const stbase& t) const
{
    const stamount* v = dynamic_cast<const stamount*> (&t);
    return v && (*v == *this);
}

#if (defined (_win32) || defined (_win64))
// from https://llvm.org/svn/llvm-project/libcxx/trunk/include/support/win32/support.h
static int __builtin_ctzll(uint64_t mask)
{
  unsigned long where;
// search from lsb to msb for first set bit.
// returns zero if no set bit is found.
#if defined(_win64)
  if (_bitscanforward64(&where, mask))
    return static_cast<int>(where);
#elif defined(_win32)
  // win32 doesn't have _bitscanforward64 so emulate it with two 32 bit calls.
  // scan the low word.
  if (_bitscanforward(&where, static_cast<unsigned long>(mask)))
    return static_cast<int>(where);
  // scan the high word.
  if (_bitscanforward(&where, static_cast<unsigned long>(mask >> 32)))
    return static_cast<int>(where + 32); // create a bit offset from the lsb.
#else
#error "implementation of __builtin_ctzll required"
#endif
  return 64;
}
#endif

bool stamount::ismathematicalinteger() const
{
    return __builtin_ctzll(mantissa()) >= -exponent();
}

void stamount::floor(int offset)
{
    while (moffset < offset) {
        mvalue /= 10;
        ++moffset;
    }
    canonicalize();
}

stamount*
stamount::duplicate () const
{
    return new stamount (*this);
}

//------------------------------------------------------------------------------

// amount = value * [10 ^ offset]
// representation range is 10^80 - 10^(-80).
// on the wire, high 8 bits are (offset+142), low 56 bits are value.
//
// value is zero if amount is zero, otherwise value is 10^15 to (10^16 - 1)
// inclusive.
void stamount::canonicalize ()
{
    if (::ripple::isnative (*this))
    {
        // native currency amounts should always have an offset of zero
        misnative = true;

        if (mvalue == 0)
        {
            moffset = 0;
            misnegative = false;
            return;
        }

        while (moffset < 0)
        {
            mvalue /= 10;
            ++moffset;
        }

        while (moffset > 0)
        {
            mvalue *= 10;
            --moffset;
        }

        if (mvalue > cmaxnativen)
            throw std::runtime_error ("native currency amount out of range");

        return;
    }

    misnative = false;

    if (mvalue == 0)
    {
        moffset = -100;
        misnegative = false;
        return;
    }

    while ((mvalue < cminvalue) && (moffset > cminoffset))
    {
        mvalue *= 10;
        --moffset;
    }

    while (mvalue > cmaxvalue)
    {
        if (moffset >= cmaxoffset)
            throw std::runtime_error ("value overflow");

        mvalue /= 10;
        ++moffset;
    }

    if ((moffset < cminoffset) || (mvalue < cminvalue))
    {
        mvalue = 0;
        moffset = 0;
    }

    if (moffset > cmaxoffset)
        throw std::runtime_error ("value overflow");

    assert ((mvalue == 0) || ((mvalue >= cminvalue) && (mvalue <= cmaxvalue)));
    assert ((mvalue == 0) || ((moffset >= cminoffset) && (moffset <= cmaxoffset)));
    assert ((mvalue != 0) || (moffset != -100));
}

void stamount::set (std::int64_t v)
{
    if (v < 0)
    {
        misnegative = true;
        mvalue = static_cast<std::uint64_t> (-v);
    }
    else
    {
        misnegative = false;
        mvalue = static_cast<std::uint64_t> (v);
    }
}

//------------------------------------------------------------------------------

stamount
amountfromrate (std::uint64_t urate)
{
    return stamount (noissue(), urate, -9, false);
}

stamount
amountfromquality (std::uint64_t rate)
{
    if (rate == 0)
        return stamount (noissue());

    std::uint64_t mantissa = rate & ~ (255ull << (64 - 8));
    int exponent = static_cast<int> (rate >> (64 - 8)) - 100;

    return stamount (noissue(), mantissa, exponent);
}

stamount
amountfromjson (sfield::ref name, json::value const& v)
{
    stamount::mantissa_type mantissa = 0;
    stamount::exponent_type exponent = 0;
    bool negative = false;
    issue issue;

    json::value value;
    json::value currency;
    json::value issuer;

    if (v.isobject ())
    {
        writelog (lstrace, stamount) <<
            "value='" << v["value"].asstring () <<
            "', currency='" << v["currency"].asstring () <<
            "', issuer='" << v["issuer"].asstring () <<
            "')";

        value       = v[jss::value];
        currency    = v[jss::currency];
        issuer      = v[jss::issuer];
    }
    else if (v.isarray ())
    {
        value = v.get (json::uint (0), 0);
        currency = v.get (json::uint (1), json::nullvalue);
        issuer = v.get (json::uint (2), json::nullvalue);
    }
    else if (v.isstring ())
    {
        std::string val = v.asstring ();
        std::vector<std::string> elements;
        boost::split (elements, val, boost::is_any_of ("\t\n\r ,/"));

        if (elements.size () > 3)
            throw std::runtime_error ("invalid amount string");

        value = elements[0];

        if (elements.size () > 1)
            currency = elements[1];

        if (elements.size () > 2)
            issuer = elements[2];
    }
    else
    {
        value = v;
    }

    bool const native = ! currency.isstring () ||
        currency.asstring ().empty () ||
		(currency.asstring() == systemcurrencycode() || currency.asstring() == systemcurrencycodevbc());

    if (! to_currency (issue.currency, currency.asstring ()))
        throw std::runtime_error ("invalid currency");

    //if (native)
    //{
    //    //if (v.isobject ())
    //    //    throw std::runtime_error ("xrp may not be specified as an object");
    //}
    //else
    if (!native)
    {
        // non-xrp
        if (! issuer.isstring ()
                || !to_issuer (issue.account, issuer.asstring ()))
            throw std::runtime_error ("invalid issuer");

		if (isnative(issue.currency))
            throw std::runtime_error ("invalid issuer");
    }
    else if (isvbc(issue.currency))
        issue.account = vbcaccount();

    if (value.isint ())
    {
        if (value.asint () >= 0)
        {
            mantissa = value.asint ();
        }
        else
        {
            mantissa = -value.asint ();
            negative = true;
        }
    }
    else if (value.isuint ())
    {
        mantissa = v.asuint ();
    }
    else if (value.isstring ())
    {
        if (native)
        {
            std::int64_t val = beast::lexicalcastthrow <std::int64_t> (
                value.asstring ());

            if (val >= 0)
            {
                mantissa = val;
            }
            else
            {
                mantissa = -val;
                negative = true;
            }
        }
        else
        {
            stamount amount (name, issue, mantissa, exponent,
                native, negative, stamount::unchecked{});
            amount.setvalue (value.asstring ());
            return amount;
        }
    }
    else
    {
        throw std::runtime_error ("invalid amount type");
    }

    return stamount (name, issue, mantissa, exponent, native, negative);
}

bool
amountfromjsonnothrow (stamount& result, json::value const& jvsource)
{
    try
    {
        result = amountfromjson (sfgeneric, jvsource);
        return true;
    }
    catch (const std::exception& e)
    {
        writelog (lsdebug, stamount) <<
            "amountfromjsonnothrow: caught: " << e.what ();
    }
    return false;
}

//------------------------------------------------------------------------------
//
// operators
//
//------------------------------------------------------------------------------

static
int
compare (stamount const& lhs, stamount const& rhs)
{
    // compares the value of a to the value of this stamount, amounts must be comparable
    if (lhs.negative() != rhs.negative())
        return lhs.negative() ? -1 : 1;
    if (lhs.mantissa() == 0)
    {
        if (rhs.negative())
            return 1;
        return (rhs.mantissa() != 0) ? -1 : 0;
    }
    if (rhs.mantissa() == 0) return 1;
    if (lhs.exponent() > rhs.exponent()) return lhs.negative() ? -1 : 1;
    if (lhs.exponent() < rhs.exponent()) return lhs.negative() ? 1 : -1;
    if (lhs.mantissa() > rhs.mantissa()) return lhs.negative() ? -1 : 1;
    if (lhs.mantissa() < rhs.mantissa()) return lhs.negative() ? 1 : -1;
    return 0;
}

bool
operator== (stamount const& lhs, stamount const& rhs)
{
    return lhs.iscomparable (rhs) && lhs.negative() == rhs.negative() &&
        lhs.exponent() == rhs.exponent() && lhs.mantissa() == rhs.mantissa();
}

bool
operator!= (stamount const& lhs, stamount const& rhs)
{
    return lhs.exponent() != rhs.exponent() ||
        lhs.mantissa() != rhs.mantissa() ||
        lhs.negative() != rhs.negative() || ! lhs.iscomparable (rhs);
}

bool
operator< (stamount const& lhs, stamount const& rhs)
{
    lhs.throwcomparable (rhs);
    return compare (lhs, rhs) < 0;
}

bool
operator> (stamount const& lhs, stamount const& rhs)
{
    lhs.throwcomparable (rhs);
    return compare (lhs, rhs) > 0;
}

bool
operator<= (stamount const& lhs, stamount const& rhs)
{
    lhs.throwcomparable (rhs);
    return compare (lhs, rhs) <= 0;
}

bool
operator>= (stamount const& lhs, stamount const& rhs)
{
    lhs.throwcomparable (rhs);
    return compare (lhs, rhs) >= 0;
}

// native currency only

bool
operator< (stamount const& lhs, std::uint64_t rhs)
{
    // vfalco why the cast?
    return lhs.getsnvalue() < static_cast <std::int64_t> (rhs);
}

bool
operator> (stamount const& lhs, std::uint64_t rhs)
{
    // vfalco why the cast?
    return lhs.getsnvalue() > static_cast <std::int64_t> (rhs);
}

bool
operator<= (stamount const& lhs, std::uint64_t rhs)
{
    // vfalco todo the cast looks dangerous, is it needed?
    return lhs.getsnvalue () <= static_cast <std::int64_t> (rhs);
}

bool
operator>= (stamount const& lhs, std::uint64_t rhs)
{
    // vfalco todo the cast looks dangerous, is it needed?
    return lhs.getsnvalue() >= static_cast<std::int64_t> (rhs);
}

stamount
operator+ (stamount const& lhs, std::uint64_t rhs)
{
    // vfalco todo the cast looks dangerous, is it needed?
    return stamount(lhs.getfname(), isvbc(lhs),
        lhs.getsnvalue () + static_cast <std::int64_t> (rhs));
}

stamount
operator- (stamount const& lhs, std::uint64_t rhs)
{
    // vfalco todo the cast looks dangerous, is it needed?
    return stamount(lhs.getfname(), isvbc(lhs),
        lhs.getsnvalue () - static_cast <std::int64_t> (rhs));
}

stamount
operator- (stamount const& value)
{
    if (value.mantissa() == 0)
        return value;
    return stamount (value.getfname (),
        value.issue(), value.mantissa(), value.exponent(),
            value.native(), ! value.negative(), stamount::unchecked{});
}

//------------------------------------------------------------------------------
//
// arithmetic
//
//------------------------------------------------------------------------------

stamount
divide (stamount const& num, stamount const& den, issue const& issue)
{
    if (den == zero)
        throw std::runtime_error ("division by zero");

    if (num == zero)
        return {issue};

    std::uint64_t numval = num.mantissa();
    std::uint64_t denval = den.mantissa();
    int numoffset = num.exponent();
    int denoffset = den.exponent();

    if (num.native())
    {
        while (numval < stamount::cminvalue)
        {
            // need to bring into range
            numval *= 10;
            --numoffset;
        }
    }

    if (den.native())
    {
        while (denval < stamount::cminvalue)
        {
            denval *= 10;
            --denoffset;
        }
    }

    // compute (numerator * 10^17) / denominator
    cbignum v;

    if ((bn_add_word64 (&v, numval) != 1) ||
            (bn_mul_word64 (&v, tento17) != 1) ||
            (bn_div_word64 (&v, denval) == ((std::uint64_t) - 1)))
    {
        throw std::runtime_error ("internal bn error");
    }

    // 10^16 <= quotient <= 10^18
    assert (bn_num_bytes (&v) <= 64);

    // todo(tom): where do 5 and 17 come from?
    return stamount (issue, v.getuint64 () + 5,
                     numoffset - denoffset - 17,
                     num.negative() != den.negative());
}

stamount
multiply (stamount const& v1, stamount const& v2, issue const& issue)
{
    if (v1 == zero || v2 == zero)
        return stamount (issue);

	if (v1.native() && v2.native() && isnative(issue))
    {
        std::uint64_t const minv = v1.getsnvalue () < v2.getsnvalue ()
                ? v1.getsnvalue () : v2.getsnvalue ();
        std::uint64_t const maxv = v1.getsnvalue () < v2.getsnvalue ()
                ? v2.getsnvalue () : v1.getsnvalue ();

        if (minv > 3000000000ull) // sqrt(cmaxnative)
            throw std::runtime_error ("native value overflow");

        if (((maxv >> 32) * minv) > 2095475792ull) // cmaxnative / 2^32
            throw std::runtime_error ("native value overflow");

        return stamount(v1.getfname (), isvbc(v1), minv * maxv);
    }

    std::uint64_t value1 = v1.mantissa();
    std::uint64_t value2 = v2.mantissa();
    int offset1 = v1.exponent();
    int offset2 = v2.exponent();

    if (v1.native())
    {
        while (value1 < stamount::cminvalue)
        {
            value1 *= 10;
            --offset1;
        }
    }

    if (v2.native())
    {
        while (value2 < stamount::cminvalue)
        {
            value2 *= 10;
            --offset2;
        }
    }

    // compute (numerator * denominator) / 10^14 with rounding
    // 10^16 <= result <= 10^18
    cbignum v;

    if ((bn_add_word64 (&v, value1) != 1) ||
            (bn_mul_word64 (&v, value2) != 1) ||
            (bn_div_word64 (&v, tento14) == ((std::uint64_t) - 1)))
    {
        throw std::runtime_error ("internal bn error");
    }

    // 10^16 <= product <= 10^18
    assert (bn_num_bytes (&v) <= 64);

    // todo(tom): where do 7 and 14 come from?
    return stamount (issue, v.getuint64 () + 7,
        offset1 + offset2 + 14, v1.negative() != v2.negative());
}

void
canonicalizeround (bool isnative, std::uint64_t& value,
    int& offset, bool roundup)
{
    if (!roundup) // canonicalize already rounds down
        return;

    writelog (lstrace, stamount)
        << "canonicalizeround< " << value << ":" << offset;

    if (isnative)
    {
        if (offset < 0)
        {
            int loops = 0;

            while (offset < -1)
            {
                value /= 10;
                ++offset;
                ++loops;
            }

            value += (loops >= 2) ? 9 : 10; // add before last divide
            value /= 10;
            ++offset;
        }
    }
    else if (value > stamount::cmaxvalue)
    {
        while (value > (10 * stamount::cmaxvalue))
        {
            value /= 10;
            ++offset;
        }

        value += 9;     // add before last divide
        value /= 10;
        ++offset;
    }

    writelog (lstrace, stamount)
        << "canonicalizeround> " << value << ":" << offset;
}

stamount
addround (stamount const& v1, stamount const& v2, bool roundup)
{
    v1.throwcomparable (v2);

    if (v2.mantissa() == 0)
        return v1;

    if (v1.mantissa() == 0)
        return stamount (v1.getfname (), v1.issue(), v2.mantissa(),
                         v2.exponent(), v2.negative());

    if (v1.native())
        return stamount(v1.getfname(), isvbc(v1), v1.getsnvalue() + v2.getsnvalue());

    int ov1 = v1.exponent(), ov2 = v2.exponent();
    auto vv1 = static_cast<std::int64_t> (v1.mantissa());
    auto vv2 = static_cast<std::int64_t> (v2.mantissa());

    if (v1.negative())
        vv1 = -vv1;

    if (v2.negative())
        vv2 = -vv2;

    if (ov1 < ov2)
    {
        while (ov1 < (ov2 - 1))
        {
            vv1 /= 10;
            ++ov1;
        }

        if (roundup)
            vv1 += 9;

        vv1 /= 10;
        ++ov1;
    }

    if (ov2 < ov1)
    {
        while (ov2 < (ov1 - 1))
        {
            vv2 /= 10;
            ++ov2;
        }

        if (roundup)
            vv2 += 9;

        vv2 /= 10;
        ++ov2;
    }

    std::int64_t fv = vv1 + vv2;

    if ((fv >= -10) && (fv <= 10))
        return stamount (v1.getfname (), v1.issue());
    else if (fv >= 0)
    {
        std::uint64_t v = static_cast<std::uint64_t> (fv);
        canonicalizeround (false, v, ov1, roundup);
        return stamount (v1.getfname (), v1.issue(), v, ov1, false);
    }
    else
    {
        std::uint64_t v = static_cast<std::uint64_t> (-fv);
        canonicalizeround (false, v, ov1, !roundup);
        return stamount (v1.getfname (), v1.issue(), v, ov1, true);
    }
}

stamount
subround (stamount const& v1, stamount const& v2, bool roundup)
{
    v1.throwcomparable (v2);

    if (v2.mantissa() == 0)
        return v1;

    if (v1.mantissa() == 0)
        return stamount (v1.getfname (), v1.issue(), v2.mantissa(),
                         v2.exponent(), !v2.negative());

    if (v1.native())
        return stamount(v1.getfname(), isvbc(v1), v1.getsnvalue() - v2.getsnvalue());

    int ov1 = v1.exponent(), ov2 = v2.exponent();
    auto vv1 = static_cast<std::int64_t> (v1.mantissa());
    auto vv2 = static_cast<std::int64_t> (v2.mantissa());

    if (v1.negative())
        vv1 = -vv1;

    if (!v2.negative())
        vv2 = -vv2;

    if (ov1 < ov2)
    {
        while (ov1 < (ov2 - 1))
        {
            vv1 /= 10;
            ++ov1;
        }

        if (roundup)
            vv1 += 9;

        vv1 /= 10;
        ++ov1;
    }

    if (ov2 < ov1)
    {
        while (ov2 < (ov1 - 1))
        {
            vv2 /= 10;
            ++ov2;
        }

        if (roundup)
            vv2 += 9;

        vv2 /= 10;
        ++ov2;
    }

    std::int64_t fv = vv1 + vv2;

    if ((fv >= -10) && (fv <= 10))
        return stamount (v1.getfname (), v1.issue());

    if (fv >= 0)
    {
        std::uint64_t v = static_cast<std::uint64_t> (fv);
        canonicalizeround (false, v, ov1, roundup);
        return stamount (v1.getfname (), v1.issue(), v, ov1, false);
    }
    else
    {
        std::uint64_t v = static_cast<std::uint64_t> (-fv);
        canonicalizeround (false, v, ov1, !roundup);
        return stamount (v1.getfname (), v1.issue(), v, ov1, true);
    }
}

stamount
mulround (stamount const& v1, stamount const& v2,
    issue const& issue, bool roundup)
{
    if (v1 == zero || v2 == zero)
        return {issue};

    if (v1.native() && v2.native() && isnative(issue))
    {
        std::uint64_t minv = (v1.getsnvalue () < v2.getsnvalue ()) ?
                v1.getsnvalue () : v2.getsnvalue ();
        std::uint64_t maxv = (v1.getsnvalue () < v2.getsnvalue ()) ?
                v2.getsnvalue () : v1.getsnvalue ();

        if (minv > 3000000000ull) // sqrt(cmaxnative)
            throw std::runtime_error ("native value overflow");

        if (((maxv >> 32) * minv) > 2095475792ull) // cmaxnative / 2^32
            throw std::runtime_error ("native value overflow");

        return stamount(v1.getfname(), isvbc(v1), minv * maxv);
    }

    std::uint64_t value1 = v1.mantissa(), value2 = v2.mantissa();
    int offset1 = v1.exponent(), offset2 = v2.exponent();

    if (v1.native())
    {
        while (value1 < stamount::cminvalue)
        {
            value1 *= 10;
            --offset1;
        }
    }

    if (v2.native())
    {
        while (value2 < stamount::cminvalue)
        {
            value2 *= 10;
            --offset2;
        }
    }

    bool resultnegative = v1.negative() != v2.negative();
    // compute (numerator * denominator) / 10^14 with rounding
    // 10^16 <= result <= 10^18
    cbignum v;

    if ((bn_add_word64 (&v, value1) != 1) || (bn_mul_word64 (&v, value2) != 1))
        throw std::runtime_error ("internal bn error");

    if (resultnegative != roundup) // rounding down is automatic when we divide
        bn_add_word64 (&v, tento14m1);

    if  (bn_div_word64 (&v, tento14) == ((std::uint64_t) - 1))
        throw std::runtime_error ("internal bn error");

    // 10^16 <= product <= 10^18
    assert (bn_num_bytes (&v) <= 64);

    std::uint64_t amount = v.getuint64 ();
    int offset = offset1 + offset2 + 14;
    canonicalizeround (
		isnative(issue), amount, offset, resultnegative != roundup);
    return stamount (issue, amount, offset, resultnegative);
}

stamount
divround (stamount const& num, stamount const& den,
    issue const& issue, bool roundup)
{
    if (den == zero)
        throw std::runtime_error ("division by zero");

    if (num == zero)
        return {issue};

    std::uint64_t numval = num.mantissa(), denval = den.mantissa();
    int numoffset = num.exponent(), denoffset = den.exponent();

    if (num.native())
        while (numval < stamount::cminvalue)
        {
            // need to bring into range
            numval *= 10;
            --numoffset;
        }

    if (den.native())
        while (denval < stamount::cminvalue)
        {
            denval *= 10;
            --denoffset;
        }

    bool resultnegative = num.negative() != den.negative();
    // compute (numerator * 10^17) / denominator
    cbignum v;

    if ((bn_add_word64 (&v, numval) != 1) || (bn_mul_word64 (&v, tento17) != 1))
        throw std::runtime_error ("internal bn error");

    if (resultnegative != roundup) // rounding down is automatic when we divide
        bn_add_word64 (&v, denval - 1);

    if (bn_div_word64 (&v, denval) == ((std::uint64_t) - 1))
        throw std::runtime_error ("internal bn error");

    // 10^16 <= quotient <= 10^18
    assert (bn_num_bytes (&v) <= 64);

    std::uint64_t amount = v.getuint64 ();
    int offset = numoffset - denoffset - 17;
    canonicalizeround (
        isnative (issue), amount, offset, resultnegative != roundup);
    return stamount (issue, amount, offset, resultnegative);
}

} // ripple
