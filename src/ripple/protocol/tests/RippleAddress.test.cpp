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
#include <ripple/protocol/rippleaddress.h>
#include <ripple/protocol/ripplepublickey.h>
#include <ripple/protocol/serializer.h>
#include <ripple/basics/stringutilities.h>
#include <beast/unit_test/suite.h>

namespace ripple {

class rippleaddress_test : public beast::unit_test::suite
{
public:
    void run()
    {
        // construct a seed.
        rippleaddress naseed;

        expect (naseed.setseedgeneric ("masterpassphrase"));
        expect (naseed.humanseed () == "snopbrxtmemymhuvtgbuqafg1sutb", naseed.humanseed ());

        // create node public/private key pair
        rippleaddress nanodepublic    = rippleaddress::createnodepublic (naseed);
        rippleaddress nanodeprivate   = rippleaddress::createnodeprivate (naseed);

        expect (nanodepublic.humannodepublic () == "n94a1u4jaz288pzltw6yfwvbi89yamic6jbxpvuj5zmexe5ftvg9", nanodepublic.humannodepublic ());
        expect (nanodeprivate.humannodeprivate () == "pnen77yeeud4ffkg7iycbwcwkptaefrkw2wfostaaty1dsupwxe", nanodeprivate.humannodeprivate ());

        // check node signing.
        blob vuctextsrc = strcopy ("hello, nurse!");
        uint256 uhash   = serializer::getsha512half (vuctextsrc);
        blob vuctextsig;

        nanodeprivate.signnodeprivate (uhash, vuctextsig);
        expect (nanodepublic.verifynodepublic (uhash, vuctextsig, ecdsa::strict), "verify failed.");

        // construct a public generator from the seed.
        rippleaddress   generator     = rippleaddress::creategeneratorpublic (naseed);

        expect (generator.humangenerator () == "fhujkrhsdzv2skjln9qbwm5aarmrxdpffshdcp6yfdzwcxdfz4mt", generator.humangenerator ());

        // create account #0 public/private key pair.
        rippleaddress   naaccountpublic0    = rippleaddress::createaccountpublic (generator, 0);
        rippleaddress   naaccountprivate0   = rippleaddress::createaccountprivate (generator, naseed, 0);

        expect (naaccountpublic0.humanaccountid () == "rhb9cjawyb4rj91vrwn96dkukg4bwdtyth", naaccountpublic0.humanaccountid ());
        expect (naaccountpublic0.humanaccountpublic () == "abqg8rqazjs1etkfeaqxr2gs4utcdiec9wmi7pfupti27vcahwgw", naaccountpublic0.humanaccountpublic ());

        // create account #1 public/private key pair.
        rippleaddress   naaccountpublic1    = rippleaddress::createaccountpublic (generator, 1);
        rippleaddress   naaccountprivate1   = rippleaddress::createaccountprivate (generator, naseed, 1);

        expect (naaccountpublic1.humanaccountid () == "r4byf7slumd7qgsllpgjx38wjsy12virjp", naaccountpublic1.humanaccountid ());
        expect (naaccountpublic1.humanaccountpublic () == "abpxptfuly1bhk3hngttaqnovpkwq23npfmnkaf6f1atg5vdyprw", naaccountpublic1.humanaccountpublic ());

        // check account signing.
        expect (naaccountprivate0.accountprivatesign (uhash, vuctextsig), "signing failed.");
        expect (naaccountpublic0.accountpublicverify (uhash, vuctextsig, ecdsa::strict), "verify failed.");
        expect (!naaccountpublic1.accountpublicverify (uhash, vuctextsig, ecdsa::not_strict), "anti-verify failed.");
        expect (!naaccountpublic1.accountpublicverify (uhash, vuctextsig, ecdsa::strict), "anti-verify failed.");

        expect (naaccountprivate1.accountprivatesign (uhash, vuctextsig), "signing failed.");
        expect (naaccountpublic1.accountpublicverify (uhash, vuctextsig, ecdsa::strict), "verify failed.");
        expect (!naaccountpublic0.accountpublicverify (uhash, vuctextsig, ecdsa::not_strict), "anti-verify failed.");
        expect (!naaccountpublic0.accountpublicverify (uhash, vuctextsig, ecdsa::strict), "anti-verify failed.");

        // check account encryption.
        blob vuctextcipher
            = naaccountprivate0.accountprivateencrypt (naaccountpublic1, vuctextsrc);
        blob vuctextrecovered
            = naaccountprivate1.accountprivatedecrypt (naaccountpublic0, vuctextcipher);

        expect (vuctextsrc == vuctextrecovered, "encrypt-decrypt failed.");

        {
            rippleaddress nseed;
            uint128 seed1, seed2;
            seed1.sethex ("71ed064155ffadfa38782c5e0158cb26");
            nseed.setseed (seed1);
            expect (nseed.humanseed() == "shhm53kpz87gwdqarm1bampexg8tn",
                "incorrect human seed");
            expect (nseed.humanseed1751() == "mad body ace mint okay hub what data sack flat dana math",
                "incorrect 1751 seed");
        }
    }
};

//------------------------------------------------------------------------------

class rippleidentifier_test : public beast::unit_test::suite
{
public:
    void run ()
    {
        testcase ("seed");
        rippleaddress seed;
        expect (seed.setseedgeneric ("masterpassphrase"));
        expect (seed.humanseed () == "snopbrxtmemymhuvtgbuqafg1sutb", seed.humanseed ());

        testcase ("ripplepublickey");
        rippleaddress deprecatedpublickey (rippleaddress::createnodepublic (seed));
        expect (deprecatedpublickey.humannodepublic () ==
            "n94a1u4jaz288pzltw6yfwvbi89yamic6jbxpvuj5zmexe5ftvg9",
                deprecatedpublickey.humannodepublic ());
        ripplepublickey publickey = deprecatedpublickey.topublickey();
        expect (publickey.to_string() == deprecatedpublickey.humannodepublic(),
            publickey.to_string());

        testcase ("generator");
        rippleaddress generator (rippleaddress::creategeneratorpublic (seed));
        expect (generator.humangenerator () ==
            "fhujkrhsdzv2skjln9qbwm5aarmrxdpffshdcp6yfdzwcxdfz4mt",
                generator.humangenerator ());
    }
};

beast_define_testsuite(rippleaddress,ripple_data,ripple);
beast_define_testsuite(rippleidentifier,ripple_data,ripple);

} // ripple
