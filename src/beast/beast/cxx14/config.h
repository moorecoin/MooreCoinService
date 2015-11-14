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

#ifndef beast_cxx14_config_h_included
#define beast_cxx14_config_h_included

// sets c++14 compatibility configuration macros based on build environment

// disables beast c++14 compatibility additions when set to 1
// note, some compatibilty features are enabled or disabled individually.
//
#ifndef beast_no_cxx14_compatibility
# ifdef _msc_ver
#  define beast_no_cxx14_compatibility 1
# elif defined(__clang__) && defined(_libcpp_version) && __cplusplus >= 201305
#  define beast_no_cxx14_compatibility 1
# else
#  define beast_no_cxx14_compatibility 0
# endif
#endif

// disables beast's std::make_unique
#ifndef beast_no_cxx14_make_unique
# ifdef _msc_ver
#  define beast_no_cxx14_make_unique 1
# elif defined(__clang__) && defined(_libcpp_version) && __cplusplus >= 201305
#  define beast_no_cxx14_make_unique 1
# else
#  define beast_no_cxx14_make_unique 0
# endif
#endif

// disables beast's std::equal safe iterator overloads
#ifndef beast_no_cxx14_equal
# define beast_no_cxx14_equal 0
#endif

#endif
