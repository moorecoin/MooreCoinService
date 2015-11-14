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

/**
 * <p>an {@code rpccontroller} mediates a single method call.  the primary
 * purpose of the controller is to provide a way to manipulate settings
 * specific to the rpc implementation and to find out about rpc-level errors.
 *
 * <p>starting with version 2.3.0, rpc implementations should not try to build
 * on this, but should instead provide code generator plugins which generate
 * code specific to the particular rpc implementation.  this way the generated
 * code can be more appropriate for the implementation in use and can avoid
 * unnecessary layers of indirection.
 *
 * <p>the methods provided by the {@code rpccontroller} interface are intended
 * to be a "least common denominator" set of features which we expect all
 * implementations to support.  specific implementations may provide more
 * advanced features (e.g. deadline propagation).
 *
 * @author kenton@google.com kenton varda
 */
public interface rpccontroller {
  // -----------------------------------------------------------------
  // these calls may be made from the client side only.  their results
  // are undefined on the server side (may throw runtimeexceptions).

  /**
   * resets the rpccontroller to its initial state so that it may be reused in
   * a new call.  this can be called from the client side only.  it must not
   * be called while an rpc is in progress.
   */
  void reset();

  /**
   * after a call has finished, returns true if the call failed.  the possible
   * reasons for failure depend on the rpc implementation.  {@code failed()}
   * most only be called on the client side, and must not be called before a
   * call has finished.
   */
  boolean failed();

  /**
   * if {@code failed()} is {@code true}, returns a human-readable description
   * of the error.
   */
  string errortext();

  /**
   * advises the rpc system that the caller desires that the rpc call be
   * canceled.  the rpc system may cancel it immediately, may wait awhile and
   * then cancel it, or may not even cancel the call at all.  if the call is
   * canceled, the "done" callback will still be called and the rpccontroller
   * will indicate that the call failed at that time.
   */
  void startcancel();

  // -----------------------------------------------------------------
  // these calls may be made from the server side only.  their results
  // are undefined on the client side (may throw runtimeexceptions).

  /**
   * causes {@code failed()} to return true on the client side.  {@code reason}
   * will be incorporated into the message returned by {@code errortext()}.
   * if you find you need to return machine-readable information about
   * failures, you should incorporate it into your response protocol buffer
   * and should not call {@code setfailed()}.
   */
  void setfailed(string reason);

  /**
   * if {@code true}, indicates that the client canceled the rpc, so the server
   * may as well give up on replying to it.  this method must be called on the
   * server side only.  the server should still call the final "done" callback.
   */
  boolean iscanceled();

  /**
   * asks that the given callback be called when the rpc is canceled.  the
   * parameter passed to the callback will always be {@code null}.  the
   * callback will always be called exactly once.  if the rpc completes without
   * being canceled, the callback will be called after completion.  if the rpc
   * has already been canceled when notifyoncancel() is called, the callback
   * will be called immediately.
   *
   * <p>{@code notifyoncancel()} must be called no more than once per request.
   * it must be called on the server side only.
   */
  void notifyoncancel(rpccallback<object> callback);
}
