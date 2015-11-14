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

#ifndef ripple_crypto_rfc1751_h_included
#define ripple_crypto_rfc1751_h_included

#include <string>
#include <vector>

namespace ripple {

class rfc1751
{
public:
    static int getkeyfromenglish (std::string& strkey, std::string const& strhuman);

    static void getenglishfromkey (std::string& strhuman, std::string const& strkey);

    /** chooses a single dictionary word from the data.

        this is not particularly secure but it can be useful to provide
        a unique name for something given a guid or fixed data. we use
        it to turn the pubkey_node into an easily remembered and identified
        4 character string.
    */
    static std::string getwordfromblob (void const* blob, size_t bytes);

private:
    static unsigned long extract (char const* s, int start, int length);
    static void btoe (std::string& strhuman, std::string const& strdata);
    static void insert (char* s, int x, int start, int length);
    static void standard (std::string& strword);
    static int wsrch (std::string const& strword, int imin, int imax);
    static int etob (std::string& strdata, std::vector<std::string> vshuman);

    static char const* s_dictionary [];
};

} // ripple

#endif
