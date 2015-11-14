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

#ifndef beast_unit_test_runner_h_included
#define beast_unit_test_runner_h_included

#include <beast/unit_test/suite_info.h>
#include <beast/streams/abstract_ostream.h>
#include <cassert>
#include <mutex>
#include <string>

namespace beast {
namespace unit_test {

/** unit test runner interface.
    derived classes can customize the reporting behavior. this interface is
    injected into the unit_test class to receive the results of the tests.
*/
class runner
{
private:
    // reroutes log output to the runner
    class stream_t : public abstract_ostream
    {
    private:
        runner& owner_;

    public:
        stream_t() = delete;
        stream_t& operator= (stream_t const&) = delete;

        template <class = void>
        stream_t (runner& owner);

        void
        write (string_type const& s) override
        {
            owner_.log (s);
        }
    };

    stream_t stream_;
    std::string arg_;
    bool default_ = false;
    bool failed_ = false;
    bool cond_ = false;
    std::recursive_mutex mutex_;

public:
    virtual ~runner() = default;
    runner (runner const&) = default;
    runner& operator= (runner const&) = default;

    template <class = void>
    runner();

    /** set the argument string.
        the argument string is available to suites and
        allows for customization of the test. each suite
        defines its own syntax for the argumnet string.
        the same argument is passed to all suites.
    */
    void
    arg (std::string const& s)
    {
        arg_ = s;
    }

    /** returns the argument string. */
    std::string const&
    arg() const
    {
        return arg_;
    }

    /** run the specified suite.
        @return `true` if any conditions failed.
    */
    template <class = void>
    bool
    run (suite_info const& s);

    /** run a sequence of suites.
        the expression
            `fwditer::value_type`
        must be convertible to `suite_info`.
        @return `true` if any conditions failed.
    */
    template <class fwditer>
    bool
    run (fwditer first, fwditer last);

    /** conditionally run a sequence of suites.
        pred will be called as:
        @code
            bool pred (suite_info const&);
        @endcode
        @return `true` if any conditions failed.
    */
    template <class fwditer, class pred>
    bool
    run_if (fwditer first, fwditer last, pred pred = pred{});

    /** run all suites in a container.
        @return `true` if any conditions failed.
    */
    template <class sequencecontainer>
    bool
    run_each (sequencecontainer const& c);

    /** conditionally run suites in a container.
        pred will be called as:
        @code
            bool pred (suite_info const&);
        @endcode
        @return `true` if any conditions failed.
    */
    template <class sequencecontainer, class pred>
    bool
    run_each_if (sequencecontainer const& c, pred pred = pred{});

private:
    //
    // overrides
    //

    /** called when a new suite starts. */
    virtual
    void
    on_suite_begin (suite_info const&)
    {
    }

    /** called when a suite ends. */
    virtual
    void
    on_suite_end()
    {
    }

    /** called when a new case starts. */
    virtual
    void
    on_case_begin (std::string const&)
    {
    }

    /** called when a new case ends. */
    virtual
    void
    on_case_end()
    {
    }

    /** called for each passing condition. */
    virtual
    void
    on_pass ()
    {
    }

    /** called for each failing condition. */
    virtual
    void
    on_fail (std::string const&)
    {
    }

    /** called when a test logs output. */
    virtual
    void
    on_log (std::string const&)
    {
    }

private:
    friend class suite;

    abstract_ostream&
    stream()
    {
        return stream_;
    }

    // start a new testcase.
    template <class = void>
    void
    testcase (std::string const& name);

    template <class = void>
    void
    pass();

    template <class = void>
    void
    fail (std::string const& reason);

    template <class = void>
    void
    log (std::string const& s);
};

//------------------------------------------------------------------------------

template <class>
runner::stream_t::stream_t (runner& owner)
    : owner_ (owner)
{
}

//------------------------------------------------------------------------------

template <class>
runner::runner()
    : stream_ (*this)
{
}

template <class>
bool
runner::run (suite_info const& s)
{
    // enable 'default' testcase
    default_ = true;
    failed_ = false;
    on_suite_begin (s);
    s.run (*this);
    // forgot to call pass or fail.
    assert (cond_);
    on_case_end();
    on_suite_end();
    return failed_;
}

template <class fwditer>
bool
runner::run (fwditer first, fwditer last)
{
    bool failed (false);
    for (;first != last; ++first)
        failed = run (*first) || failed;
    return failed;
}

template <class fwditer, class pred>
bool
runner::run_if (fwditer first, fwditer last, pred pred)
{
    bool failed (false);
    for (;first != last; ++first)
        if (pred (*first))
            failed = run (*first) || failed;
    return failed;
}

template <class sequencecontainer>
bool
runner::run_each (sequencecontainer const& c)
{
    bool failed (false);
    for (auto const& s : c)
        failed = run (s) || failed;
    return failed;
}

template <class sequencecontainer, class pred>
bool
runner::run_each_if (sequencecontainer const& c, pred pred)
{
    bool failed (false);
    for (auto const& s : c)
        if (pred (s))
            failed = run (s) || failed;
    return failed;
}

template <class>
void
runner::testcase (std::string const& name)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    // name may not be empty
    assert (default_ || ! name.empty());
    // forgot to call pass or fail
    assert (default_ || cond_);
    if (! default_)
        on_case_end();
    default_ = false;
    cond_ = false;
    on_case_begin (name);
}

template <class>
void
runner::pass()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (default_)
        testcase ("");
    on_pass();
    cond_ = true;
}

template <class>
void
runner::fail (std::string const& reason)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (default_)
        testcase ("");
    on_fail (reason);
    failed_ = true;
    cond_ = true;
}

template <class>
void
runner::log (std::string const& s)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (default_)
        testcase ("");
    on_log (s);
}

} // unit_test
} // beast

#endif
