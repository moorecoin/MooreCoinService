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

/*  this file includes all of the c-language beast sources needed to link.
    by including them here, we avoid having to muck with the sconstruct
    makefile, project file, or whatever.

    note that these sources must be compiled using the c compiler.
*/
    
#include <beastconfig.h>

#ifdef __cplusplus
#error "whoops! this file must be compiled with a c compiler!"
#endif

#include <beast/module/sqlite/sqlite.unity.c>
