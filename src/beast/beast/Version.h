//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2013, vinnie falco <vinnie.falco@gmail.com>

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

#ifndef beast_version_h_included
#define beast_version_h_included

/** current beast version number.
    see also systemstats::getbeastversion() for a string version.
*/
// vfalco todo replace this with semanticverson
#define beast_major_version      1
#define beast_minor_version      0
#define beast_buildnumber        0

/** current beast version number.
    bits 16 to 32 = major version.
    bits 8 to 16 = minor version.
    bits 0 to 8 = point release.
    see also systemstats::getbeastversion() for a string version.
*/
#define beast_version ((beast_major_version << 16) + (beast_minor_version << 8) + beast_buildnumber)

#endif

