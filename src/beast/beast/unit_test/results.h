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

#ifndef beast_unit_test_results_h_inlcuded
#define beast_unit_test_results_h_inlcuded

#include <beast/container/const_container.h>

#include <string>
#include <vector>

namespace beast {
namespace unit_test {

/** holds a set of test condition outcomes in a testcase. */
class case_results
{
public:
    /** holds the result of evaluating one test condition. */
    struct test
    {
        explicit test (bool pass_)
            : pass (pass_)
        {
        }

        test (bool pass_, std::string const& reason_)
            : pass (pass_)
            , reason (reason_)
        {
        }

        bool pass;
        std::string reason;
    };

private:
    class tests_t
        : public const_container <std::vector <test>>
    {
    private:
        std::size_t failed_;

    public:
        tests_t ()
            : failed_ (0)
        {
        }

        /** returns the total number of test conditions. */
        std::size_t
        total() const
        {
            return cont().size();
        }

        /** returns the number of failed test conditions. */
        std::size_t
        failed() const
        {
            return failed_;
        }

        /** register a successful test condition. */
        void
        pass()
        {
            cont().emplace_back (true);
        }

        /** register a failed test condition. */
        void
        fail (std::string const& reason = "")
        {
            ++failed_;
            cont().emplace_back (false, reason);
        }
    };

    class log_t
        : public const_container <std::vector <std::string>>
    {
    public:
        /** insert a string into the log. */
        void
        insert (std::string const& s)
        {
            cont().push_back (s);
        }
    };

    std::string name_;

public:
    explicit case_results (std::string const& name = "")
        : name_ (name)
    {
    }

    /** returns the name of this testcase. */
    std::string const&
    name() const
    {
        return name_;
    }

    /** memberspace for a container of test condition outcomes. */
    tests_t tests;

    /** memberspace for a container of testcase log messages. */
    log_t log;
};

//--------------------------------------------------------------------------

/** holds the set of testcase results in a suite. */
class suite_results
    : public const_container <std::vector <case_results>>
{
private:
    std::string name_;
    std::size_t total_ = 0;
    std::size_t failed_ = 0;

public:
    explicit suite_results (std::string const& name = "")
        : name_ (name)
    {
    }

    /** returns the name of this suite. */
    std::string const&
    name() const
    {
        return name_;
    }

    /** returns the total number of test conditions. */
    std::size_t
    total() const
    {
        return total_;
    }

    /** returns the number of failures. */
    std::size_t
    failed() const
    {
        return failed_;
    }

    /** insert a set of testcase results. */
    /** @{ */
    void
    insert (case_results&& r)
    {
        cont().emplace_back (std::move (r));
        total_ += r.tests.total();
        failed_ += r.tests.failed();
    }

    void
    insert (case_results const& r)
    {
        cont().push_back (r);
        total_ += r.tests.total();
        failed_ += r.tests.failed();
    }
    /** @} */
};

//------------------------------------------------------------------------------

// vfalco todo make this a template class using scoped allocators
/** holds the results of running a set of testsuites. */
class results
    : public const_container <std::vector <suite_results>>
{
private:
    std::size_t m_cases;
    std::size_t total_;
    std::size_t failed_;

public:
    results()
        : m_cases (0)
        , total_ (0)
        , failed_ (0)
    {
    }

    /** returns the total number of test cases. */
    std::size_t
    cases() const
    {
        return m_cases;
    }

    /** returns the total number of test conditions. */
    std::size_t
    total() const
    {
        return total_;
    }

    /** returns the number of failures. */
    std::size_t
    failed() const
    {
        return failed_;
    }

    /** insert a set of suite results. */
    /** @{ */
    void
    insert (suite_results&& r)
    {
        m_cases += r.size();
        total_ += r.total();
        failed_ += r.failed();
        cont().emplace_back (std::move (r));
    }

    void
    insert (suite_results const& r)
    {
        m_cases += r.size();
        total_ += r.total();
        failed_ += r.failed();
        cont().push_back (r);
    }
    /** @} */
};

} // unit_test
} // beast

#endif
