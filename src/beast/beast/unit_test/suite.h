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

#ifndef beast_unit_test_suite_h_included
#define beast_unit_test_suite_h_included

#include <beast/unit_test/runner.h>

#include <beast/utility/noexcept.h>
#include <string>
#include <sstream>

namespace beast {
namespace unit_test {

class thread;

/** a testsuite class.
    derived classes execute a series of testcases, where each testcase is
    a series of pass/fail tests. to provide a unit test using this class,
    derive from it and use the beast_define_unit_test macro in a
    translation unit.
*/
class suite
{
public:
    enum abort_t
    {
        no_abort_on_fail,
        abort_on_fail
    };

private:
    bool abort_ = false;
    bool aborted_ = false;
    runner* runner_ = nullptr;

    // this exception is thrown internally to stop the current suite
    // in the event of a failure, if the option to stop is set.
    struct abort_exception : public std::exception
    {
        char const*
        what() const noexcept override
        {
            return "suite aborted";
        }
    };

    // memberspace
    class log_t
    {
    private:
        friend class suite;
        suite* suite_ = nullptr;

    public:
        log_t () = default;

        template <class t>
        abstract_ostream::scoped_stream_type
        operator<< (t const& t);

        /** returns the raw stream used for output. */
        abstract_ostream&
        stream();
    };

    class scoped_testcase;

    // memberspace
    class testcase_t
    {
    private:
        friend class suite;
        suite* suite_ = nullptr;
        std::stringstream ss_;

    public:
        testcase_t() = default;

        /** open a new testcase.
            a testcase is a series of evaluated test conditions. a test suite
            may have multiple test cases. a test is associated with the last
            opened testcase. when the test first runs, a default unnamed
            case is opened. tests with only one case may omit the call
            to testcase.
            @param abort if `true`, the suite will be stopped on first failure.
        */
        void
        operator() (std::string const& name,
            abort_t abort = no_abort_on_fail);

        /** stream style composition of testcase names. */
        /** @{ */
        scoped_testcase
        operator() (abort_t abort);

        template <class t>
        scoped_testcase
        operator<< (t const& t);
        /** @} */
    };

public:
    /** type for scoped stream logging.
        to use this type, declare a local variable of the type
        on the stack in derived class member function and construct
        it from log.stream();

        @code

        scoped_stream ss (log.stream();

        ss << "hello" << std::endl;
        ss << "world" << std::endl;

        @endcode

        streams constructed in this fashion will not have the line
        ending automatically appended.

        thread safety:

            the scoped_stream may only be used by one thread.
            multiline output sent to the stream will be atomically
            written to the underlying abstract_ostream
    */
    typedef abstract_ostream::scoped_stream_type scoped_stream;

    /** memberspace for logging. */
    log_t log;

    /** memberspace for declaring test cases. */
    testcase_t testcase;

    /** invokes the test using the specified runner.
        data members are set up here instead of the constructor as a
        convenience to writing the derived class to avoid repetition of
        forwarded constructor arguments to the base.
        normally this is called by the framework for you.
    */
    void
    operator() (runner& r);

    /** evaluate a test condition.
        the condition is passed as a template argument instead of `bool` so
        that implicit conversion is not required. the `reason` argument is
        logged if the condition is false.
        @return `true` if the test condition indicates success.
    */
    template <class condition, class string>
    bool
    expect (condition shouldbetrue,
        string const& reason);

    template <class condition>
    bool
    expect (condition shouldbetrue)
    {
        return expect (shouldbetrue, "");
    }

    /** return the argument associated with the runner. */
    std::string const&
    arg() const
    {
        return runner_->arg();
    }

    // deprecated
    // @return `true` if the test condition indicates success (a false value)
    template <class condition, class string>
    bool
    unexpected (condition shouldbefalse,
        string const& reason);

    template <class condition>
    bool
    unexpected (condition shouldbefalse)
    {
        return unexpected (shouldbefalse, "");
    }

    /** record a successful test condition. */
    template <class = void>
    void
    pass();

    /** record a failure. */
    template <class = void>
    void
    fail (std::string const& reason = "");

private:
    friend class thread;

    /** runs the suite. */
    virtual
    void
    run() = 0;

    template <class = void>
    void
    propagate_abort();

    template <class = void>
    void
    run (runner& r);
};

//------------------------------------------------------------------------------

template <class t>
inline
abstract_ostream::scoped_stream_type
suite::log_t::operator<< (t const& t)
{
    return suite_->runner_->stream() << t;
}

/** returns the raw stream used for output. */
inline
abstract_ostream&
suite::log_t::stream()
{
    return suite_->runner_->stream();
}

//------------------------------------------------------------------------------

// helper for streaming testcase names
class suite::scoped_testcase
{
private:
    suite* suite_;
    std::stringstream* ss_;

public:
    ~scoped_testcase();

    scoped_testcase (suite* s, std::stringstream* ss);

    template <class t>
    scoped_testcase (suite* s, std::stringstream* ss, t const& t);

    scoped_testcase& operator= (scoped_testcase const&) = delete;

    template <class t>
    scoped_testcase&
    operator<< (t const& t);
};

inline
suite::scoped_testcase::~scoped_testcase()
{
    auto const& name (ss_->str());
    if (! name.empty())
        suite_->runner_->testcase (name);
}

inline
suite::scoped_testcase::scoped_testcase (suite* s, std::stringstream* ss)
    : suite_ (s)
    , ss_ (ss)
{
    ss_->clear();
    ss_->str({});

}

template <class t>
inline
suite::scoped_testcase::scoped_testcase (suite* s, std::stringstream* ss, t const& t)
    : suite_ (s)
    , ss_ (ss)
{
    ss_->clear();
    ss_->str({});
    *ss_ << t;
}

template <class t>
inline
suite::scoped_testcase&
suite::scoped_testcase::operator<< (t const& t)
{
    *ss_ << t;
    return *this;
}

//------------------------------------------------------------------------------

inline
void
suite::testcase_t::operator() (std::string const& name,
    abort_t abort)
{
    suite_->abort_ = abort == abort_on_fail;
    suite_->runner_->testcase (name);
}

inline
suite::scoped_testcase
suite::testcase_t::operator() (abort_t abort)
{
    suite_->abort_ = abort == abort_on_fail;
    return { suite_, &ss_ };
}

template <class t>
inline
suite::scoped_testcase
suite::testcase_t::operator<< (t const& t)
{
    return { suite_, &ss_, t };
}

//------------------------------------------------------------------------------

inline
void
suite::operator() (runner& r)
{
    run (r);
}

template <class condition, class string>
inline
bool
suite::expect (condition shouldbetrue,
    string const& reason)
{
    if (shouldbetrue)
        pass();
    else
        fail (reason);
    return shouldbetrue;
}

template <class condition, class string>
inline
bool
suite::unexpected (condition shouldbefalse,
    string const& reason)
{
    if (! shouldbefalse)
        pass();
    else
        fail (reason);
    return ! shouldbefalse;
}

template <class>
void
suite::pass()
{
    propagate_abort();
    runner_->pass();
}

template <class>
void
suite::fail (std::string const& reason)
{
    propagate_abort();
    runner_->fail (reason);
    if (abort_)
    {
        aborted_ = true;
        throw abort_exception();
    }
}

template <class>
void
suite::propagate_abort()
{
    if (abort_ && aborted_)
        throw abort_exception();
}

template <class>
void
suite::run (runner& r)
{
    runner_ = &r;
    log.suite_ = this;
    testcase.suite_ = this;

    try
    {
        run();
    }
    catch (abort_exception const&)
    {
        // ends the suite
    }
    catch (std::exception const& e)
    {
        runner_->fail ("unhandled exception: " +
            std::string (e.what()));
    }
    catch (...)
    {
        runner_->fail ("unhandled exception");
    }
}

} // unit_test
} // beast

//------------------------------------------------------------------------------

// detail:
// this inserts the suite with the given manual flag
#define beast_define_testsuite_insert(class,module,library,manual) \
    static beast::unit_test::detail::insert_suite <class##_test>   \
        library ## module ## class ## _test_instance (             \
            #class, #module, #library, manual);

//------------------------------------------------------------------------------

// preprocessor directives for controlling unit test definitions.

// if this is already defined, don't redefine it. this allows
// programs to provide custom behavior for testsuite definitions
//
#ifndef beast_define_testsuite

/** enables insertion of test suites into the global container.
    the default is to insert all test suite definitions into the global
    container. if beast_define_testsuite is user defined, this macro
    has no effect.
*/
#ifndef beast_no_unit_test_inline
#define beast_no_unit_test_inline 0
#endif

/** define a unit test suite.

    class     the type representing the class being tested.
    module    identifies the module.
    library   identifies the library.

    the declaration for the class implementing the test should be the same
    as class ## _test. for example, if class is aged_ordered_container, the
    test class must be declared as:

    @code

    struct aged_ordered_container_test : beast::unit_test::suite
    {
        //...
    };

    @endcode

    the macro invocation must appear in the same namespace as the test class.
*/

#if beast_no_unit_test_inline
#define beast_define_testsuite(class,module,library)

#else
#include <beast/unit_test/global_suites.h>
#define beast_define_testsuite(class,module,library) \
        beast_define_testsuite_insert(class,module,library,false)
#define beast_define_testsuite_manual(class,module,library) \
        beast_define_testsuite_insert(class,module,library,true)

#endif

#endif

//------------------------------------------------------------------------------

#endif
