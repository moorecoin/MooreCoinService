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

// modules: ../impl/ipendpoint.cpp ../impl/ipaddressv4.cpp ../impl/ipaddressv6.cpp

#if beast_include_beastconfig
#include "../../beastconfig.h"
#endif

#include <beast/net/ipendpoint.h>
#include <beast/net/detail/parse.h>

#include <beast/unit_test/suite.h>

#include <typeinfo>

namespace beast {
namespace ip {

//------------------------------------------------------------------------------

class ipendpoint_test : public unit_test::suite
{
public:
    void shouldparsev4 (std::string const& s, std::uint32_t value)
    {
        std::pair <addressv4, bool> const result (
            addressv4::from_string (s));

        if (expect (result.second))
        {
            if (expect (result.first.value == value))
            {
                expect (to_string (result.first) == s);
            }
        }
    }

    void failparsev4 (std::string const& s)
    {
        unexpected (addressv4::from_string (s).second);
    }

    void testaddressv4 ()
    {
        testcase ("addressv4");

        expect (addressv4().value == 0);
        expect (is_unspecified (addressv4()));
        expect (addressv4(0x01020304).value == 0x01020304);
        expect (addressv4(1, 2, 3, 4).value == 0x01020304);

        unexpected (is_unspecified (addressv4(1, 2, 3, 4)));

        addressv4 const v1 (1);
        expect (addressv4(v1).value == 1);

        {
            addressv4 v;
            v = v1;
            expect (v.value == v1.value);
        }

        {
            addressv4 v;
            v [0] = 1;
            v [1] = 2;
            v [2] = 3;
            v [3] = 4;
            expect (v.value == 0x01020304);
        }

        expect (to_string (addressv4(0x01020304)) == "1.2.3.4");

        shouldparsev4 ("1.2.3.4", 0x01020304);
        shouldparsev4 ("255.255.255.255", 0xffffffff);
        shouldparsev4 ("0.0.0.0", 0);

        failparsev4 (".");
        failparsev4 ("..");
        failparsev4 ("...");
        failparsev4 ("....");
        failparsev4 ("1");
        failparsev4 ("1.");
        failparsev4 ("1.2");
        failparsev4 ("1.2.");
        failparsev4 ("1.2.3");
        failparsev4 ("1.2.3.");
        failparsev4 ("256.0.0.0");
        failparsev4 ("-1.2.3.4");
    }

    void testaddressv4proxy ()
    {
      testcase ("addressv4::proxy");

      addressv4 v4 (10, 0, 0, 1);
      expect (v4[0]==10);
      expect (v4[1]==0);
      expect (v4[2]==0);
      expect (v4[3]==1);

      expect((!((0xff)<<16)) == 0x00000000);
      expect((~((0xff)<<16)) == 0xff00ffff);

      v4[1] = 10;
      expect (v4[0]==10);
      expect (v4[1]==10);
      expect (v4[2]==0);
      expect (v4[3]==1);
    }

    //--------------------------------------------------------------------------

    void testaddress ()
    {
        testcase ("address");

        std::pair <address, bool> result (
            address::from_string ("1.2.3.4"));
        expect (result.second);
        if (expect (result.first.is_v4 ()))
            expect (result.first.to_v4() == addressv4 (1, 2, 3, 4));
    }

    //--------------------------------------------------------------------------

    void testendpoint ()
    {
        testcase ("endpoint");

        {
            std::pair <endpoint, bool> result (
                endpoint::from_string_checked ("1.2.3.4"));
            expect (result.second);
            if (expect (result.first.address().is_v4 ()))
            {
                expect (result.first.address().to_v4() ==
                    addressv4 (1, 2, 3, 4));
                expect (result.first.port() == 0);
                expect (to_string (result.first) == "1.2.3.4");
            }
        }

        {
            std::pair <endpoint, bool> result (
                endpoint::from_string_checked ("1.2.3.4:5"));
            expect (result.second);
            if (expect (result.first.address().is_v4 ()))
            {
                expect (result.first.address().to_v4() ==
                    addressv4 (1, 2, 3, 4));
                expect (result.first.port() == 5);
                expect (to_string (result.first) == "1.2.3.4:5");
            }
        }

        endpoint ep;

        ep = endpoint (addressv4 (127,0,0,1), 80);
        expect (! is_unspecified (ep));
        expect (! is_public (ep));
        expect (  is_private (ep));
        expect (! is_multicast (ep));
        expect (  is_loopback (ep));
        expect (to_string (ep) == "127.0.0.1:80");

        ep = endpoint (addressv4 (10,0,0,1));
        expect (addressv4::get_class (ep.to_v4()) == 'a');
        expect (! is_unspecified (ep));
        expect (! is_public (ep));
        expect (  is_private (ep));
        expect (! is_multicast (ep));
        expect (! is_loopback (ep));
        expect (to_string (ep) == "10.0.0.1");

        ep = endpoint (addressv4 (166,78,151,147));
        expect (! is_unspecified (ep));
        expect (  is_public (ep));
        expect (! is_private (ep));
        expect (! is_multicast (ep));
        expect (! is_loopback (ep));
        expect (to_string (ep) == "166.78.151.147");
    }

    //--------------------------------------------------------------------------

    template <typename t>
    bool parse (char const* text, t& t)
    {
        std::string input (text);
        std::istringstream stream (input);
        stream >> t;
        return !stream.fail();
    }

    template <typename t>
    void shouldpass (char const* text)
    {
        t t;
        expect (parse (text, t));
        expect (to_string (t) == std::string (text));
    }

    template <typename t>
    void shouldfail (char const* text)
    {
        t t;
        unexpected (parse (text, t));
    }

    template <typename t>
    void testparse (char const* name)
    {
        testcase (name);

        shouldpass <t> ("0.0.0.0");
        shouldpass <t> ("192.168.0.1");
        shouldpass <t> ("168.127.149.132");
        shouldpass <t> ("168.127.149.132:80");
        shouldpass <t> ("168.127.149.132:54321");

        shouldfail <t> ("");
        shouldfail <t> ("255");
        shouldfail <t> ("512");
        shouldfail <t> ("1.2.3.256");
        shouldfail <t> ("1.2.3:80");
    }

    void run ()
    {
        testaddressv4 ();
        testaddressv4proxy();
        testaddress ();
        testendpoint ();

        testparse <endpoint> ("parse endpoint");
    }
};

beast_define_testsuite(ipendpoint,net,beast);

}
}
