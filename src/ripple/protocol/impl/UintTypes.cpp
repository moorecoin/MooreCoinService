//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2014 ripple labs inc.

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
#include <ripple/protocol/serializer.h>
#include <ripple/protocol/systemparameters.h>
#include <ripple/protocol/rippleaddress.h>
#include <ripple/protocol/uinttypes.h>

namespace ripple {

std::string to_string(account const& account)
{
    return rippleaddress::createaccountid (account).humanaccountid ();
}

std::string to_string(currency const& currency)
{
    static currency const sisobits ("ffffffffffffffffffffffff000000ffffffffff");

    // characters we are willing to allow in the ascii representation of a
    // three-letter currency code.
    static std::string const allowed_characters =
        "abcdefghijklmnopqrstuvwxyz"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789"
        "<>(){}[]|?!@#$%^&*";

    if (currency == zero)
        return systemcurrencycode();

	if (currency == vbccurrency())
		return systemcurrencycodevbc();

    if (currency == nocurrency())
        return "1";

    if ((currency & sisobits).iszero ())
    {
        // the offset of the 3 character iso code in the currency descriptor
        int const isooffset = 12;

        std::string const iso(
            currency.data () + isooffset,
            currency.data () + isooffset + 3);

        // specifying the system currency code using iso-style representation
        // is not allowed.
		if ((iso != systemcurrencycode()) && (iso != systemcurrencycodevbc()) &&
            (iso.find_first_not_of (allowed_characters) == std::string::npos))
        {
            return iso;
        }
    }

    return strhex (currency.begin (), currency.size ());
}

bool to_currency(currency& currency, std::string const& code)
{
    if (code.empty () || !code.compare (systemcurrencycode()))
    {
        currency = zero;
        return true;
    }

	if (code.empty() || !code.compare(systemcurrencycodevbc()))
	{
		currency = vbccurrency();
		return true;
	}

    static const int currency_code_length = 3;
    if (code.size () == currency_code_length)
    {
        blob codeblob (currency_code_length);

        std::transform (code.begin (), code.end (), codeblob.begin (), ::toupper);

        serializer  s;

        s.addzeros (96 / 8);
        s.addraw (codeblob);
        s.addzeros (16 / 8);
        s.addzeros (24 / 8);

        s.get160 (currency, 0);
        return true;
    }

    if (40 == code.size ())
        return currency.sethex (code);

    return false;
}

currency to_currency(std::string const& code)
{
    currency currency;
    if (!to_currency(currency, code))
        currency = nocurrency();
    return currency;
}

bool to_issuer(account& issuer, std::string const& s)
{
    if (s.size () == (160 / 4))
    {
        issuer.sethex (s);
        return true;
    }
    rippleaddress address;
    bool success = address.setaccountid (s);
    if (success)
        issuer = address.getaccountid ();
    return success;
}

account const& xrpaccount()
{
    static account const account(0);
    return account;
}

currency const& xrpcurrency()
{
    static currency const currency(0);
    return currency;
}

account const& vbcaccount()
{
	static account const account(0xff);
	return account;
}

currency const& vbccurrency()
{
	static currency const currency(0xff);
	return currency;
}

account const& noaccount()
{
    static account const account(1);
    return account;
}

currency const& nocurrency()
{
    static currency const currency(1);
    return currency;
}

currency const& badcurrency()
{
    static currency const currency(0x5652500000000000);
    return currency;
}
    
currency const& assetcurrency()
{
    static currency const currency("0x4153534554000000000000000000000000000000");
    return currency;
}

} // ripple
