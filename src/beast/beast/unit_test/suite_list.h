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

#ifndef beast_unit_test_suite_list_h_included
#define beast_unit_test_suite_list_h_included

#include <beast/unit_test/suite_info.h>

#include <beast/container/const_container.h>

#include <cassert>
//#include <list>
#include <typeindex>
#include <set>
#include <unordered_set>

namespace beast {
namespace unit_test {

/** a container of test suites. */
class suite_list
    : public const_container <std::set <suite_info>>
{
private:
#ifndef ndebug
    std::unordered_set <std::string> names_;
    std::unordered_set <std::type_index> classes_;
#endif

public:
    /** insert a suite into the set.
        the suite must not already exist.
    */
    template <class suite>
    void
    insert (char const* name, char const* module, char const* library,
        bool manual);
};

//------------------------------------------------------------------------------

template <class suite>
void
suite_list::insert (char const* name, char const* module, char const* library,
    bool manual)
{
#ifndef ndebug
    {
        auto const result (names_.insert (name));
        assert (result.second); // duplicate name
    }

    {
        auto const result (classes_.insert (
            std::type_index (typeid(suite))));
        assert (result.second); // duplicate type
    }
#endif

    cont().emplace (std::move (make_suite_info <suite> (
        name, module, library, manual)));
}

} // unit_test
} // beast

#endif

