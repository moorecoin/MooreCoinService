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

import protobuf_unittest.unittestproto.testalltypes;
import protobuf_unittest.unittestproto.testalltypesorbuilder;

import junit.framework.testcase;

/**
 * tests for {@link singlefieldbuilder}. this tests basic functionality.
 * more extensive testing is provided via other tests that exercise the
 * builder.
 *
 * @author jonp@google.com (jon perlow)
 */
public class singlefieldbuildertest extends testcase {

  public void testbasicuseandinvalidations() {
    testutil.mockbuilderparent mockparent = new testutil.mockbuilderparent();
    singlefieldbuilder<testalltypes, testalltypes.builder,
        testalltypesorbuilder> builder =
        new singlefieldbuilder<testalltypes, testalltypes.builder,
            testalltypesorbuilder>(
            testalltypes.getdefaultinstance(),
            mockparent,
            false);
    assertsame(testalltypes.getdefaultinstance(), builder.getmessage());
    assertequals(testalltypes.getdefaultinstance(),
        builder.getbuilder().buildpartial());
    assertequals(0, mockparent.getinvalidationcount());

    builder.getbuilder().setoptionalint32(10);
    assertequals(0, mockparent.getinvalidationcount());
    testalltypes message = builder.build();
    assertequals(10, message.getoptionalint32());

    // test that we receive invalidations now that build has been called.
    assertequals(0, mockparent.getinvalidationcount());
    builder.getbuilder().setoptionalint32(20);
    assertequals(1, mockparent.getinvalidationcount());

    // test that we don't keep getting invalidations on every change
    builder.getbuilder().setoptionalint32(30);
    assertequals(1, mockparent.getinvalidationcount());

  }

  public void testsetmessage() {
    testutil.mockbuilderparent mockparent = new testutil.mockbuilderparent();
    singlefieldbuilder<testalltypes, testalltypes.builder,
        testalltypesorbuilder> builder =
        new singlefieldbuilder<testalltypes, testalltypes.builder,
            testalltypesorbuilder>(
            testalltypes.getdefaultinstance(),
            mockparent,
            false);
    builder.setmessage(testalltypes.newbuilder().setoptionalint32(0).build());
    assertequals(0, builder.getmessage().getoptionalint32());

    // update message using the builder
    builder.getbuilder().setoptionalint32(1);
    assertequals(0, mockparent.getinvalidationcount());
    assertequals(1, builder.getbuilder().getoptionalint32());
    assertequals(1, builder.getmessage().getoptionalint32());
    builder.build();
    builder.getbuilder().setoptionalint32(2);
    assertequals(2, builder.getbuilder().getoptionalint32());
    assertequals(2, builder.getmessage().getoptionalint32());

    // make sure message stays cached
    assertsame(builder.getmessage(), builder.getmessage());
  }

  public void testclear() {
    testutil.mockbuilderparent mockparent = new testutil.mockbuilderparent();
    singlefieldbuilder<testalltypes, testalltypes.builder,
        testalltypesorbuilder> builder =
        new singlefieldbuilder<testalltypes, testalltypes.builder,
            testalltypesorbuilder>(
            testalltypes.getdefaultinstance(),
            mockparent,
            false);
    builder.setmessage(testalltypes.newbuilder().setoptionalint32(0).build());
    assertnotsame(testalltypes.getdefaultinstance(), builder.getmessage());
    builder.clear();
    assertsame(testalltypes.getdefaultinstance(), builder.getmessage());

    builder.getbuilder().setoptionalint32(1);
    assertnotsame(testalltypes.getdefaultinstance(), builder.getmessage());
    builder.clear();
    assertsame(testalltypes.getdefaultinstance(), builder.getmessage());
  }

  public void testmerge() {
    testutil.mockbuilderparent mockparent = new testutil.mockbuilderparent();
    singlefieldbuilder<testalltypes, testalltypes.builder,
        testalltypesorbuilder> builder =
        new singlefieldbuilder<testalltypes, testalltypes.builder,
            testalltypesorbuilder>(
            testalltypes.getdefaultinstance(),
            mockparent,
            false);

    // merge into default field.
    builder.mergefrom(testalltypes.getdefaultinstance());
    assertsame(testalltypes.getdefaultinstance(), builder.getmessage());

    // merge into non-default field on existing builder.
    builder.getbuilder().setoptionalint32(2);
    builder.mergefrom(testalltypes.newbuilder()
        .setoptionaldouble(4.0)
        .buildpartial());
    assertequals(2, builder.getmessage().getoptionalint32());
    assertequals(4.0, builder.getmessage().getoptionaldouble());

    // merge into non-default field on existing message
    builder.setmessage(testalltypes.newbuilder()
        .setoptionalint32(10)
        .buildpartial());
    builder.mergefrom(testalltypes.newbuilder()
        .setoptionaldouble(5.0)
        .buildpartial());
    assertequals(10, builder.getmessage().getoptionalint32());
    assertequals(5.0, builder.getmessage().getoptionaldouble());
  }
}
