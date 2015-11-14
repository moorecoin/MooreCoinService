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

#ifndef beast_unit_test_suite_info_h_included
#define beast_unit_test_suite_info_h_included

#include <functional>
#include <string>
#include <utility>

namespace beast {
namespace unit_test {

class runner;

/** associates a unit test type with metadata. */
class suite_info
{
private:
    using run_type = std::function <void (runner&)>;

    std::string name_;
    std::string module_;
    std::string library_;
    bool m_manual;
    run_type m_run;

public:
    template <class = void>
    suite_info (std::string const& name, std::string const& module,
        std::string const& library, bool manual, run_type run);

    std::string const&
    name() const
    {
        return name_;
    }

    std::string const&
    module() const
    {
        return module_;
    }

    std::string const&
    library() const
    {
        return library_;
    }

    /** returns `true` if this suite only runs manually. */
    bool
    manual() const
    {
        return m_manual;
    }

    /** return the canonical suite name as a string. */
    template <class = void>
    std::string
    full_name() const;

    /** run a new instance of the associated test suite. */
    void
    run (runner& r) const
    {
        m_run (r);
    }
};

//------------------------------------------------------------------------------

template <class>
suite_info::suite_info (std::string const& name, std::string const& module,
        std::string const& library, bool manual, run_type run)
    : name_ (name)
    , module_ (module)
    , library_ (library)
    , m_manual (manual)
    , m_run (std::move (run))
{
}

template <class>
std::string
suite_info::full_name() const
{
    return library_ + "." + module_ + "." + name_;
}

inline
bool
operator< (suite_info const& lhs, suite_info const& rhs)
{
    return lhs.full_name() < rhs.full_name();
}

/** convenience for producing suite_info for a given test type. */
template <class suite>
suite_info
make_suite_info (std::string const& name, std::string const& module,
    std::string const& library, bool manual)
{
    return suite_info(name, module, library, manual,
        [](runner& r) { return suite{}(r); });
}

} // unit_test
} // beast

#endif
