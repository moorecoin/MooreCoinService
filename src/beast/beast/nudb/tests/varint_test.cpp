//------------------------------------------------------------------------------
/*
    this file is part of beast: https://github.com/vinniefalco/beast
    copyright 2014, vinnie falco <vinnie.falco@gmail.com>

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

#include <beast/nudb/detail/varint.h>
#include <beast/unit_test/suite.h>
#include <array>

namespace beast {
namespace nudb {
namespace tests {

class varint_test : public unit_test::suite
{
public:
    void
    test_varints (std::vector<std::size_t> vv)
    {
        testcase("encode, decode");
        for (auto const v : vv)
        {
            std::array<std::uint8_t,
                detail::varint_traits<
                    std::size_t>::max> vi;
            auto const n0 =
                detail::write_varint(
                    vi.data(), v);
            expect (n0 > 0, "write error");
            std::size_t v1;
            auto const n1 =
                detail::read_varint(
                    vi.data(), n0, v1);
            expect(n1 == n0, "read error");
            expect(v == v1, "wrong value");
        }
    }

    void
    run() override
    {
        test_varints({
                0,     1,     2,
              126,   127,   128,
              253,   254,   255,
            16127, 16128, 16129,
            0xff,
            0xffff,
            0xffffffff,
            0xfffffffffffful,
            0xfffffffffffffffful});
    }
};

beast_define_testsuite(varint,nudb,beast);

} // test
} // nudb
} // beast
