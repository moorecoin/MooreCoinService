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
#include <ripple/protocol/sttx.h>
#include <ripple/protocol/stparsedjson.h>
#include <ripple/json/to_string.h>
#include <beast/unit_test/suite.h>

namespace ripple {

class sttx_test : public beast::unit_test::suite
{
public:
    void run()
    {
        rippleaddress seed;
        seed.setseedrandom ();
        rippleaddress generator = rippleaddress::creategeneratorpublic (seed);
        rippleaddress publicacct = rippleaddress::createaccountpublic (generator, 1);
        rippleaddress privateacct = rippleaddress::createaccountprivate (generator, seed, 1);

        sttx j (ttaccount_set);
        j.setsourceaccount (publicacct);
        j.setsigningpubkey (publicacct);
        j.setfieldvl (sfmessagekey, publicacct.getaccountpublic ());
        j.sign (privateacct);

        unexpected (!j.checksign (), "transaction fails signature test");

        serializer rawtxn;
        j.add (rawtxn);
        serializeriterator sit (rawtxn);
        sttx copy (sit);

        if (copy != j)
        {
            log << j.getjson (0);
            log << copy.getjson (0);
            fail ("transaction fails serialize/deserialize test");
        }
        else
        {
            pass ();
        }

        stparsedjsonobject parsed ("test", j.getjson (0));
        std::unique_ptr <stobject> new_obj (std::move (parsed.object));

        if (new_obj.get () == nullptr)
            fail ("unable to build object from json");

        if (stobject (j) != *new_obj)
        {
            log << "orig: " << j.getjson (0);
            log << "built " << new_obj->getjson (0);
            fail ("built a different transaction");
        }
        else
        {
            pass ();
        }
    }
};

beast_define_testsuite(sttx,ripple_app,ripple);

} // ripple
