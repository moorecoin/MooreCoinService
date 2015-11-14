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

import com.google.protobuf.descriptors.filedescriptor;
import com.google.protobuf.descriptors.methoddescriptor;
import google.protobuf.no_generic_services_test.unittestnogenericservices;
import protobuf_unittest.messagewithnoouter;
import protobuf_unittest.servicewithnoouter;
import protobuf_unittest.unittestproto.testalltypes;
import protobuf_unittest.unittestproto.testservice;
import protobuf_unittest.unittestproto.foorequest;
import protobuf_unittest.unittestproto.fooresponse;
import protobuf_unittest.unittestproto.barrequest;
import protobuf_unittest.unittestproto.barresponse;

import org.easymock.classextension.easymock;
import org.easymock.classextension.imockscontrol;
import org.easymock.iargumentmatcher;

import java.util.hashset;
import java.util.set;

import junit.framework.testcase;

/**
 * tests services and stubs.
 *
 * @author kenton@google.com kenton varda
 */
public class servicetest extends testcase {
  private imockscontrol control;
  private rpccontroller mockcontroller;

  private final descriptors.methoddescriptor foodescriptor =
    testservice.getdescriptor().getmethods().get(0);
  private final descriptors.methoddescriptor bardescriptor =
    testservice.getdescriptor().getmethods().get(1);

  @override
  protected void setup() throws exception {
    super.setup();
    control = easymock.createstrictcontrol();
    mockcontroller = control.createmock(rpccontroller.class);
  }

  // =================================================================

  /** tests service.callmethod(). */
  public void testcallmethod() throws exception {
    foorequest foorequest = foorequest.newbuilder().build();
    barrequest barrequest = barrequest.newbuilder().build();
    mockcallback<message> foocallback = new mockcallback<message>();
    mockcallback<message> barcallback = new mockcallback<message>();
    testservice mockservice = control.createmock(testservice.class);

    mockservice.foo(easymock.same(mockcontroller), easymock.same(foorequest),
                    this.<fooresponse>wrapscallback(foocallback));
    mockservice.bar(easymock.same(mockcontroller), easymock.same(barrequest),
                    this.<barresponse>wrapscallback(barcallback));
    control.replay();

    mockservice.callmethod(foodescriptor, mockcontroller,
                           foorequest, foocallback);
    mockservice.callmethod(bardescriptor, mockcontroller,
                           barrequest, barcallback);
    control.verify();
  }

  /** tests service.get{request,response}prototype(). */
  public void testgetprototype() throws exception {
    testservice mockservice = control.createmock(testservice.class);

    assertsame(mockservice.getrequestprototype(foodescriptor),
               foorequest.getdefaultinstance());
    assertsame(mockservice.getresponseprototype(foodescriptor),
               fooresponse.getdefaultinstance());
    assertsame(mockservice.getrequestprototype(bardescriptor),
               barrequest.getdefaultinstance());
    assertsame(mockservice.getresponseprototype(bardescriptor),
               barresponse.getdefaultinstance());
  }

  /** tests generated stubs. */
  public void teststub() throws exception {
    foorequest foorequest = foorequest.newbuilder().build();
    barrequest barrequest = barrequest.newbuilder().build();
    mockcallback<fooresponse> foocallback = new mockcallback<fooresponse>();
    mockcallback<barresponse> barcallback = new mockcallback<barresponse>();
    rpcchannel mockchannel = control.createmock(rpcchannel.class);
    testservice stub = testservice.newstub(mockchannel);

    mockchannel.callmethod(
      easymock.same(foodescriptor),
      easymock.same(mockcontroller),
      easymock.same(foorequest),
      easymock.same(fooresponse.getdefaultinstance()),
      this.<message>wrapscallback(foocallback));
    mockchannel.callmethod(
      easymock.same(bardescriptor),
      easymock.same(mockcontroller),
      easymock.same(barrequest),
      easymock.same(barresponse.getdefaultinstance()),
      this.<message>wrapscallback(barcallback));
    control.replay();

    stub.foo(mockcontroller, foorequest, foocallback);
    stub.bar(mockcontroller, barrequest, barcallback);
    control.verify();
  }

  /** tests generated blocking stubs. */
  public void testblockingstub() throws exception {
    foorequest foorequest = foorequest.newbuilder().build();
    barrequest barrequest = barrequest.newbuilder().build();
    blockingrpcchannel mockchannel =
        control.createmock(blockingrpcchannel.class);
    testservice.blockinginterface stub =
        testservice.newblockingstub(mockchannel);

    fooresponse fooresponse = fooresponse.newbuilder().build();
    barresponse barresponse = barresponse.newbuilder().build();

    easymock.expect(mockchannel.callblockingmethod(
      easymock.same(foodescriptor),
      easymock.same(mockcontroller),
      easymock.same(foorequest),
      easymock.same(fooresponse.getdefaultinstance()))).andreturn(fooresponse);
    easymock.expect(mockchannel.callblockingmethod(
      easymock.same(bardescriptor),
      easymock.same(mockcontroller),
      easymock.same(barrequest),
      easymock.same(barresponse.getdefaultinstance()))).andreturn(barresponse);
    control.replay();

    assertsame(fooresponse, stub.foo(mockcontroller, foorequest));
    assertsame(barresponse, stub.bar(mockcontroller, barrequest));
    control.verify();
  }

  public void testnewreflectiveservice() {
    servicewithnoouter.interface impl =
        control.createmock(servicewithnoouter.interface.class);
    rpccontroller controller = control.createmock(rpccontroller.class);
    service service = servicewithnoouter.newreflectiveservice(impl);

    methoddescriptor foomethod =
        servicewithnoouter.getdescriptor().findmethodbyname("foo");
    messagewithnoouter request = messagewithnoouter.getdefaultinstance();
    rpccallback<message> callback = new rpccallback<message>() {
      public void run(message parameter) {
        // no reason this should be run.
        fail();
      }
    };
    rpccallback<testalltypes> specializedcallback =
        rpcutil.specializecallback(callback);

    impl.foo(easymock.same(controller), easymock.same(request),
        easymock.same(specializedcallback));
    easymock.expectlastcall();

    control.replay();

    service.callmethod(foomethod, controller, request, callback);

    control.verify();
  }

  public void testnewreflectiveblockingservice() throws serviceexception {
    servicewithnoouter.blockinginterface impl =
        control.createmock(servicewithnoouter.blockinginterface.class);
    rpccontroller controller = control.createmock(rpccontroller.class);
    blockingservice service =
        servicewithnoouter.newreflectiveblockingservice(impl);

    methoddescriptor foomethod =
        servicewithnoouter.getdescriptor().findmethodbyname("foo");
    messagewithnoouter request = messagewithnoouter.getdefaultinstance();

    testalltypes expectedresponse = testalltypes.getdefaultinstance();
    easymock.expect(impl.foo(easymock.same(controller), easymock.same(request)))
        .andreturn(expectedresponse);

    control.replay();

    message response =
        service.callblockingmethod(foomethod, controller, request);
    assertequals(expectedresponse, response);

    control.verify();
  }

  public void testnogenericservices() throws exception {
    // non-services should be usable.
    unittestnogenericservices.testmessage message =
      unittestnogenericservices.testmessage.newbuilder()
        .seta(123)
        .setextension(unittestnogenericservices.testextension, 456)
        .build();
    assertequals(123, message.geta());
    assertequals(1, unittestnogenericservices.testenum.foo.getnumber());

    // build a list of the class names nested in unittestnogenericservices.
    string outername = "google.protobuf.no_generic_services_test." +
                       "unittestnogenericservices";
    class<?> outerclass = class.forname(outername);

    set<string> innerclassnames = new hashset<string>();
    for (class<?> innerclass : outerclass.getclasses()) {
      string fullname = innerclass.getname();
      // figure out the unqualified name of the inner class.
      // note:  surprisingly, the full name of an inner class will be separated
      //   from the outer class name by a '$' rather than a '.'.  this is not
      //   mentioned in the documentation for java.lang.class.  i don't want to
      //   make assumptions, so i'm just going to accept any character as the
      //   separator.
      asserttrue(fullname.startswith(outername));

      if (!service.class.isassignablefrom(innerclass) &&
          !message.class.isassignablefrom(innerclass) &&
          !protocolmessageenum.class.isassignablefrom(innerclass)) {
        // ignore any classes not generated by the base code generator.
        continue;
      }

      innerclassnames.add(fullname.substring(outername.length() + 1));
    }

    // no service class should have been generated.
    asserttrue(innerclassnames.contains("testmessage"));
    asserttrue(innerclassnames.contains("testenum"));
    assertfalse(innerclassnames.contains("testservice"));

    // but descriptors are there.
    filedescriptor file = unittestnogenericservices.getdescriptor();
    assertequals(1, file.getservices().size());
    assertequals("testservice", file.getservices().get(0).getname());
    assertequals(1, file.getservices().get(0).getmethods().size());
    assertequals("foo",
        file.getservices().get(0).getmethods().get(0).getname());
  }

  // =================================================================

  /**
   * wrapscallback() is an easymock argument predicate.  wrapscallback(c)
   * matches a callback if calling that callback causes c to be called.
   * in other words, c wraps the given callback.
   */
  private <type extends message> rpccallback<type> wrapscallback(
      mockcallback<?> callback) {
    easymock.reportmatcher(new wrapscallback(callback));
    return null;
  }

  /** the parameter to wrapscallback() must be a mockcallback. */
  private static class mockcallback<type extends message>
      implements rpccallback<type> {
    private boolean called = false;

    public boolean iscalled() { return called; }

    public void reset() { called = false; }
    public void run(type message) { called = true; }
  }

  /** implementation of the wrapscallback() argument matcher. */
  private static class wrapscallback implements iargumentmatcher {
    private mockcallback<?> callback;

    public wrapscallback(mockcallback<?> callback) {
      this.callback = callback;
    }

    @suppresswarnings("unchecked")
    public boolean matches(object actual) {
      if (!(actual instanceof rpccallback)) {
        return false;
      }
      rpccallback actualcallback = (rpccallback)actual;

      callback.reset();
      actualcallback.run(null);
      return callback.iscalled();
    }

    public void appendto(stringbuffer buffer) {
      buffer.append("wrapscallback(mockcallback)");
    }
  }
}
