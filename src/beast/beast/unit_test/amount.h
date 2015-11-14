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

#ifndef beast_unit_test_amount_h_inlcuded
#define beast_unit_test_amount_h_inlcuded

#include <cstddef>
#include <ostream>
#include <string>

namespace beast {
namespace unit_test {

/** utility for producing nicely composed output of amounts with units. */
class amount
{
private:
    std::size_t n_;
    std::string const& what_;

public:
    amount (amount const&) = default;
    amount& operator= (amount const&) = delete;

    template <class = void>
    amount (std::size_t n, std::string const& what);

    friend
    std::ostream&
    operator<< (std::ostream& s, amount const& t);
};

template <class>
amount::amount (std::size_t n, std::string const& what)
    : n_ (n)
    , what_ (what)
{
}

inline
std::ostream&
operator<< (std::ostream& s, amount const& t)
{
    s << t.n_ << " " << t.what_ << ((t.n_ != 1) ? "s" : "");
    return s;
}

} // unit_test
} // beast

#endif
