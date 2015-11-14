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

#ifndef ripple_basics_strhex_h_included
#define ripple_basics_strhex_h_included

#include <string>
    
namespace ripple {

/** converts an integer to the corresponding hex digit
    @param idigit 0-15 inclusive
    @return a character from '0'-'9' or 'a'-'f' on success; 0 on failure.
*/
char charhex (int idigit);

/** @{ */
/** converts a hex digit to the corresponding integer
    @param cdigit one of '0'-'9', 'a'-'f' or 'a'-'f'
    @return an integer from 0 to 15 on success; -1 on failure.
*/
int
charunhex (unsigned char c);

inline
int
charunhex (char c)
{
    return charunhex (static_cast<unsigned char>(c));
}
/** @} */

// nikb todo cleanup this function and reduce the need for the many overloads
//           it has in various places.
template<class iterator>
std::string strhex (iterator first, int isize)
{
    std::string strdst;

    strdst.resize (isize * 2);

    for (int i = 0; i < isize; i++)
    {
        unsigned char c = *first++;

        strdst[i * 2]     = charhex (c >> 4);
        strdst[i * 2 + 1] = charhex (c & 15);
    }

    return strdst;
}

}

#endif
