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

import java.util.collections;
import java.util.list;

/**
 * tests for {@link repeatedfieldbuilder}. this tests basic functionality.
 * more extensive testing is provided via other tests that exercise the
 * builder.
 *
 * @author jonp@google.com (jon perlow)
 */
public class repeatedfieldbuildertest extends testcase {

  public void testbasicuse() {
    testutil.mockbuilderparent mockparent = new testutil.mockbuilderparent();
    repeatedfieldbuilder<testalltypes, testalltypes.builder,
        testalltypesorbuilder> builder = newrepeatedfieldbuilder(mockparent);
    builder.addmessage(testalltypes.newbuilder().setoptionalint32(0).build());
    builder.addmessage(testalltypes.newbuilder().setoptionalint32(1).build());
    assertequals(0, builder.getmessage(0).getoptionalint32());
    assertequals(1, builder.getmessage(1).getoptionalint32());

    list<testalltypes> list = builder.build();
    assertequals(2, list.size());
    assertequals(0, list.get(0).getoptionalint32());
    assertequals(1, list.get(1).getoptionalint32());
    assertisunmodifiable(list);

    // make sure it doesn't change.
    list<testalltypes> list2 = builder.build();
    assertsame(list, list2);
    assertequals(0, mockparent.getinvalidationcount());
  }

  public void testgoingbackandforth() {
    testutil.mockbuilderparent mockparent = new testutil.mockbuilderparent();
    repeatedfieldbuilder<testalltypes, testalltypes.builder,
        testalltypesorbuilder> builder = newrepeatedfieldbuilder(mockparent);
    builder.addmessage(testalltypes.newbuilder().setoptionalint32(0).build());
    builder.addmessage(testalltypes.newbuilder().setoptionalint32(1).build());
    assertequals(0, builder.getmessage(0).getoptionalint32());
    assertequals(1, builder.getmessage(1).getoptionalint32());

    // convert to list
    list<testalltypes> list = builder.build();
    assertequals(2, list.size());
    assertequals(0, list.get(0).getoptionalint32());
    assertequals(1, list.get(1).getoptionalint32());
    assertisunmodifiable(list);

    // update 0th item
    assertequals(0, mockparent.getinvalidationcount());
    builder.getbuilder(0).setoptionalstring("foo");
    assertequals(1, mockparent.getinvalidationcount());
    list = builder.build();
    assertequals(2, list.size());
    assertequals(0, list.get(0).getoptionalint32());
      assertequals("foo", list.get(0).getoptionalstring());
    assertequals(1, list.get(1).getoptionalint32());
    assertisunmodifiable(list);
    assertequals(1, mockparent.getinvalidationcount());
  }

  public void testvariousmethods() {
    testutil.mockbuilderparent mockparent = new testutil.mockbuilderparent();
    repeatedfieldbuilder<testalltypes, testalltypes.builder,
        testalltypesorbuilder> builder = newrepeatedfieldbuilder(mockparent);
    builder.addmessage(testalltypes.newbuilder().setoptionalint32(1).build());
    builder.addmessage(testalltypes.newbuilder().setoptionalint32(2).build());
    builder.addbuilder(0, testalltypes.getdefaultinstance())
        .setoptionalint32(0);
    builder.addbuilder(testalltypes.getdefaultinstance()).setoptionalint32(3);

    assertequals(0, builder.getmessage(0).getoptionalint32());
    assertequals(1, builder.getmessage(1).getoptionalint32());
    assertequals(2, builder.getmessage(2).getoptionalint32());
    assertequals(3, builder.getmessage(3).getoptionalint32());

    assertequals(0, mockparent.getinvalidationcount());
    list<testalltypes> messages = builder.build();
    assertequals(4, messages.size());
    assertsame(messages, builder.build()); // expect same list

    // remove a message.
    builder.remove(2);
    assertequals(1, mockparent.getinvalidationcount());
    assertequals(3, builder.getcount());
    assertequals(0, builder.getmessage(0).getoptionalint32());
    assertequals(1, builder.getmessage(1).getoptionalint32());
    assertequals(3, builder.getmessage(2).getoptionalint32());

    // remove a builder.
    builder.remove(0);
    assertequals(1, mockparent.getinvalidationcount());
    assertequals(2, builder.getcount());
    assertequals(1, builder.getmessage(0).getoptionalint32());
    assertequals(3, builder.getmessage(1).getoptionalint32());

    // test clear.
    builder.clear();
    assertequals(1, mockparent.getinvalidationcount());
    assertequals(0, builder.getcount());
    asserttrue(builder.isempty());
  }

  public void testlists() {
    testutil.mockbuilderparent mockparent = new testutil.mockbuilderparent();
    repeatedfieldbuilder<testalltypes, testalltypes.builder,
        testalltypesorbuilder> builder = newrepeatedfieldbuilder(mockparent);
    builder.addmessage(testalltypes.newbuilder().setoptionalint32(1).build());
    builder.addmessage(0,
        testalltypes.newbuilder().setoptionalint32(0).build());
    assertequals(0, builder.getmessage(0).getoptionalint32());
    assertequals(1, builder.getmessage(1).getoptionalint32());

    // use list of builders.
    list<testalltypes.builder> builders = builder.getbuilderlist();
    assertequals(0, builders.get(0).getoptionalint32());
    assertequals(1, builders.get(1).getoptionalint32());
    builders.get(0).setoptionalint32(10);
    builders.get(1).setoptionalint32(11);

    // use list of protos
    list<testalltypes> protos = builder.getmessagelist();
    assertequals(10, protos.get(0).getoptionalint32());
    assertequals(11, protos.get(1).getoptionalint32());

    // add an item to the builders and verify it's updated in both
    builder.addmessage(testalltypes.newbuilder().setoptionalint32(12).build());
    assertequals(3, builders.size());
    assertequals(3, protos.size());
  }

  private void assertisunmodifiable(list<?> list) {
    if (list == collections.emptylist()) {
      // okay -- need to check this b/c emptylist allows you to call clear.
    } else {
      try {
        list.clear();
        fail("list wasn't immutable");
      } catch (unsupportedoperationexception e) {
        // good
      }
    }
  }

  private repeatedfieldbuilder<testalltypes, testalltypes.builder,
      testalltypesorbuilder>
      newrepeatedfieldbuilder(generatedmessage.builderparent parent) {
    return new repeatedfieldbuilder<testalltypes, testalltypes.builder,
        testalltypesorbuilder>(collections.<testalltypes>emptylist(), false,
        parent, false);
  }
}
