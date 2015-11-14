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

#ifndef beast_unit_test_recorder_h_inlcuded
#define beast_unit_test_recorder_h_inlcuded

#include <beast/unit_test/results.h>
#include <beast/unit_test/runner.h>

namespace beast {
namespace unit_test {

/** a test runner that stores the results. */
class recorder : public runner
{
private:
    results m_results;
    suite_results m_suite;
    case_results m_case;

public:
    recorder() = default;
    recorder (recorder const&) = default;
    recorder& operator= (recorder const&) = default;

    /** returns a report with the results of all completed suites. */
    results const&
    report() const
    {
        return m_results;
    }

private:
    virtual
    void
    on_suite_begin (suite_info const& info) override
    {
        m_suite = suite_results (info.full_name());
    }

    virtual
    void
    on_suite_end() override
    {
        m_results.insert (std::move (m_suite));
    }

    virtual
    void
    on_case_begin (std::string const& name) override
    {
        m_case = case_results (name);
    }

    virtual
    void
    on_case_end() override
    {
        if (m_case.tests.size() > 0)
            m_suite.insert (std::move (m_case));
    }

    virtual
    void
    on_pass() override
    {
        m_case.tests.pass();
    }

    virtual
    void
    on_fail (std::string const& reason) override
    {
        m_case.tests.fail (reason);
    }

    virtual
    void
    on_log (std::string const& s) override
    {
        m_case.log.insert (s);
    }
};

} // unit_test
} // beast

#endif
