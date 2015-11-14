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

import protobuf_unittest.vehicle;
import protobuf_unittest.wheel;

import junit.framework.testcase;

import java.util.list;
import java.util.arraylist;

/**
 * test cases that exercise end-to-end use cases involving
 * {@link singlefieldbuilder} and {@link repeatedfieldbuilder}.
 *
 * @author jonp@google.com (jon perlow)
 */
public class nestedbuilderstest extends testcase {

  public void testmessagesandbuilders() {
    vehicle.builder vehiclebuilder = vehicle.newbuilder();
    vehiclebuilder.addwheelbuilder()
        .setradius(4)
        .setwidth(1);
    vehiclebuilder.addwheelbuilder()
        .setradius(4)
        .setwidth(2);
    vehiclebuilder.addwheelbuilder()
        .setradius(4)
        .setwidth(3);
    vehiclebuilder.addwheelbuilder()
        .setradius(4)
        .setwidth(4);
    vehiclebuilder.getenginebuilder()
        .setliters(10);

    vehicle vehicle = vehiclebuilder.build();
    assertequals(4, vehicle.getwheelcount());
    for (int i = 0; i < 4; i++) {
      wheel wheel = vehicle.getwheel(i);
      assertequals(4, wheel.getradius());
      assertequals(i + 1, wheel.getwidth());
    }
    assertequals(10, vehicle.getengine().getliters());

    for (int i = 0; i < 4; i++) {
      vehiclebuilder.getwheelbuilder(i)
          .setradius(5)
          .setwidth(i + 10);
    }
    vehiclebuilder.getenginebuilder().setliters(20);

    vehicle = vehiclebuilder.build();
    for (int i = 0; i < 4; i++) {
      wheel wheel = vehicle.getwheel(i);
      assertequals(5, wheel.getradius());
      assertequals(i + 10, wheel.getwidth());
    }
    assertequals(20, vehicle.getengine().getliters());
    asserttrue(vehicle.hasengine());
  }

  public void testmessagesarecached() {
    vehicle.builder vehiclebuilder = vehicle.newbuilder();
    vehiclebuilder.addwheelbuilder()
        .setradius(1)
        .setwidth(2);
    vehiclebuilder.addwheelbuilder()
        .setradius(3)
        .setwidth(4);
    vehiclebuilder.addwheelbuilder()
        .setradius(5)
        .setwidth(6);
    vehiclebuilder.addwheelbuilder()
        .setradius(7)
        .setwidth(8);

    // make sure messages are cached.
    list<wheel> wheels = new arraylist<wheel>(vehiclebuilder.getwheellist());
    for (int i = 0; i < wheels.size(); i++) {
      assertsame(wheels.get(i), vehiclebuilder.getwheel(i));
    }

    // now get builders and check they didn't change.
    for (int i = 0; i < wheels.size(); i++) {
      vehiclebuilder.getwheel(i);
    }
    for (int i = 0; i < wheels.size(); i++) {
      assertsame(wheels.get(i), vehiclebuilder.getwheel(i));
    }

    // change just one
    vehiclebuilder.getwheelbuilder(3)
        .setradius(20).setwidth(20);

    // now get wheels and check that only that one changed
    for (int i = 0; i < wheels.size(); i++) {
      if (i < 3) {
        assertsame(wheels.get(i), vehiclebuilder.getwheel(i));
      } else {
        assertnotsame(wheels.get(i), vehiclebuilder.getwheel(i));
      }
    }
  }

  public void testremove_withnestedbuilders() {
    vehicle.builder vehiclebuilder = vehicle.newbuilder();
    vehiclebuilder.addwheelbuilder()
        .setradius(1)
        .setwidth(1);
    vehiclebuilder.addwheelbuilder()
        .setradius(2)
        .setwidth(2);
    vehiclebuilder.removewheel(0);

    assertequals(1, vehiclebuilder.getwheelcount());
    assertequals(2, vehiclebuilder.getwheel(0).getradius());
  }

  public void testremove_withnestedmessages() {
    vehicle.builder vehiclebuilder = vehicle.newbuilder();
    vehiclebuilder.addwheel(wheel.newbuilder()
        .setradius(1)
        .setwidth(1));
    vehiclebuilder.addwheel(wheel.newbuilder()
        .setradius(2)
        .setwidth(2));
    vehiclebuilder.removewheel(0);

    assertequals(1, vehiclebuilder.getwheelcount());
    assertequals(2, vehiclebuilder.getwheel(0).getradius());
  }

  public void testmerge() {
    vehicle vehicle1 = vehicle.newbuilder()
        .addwheel(wheel.newbuilder().setradius(1).build())
        .addwheel(wheel.newbuilder().setradius(2).build())
        .build();

    vehicle vehicle2 = vehicle.newbuilder()
        .mergefrom(vehicle1)
        .build();
    // list should be the same -- no allocation
    assertsame(vehicle1.getwheellist(), vehicle2.getwheellist());

    vehicle vehicle3 = vehicle1.tobuilder().build();
    assertsame(vehicle1.getwheellist(), vehicle3.getwheellist());
  }

  public void testgettingbuildermarksfieldashaving() {
    vehicle.builder vehiclebuilder = vehicle.newbuilder();
    vehiclebuilder.getenginebuilder();
    vehicle vehicle = vehiclebuilder.buildpartial();
    asserttrue(vehicle.hasengine());
  }
}
