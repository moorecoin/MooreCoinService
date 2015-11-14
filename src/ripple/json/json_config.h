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

#ifndef json_config_h_included
#define json_config_h_included

/// if defined, indicates that json library is embedded in cpptl library.
//# define json_in_cpptl 1

/// if defined, indicates that json may leverage cpptl library
//#  define json_use_cpptl 1
/// if defined, indicates that cpptl vector based map should be used instead of std::map
/// as value container.
//#  define json_use_cpptl_smallmap 1
/// if defined, indicates that json specific container should be used
/// (hash table & simple deque container with customizable allocator).
/// this feature is still experimental!
//#  define json_value_use_internal_map 1
/// force usage of standard new/malloc based allocator instead of memory pool based allocator.
/// the memory pools allocator used optimization (initializing value and valueinternallink
/// as if it was a pod) that may cause some validation tool to report errors.
/// only has effects if json_value_use_internal_map is defined.
//#  define json_use_simple_internal_allocator 1

/// if defined, indicates that json use exception to report invalid type manipulation
/// instead of c assert macro.
# define json_use_exception 1

# ifdef json_in_cpptl
#  include <cpptl/config.h>
#  ifndef json_use_cpptl
#   define json_use_cpptl 1
#  endif
# endif

# ifdef json_in_cpptl
#  define json_api cpptl_api
# elif defined(json_dll_build)
#  define json_api __declspec(dllexport)
# elif defined(json_dll)
#  define json_api __declspec(dllimport)
# else
#  define json_api
# endif

#endif // json_config_h_included
