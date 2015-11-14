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

import com.google.protobuf.descriptorprotos.descriptorproto;
import com.google.protobuf.descriptorprotos.enumdescriptorproto;
import com.google.protobuf.descriptorprotos.enumvaluedescriptorproto;
import com.google.protobuf.descriptorprotos.fielddescriptorproto;
import com.google.protobuf.descriptorprotos.filedescriptorproto;
import com.google.protobuf.descriptors.descriptorvalidationexception;
import com.google.protobuf.descriptors.filedescriptor;
import com.google.protobuf.descriptors.descriptor;
import com.google.protobuf.descriptors.fielddescriptor;
import com.google.protobuf.descriptors.enumdescriptor;
import com.google.protobuf.descriptors.enumvaluedescriptor;
import com.google.protobuf.descriptors.servicedescriptor;
import com.google.protobuf.descriptors.methoddescriptor;

import com.google.protobuf.test.unittestimport;
import com.google.protobuf.test.unittestimport.importenum;
import com.google.protobuf.test.unittestimport.importmessage;
import protobuf_unittest.unittestproto;
import protobuf_unittest.unittestproto.foreignenum;
import protobuf_unittest.unittestproto.foreignmessage;
import protobuf_unittest.unittestproto.testalltypes;
import protobuf_unittest.unittestproto.testallextensions;
import protobuf_unittest.unittestproto.testextremedefaultvalues;
import protobuf_unittest.unittestproto.testrequired;
import protobuf_unittest.unittestproto.testservice;
import protobuf_unittest.unittestcustomoptions;


import junit.framework.testcase;

import java.util.arrays;
import java.util.collections;
import java.util.list;

/**
 * unit test for {@link descriptors}.
 *
 * @author kenton@google.com kenton varda
 */
public class descriptorstest extends testcase {

  // regression test for bug where referencing a fielddescriptor.type value
  // before a fielddescriptorproto.type value would yield a
  // exceptionininitializererror.
  @suppresswarnings("unused")
  private static final object static_init_test = fielddescriptor.type.bool;

  public void testfieldtypeenummapping() throws exception {
    assertequals(fielddescriptor.type.values().length,
        fielddescriptorproto.type.values().length);
    for (fielddescriptor.type type : fielddescriptor.type.values()) {
      fielddescriptorproto.type prototype = type.toproto();
      assertequals("type_" + type.name(), prototype.name());
      assertequals(type, fielddescriptor.type.valueof(prototype));
    }
  }

  public void testfiledescriptor() throws exception {
    filedescriptor file = unittestproto.getdescriptor();

    assertequals("google/protobuf/unittest.proto", file.getname());
    assertequals("protobuf_unittest", file.getpackage());

    assertequals("unittestproto", file.getoptions().getjavaouterclassname());
    assertequals("google/protobuf/unittest.proto",
                 file.toproto().getname());

    assertequals(arrays.aslist(unittestimport.getdescriptor()),
                 file.getdependencies());

    descriptor messagetype = testalltypes.getdescriptor();
    assertequals(messagetype, file.getmessagetypes().get(0));
    assertequals(messagetype, file.findmessagetypebyname("testalltypes"));
    assertnull(file.findmessagetypebyname("nosuchtype"));
    assertnull(file.findmessagetypebyname("protobuf_unittest.testalltypes"));
    for (int i = 0; i < file.getmessagetypes().size(); i++) {
      assertequals(i, file.getmessagetypes().get(i).getindex());
    }

    enumdescriptor enumtype = foreignenum.getdescriptor();
    assertequals(enumtype, file.getenumtypes().get(0));
    assertequals(enumtype, file.findenumtypebyname("foreignenum"));
    assertnull(file.findenumtypebyname("nosuchtype"));
    assertnull(file.findenumtypebyname("protobuf_unittest.foreignenum"));
    assertequals(arrays.aslist(importenum.getdescriptor()),
                 unittestimport.getdescriptor().getenumtypes());
    for (int i = 0; i < file.getenumtypes().size(); i++) {
      assertequals(i, file.getenumtypes().get(i).getindex());
    }

    servicedescriptor service = testservice.getdescriptor();
    assertequals(service, file.getservices().get(0));
    assertequals(service, file.findservicebyname("testservice"));
    assertnull(file.findservicebyname("nosuchtype"));
    assertnull(file.findservicebyname("protobuf_unittest.testservice"));
    assertequals(collections.emptylist(),
                 unittestimport.getdescriptor().getservices());
    for (int i = 0; i < file.getservices().size(); i++) {
      assertequals(i, file.getservices().get(i).getindex());
    }

    fielddescriptor extension =
      unittestproto.optionalint32extension.getdescriptor();
    assertequals(extension, file.getextensions().get(0));
    assertequals(extension,
                 file.findextensionbyname("optional_int32_extension"));
    assertnull(file.findextensionbyname("no_such_ext"));
    assertnull(file.findextensionbyname(
      "protobuf_unittest.optional_int32_extension"));
    assertequals(collections.emptylist(),
                 unittestimport.getdescriptor().getextensions());
    for (int i = 0; i < file.getextensions().size(); i++) {
      assertequals(i, file.getextensions().get(i).getindex());
    }
  }

  public void testdescriptor() throws exception {
    descriptor messagetype = testalltypes.getdescriptor();
    descriptor nestedtype = testalltypes.nestedmessage.getdescriptor();

    assertequals("testalltypes", messagetype.getname());
    assertequals("protobuf_unittest.testalltypes", messagetype.getfullname());
    assertequals(unittestproto.getdescriptor(), messagetype.getfile());
    assertnull(messagetype.getcontainingtype());
    assertequals(descriptorprotos.messageoptions.getdefaultinstance(),
                 messagetype.getoptions());
    assertequals("testalltypes", messagetype.toproto().getname());

    assertequals("nestedmessage", nestedtype.getname());
    assertequals("protobuf_unittest.testalltypes.nestedmessage",
                 nestedtype.getfullname());
    assertequals(unittestproto.getdescriptor(), nestedtype.getfile());
    assertequals(messagetype, nestedtype.getcontainingtype());

    fielddescriptor field = messagetype.getfields().get(0);
    assertequals("optional_int32", field.getname());
    assertequals(field, messagetype.findfieldbyname("optional_int32"));
    assertnull(messagetype.findfieldbyname("no_such_field"));
    assertequals(field, messagetype.findfieldbynumber(1));
    assertnull(messagetype.findfieldbynumber(571283));
    for (int i = 0; i < messagetype.getfields().size(); i++) {
      assertequals(i, messagetype.getfields().get(i).getindex());
    }

    assertequals(nestedtype, messagetype.getnestedtypes().get(0));
    assertequals(nestedtype, messagetype.findnestedtypebyname("nestedmessage"));
    assertnull(messagetype.findnestedtypebyname("nosuchtype"));
    for (int i = 0; i < messagetype.getnestedtypes().size(); i++) {
      assertequals(i, messagetype.getnestedtypes().get(i).getindex());
    }

    enumdescriptor enumtype = testalltypes.nestedenum.getdescriptor();
    assertequals(enumtype, messagetype.getenumtypes().get(0));
    assertequals(enumtype, messagetype.findenumtypebyname("nestedenum"));
    assertnull(messagetype.findenumtypebyname("nosuchtype"));
    for (int i = 0; i < messagetype.getenumtypes().size(); i++) {
      assertequals(i, messagetype.getenumtypes().get(i).getindex());
    }
  }

  public void testfielddescriptor() throws exception {
    descriptor messagetype = testalltypes.getdescriptor();
    fielddescriptor primitivefield =
      messagetype.findfieldbyname("optional_int32");
    fielddescriptor enumfield =
      messagetype.findfieldbyname("optional_nested_enum");
    fielddescriptor messagefield =
      messagetype.findfieldbyname("optional_foreign_message");
    fielddescriptor cordfield =
      messagetype.findfieldbyname("optional_cord");
    fielddescriptor extension =
      unittestproto.optionalint32extension.getdescriptor();
    fielddescriptor nestedextension = testrequired.single.getdescriptor();

    assertequals("optional_int32", primitivefield.getname());
    assertequals("protobuf_unittest.testalltypes.optional_int32",
                 primitivefield.getfullname());
    assertequals(1, primitivefield.getnumber());
    assertequals(messagetype, primitivefield.getcontainingtype());
    assertequals(unittestproto.getdescriptor(), primitivefield.getfile());
    assertequals(fielddescriptor.type.int32, primitivefield.gettype());
    assertequals(fielddescriptor.javatype.int, primitivefield.getjavatype());
    assertequals(descriptorprotos.fieldoptions.getdefaultinstance(),
                 primitivefield.getoptions());
    assertfalse(primitivefield.isextension());
    assertequals("optional_int32", primitivefield.toproto().getname());

    assertequals("optional_nested_enum", enumfield.getname());
    assertequals(fielddescriptor.type.enum, enumfield.gettype());
    assertequals(fielddescriptor.javatype.enum, enumfield.getjavatype());
    assertequals(testalltypes.nestedenum.getdescriptor(),
                 enumfield.getenumtype());

    assertequals("optional_foreign_message", messagefield.getname());
    assertequals(fielddescriptor.type.message, messagefield.gettype());
    assertequals(fielddescriptor.javatype.message, messagefield.getjavatype());
    assertequals(foreignmessage.getdescriptor(), messagefield.getmessagetype());

    assertequals("optional_cord", cordfield.getname());
    assertequals(fielddescriptor.type.string, cordfield.gettype());
    assertequals(fielddescriptor.javatype.string, cordfield.getjavatype());
    assertequals(descriptorprotos.fieldoptions.ctype.cord,
                 cordfield.getoptions().getctype());

    assertequals("optional_int32_extension", extension.getname());
    assertequals("protobuf_unittest.optional_int32_extension",
                 extension.getfullname());
    assertequals(1, extension.getnumber());
    assertequals(testallextensions.getdescriptor(),
                 extension.getcontainingtype());
    assertequals(unittestproto.getdescriptor(), extension.getfile());
    assertequals(fielddescriptor.type.int32, extension.gettype());
    assertequals(fielddescriptor.javatype.int, extension.getjavatype());
    assertequals(descriptorprotos.fieldoptions.getdefaultinstance(),
                 extension.getoptions());
    asserttrue(extension.isextension());
    assertequals(null, extension.getextensionscope());
    assertequals("optional_int32_extension", extension.toproto().getname());

    assertequals("single", nestedextension.getname());
    assertequals("protobuf_unittest.testrequired.single",
                 nestedextension.getfullname());
    assertequals(testrequired.getdescriptor(),
                 nestedextension.getextensionscope());
  }

  public void testfielddescriptorlabel() throws exception {
    fielddescriptor requiredfield =
      testrequired.getdescriptor().findfieldbyname("a");
    fielddescriptor optionalfield =
      testalltypes.getdescriptor().findfieldbyname("optional_int32");
    fielddescriptor repeatedfield =
      testalltypes.getdescriptor().findfieldbyname("repeated_int32");

    asserttrue(requiredfield.isrequired());
    assertfalse(requiredfield.isrepeated());
    assertfalse(optionalfield.isrequired());
    assertfalse(optionalfield.isrepeated());
    assertfalse(repeatedfield.isrequired());
    asserttrue(repeatedfield.isrepeated());
  }

  public void testfielddescriptordefault() throws exception {
    descriptor d = testalltypes.getdescriptor();
    assertfalse(d.findfieldbyname("optional_int32").hasdefaultvalue());
    assertequals(0, d.findfieldbyname("optional_int32").getdefaultvalue());
    asserttrue(d.findfieldbyname("default_int32").hasdefaultvalue());
    assertequals(41, d.findfieldbyname("default_int32").getdefaultvalue());

    d = testextremedefaultvalues.getdescriptor();
    assertequals(
      bytestring.copyfrom(
        "\0\001\007\b\f\n\r\t\013\\\'\"\u00fe".getbytes("iso-8859-1")),
      d.findfieldbyname("escaped_bytes").getdefaultvalue());
    assertequals(-1, d.findfieldbyname("large_uint32").getdefaultvalue());
    assertequals(-1l, d.findfieldbyname("large_uint64").getdefaultvalue());
  }

  public void testenumdescriptor() throws exception {
    enumdescriptor enumtype = foreignenum.getdescriptor();
    enumdescriptor nestedtype = testalltypes.nestedenum.getdescriptor();

    assertequals("foreignenum", enumtype.getname());
    assertequals("protobuf_unittest.foreignenum", enumtype.getfullname());
    assertequals(unittestproto.getdescriptor(), enumtype.getfile());
    assertnull(enumtype.getcontainingtype());
    assertequals(descriptorprotos.enumoptions.getdefaultinstance(),
                 enumtype.getoptions());

    assertequals("nestedenum", nestedtype.getname());
    assertequals("protobuf_unittest.testalltypes.nestedenum",
                 nestedtype.getfullname());
    assertequals(unittestproto.getdescriptor(), nestedtype.getfile());
    assertequals(testalltypes.getdescriptor(), nestedtype.getcontainingtype());

    enumvaluedescriptor value = foreignenum.foreign_foo.getvaluedescriptor();
    assertequals(value, enumtype.getvalues().get(0));
    assertequals("foreign_foo", value.getname());
    assertequals(4, value.getnumber());
    assertequals(value, enumtype.findvaluebyname("foreign_foo"));
    assertequals(value, enumtype.findvaluebynumber(4));
    assertnull(enumtype.findvaluebyname("no_such_value"));
    for (int i = 0; i < enumtype.getvalues().size(); i++) {
      assertequals(i, enumtype.getvalues().get(i).getindex());
    }
  }

  public void testservicedescriptor() throws exception {
    servicedescriptor service = testservice.getdescriptor();

    assertequals("testservice", service.getname());
    assertequals("protobuf_unittest.testservice", service.getfullname());
    assertequals(unittestproto.getdescriptor(), service.getfile());

    assertequals(2, service.getmethods().size());

    methoddescriptor foomethod = service.getmethods().get(0);
    assertequals("foo", foomethod.getname());
    assertequals(unittestproto.foorequest.getdescriptor(),
                 foomethod.getinputtype());
    assertequals(unittestproto.fooresponse.getdescriptor(),
                 foomethod.getoutputtype());
    assertequals(foomethod, service.findmethodbyname("foo"));

    methoddescriptor barmethod = service.getmethods().get(1);
    assertequals("bar", barmethod.getname());
    assertequals(unittestproto.barrequest.getdescriptor(),
                 barmethod.getinputtype());
    assertequals(unittestproto.barresponse.getdescriptor(),
                 barmethod.getoutputtype());
    assertequals(barmethod, service.findmethodbyname("bar"));

    assertnull(service.findmethodbyname("nosuchmethod"));

    for (int i = 0; i < service.getmethods().size(); i++) {
      assertequals(i, service.getmethods().get(i).getindex());
    }
  }


  public void testcustomoptions() throws exception {
    descriptor descriptor =
      unittestcustomoptions.testmessagewithcustomoptions.getdescriptor();

    asserttrue(
      descriptor.getoptions().hasextension(unittestcustomoptions.messageopt1));
    assertequals(integer.valueof(-56),
      descriptor.getoptions().getextension(unittestcustomoptions.messageopt1));

    fielddescriptor field = descriptor.findfieldbyname("field1");
    assertnotnull(field);

    asserttrue(
      field.getoptions().hasextension(unittestcustomoptions.fieldopt1));
    assertequals(long.valueof(8765432109l),
      field.getoptions().getextension(unittestcustomoptions.fieldopt1));

    enumdescriptor enumtype =
      unittestcustomoptions.testmessagewithcustomoptions.anenum.getdescriptor();

    asserttrue(
      enumtype.getoptions().hasextension(unittestcustomoptions.enumopt1));
    assertequals(integer.valueof(-789),
      enumtype.getoptions().getextension(unittestcustomoptions.enumopt1));

    servicedescriptor service =
      unittestcustomoptions.testservicewithcustomoptions.getdescriptor();

    asserttrue(
      service.getoptions().hasextension(unittestcustomoptions.serviceopt1));
    assertequals(long.valueof(-9876543210l),
      service.getoptions().getextension(unittestcustomoptions.serviceopt1));

    methoddescriptor method = service.findmethodbyname("foo");
    assertnotnull(method);

    asserttrue(
      method.getoptions().hasextension(unittestcustomoptions.methodopt1));
    assertequals(unittestcustomoptions.methodopt1.methodopt1_val2,
      method.getoptions().getextension(unittestcustomoptions.methodopt1));
  }

  /**
   * test that the fielddescriptor.type enum is the same as the
   * wireformat.fieldtype enum.
   */
  public void testfieldtypetablesmatch() throws exception {
    fielddescriptor.type[] values1 = fielddescriptor.type.values();
    wireformat.fieldtype[] values2 = wireformat.fieldtype.values();

    assertequals(values1.length, values2.length);

    for (int i = 0; i < values1.length; i++) {
      assertequals(values1[i].tostring(), values2[i].tostring());
    }
  }

  /**
   * test that the fielddescriptor.javatype enum is the same as the
   * wireformat.javatype enum.
   */
  public void testjavatypetablesmatch() throws exception {
    fielddescriptor.javatype[] values1 = fielddescriptor.javatype.values();
    wireformat.javatype[] values2 = wireformat.javatype.values();

    assertequals(values1.length, values2.length);

    for (int i = 0; i < values1.length; i++) {
      assertequals(values1[i].tostring(), values2[i].tostring());
    }
  }

  public void testenormousdescriptor() throws exception {
    // the descriptor for this file is larger than 64k, yet it did not cause
    // a compiler error due to an over-long string literal.
    asserttrue(
        unittestenormousdescriptor.getdescriptor()
          .toproto().getserializedsize() > 65536);
  }

  /**
   * tests that the descriptorvalidationexception works as intended.
   */
  public void testdescriptorvalidatorexception() throws exception {
    filedescriptorproto filedescriptorproto = filedescriptorproto.newbuilder()
      .setname("foo.proto")
      .addmessagetype(descriptorproto.newbuilder()
      .setname("foo")
        .addfield(fielddescriptorproto.newbuilder()
          .setlabel(fielddescriptorproto.label.label_optional)
          .settype(fielddescriptorproto.type.type_int32)
          .setname("foo")
          .setnumber(1)
          .setdefaultvalue("invalid")
          .build())
        .build())
      .build();
    try {
      descriptors.filedescriptor.buildfrom(filedescriptorproto,
          new filedescriptor[0]);
      fail("descriptorvalidationexception expected");
    } catch (descriptorvalidationexception e) {
      // expected; check that the error message contains some useful hints
      asserttrue(e.getmessage().indexof("foo") != -1);
      asserttrue(e.getmessage().indexof("foo") != -1);
      asserttrue(e.getmessage().indexof("invalid") != -1);
      asserttrue(e.getcause() instanceof numberformatexception);
      asserttrue(e.getcause().getmessage().indexof("invalid") != -1);
    }
  }

  /**
   * tests the translate/crosslink for an example where a message field's name
   * and type name are the same.
   */
  public void testdescriptorcomplexcrosslink() throws exception {
    filedescriptorproto filedescriptorproto = filedescriptorproto.newbuilder()
      .setname("foo.proto")
      .addmessagetype(descriptorproto.newbuilder()
        .setname("foo")
        .addfield(fielddescriptorproto.newbuilder()
          .setlabel(fielddescriptorproto.label.label_optional)
          .settype(fielddescriptorproto.type.type_int32)
          .setname("foo")
          .setnumber(1)
          .build())
        .build())
      .addmessagetype(descriptorproto.newbuilder()
        .setname("bar")
        .addfield(fielddescriptorproto.newbuilder()
          .setlabel(fielddescriptorproto.label.label_optional)
          .settypename("foo")
          .setname("foo")
          .setnumber(1)
          .build())
        .build())
      .build();
    // translate and crosslink
    filedescriptor file =
      descriptors.filedescriptor.buildfrom(filedescriptorproto, 
          new filedescriptor[0]);
    // verify resulting descriptors
    assertnotnull(file);
    list<descriptor> msglist = file.getmessagetypes();
    assertnotnull(msglist);
    asserttrue(msglist.size() == 2);
    boolean barfound = false;
    for (descriptor desc : msglist) {
      if (desc.getname().equals("bar")) {
        barfound = true;
        assertnotnull(desc.getfields());
        list<fielddescriptor> fieldlist = desc.getfields();
        assertnotnull(fieldlist);
        asserttrue(fieldlist.size() == 1);
        asserttrue(fieldlist.get(0).gettype() == fielddescriptor.type.message);
        asserttrue(fieldlist.get(0).getmessagetype().getname().equals("foo"));
      }
    }
    asserttrue(barfound);
  }
  
  public void testinvalidpublicdependency() throws exception {
    filedescriptorproto fooproto = filedescriptorproto.newbuilder()
        .setname("foo.proto") .build();
    filedescriptorproto barproto = filedescriptorproto.newbuilder()
        .setname("boo.proto")
        .adddependency("foo.proto")
        .addpublicdependency(1)  // error, should be 0.
        .build();
    filedescriptor foofile = descriptors.filedescriptor.buildfrom(fooproto,
        new filedescriptor[0]);
    try {
      descriptors.filedescriptor.buildfrom(barproto,
          new filedescriptor[] {foofile});
      fail("descriptorvalidationexception expected");
    } catch (descriptorvalidationexception e) {
      asserttrue(
          e.getmessage().indexof("invalid public dependency index.") != -1);
    }
  }

  public void testhiddendependency() throws exception {
    filedescriptorproto barproto = filedescriptorproto.newbuilder()
        .setname("bar.proto")
        .addmessagetype(descriptorproto.newbuilder().setname("bar"))
        .build();
    filedescriptorproto forwardproto = filedescriptorproto.newbuilder()
        .setname("forward.proto")
        .adddependency("bar.proto")
        .build();
    filedescriptorproto fooproto = filedescriptorproto.newbuilder()
        .setname("foo.proto")
        .adddependency("forward.proto")
        .addmessagetype(descriptorproto.newbuilder()
            .setname("foo")
            .addfield(fielddescriptorproto.newbuilder()
                .setlabel(fielddescriptorproto.label.label_optional)
                .settypename("bar")
                .setname("bar")
                .setnumber(1)))
        .build();
    filedescriptor barfile = descriptors.filedescriptor.buildfrom(
        barproto, new filedescriptor[0]);
    filedescriptor forwardfile = descriptors.filedescriptor.buildfrom(
        forwardproto, new filedescriptor[] {barfile});

    try {
      descriptors.filedescriptor.buildfrom(
          fooproto, new filedescriptor[] {forwardfile});
      fail("descriptorvalidationexception expected");
    } catch (descriptorvalidationexception e) {
      asserttrue(e.getmessage().indexof("bar") != -1);
      asserttrue(e.getmessage().indexof("is not defined") != -1);
    }
  }

  public void testpublicdependency() throws exception {
    filedescriptorproto barproto = filedescriptorproto.newbuilder()
        .setname("bar.proto")
        .addmessagetype(descriptorproto.newbuilder().setname("bar"))
        .build();
    filedescriptorproto forwardproto = filedescriptorproto.newbuilder()
        .setname("forward.proto")
        .adddependency("bar.proto")
        .addpublicdependency(0)
        .build();
    filedescriptorproto fooproto = filedescriptorproto.newbuilder()
        .setname("foo.proto")
        .adddependency("forward.proto")
        .addmessagetype(descriptorproto.newbuilder()
            .setname("foo")
            .addfield(fielddescriptorproto.newbuilder()
                .setlabel(fielddescriptorproto.label.label_optional)
                .settypename("bar")
                .setname("bar")
                .setnumber(1)))
        .build();
    filedescriptor barfile = descriptors.filedescriptor.buildfrom(
        barproto, new filedescriptor[0]);
    filedescriptor forwardfile = descriptors.filedescriptor.buildfrom(
        forwardproto, new filedescriptor[]{barfile});
    descriptors.filedescriptor.buildfrom(
        fooproto, new filedescriptor[] {forwardfile});
  }
  
  /**
   * tests the translate/crosslink for an example with a more complex namespace
   * referencing.
   */
  public void testcomplexnamespacepublicdependency() throws exception {
    filedescriptorproto fooproto = filedescriptorproto.newbuilder()
        .setname("bar.proto")
        .setpackage("a.b.c.d.bar.shared")
        .addenumtype(enumdescriptorproto.newbuilder()
            .setname("myenum")
            .addvalue(enumvaluedescriptorproto.newbuilder()
                .setname("blah")
                .setnumber(1)))
        .build();
    filedescriptorproto barproto = filedescriptorproto.newbuilder()
        .setname("foo.proto")
        .adddependency("bar.proto")
        .setpackage("a.b.c.d.foo.shared")
        .addmessagetype(descriptorproto.newbuilder()
            .setname("mymessage")
            .addfield(fielddescriptorproto.newbuilder()
                .setlabel(fielddescriptorproto.label.label_repeated)
                .settypename("bar.shared.myenum")
                .setname("myfield")
                .setnumber(1)))
        .build();
    // translate and crosslink
    filedescriptor foofile = descriptors.filedescriptor.buildfrom(
        fooproto, new filedescriptor[0]);
    filedescriptor barfile = descriptors.filedescriptor.buildfrom(
        barproto, new filedescriptor[]{foofile});
    // verify resulting descriptors
    assertnotnull(barfile);
    list<descriptor> msglist = barfile.getmessagetypes();
    assertnotnull(msglist);
    asserttrue(msglist.size() == 1);
    descriptor desc = msglist.get(0);
    if (desc.getname().equals("mymessage")) {
      assertnotnull(desc.getfields());
      list<fielddescriptor> fieldlist = desc.getfields();
      assertnotnull(fieldlist);
      asserttrue(fieldlist.size() == 1);
      fielddescriptor field = fieldlist.get(0);
      asserttrue(field.gettype() == fielddescriptor.type.enum);
      asserttrue(field.getenumtype().getname().equals("myenum"));
      asserttrue(field.getenumtype().getfile().getname().equals("bar.proto"));
      asserttrue(field.getenumtype().getfile().getpackage().equals(
          "a.b.c.d.bar.shared"));
    }   
  }
}
