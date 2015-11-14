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
#include <ripple/basics/log.h>
#include <ripple/protocol/stbase.h>
#include <ripple/protocol/staccount.h>
#include <ripple/protocol/starray.h>
#include <ripple/protocol/stobject.h>
#include <ripple/protocol/stparsedjson.h>
#include <ripple/json/json_reader.h>
#include <ripple/json/to_string.h>
#include <beast/unit_test/suite.h>
#include <beast/cxx14/memory.h> // <memory>

namespace ripple {

class serializedobject_test : public beast::unit_test::suite
{
public:
    void run()
    {
        testserialization();
        testparsejsonarray();
        testparsejsonarraywithinvalidchildrenobjects();
    }

    bool parsejsonstring (std::string const& json, json::value& to)
    {
        json::reader reader;
        return (reader.parse(json, to) &&
               !to.isnull() &&
                to.isobject());
    }

    void testparsejsonarraywithinvalidchildrenobjects ()
    {
        testcase ("parse json array invalid children");
        try
        {
            /*

            starray/stobject constructs don't really map perfectly to json
            arrays/objects.

            stobject is an associative container, mapping fields to value, but
            an stobject may also have a field as it's name, stored outside the
            associative structure. the name is important, so to maintain
            fidelity, it will take two json objects to represent them.

            */
            std::string faulty ("{\"template\":[{"
                                    "\"modifiednode\":{\"sequence\":1}, "
                                    "\"deletednode\":{\"sequence\":1}"
                                "}]}");

            std::unique_ptr<stobject> so;
            json::value faultyjson;
            bool parsedok (parsejsonstring(faulty, faultyjson));
            unexpected(!parsedok, "failed to parse");
            stparsedjsonobject parsed ("test", faultyjson);
            expect (parsed.object.get() == nullptr,
                "it should have thrown. "
                  "immediate children of starray encoded as json must "
                  "have one key only.");
        }
        catch(std::runtime_error& e)
        {
            std::string what(e.what());
            unexpected (what.find("first level children of `template`") != 0);
        }
    }

    void testparsejsonarray ()
    {
        testcase ("parse json array");
        std::string const json (
            "{\"template\":[{\"modifiednode\":{\"sequence\":1}}]}\n");

        json::value jsonobject;
        bool parsedok (parsejsonstring(json, jsonobject));
        if (parsedok)
        {
            stparsedjsonobject parsed ("test", jsonobject);
            std::string const& serialized (
                to_string (parsed.object->getjson(0)));
            expect (serialized == json, serialized + " should equal: " + json);
        }
        else
        {
            fail ("couldn't parse json: " + json);
        }
    }

    void testserialization ()
    {
        testcase ("serialization");

        unexpected (sfgeneric.isuseful (), "sfgeneric must not be useful");

        sfield const& sftestvl = sfield::getfield (sti_vl, 255);
        sfield const& sftesth256 = sfield::getfield (sti_hash256, 255);
        sfield const& sftestu32 = sfield::getfield (sti_uint32, 255);
        sfield const& sftestobject = sfield::getfield (sti_object, 255);

        sotemplate elements;
        elements.push_back (soelement (sfflags, soe_required));
        elements.push_back (soelement (sftestvl, soe_required));
        elements.push_back (soelement (sftesth256, soe_optional));
        elements.push_back (soelement (sftestu32, soe_required));

        stobject object1 (elements, sftestobject);
        stobject object2 (object1);

        unexpected (object1.getserializer () != object2.getserializer (),
            "stobject error 1");

        unexpected (object1.isfieldpresent (sftesth256) ||
            !object1.isfieldpresent (sftestvl), "stobject error");

        object1.makefieldpresent (sftesth256);

        unexpected (!object1.isfieldpresent (sftesth256), "stobject error 2");

        unexpected (object1.getfieldh256 (sftesth256) != uint256 (),
            "stobject error 3");

        if (object1.getserializer () == object2.getserializer ())
        {
            writelog (lsinfo, stobject) << "o1: " << object1.getjson (0);
            writelog (lsinfo, stobject) << "o2: " << object2.getjson (0);
            fail ("stobject error 4");
        }
        else
        {
            pass ();
        }

        object1.makefieldabsent (sftesth256);

        unexpected (object1.isfieldpresent (sftesth256), "stobject error 5");

        unexpected (object1.getflags () != 0, "stobject error 6");

        unexpected (object1.getserializer () != object2.getserializer (),
            "stobject error 7");

        stobject copy (object1);

        unexpected (object1.isfieldpresent (sftesth256), "stobject error 8");

        unexpected (copy.isfieldpresent (sftesth256), "stobject error 9");

        unexpected (object1.getserializer () != copy.getserializer (),
            "stobject error 10");

        copy.setfieldu32 (sftestu32, 1);

        unexpected (object1.getserializer () == copy.getserializer (),
            "stobject error 11");

        for (int i = 0; i < 1000; i++)
        {
            blob j (i, 2);

            object1.setfieldvl (sftestvl, j);

            serializer s;
            object1.add (s);
            serializeriterator it (s);

            stobject object3 (elements, it, sftestobject);

            unexpected (object1.getfieldvl (sftestvl) != j, "stobject error");

            unexpected (object3.getfieldvl (sftestvl) != j, "stobject error");
        }
    }
};

beast_define_testsuite(serializedobject,ripple_data,ripple);

} // ripple
