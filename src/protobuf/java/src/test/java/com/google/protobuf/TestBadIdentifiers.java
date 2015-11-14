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

import junit.framework.testcase;

/**
 * tests that proto2 api generation doesn't cause compile errors when
 * compiling protocol buffers that have names that would otherwise conflict
 * if not fully qualified (like @deprecated and @override).
 *
 * @author jonp@google.com (jon perlow)
 */
public class testbadidentifiers extends testcase {

  public void testcompilation() {
    // if this compiles, it means the generation was correct.
    testbadidentifiersproto.deprecated.newbuilder();
    testbadidentifiersproto.override.newbuilder();
  }

  public void testgetdescriptor() {
    descriptors.filedescriptor filedescriptor =
        testbadidentifiersproto.getdescriptor();
    string descriptorfield = testbadidentifiersproto.descriptor
        .getdefaultinstance().getdescriptor();
    descriptors.descriptor protodescriptor = testbadidentifiersproto.descriptor
        .getdefaultinstance().getdescriptorfortype();
    string nesteddescriptorfield = testbadidentifiersproto.descriptor
        .nesteddescriptor.getdefaultinstance().getdescriptor();
    descriptors.descriptor nestedprotodescriptor = testbadidentifiersproto
        .descriptor.nesteddescriptor.getdefaultinstance()
        .getdescriptorfortype();
  }
}
