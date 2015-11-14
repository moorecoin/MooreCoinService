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

#include <beast/crypto/base64.h>
#include <beast/unit_test/suite.h>

namespace beast {

class base64_test : public unit_test::suite
{
public:
    void
    check (std::string const& in, std::string const& out)
    {
        auto const encoded = base64_encode (in);
        expect (encoded == out);
        expect (base64_decode (encoded) == in);
    }

    void
    run()
    {
        check ("",       "");
        check ("f",      "zg==");
        check ("fo",     "zm8=");
        check ("foo",    "zm9v");
        check ("foob",   "zm9vyg==");
        check ("fooba",  "zm9vyme=");
        check ("foobar", "zm9vymfy");
    }
};

beast_define_testsuite(base64,crypto,beast);

}
