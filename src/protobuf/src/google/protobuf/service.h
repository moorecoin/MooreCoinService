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

// author: kenton@google.com (kenton varda)
//  based on original protocol buffers design by
//  sanjay ghemawat, jeff dean, and others.
//
// deprecated:  this module declares the abstract interfaces underlying proto2
// rpc services.  these are intented to be independent of any particular rpc
// implementation, so that proto2 services can be used on top of a variety
// of implementations.  starting with version 2.3.0, rpc implementations should
// not try to build on these, but should instead provide code generator plugins
// which generate code specific to the particular rpc implementation.  this way
// the generated code can be more appropriate for the implementation in use
// and can avoid unnecessary layers of indirection.
//
//
// when you use the protocol compiler to compile a service definition, it
// generates two classes:  an abstract interface for the service (with
// methods matching the service definition) and a "stub" implementation.
// a stub is just a type-safe wrapper around an rpcchannel which emulates a
// local implementation of the service.
//
// for example, the service definition:
//   service myservice {
//     rpc foo(myrequest) returns(myresponse);
//   }
// will generate abstract interface "myservice" and class "myservice::stub".
// you could implement a myservice as follows:
//   class myserviceimpl : public myservice {
//    public:
//     myserviceimpl() {}
//     ~myserviceimpl() {}
//
//     // implements myservice ---------------------------------------
//
//     void foo(google::protobuf::rpccontroller* controller,
//              const myrequest* request,
//              myresponse* response,
//              closure* done) {
//       // ... read request and fill in response ...
//       done->run();
//     }
//   };
// you would then register an instance of myserviceimpl with your rpc server
// implementation.  (how to do that depends on the implementation.)
//
// to call a remote myserviceimpl, first you need an rpcchannel connected to it.
// how to construct a channel depends, again, on your rpc implementation.
// here we use a hypothentical "myrpcchannel" as an example:
//   myrpcchannel channel("rpc:hostname:1234/myservice");
//   myrpccontroller controller;
//   myserviceimpl::stub stub(&channel);
//   foorequest request;
//   foorespnose response;
//
//   // ... fill in request ...
//
//   stub.foo(&controller, request, &response, newcallback(handleresponse));
//
// on thread-safety:
//
// different rpc implementations may make different guarantees about what
// threads they may run callbacks on, and what threads the application is
// allowed to use to call the rpc system.  portable software should be ready
// for callbacks to be called on any thread, but should not try to call the
// rpc system from any thread except for the ones on which it received the
// callbacks.  realistically, though, simple software will probably want to
// use a single-threaded rpc system while high-end software will want to
// use multiple threads.  rpc implementations should provide multiple
// choices.

#ifndef google_protobuf_service_h__
#define google_protobuf_service_h__

#include <string>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {

// defined in this file.
class service;
class rpccontroller;
class rpcchannel;

// defined in other files.
class descriptor;            // descriptor.h
class servicedescriptor;     // descriptor.h
class methoddescriptor;      // descriptor.h
class message;               // message.h

// abstract base interface for protocol-buffer-based rpc services.  services
// themselves are abstract interfaces (implemented either by servers or as
// stubs), but they subclass this base interface.  the methods of this
// interface can be used to call the methods of the service without knowing
// its exact type at compile time (analogous to reflection).
class libprotobuf_export service {
 public:
  inline service() {}
  virtual ~service();

  // when constructing a stub, you may pass stub_owns_channel as the second
  // parameter to the constructor to tell it to delete its rpcchannel when
  // destroyed.
  enum channelownership {
    stub_owns_channel,
    stub_doesnt_own_channel
  };

  // get the servicedescriptor describing this service and its methods.
  virtual const servicedescriptor* getdescriptor() = 0;

  // call a method of the service specified by methoddescriptor.  this is
  // normally implemented as a simple switch() that calls the standard
  // definitions of the service's methods.
  //
  // preconditions:
  // * method->service() == getdescriptor()
  // * request and response are of the exact same classes as the objects
  //   returned by getrequestprototype(method) and
  //   getresponseprototype(method).
  // * after the call has started, the request must not be modified and the
  //   response must not be accessed at all until "done" is called.
  // * "controller" is of the correct type for the rpc implementation being
  //   used by this service.  for stubs, the "correct type" depends on the
  //   rpcchannel which the stub is using.  server-side service
  //   implementations are expected to accept whatever type of rpccontroller
  //   the server-side rpc implementation uses.
  //
  // postconditions:
  // * "done" will be called when the method is complete.  this may be
  //   before callmethod() returns or it may be at some point in the future.
  // * if the rpc succeeded, "response" contains the response returned by
  //   the server.
  // * if the rpc failed, "response"'s contents are undefined.  the
  //   rpccontroller can be queried to determine if an error occurred and
  //   possibly to get more information about the error.
  virtual void callmethod(const methoddescriptor* method,
                          rpccontroller* controller,
                          const message* request,
                          message* response,
                          closure* done) = 0;

  // callmethod() requires that the request and response passed in are of a
  // particular subclass of message.  getrequestprototype() and
  // getresponseprototype() get the default instances of these required types.
  // you can then call message::new() on these instances to construct mutable
  // objects which you can then pass to callmethod().
  //
  // example:
  //   const methoddescriptor* method =
  //     service->getdescriptor()->findmethodbyname("foo");
  //   message* request  = stub->getrequestprototype (method)->new();
  //   message* response = stub->getresponseprototype(method)->new();
  //   request->parsefromstring(input);
  //   service->callmethod(method, *request, response, callback);
  virtual const message& getrequestprototype(
    const methoddescriptor* method) const = 0;
  virtual const message& getresponseprototype(
    const methoddescriptor* method) const = 0;

 private:
  google_disallow_evil_constructors(service);
};

// an rpccontroller mediates a single method call.  the primary purpose of
// the controller is to provide a way to manipulate settings specific to the
// rpc implementation and to find out about rpc-level errors.
//
// the methods provided by the rpccontroller interface are intended to be a
// "least common denominator" set of features which we expect all
// implementations to support.  specific implementations may provide more
// advanced features (e.g. deadline propagation).
class libprotobuf_export rpccontroller {
 public:
  inline rpccontroller() {}
  virtual ~rpccontroller();

  // client-side methods ---------------------------------------------
  // these calls may be made from the client side only.  their results
  // are undefined on the server side (may crash).

  // resets the rpccontroller to its initial state so that it may be reused in
  // a new call.  must not be called while an rpc is in progress.
  virtual void reset() = 0;

  // after a call has finished, returns true if the call failed.  the possible
  // reasons for failure depend on the rpc implementation.  failed() must not
  // be called before a call has finished.  if failed() returns true, the
  // contents of the response message are undefined.
  virtual bool failed() const = 0;

  // if failed() is true, returns a human-readable description of the error.
  virtual string errortext() const = 0;

  // advises the rpc system that the caller desires that the rpc call be
  // canceled.  the rpc system may cancel it immediately, may wait awhile and
  // then cancel it, or may not even cancel the call at all.  if the call is
  // canceled, the "done" callback will still be called and the rpccontroller
  // will indicate that the call failed at that time.
  virtual void startcancel() = 0;

  // server-side methods ---------------------------------------------
  // these calls may be made from the server side only.  their results
  // are undefined on the client side (may crash).

  // causes failed() to return true on the client side.  "reason" will be
  // incorporated into the message returned by errortext().  if you find
  // you need to return machine-readable information about failures, you
  // should incorporate it into your response protocol buffer and should
  // not call setfailed().
  virtual void setfailed(const string& reason) = 0;

  // if true, indicates that the client canceled the rpc, so the server may
  // as well give up on replying to it.  the server should still call the
  // final "done" callback.
  virtual bool iscanceled() const = 0;

  // asks that the given callback be called when the rpc is canceled.  the
  // callback will always be called exactly once.  if the rpc completes without
  // being canceled, the callback will be called after completion.  if the rpc
  // has already been canceled when notifyoncancel() is called, the callback
  // will be called immediately.
  //
  // notifyoncancel() must be called no more than once per request.
  virtual void notifyoncancel(closure* callback) = 0;

 private:
  google_disallow_evil_constructors(rpccontroller);
};

// abstract interface for an rpc channel.  an rpcchannel represents a
// communication line to a service which can be used to call that service's
// methods.  the service may be running on another machine.  normally, you
// should not call an rpcchannel directly, but instead construct a stub service
// wrapping it.  example:
//   rpcchannel* channel = new myrpcchannel("remotehost.example.com:1234");
//   myservice* service = new myservice::stub(channel);
//   service->mymethod(request, &response, callback);
class libprotobuf_export rpcchannel {
 public:
  inline rpcchannel() {}
  virtual ~rpcchannel();

  // call the given method of the remote service.  the signature of this
  // procedure looks the same as service::callmethod(), but the requirements
  // are less strict in one important way:  the request and response objects
  // need not be of any specific class as long as their descriptors are
  // method->input_type() and method->output_type().
  virtual void callmethod(const methoddescriptor* method,
                          rpccontroller* controller,
                          const message* request,
                          message* response,
                          closure* done) = 0;

 private:
  google_disallow_evil_constructors(rpcchannel);
};

}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_service_h__
