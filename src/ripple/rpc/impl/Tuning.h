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

#ifndef ripple_rpc_tuning_h_included
#define ripple_rpc_tuning_h_included

namespace ripple {
namespace rpc {

/** tuned constants. */
/** @{ */
namespace tuning {

/** default account lines return per request to the
account_lines command when no limit param is specified
*/
unsigned int const defaultlinesperrequest (200);

/** minimum account lines return per request to the
account_lines command. specified in the limit param.
*/
unsigned int const minlinesperrequest (10);

/** maximum account lines return per request to the
account_lines command. specified in the limit param.
*/
unsigned int const maxlinesperrequest (400);

/** default offers return per request to the account_offers command
when no limit param is specified
*/
unsigned int const defaultoffersperrequest (200);

/** minimum offers return per request to the account_offers command.
specified in the limit param.
*/
unsigned int const minoffersperrequest (10);

/** maximum offers return per request to the account_lines command.
specified in the limit param.
*/
unsigned int const maxoffersperrequest (400);

int const defaultautofillfeemultiplier (10);
int const maxpathfindsinprogress (2);
int const maxpathfindjobcount (50);
int const maxjobqueueclients (500);
int const maxvalidatedledgerage (120);
int const maxrequestsize (1000000);

} // tuning
/** @} */

} // rpc
} // ripple

#endif
