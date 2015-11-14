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

#include <beast/unit_test/amount.h>
#include <beast/unit_test/global_suites.h>
#include <beast/unit_test/suite.h>
#include <string>

// include this .cpp in your project to gain access to the printing suite

namespace beast {
namespace unit_test {
namespace detail {

/** a suite that prints the list of globally defined suites. */
class print_test : public suite
{
private:
    template <class = void>
    void
    do_run();

public:
    template <class = void>
    static
    std::string
    prefix (suite_info const& s);

    template <class = void>
    void
    print (suite_list &c);

    void
    run()
    {
        do_run();
    }
};

template <class>
void
print_test::do_run()
{
    log << "------------------------------------------";
    print (global_suites());
    log << "------------------------------------------";
    pass();
}

template <class>
std::string
print_test::prefix (suite_info const& s)
{
    if (s.manual())
        return "|m| ";
    return "    ";
}

template <class>
void
print_test::print (suite_list &c)
{
    std::size_t manual (0);
    for (auto const& s : c)
    {
        log <<
            prefix (s) <<
            s.full_name();
        if (s.manual())
            ++manual;
    }
    log <<
        amount (c.size(), "suite") << " total, " <<
        amount (manual, "manual suite")
        ;
}

beast_define_testsuite_manual(print,unit_test,beast);

}
}
}
