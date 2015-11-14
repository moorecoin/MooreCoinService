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

#ifndef json_autolink_h_included
#define json_autolink_h_included

// vfalco todo remove this file
#error this file is deprecated!

# include "config.h"

# ifdef json_in_cpptl
#  include <cpptl/cpptl_autolink.h>
# endif

# if !defined(json_no_autolink)  &&  !defined(json_dll_build)  &&  !defined(json_in_cpptl)
#  define cpptl_autolink_name "json"
#  undef cpptl_autolink_dll
#  ifdef json_dll
#   define cpptl_autolink_dll
#  endif
#  include "autolink.h"
# endif

#endif // json_autolink_h_included
