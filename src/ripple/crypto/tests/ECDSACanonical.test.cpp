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
#include <ripple/crypto/ecdsacanonical.h>
#include <beast/unit_test/suite.h>
#include <algorithm>
#include <iterator>
#include <openssl/bn.h>
#include <openssl/ecdsa.h>

namespace ripple {

class ecdsacanonical_test : public beast::unit_test::suite
{
public:
    template <class fwditer, class container>
    static
    void
    hex_to_binary (fwditer first, fwditer last, container& out)
    {
        struct table
        {
            int val[256];
            table ()
            {
                std::fill (val, val+256, 0);
                for (int i = 0; i < 10; ++i)
                    val ['0'+i] = i;
                for (int i = 0; i < 6; ++i)
                {
                    val ['a'+i] = 10 + i;
                    val ['a'+i] = 10 + i;
                }
            }
            int operator[] (int i)
            {
               return val[i];
            }
        };

        static table lut;
        out.reserve (std::distance (first, last) / 2);
        while (first != last)
        {
            auto const hi (lut[(*first++)]);
            auto const lo (lut[(*first++)]);
            out.push_back ((hi*16)+lo);
        }
    }
    blob loadsignature (std::string const& hex)
    {
        blob b;
        hex_to_binary (hex.begin (), hex.end (), b);
        return b;
    }

    // verifies that a signature is syntactically valid.
    bool isvalid (std::string const& hex)
    {
        blob j (loadsignature(hex));
        return iscanonicalecdsasig (&j[0], j.size(), ecdsa::not_strict);
    }

    // verifies that a signature is syntactically valid and in canonical form.
    bool isstrictlycanonical (std::string const& hex)
    {
        blob j (loadsignature(hex));
        return iscanonicalecdsasig (&j[0], j.size (), ecdsa::strict);
    }

    // verify that we correctly identify strictly canonical signatures
    void teststrictlycanonicalsignatures ()
    {
        testcase ("strictly canonical signature checks", abort_on_fail);

        expect (isstrictlycanonical("3045"
            "022100ff478110d1d4294471ec76e0157540c2181f47debd25d7f9e7ddcccd47eee905"
            "0220078f07cdae6c240855d084ad91d1479609533c147c93b0aef19bc9724d003f28"),
            "strictly canonical signature");

        expect (isstrictlycanonical("3045"
            "0221009218248292f1762d8a51be80f8a7f2cd288d810ce781d5955700da1684df1d2d"
            "022041a1ee1746bfd72c9760cc93a7aaa8047d52c8833a03a20eaae92ea19717b454"),
            "strictly canonical signature");

        expect (isstrictlycanonical("3044"
            "02206a9e43775f73b6d1ec420e4ddd222a80d4c6df5d1beecc431a91b63c928b7581"
            "022023e9cc2d61dda6f73eaa6bcb12688beb0f434769276b3127e4044ed895c9d96b"),
            "strictly canonical signature");

         expect (isstrictlycanonical("3044"
            "022056e720007221f3cd4efbb6352741d8e5a0968d48d8d032c2fbc4f6304ad1d04e"
            "02201f39eb392c20d7801c3e8d81d487e742fa84a1665e923225bd6323847c71879f"),
            "strictly canonical signature");

        expect (isstrictlycanonical("3045"
            "022100fdfd5ad05518cea0017a2dcb5c4df61e7c73b6d3a38e7ae93210a1564e8c2f12"
            "0220214ff061ccc123c81d0bb9d0edea04cd40d96bf1425d311da62a7096bb18ea18"),
            "strictly canonical signature");

        // these are canonical signatures, but *not* strictly canonical.
        expect (!isstrictlycanonical ("3046"
            "022100f477b3fa6f31c7cb3a0d1ad94a231fdd24b8d78862ee334cea7cd08f6cbc0a1b"
            "022100928e6bcf1ed2684679730c5414aec48fd62282b090041c41453c1d064af597a1"),
            "not strictly canonical signature");

        expect (!isstrictlycanonical ("3045"
            "022063e7c7ca93cb2400e413a342c027d00665f8bab9c22ef0a7b8ae3aaf092230b6"
            "0221008f2e8bb7d09521abbc277717b14b93170ae6465c5a1b36561099319c4beb254c"),
            "not strictly canonical signature");

        expect (!isstrictlycanonical ("3046"
            "02210099dca1188663ddea506a06a7b20c2b7d8c26aff41dece69d6c5f7c967d32625f"
            "022100897658a6b1f9eee5d140d7a332da0bd73bb98974ea53f6201b01c1b594f286ea"),
            "not strictly canonical signature");

        expect (!isstrictlycanonical ("3045"
            "02200855de366e4e323aa2ce2a25674401a7d11f72ec432770d07f7b57df7387aec0"
            "022100da4c6addea14888858de2ac5b91ed9050d6972bb388def582628cee32869ae35"),
            "not strictly canonical signature");
    }

    // verify that we correctly identify valid signatures
    void testvalidsignatures ()
    {
        testcase ("canonical signature checks", abort_on_fail);

        // r and s 1 byte 1
        expect (isvalid ("3006"
            "020101"
            "020102"),
            "well-formed short signature");

        expect (isvalid ("3044"
            "02203932c892e2e550f3af8ee4ce9c215a87f9bb831dcac87b2838e2c2eaa891df0c"
            "022030b61dd36543125d56b9f9f3a1f53189e5af33cdda8d77a5209aec03978fa001"),
            "canonical signature");

        expect (isvalid ("3045"
            "0220076045be6f9eca28ff1ec606b833d0b87e70b2a630f5e3a496b110967a40f90a"
            "0221008fffd599910eefe00bc803c688eca1d2ba7f6b180620eaa03488e6585db6ba01"),
            "canonical signature");

        expect (isvalid("3046"
            "022100876045be6f9eca28ff1ec606b833d0b87e70b2a630f5e3a496b110967a40f90a"
            "0221008fffd599910eefe00bc803c688c2eca1d2ba7f6b180620eaa03488e6585db6ba"),
            "canonical signature");

        expect (isstrictlycanonical("3045"
            "022100ff478110d1d4294471ec76e0157540c2181f47debd25d7f9e7ddcccd47eee905"
            "0220078f07cdae6c240855d084ad91d1479609533c147c93b0aef19bc9724d003f28"),
            "canonical signature");

        expect (isstrictlycanonical("3045"
            "0221009218248292f1762d8a51be80f8a7f2cd288d810ce781d5955700da1684df1d2d"
            "022041a1ee1746bfd72c9760cc93a7aaa8047d52c8833a03a20eaae92ea19717b454"),
            "canonical signature");

        expect (isstrictlycanonical("3044"
            "02206a9e43775f73b6d1ec420e4ddd222a80d4c6df5d1beecc431a91b63c928b7581"
            "022023e9cc2d61dda6f73eaa6bcb12688beb0f434769276b3127e4044ed895c9d96b"),
            "canonical signature");

         expect (isstrictlycanonical("3044"
            "022056e720007221f3cd4efbb6352741d8e5a0968d48d8d032c2fbc4f6304ad1d04e"
            "02201f39eb392c20d7801c3e8d81d487e742fa84a1665e923225bd6323847c71879f"),
            "canonical signature");

        expect (isstrictlycanonical("3045"
            "022100fdfd5ad05518cea0017a2dcb5c4df61e7c73b6d3a38e7ae93210a1564e8c2f12"
            "0220214ff061ccc123c81d0bb9d0edea04cd40d96bf1425d311da62a7096bb18ea18"),
            "canonical signature");
    }

    // verify that we correctly identify malformed or invalid signatures
    void testmalformedsignatures ()
    {
        testcase ("non-canonical signature checks", abort_on_fail);

        expect (!isvalid("3005"
            "0201ff"
            "0200"),
            "tooshort");

        expect (!isvalid ("3006"
            "020101"
            "020202"),
            "slen-overlong");

        expect (!isvalid ("3006"
            "020701"
            "020102"),
            "rlen-overlong-oob");

        expect (!isvalid ("3006"
            "020401"
            "020102"),
            "rlen-overlong-oob");

        expect (!isvalid ("3006"
            "020501"
            "020102"),
            "rlen-overlong-oob");

        expect (!isvalid ("3006"
            "020201"
            "020102"),
            "rlen-overlong");

        expect (!isvalid ("3006"
            "020301"
            "020202"),
            "rlen overlong and slen-overlong");

        expect (!isvalid ("3006"
            "020401"
            "020202"),
            "rlen overlong and oob and slen-overlong");

        expect (!isvalid("3047"
            "0221005990e0584b2b238e1dfaad8d6ed69ecc1a4a13ac85fc0b31d0df395eb1ba6105"
            "022200002d5876262c288beb511d061691bf26777344b702b00f8fe28621fe4e566695ed"),
            "toolong");

        expect (!isvalid("3144"
            "02205990e0584b2b238e1dfaad8d6ed69ecc1a4a13ac85fc0b31d0df395eb1ba6105"
            "02202d5876262c288beb511d061691bf26777344b702b00f8fe28621fe4e566695ed"),
            "type");

        expect (!isvalid("3045"
            "02205990e0584b2b238e1dfaad8d6ed69ecc1a4a13ac85fc0b31d0df395eb1ba6105"
            "02202d5876262c288beb511d061691bf26777344b702b00f8fe28621fe4e566695ed"),
            "totallength");

        expect (!isvalid("301f"
            "01205990e0584b2b238e1dfaad8d6ed69ecc1a4a13ac85fc0b31d0df395eb1"),
            "slenoob");

        expect (!isvalid("3045"
            "02205990e0584b2b238e1dfaad8d6ed69ecc1a4a13ac85fc0b31d0df395eb1ba6105"
            "02202d5876262c288beb511d061691bf26777344b702b00f8fe28621fe4e566695ed00"),
            "r+s");

        expect (!isvalid("3044"
            "01205990e0584b2b238e1dfaad8d6ed69ecc1a4a13ac85fc0b31d0df395eb1ba6105"
            "02202d5876262c288beb511d061691bf26777344b702b00f8fe28621fe4e566695ed"),
            "rtype");

        expect (!isvalid("3024"
            "0200"
            "02202d5876262c288beb511d061691bf26777344b702b00f8fe28621fe4e566695ed"),
            "rlen=0");

        expect (!isvalid("3044"
            "02208990e0584b2b238e1dfaad8d6ed69ecc1a4a13ac85fc0b31d0df395eb1ba6105"
            "02202d5876262c288beb511d061691bf26777344b702b00f8fe28621fe4e566695ed"),
            "r<0");

        expect (!isvalid("3045"
            "0221005990e0584b2b238e1dfaad8d6ed69ecc1a4a13ac85fc0b31d0df395eb1ba6105"
            "02202d5876262c288beb511d061691bf26777344b702b00f8fe28621fe4e566695ed"),
            "rpadded");

        expect (!isvalid("3044"
            "02205990e0584b2b238e1dfaad8d6ed69ecc1a4a13ac85fc0b31d0df395eb1ba6105012"
            "02d5876262c288beb511d061691bf26777344b702b00f8fe28621fe4e566695ed"),
            "stype");

        expect (!isvalid("3024"
            "02205990e0584b2b238e1dfaad8d6ed69ecc1a4a13ac85fc0b31d0df395eb1ba6105"
            "0200"),
            "slen=0");

        expect (!isvalid("3044"
            "02205990e0584b2b238e1dfaad8d6ed69ecc1a4a13ac85fc0b31d0df395eb1ba6105"
            "0220fd5876262c288beb511d061691bf26777344b702b00f8fe28621fe4e566695ed"),
            "s<0");

        expect (!isvalid("3045"
            "02205990e0584b2b238e1dfaad8d6ed69ecc1a4a13ac85fc0b31d0df395eb1ba6105"
            "0221002d5876262c288beb511d061691bf26777344b702b00f8fe28621fe4e566695ed"),
            "spadded");
    }

    void convertnoncanonical(std::string const& hex, std::string const& canonhex)
    {
        blob b (loadsignature(hex));

        // the signature ought to at least be valid before we begin.
        expect (isvalid (hex), "invalid signature");

        size_t len = b.size ();

        expect (!makecanonicalecdsasig (&b[0], len),
            "non-canonical signature was already canonical");

        expect (b.size () >= len,
            "canonicalized signature length longer than non-canonical");

        b.resize (len);

        expect (iscanonicalecdsasig (&b[0], len, ecdsa::strict),
            "canonicalized signature is not strictly canonical");

        blob canonicalform (loadsignature (canonhex));

        expect (b.size () == canonicalform.size (),
            "canonicalized signature doesn't have the expected length");

        expect (std::equal (b.begin (), b.end (), canonicalform.begin ()),
            "canonicalized signature isn't what we expected");
    }

    // verifies correctness of non-canonical to canonical conversion
    void testcanonicalconversions()
    {
        testcase ("non-canonical signature canonicalization", abort_on_fail);

        convertnoncanonical (
            "3046"
                "022100f477b3fa6f31c7cb3a0d1ad94a231fdd24b8d78862ee334cea7cd08f6cbc0a1b"
                "022100928e6bcf1ed2684679730c5414aec48fd62282b090041c41453c1d064af597a1",
            "3045"
                "022100f477b3fa6f31c7cb3a0d1ad94a231fdd24b8d78862ee334cea7cd08f6cbc0a1b"
                "02206d719430e12d97b9868cf3abeb513b6ee48c5a361f4483fa7a9641868540a9a0");

        convertnoncanonical (
            "3045"
                "022063e7c7ca93cb2400e413a342c027d00665f8bab9c22ef0a7b8ae3aaf092230b6"
                "0221008f2e8bb7d09521abbc277717b14b93170ae6465c5a1b36561099319c4beb254c",
            "3044"
                "022063e7c7ca93cb2400e413a342c027d00665f8bab9c22ef0a7b8ae3aaf092230b6"
                "022070d174482f6ade5443d888e84eb46ce7afc8968a552d69e5af392cf0844b1bf5");

        convertnoncanonical (
            "3046"
                "02210099dca1188663ddea506a06a7b20c2b7d8c26aff41dece69d6c5f7c967d32625f"
                "022100897658a6b1f9eee5d140d7a332da0bd73bb98974ea53f6201b01c1b594f286ea",
            "3045"
                "02210099dca1188663ddea506a06a7b20c2b7d8c26aff41dece69d6c5f7c967d32625f"
                "02207689a7594e06111a2ebf285ccd25f4277ef55371c4f4aa1ba4d09cd73b43ba57");

        convertnoncanonical (
            "3045"
                "02200855de366e4e323aa2ce2a25674401a7d11f72ec432770d07f7b57df7387aec0"
                "022100da4c6addea14888858de2ac5b91ed9050d6972bb388def582628cee32869ae35",
            "3044"
                "02200855de366e4e323aa2ce2a25674401a7d11f72ec432770d07f7b57df7387aec0"
                "022025b3952215eb7777a721d53a46e126f9ad456a2b76bab0e399a98fa9a7cc930c");
    }

    void run ()
    {
        testvalidsignatures ();
        teststrictlycanonicalsignatures ();
        testmalformedsignatures ();
        testcanonicalconversions ();
    }
};

beast_define_testsuite(ecdsacanonical,sslutil,ripple);

}
