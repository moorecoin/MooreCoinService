// protocol buffers - google's data interchange format
// copyright 2008 google inc.  all rights reserved.
// http://code.google.com/p/protobuf/
//
// redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * neither the name of google inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// this software is provided by the copyright holders and contributors
// "as is" and any express or implied warranties, including, but not
// limited to, the implied warranties of merchantability and fitness for
// a particular purpose are disclaimed. in no event shall the copyright
// owner or contributors be liable for any direct, indirect, incidental,
// special, exemplary, or consequential damages (including, but not
// limited to, procurement of substitute goods or services; loss of use,
// data, or profits; or business interruption) however caused and on any
// theory of liability, whether in contract, strict liability, or tort
// (including negligence or otherwise) arising in any way out of the use
// of this software, even if advised of the possibility of such damage.

package com.google.protobuf;

import protobuf_unittest.unittestproto;
import protobuf_unittest.unittestproto.testallextensions;
import protobuf_unittest.unittestproto.testalltypes;
import protobuf_unittest.unittestproto.testemptymessage;
import protobuf_unittest.unittestproto.testemptymessagewithextensions;

import junit.framework.testcase;

import java.util.arrays;
import java.util.map;

/**
 * tests related to unknown field handling.
 *
 * @author kenton@google.com (kenton varda)
 */
public class unknownfieldsettest extends testcase {
  public void setup() throws exception {
    descriptor = testalltypes.getdescriptor();
    allfields = testutil.getallset();
    allfieldsdata = allfields.tobytestring();
    emptymessage = testemptymessage.parsefrom(allfieldsdata);
    unknownfields = emptymessage.getunknownfields();
  }

  unknownfieldset.field getfield(string name) {
    descriptors.fielddescriptor field = descriptor.findfieldbyname(name);
    assertnotnull(field);
    return unknownfields.getfield(field.getnumber());
  }

  // constructs a protocol buffer which contains fields with all the same
  // numbers as allfieldsdata except that each field is some other wire
  // type.
  bytestring getbizarrodata() throws exception {
    unknownfieldset.builder bizarrofields = unknownfieldset.newbuilder();

    unknownfieldset.field varintfield =
      unknownfieldset.field.newbuilder().addvarint(1).build();
    unknownfieldset.field fixed32field =
      unknownfieldset.field.newbuilder().addfixed32(1).build();

    for (map.entry<integer, unknownfieldset.field> entry :
         unknownfields.asmap().entryset()) {
      if (entry.getvalue().getvarintlist().isempty()) {
        // original field is not a varint, so use a varint.
        bizarrofields.addfield(entry.getkey(), varintfield);
      } else {
        // original field *is* a varint, so use something else.
        bizarrofields.addfield(entry.getkey(), fixed32field);
      }
    }

    return bizarrofields.build().tobytestring();
  }

  descriptors.descriptor descriptor;
  testalltypes allfields;
  bytestring allfieldsdata;

  // an empty message that has been parsed from allfieldsdata.  so, it has
  // unknown fields of every type.
  testemptymessage emptymessage;
  unknownfieldset unknownfields;

  // =================================================================

  public void testvarint() throws exception {
    unknownfieldset.field field = getfield("optional_int32");
    assertequals(1, field.getvarintlist().size());
    assertequals(allfields.getoptionalint32(),
                 (long) field.getvarintlist().get(0));
  }

  public void testfixed32() throws exception {
    unknownfieldset.field field = getfield("optional_fixed32");
    assertequals(1, field.getfixed32list().size());
    assertequals(allfields.getoptionalfixed32(),
                 (int) field.getfixed32list().get(0));
  }

  public void testfixed64() throws exception {
    unknownfieldset.field field = getfield("optional_fixed64");
    assertequals(1, field.getfixed64list().size());
    assertequals(allfields.getoptionalfixed64(),
                 (long) field.getfixed64list().get(0));
  }

  public void testlengthdelimited() throws exception {
    unknownfieldset.field field = getfield("optional_bytes");
    assertequals(1, field.getlengthdelimitedlist().size());
    assertequals(allfields.getoptionalbytes(),
                 field.getlengthdelimitedlist().get(0));
  }

  public void testgroup() throws exception {
    descriptors.fielddescriptor nestedfielddescriptor =
      testalltypes.optionalgroup.getdescriptor().findfieldbyname("a");
    assertnotnull(nestedfielddescriptor);

    unknownfieldset.field field = getfield("optionalgroup");
    assertequals(1, field.getgrouplist().size());

    unknownfieldset group = field.getgrouplist().get(0);
    assertequals(1, group.asmap().size());
    asserttrue(group.hasfield(nestedfielddescriptor.getnumber()));

    unknownfieldset.field nestedfield =
      group.getfield(nestedfielddescriptor.getnumber());
    assertequals(1, nestedfield.getvarintlist().size());
    assertequals(allfields.getoptionalgroup().geta(),
                 (long) nestedfield.getvarintlist().get(0));
  }

  public void testserialize() throws exception {
    // check that serializing the unknownfieldset produces the original data
    // again.
    bytestring data = emptymessage.tobytestring();
    assertequals(allfieldsdata, data);
  }

  public void testcopyfrom() throws exception {
    testemptymessage message =
      testemptymessage.newbuilder().mergefrom(emptymessage).build();

    assertequals(emptymessage.tostring(), message.tostring());
  }

  public void testmergefrom() throws exception {
    testemptymessage source =
      testemptymessage.newbuilder()
        .setunknownfields(
          unknownfieldset.newbuilder()
            .addfield(2,
              unknownfieldset.field.newbuilder()
                .addvarint(2).build())
            .addfield(3,
              unknownfieldset.field.newbuilder()
                .addvarint(4).build())
            .build())
        .build();
    testemptymessage destination =
      testemptymessage.newbuilder()
        .setunknownfields(
          unknownfieldset.newbuilder()
            .addfield(1,
              unknownfieldset.field.newbuilder()
                .addvarint(1).build())
            .addfield(3,
              unknownfieldset.field.newbuilder()
                .addvarint(3).build())
            .build())
        .mergefrom(source)
        .build();

    assertequals(
      "1: 1\n" +
      "2: 2\n" +
      "3: 3\n" +
      "3: 4\n",
      destination.tostring());
  }

  public void testclear() throws exception {
    unknownfieldset fields =
      unknownfieldset.newbuilder().mergefrom(unknownfields).clear().build();
    asserttrue(fields.asmap().isempty());
  }

  public void testclearmessage() throws exception {
    testemptymessage message =
      testemptymessage.newbuilder().mergefrom(emptymessage).clear().build();
    assertequals(0, message.getserializedsize());
  }

  public void testparseknownandunknown() throws exception {
    // test mixing known and unknown fields when parsing.

    unknownfieldset fields =
      unknownfieldset.newbuilder(unknownfields)
        .addfield(123456,
          unknownfieldset.field.newbuilder().addvarint(654321).build())
        .build();

    bytestring data = fields.tobytestring();
    testalltypes destination = testalltypes.parsefrom(data);

    testutil.assertallfieldsset(destination);
    assertequals(1, destination.getunknownfields().asmap().size());

    unknownfieldset.field field =
      destination.getunknownfields().getfield(123456);
    assertequals(1, field.getvarintlist().size());
    assertequals(654321, (long) field.getvarintlist().get(0));
  }

  public void testwrongtypetreatedasunknown() throws exception {
    // test that fields of the wrong wire type are treated like unknown fields
    // when parsing.

    bytestring bizarrodata = getbizarrodata();
    testalltypes alltypesmessage = testalltypes.parsefrom(bizarrodata);
    testemptymessage emptymessage = testemptymessage.parsefrom(bizarrodata);

    // all fields should have been interpreted as unknown, so the debug strings
    // should be the same.
    assertequals(emptymessage.tostring(), alltypesmessage.tostring());
  }

  public void testunknownextensions() throws exception {
    // make sure fields are properly parsed to the unknownfieldset even when
    // they are declared as extension numbers.

    testemptymessagewithextensions message =
      testemptymessagewithextensions.parsefrom(allfieldsdata);

    assertequals(unknownfields.asmap().size(),
                 message.getunknownfields().asmap().size());
    assertequals(allfieldsdata, message.tobytestring());
  }

  public void testwrongextensiontypetreatedasunknown() throws exception {
    // test that fields of the wrong wire type are treated like unknown fields
    // when parsing extensions.

    bytestring bizarrodata = getbizarrodata();
    testallextensions allextensionsmessage =
      testallextensions.parsefrom(bizarrodata);
    testemptymessage emptymessage = testemptymessage.parsefrom(bizarrodata);

    // all fields should have been interpreted as unknown, so the debug strings
    // should be the same.
    assertequals(emptymessage.tostring(),
                 allextensionsmessage.tostring());
  }

  public void testparseunknownenumvalue() throws exception {
    descriptors.fielddescriptor singularfield =
      testalltypes.getdescriptor().findfieldbyname("optional_nested_enum");
    descriptors.fielddescriptor repeatedfield =
      testalltypes.getdescriptor().findfieldbyname("repeated_nested_enum");
    assertnotnull(singularfield);
    assertnotnull(repeatedfield);

    bytestring data =
      unknownfieldset.newbuilder()
        .addfield(singularfield.getnumber(),
          unknownfieldset.field.newbuilder()
            .addvarint(testalltypes.nestedenum.bar.getnumber())
            .addvarint(5)   // not valid
            .build())
        .addfield(repeatedfield.getnumber(),
          unknownfieldset.field.newbuilder()
            .addvarint(testalltypes.nestedenum.foo.getnumber())
            .addvarint(4)   // not valid
            .addvarint(testalltypes.nestedenum.baz.getnumber())
            .addvarint(6)   // not valid
            .build())
        .build()
        .tobytestring();

    {
      testalltypes message = testalltypes.parsefrom(data);
      assertequals(testalltypes.nestedenum.bar,
                   message.getoptionalnestedenum());
      assertequals(
        arrays.aslist(testalltypes.nestedenum.foo, testalltypes.nestedenum.baz),
        message.getrepeatednestedenumlist());
      assertequals(arrays.aslist(5l),
                   message.getunknownfields()
                          .getfield(singularfield.getnumber())
                          .getvarintlist());
      assertequals(arrays.aslist(4l, 6l),
                   message.getunknownfields()
                          .getfield(repeatedfield.getnumber())
                          .getvarintlist());
    }

    {
      testallextensions message =
        testallextensions.parsefrom(data, testutil.getextensionregistry());
      assertequals(testalltypes.nestedenum.bar,
        message.getextension(unittestproto.optionalnestedenumextension));
      assertequals(
        arrays.aslist(testalltypes.nestedenum.foo, testalltypes.nestedenum.baz),
        message.getextension(unittestproto.repeatednestedenumextension));
      assertequals(arrays.aslist(5l),
                   message.getunknownfields()
                          .getfield(singularfield.getnumber())
                          .getvarintlist());
      assertequals(arrays.aslist(4l, 6l),
                   message.getunknownfields()
                          .getfield(repeatedfield.getnumber())
                          .getvarintlist());
    }
  }

  public void testlargevarint() throws exception {
    bytestring data =
      unknownfieldset.newbuilder()
        .addfield(1,
          unknownfieldset.field.newbuilder()
            .addvarint(0x7fffffffffffffffl)
            .build())
        .build()
        .tobytestring();
    unknownfieldset parsed = unknownfieldset.parsefrom(data);
    unknownfieldset.field field = parsed.getfield(1);
    assertequals(1, field.getvarintlist().size());
    assertequals(0x7fffffffffffffffl, (long)field.getvarintlist().get(0));
  }

  public void testequalsandhashcode() {
    unknownfieldset.field fixed32field =
        unknownfieldset.field.newbuilder()
            .addfixed32(1)
            .build();
    unknownfieldset.field fixed64field =
        unknownfieldset.field.newbuilder()
            .addfixed64(1)
            .build();
    unknownfieldset.field varintfield =
        unknownfieldset.field.newbuilder()
            .addvarint(1)
            .build();
    unknownfieldset.field lengthdelimitedfield =
        unknownfieldset.field.newbuilder()
            .addlengthdelimited(bytestring.empty)
            .build();
    unknownfieldset.field groupfield =
        unknownfieldset.field.newbuilder()
            .addgroup(unknownfields)
            .build();

    unknownfieldset a =
        unknownfieldset.newbuilder()
            .addfield(1, fixed32field)
            .build();
    unknownfieldset b =
        unknownfieldset.newbuilder()
            .addfield(1, fixed64field)
            .build();
    unknownfieldset c =
        unknownfieldset.newbuilder()
            .addfield(1, varintfield)
            .build();
    unknownfieldset d =
        unknownfieldset.newbuilder()
            .addfield(1, lengthdelimitedfield)
            .build();
    unknownfieldset e =
        unknownfieldset.newbuilder()
            .addfield(1, groupfield)
            .build();

    checkequalsisconsistent(a);
    checkequalsisconsistent(b);
    checkequalsisconsistent(c);
    checkequalsisconsistent(d);
    checkequalsisconsistent(e);

    checknotequal(a, b);
    checknotequal(a, c);
    checknotequal(a, d);
    checknotequal(a, e);
    checknotequal(b, c);
    checknotequal(b, d);
    checknotequal(b, e);
    checknotequal(c, d);
    checknotequal(c, e);
    checknotequal(d, e);
  }

  /**
   * asserts that the given field sets are not equal and have different
   * hash codes.
   *
   * @warning it's valid for non-equal objects to have the same hash code, so
   *   this test is stricter than it needs to be. however, this should happen
   *   relatively rarely.
   */
  private void checknotequal(unknownfieldset s1, unknownfieldset s2) {
    string equalserror = string.format("%s should not be equal to %s", s1, s2);
    assertfalse(equalserror, s1.equals(s2));
    assertfalse(equalserror, s2.equals(s1));

    assertfalse(
        string.format("%s should have a different hash code from %s", s1, s2),
        s1.hashcode() == s2.hashcode());
  }

  /**
   * asserts that the given field sets are equal and have identical hash codes.
   */
  private void checkequalsisconsistent(unknownfieldset set) {
    // object should be equal to itself.
    assertequals(set, set);

    // object should be equal to a copy of itself.
    unknownfieldset copy = unknownfieldset.newbuilder(set).build();
    assertequals(set, copy);
    assertequals(copy, set);
    assertequals(set.hashcode(), copy.hashcode());
  }
}
