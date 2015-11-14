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

#ifndef beast_unit_test_print_h_inlcuded
#define beast_unit_test_print_h_inlcuded

#include <beast/unit_test/amount.h>
#include <beast/unit_test/results.h>

#include <beast/streams/abstract_ostream.h>
#include <beast/streams/basic_std_ostream.h>

#include <iostream>
#include <string>

namespace beast {
namespace unit_test {

/** write test results to the specified output stream. */
/** @{ */
template <class = void>
void
print (results const& r, abstract_ostream& stream)
{
    for (auto const& s : r)
    {
        for (auto const& c : s)
        {
            stream <<
                s.name() <<
                (c.name().empty() ? "" : ("." + c.name()));

            std::size_t i (1);
            for (auto const& t : c.tests)
            {
                if (! t.pass)
                    stream <<
                        "#" << i <<
                        " failed: " << t.reason;
                ++i;
            }
        }
    }

    stream <<
        amount (r.size(), "suite") << ", " <<
        amount (r.cases(), "case") << ", " <<
        amount (r.total(), "test") << " total, " <<
        amount (r.failed(), "failure")
        ;
}

template <class = void>
void
print (results const& r, std::ostream& stream = std::cout)
{
    auto s (make_std_ostream (stream));
    print (r, s);
}

} // unit_test
} // beast

#endif
