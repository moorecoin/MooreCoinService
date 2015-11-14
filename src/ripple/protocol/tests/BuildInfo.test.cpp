//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012, 2013 ripple labs inc.

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

#include <beastconfig.h>
#include <ripple/protocol/buildinfo.h>
#include <beast/module/core/diagnostic/semanticversion.h>
#include <beast/unit_test/suite.h>

namespace ripple {

class buildinfo_test : public beast::unit_test::suite
{
public:
    void testversion ()
    {
        testcase ("version");

        beast::semanticversion v;

        expect (v.parse (buildinfo::getrawversionstring ()));
    }


    protocolversion
    from_version (std::uint16_t major, std::uint16_t minor)
    {
        return protocolversion (major, minor);
    }

    void testvalues ()
    {
        testcase ("comparison");

        expect (from_version (1,2) == from_version (1,2));
        expect (from_version (3,4) >= from_version (3,4));
        expect (from_version (5,6) <= from_version (5,6));
        expect (from_version (7,8) >  from_version (6,7));
        expect (from_version (7,8) <  from_version (8,9));
        expect (from_version (65535,0) <  from_version (65535,65535));
        expect (from_version (65535,65535) >= from_version (65535,65535));
    }

    void teststringversion ()
    {
        testcase ("string version");

        for (std::uint16_t major = 0; major < 8; major++)
        {
            for (std::uint16_t minor = 0; minor < 8; minor++)
            {
                expect (to_string (from_version (major, minor)) ==
                    std::to_string (major) + "." + std::to_string (minor));
            }
        }
    }

    void testversionpacking ()
    {
        testcase ("version packing");

        expect (to_packed (from_version (0, 0)) == 0);
        expect (to_packed (from_version (0, 1)) == 1);
        expect (to_packed (from_version (0, 255)) == 255);
        expect (to_packed (from_version (0, 65535)) == 65535);

        expect (to_packed (from_version (1, 0)) == 65536);
        expect (to_packed (from_version (1, 1)) == 65537);
        expect (to_packed (from_version (1, 255)) == 65791);
        expect (to_packed (from_version (1, 65535)) == 131071);

        expect (to_packed (from_version (255, 0)) == 16711680);
        expect (to_packed (from_version (255, 1)) == 16711681);
        expect (to_packed (from_version (255, 255)) == 16711935);
        expect (to_packed (from_version (255, 65535)) == 16777215);

        expect (to_packed (from_version (65535, 0)) == 4294901760);
        expect (to_packed (from_version (65535, 1)) == 4294901761);
        expect (to_packed (from_version (65535, 255)) == 4294902015);
        expect (to_packed (from_version (65535, 65535)) == 4294967295);
    }

    void run ()
    {
        testversion ();
        testvalues ();
        teststringversion ();
        testversionpacking ();

        auto const current_protocol = buildinfo::getcurrentprotocol ();
        auto const minimum_protocol = buildinfo::getminimumprotocol ();

        expect (current_protocol >= minimum_protocol);

        log << "   moorecoin version: " << buildinfo::getversionstring();
        log << " protocol version: " << to_string (current_protocol);
    }
};

beast_define_testsuite(buildinfo,ripple_data,ripple);

} // ripple
