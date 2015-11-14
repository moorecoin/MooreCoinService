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

#ifndef beast_unit_test_match_h_inlcuded
#define beast_unit_test_match_h_inlcuded

#include <beast/unit_test/suite_info.h>

namespace beast {
namespace unit_test {

// predicate for implementing matches
class selector
{
public:
    enum mode_t
    {
        // run all tests except manual ones
        all,

        // run tests that match in any field
        automatch,

        // match on suite
        suite,

        // match on library
        library,

        // match on module (used internally)
        module,

        // match nothing (used internally)
        none
    };

private:
    mode_t mode_;
    std::string pat_;
    std::string library_;

public:
    template <class = void>
    explicit
    selector (mode_t mode, std::string const& pattern = "");

    template <class = void>
    bool
    operator() (suite_info const& s);
};

//------------------------------------------------------------------------------

template <class>
selector::selector (mode_t mode, std::string const& pattern)
    : mode_ (mode)
    , pat_ (pattern)
{
    if (mode_ == automatch && pattern.empty())
        mode_ = all;
}

template <class>
bool
selector::operator() (suite_info const& s)
{
    switch (mode_)
    {
    case automatch:
        // suite or full name
        if (s.name() == pat_ || s.full_name() == pat_)
        {
            mode_ = none;
            return true;
        }

        // check module
        if (pat_ == s.module())
        {
            mode_ = module;
            library_ = s.library();
            return ! s.manual();
        }

        // check library
        if (pat_ == s.library())
        {
            mode_ = library;
            return ! s.manual();
        }

        return false;

    case suite:
        return pat_ == s.name();

    case module:
        return pat_ == s.module() && ! s.manual();

    case library:
        return pat_ == s.library() && ! s.manual();

    case none:
        return false;

    case all:
    default:
        // fall through
        break;
    };

    return ! s.manual();
}

//------------------------------------------------------------------------------

// utility functions for producing predicates to select suites.

/** returns a predicate that implements a smart matching rule.
    the predicate checks the suite, module, and library fields of the
    suite_info in that order. when it finds a match, it changes modes
    depending on what was found:
    
        if a suite is matched first, then only the suite is selected. the
        suite may be marked manual.

        if a module is matched first, then only suites from that module
        and library not marked manual are selected from then on.

        if a library is matched first, then only suites from that library
        not marked manual are selected from then on.

*/
inline
selector
match_auto (std::string const& name)
{
    return selector (selector::automatch, name);
}

/** return a predicate that matches all suites not marked manual. */
inline
selector
match_all()
{
    return selector (selector::all);
}

/** returns a predicate that matches a specific suite. */
inline
selector
match_suite (std::string const& name)
{
    return selector (selector::suite, name);
}

/** returns a predicate that matches all suites in a library. */
inline
selector
match_library (std::string const& name)
{
    return selector (selector::library, name);
}

} // unit_test
} // beast

#endif
