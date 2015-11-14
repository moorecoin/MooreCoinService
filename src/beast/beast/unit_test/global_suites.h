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

#ifndef beast_unit_test_global_suites_h_included
#define beast_unit_test_global_suites_h_included

#include <beast/unit_test/suite_list.h>

namespace beast {
namespace unit_test {

namespace detail {

template <class = void>
suite_list&
global_suites()
{
    static suite_list s;
    return s;
}

template <class suite>
struct insert_suite
{
    template <class = void>
    insert_suite (char const* name, char const* module,
        char const* library, bool manual);
};

template <class suite>
template <class>
insert_suite<suite>::insert_suite (char const* name,
    char const* module, char const* library, bool manual)
{
    global_suites().insert <suite> (
        name, module, library, manual);
}

} // detail

/** holds suites registered during static initialization. */
inline
suite_list const&
global_suites()
{
    return detail::global_suites();
}

}
}

#endif
