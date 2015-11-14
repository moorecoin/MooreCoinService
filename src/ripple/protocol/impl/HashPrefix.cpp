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
#include <ripple/protocol/hashprefix.h>

namespace ripple {

// the prefix codes are part of the ripple protocol and existing codes cannot be
// arbitrarily changed.

hashprefix const hashprefix::transactionid               ('t', 'x', 'n');
hashprefix const hashprefix::txnode                      ('s', 'n', 'd');
hashprefix const hashprefix::leafnode                    ('m', 'l', 'n');
hashprefix const hashprefix::innernode                   ('m', 'i', 'n');
hashprefix const hashprefix::ledgermaster                ('l', 'w', 'r');
hashprefix const hashprefix::txsign                      ('s', 't', 'x');
hashprefix const hashprefix::validation                  ('v', 'a', 'l');
hashprefix const hashprefix::proposal                    ('p', 'r', 'p');

} // ripple
