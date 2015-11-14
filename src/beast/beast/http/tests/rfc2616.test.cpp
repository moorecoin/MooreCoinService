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

#if beast_include_beastconfig
#include "../../beastconfig.h"
#endif

#include <beast/http/rfc2616.h>
#include <beast/unit_test/suite.h>
#include <string>
#include <vector>

namespace beast {
namespace rfc2616 {

class rfc2616_test : public beast::unit_test::suite
{
public:
    void
    check (std::string const& s,
        std::vector <std::string> const& expected)
    {
        auto const parsed = split_commas(s.begin(), s.end());
        expect (parsed == expected);
    }

    void test_split_commas()
    {
        testcase("split_commas");
        check ("",              {});
        check (" ",             {});
        check ("  ",            {});
        check ("\t",            {});
        check (" \t ",          {});
        check (",",             {});
        check (",,",            {});
        check (" ,",            {});
        check (" , ,",          {});
        check ("x",             {"x"});
        check (" x",            {"x"});
        check (" \t x",         {"x"});
        check ("x ",            {"x"});
        check ("x \t",          {"x"});
        check (" \t x \t ",     {"x"});
        check ("\"\"",          {});
        check (" \"\"",         {});
        check ("\"\" ",         {});
        check ("\"x\"",         {"x"});
        check ("\" \"",         {" "});
        check ("\" x\"",        {" x"});
        check ("\"x \"",        {"x "});
        check ("\" x \"",       {" x "});
        check ("\"\tx \"",      {"\tx "});
        check ("x,y",           { "x", "y" });
        check ("x ,\ty ",       { "x", "y" });
        check ("x, y, z",       {"x","y","z"});
        check ("x, \"y\", z",   {"x","y","z"});
    }

    void
    run()
    {
        test_split_commas();
    }
};

beast_define_testsuite(rfc2616,http,beast);

}
}
