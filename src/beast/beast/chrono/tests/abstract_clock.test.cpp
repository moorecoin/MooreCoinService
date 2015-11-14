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

// modules: ../impl/chrono_io.cpp

#include <beast/chrono/abstract_clock.h>
#include <beast/chrono/manual_clock.h>
#include <beast/unit_test/suite.h>
#include <sstream>
#include <string>
#include <thread>

namespace beast {

class abstract_clock_test : public unit_test::suite
{
public:
    template <class clock>
    void test (abstract_clock<clock>& c)
    {
        {
            auto const t1 (c.now ());
            std::this_thread::sleep_for (
                std::chrono::milliseconds (1500));
            auto const t2 (c.now ());

            std::stringstream ss;
            ss <<
                "t1= " << t1.time_since_epoch() <<
                ", t2= " << t2.time_since_epoch() <<
                ", elapsed= " << (t2 - t1);
            log << ss.str();
        }
    }

    void test_manual ()
    {
        using clock_type = manual_clock<std::chrono::steady_clock>;
        clock_type c;

        std::stringstream ss;

        ss << "now() = " << c.now().time_since_epoch() << std::endl;

        c.set (clock_type::time_point (std::chrono::seconds(1)));
        ss << "now() = " << c.now().time_since_epoch() << std::endl;

        c.set (clock_type::time_point (std::chrono::seconds(2)));
        ss << "now() = " << c.now().time_since_epoch() << std::endl;

        log << ss.str();
    }

    void run ()
    {
        log << "steady_clock";
        test (get_abstract_clock<
            std::chrono::steady_clock>());

        log << "system_clock";
        test (get_abstract_clock<
            std::chrono::system_clock>());

        log << "high_resolution_clock";
        test (get_abstract_clock<
            std::chrono::high_resolution_clock>());

        log << "manual_clock";
        test_manual ();

        pass ();
    }
};

beast_define_testsuite(abstract_clock,chrono,beast);

}
