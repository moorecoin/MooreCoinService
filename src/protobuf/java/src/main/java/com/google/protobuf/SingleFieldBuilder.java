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
 * {@code singlefieldbuilder} implements a structure that a protocol
 * message uses to hold a single field of another protocol message. it supports
 * the classical use case of setting an immutable {@link message} as the value
 * of the field and is highly optimized around this.
 * <br>
 * it also supports the additional use case of setting a {@link message.builder}
 * as the field and deferring conversion of that {@code builder}
 * to an immutable {@code message}. in this way, it's possible to maintain
 * a tree of {@code builder}'s that acts as a fully read/write data
 * structure.
 * <br>
 * logically, one can think of a tree of builders as converting the entire tree
 * to messages when build is called on the root or when any method is called
 * that desires a message instead of a builder. in terms of the implementation,
 * the {@code singlefieldbuilder} and {@code repeatedfieldbuilder}
 * classes cache messages that were created so that messages only need to be
 * created when some change occured in its builder or a builder for one of its
 * descendants.
 *
 * @param <mtype> the type of message for the field
 * @param <btype> the type of builder for the field
 * @param <itype> the common interface for the message and the builder
 *
 * @author jonp@google.com (jon perlow)
 */
public class singlefieldbuilder
    <mtype extends generatedmessage,
     btype extends generatedmessage.builder,
     itype extends messageorbuilder>
    implements generatedmessage.builderparent {

  // parent to send changes to.
  private generatedmessage.builderparent parent;

  // invariant: one of builder or message fields must be non-null.

  // if set, this is the case where we are backed by a builder. in this case,
  // message field represents a cached message for the builder (or null if
  // there is no cached message).
  private btype builder;

  // if builder is non-null, this represents a cached message from the builder.
  // if builder is null, this is the authoritative message for the field.
  private mtype message;

  // indicates that we've built a message and so we are now obligated
  // to dispatch dirty invalidations. see generatedmessage.builderlistener.
  private boolean isclean;

  public singlefieldbuilder(
      mtype message,
      generatedmessage.builderparent parent,
      boolean isclean) {
    if (message == null) {
      throw new nullpointerexception();
    }
    this.message = message;
    this.parent = parent;
    this.isclean = isclean;
  }

  public void dispose() {
    // null out parent so we stop sending it invalidations.
    parent = null;
  }

  /**
   * get the message for the field. if the message is currently stored
   * as a {@code builder}, it is converted to a {@code message} by
   * calling {@link message.builder#buildpartial} on it. if no message has
   * been set, returns the default instance of the message.
   *
   * @return the message for the field
   */
  @suppresswarnings("unchecked")
  public mtype getmessage() {
    if (message == null) {
      // if message is null, the invariant is that we must be have a builder.
      message = (mtype) builder.buildpartial();
    }
    return message;
  }

  /**
   * builds the message and returns it.
   *
   * @return the message
   */
  public mtype build() {
    // now that build has been called, we are required to dispatch
    // invalidations.
    isclean = true;
    return getmessage();
  }

  /**
   * gets a builder for the field. if no builder has been created yet, a
   * builder is created on demand by calling {@link message#tobuilder}.
   *
   * @return the builder for the field
   */
  @suppresswarnings("unchecked")
  public btype getbuilder() {
    if (builder == null) {
      // builder.mergefrom() on a fresh builder
      // does not create any sub-objects with independent clean/dirty states,
      // therefore setting the builder itself to clean without actually calling
      // build() cannot break any invariants.
      builder = (btype) message.newbuilderfortype(this);
      builder.mergefrom(message); // no-op if message is the default message
      builder.markclean();
    }
    return builder;
  }

  /**
   * gets the base class interface for the field. this may either be a builder
   * or a message. it will return whatever is more efficient.
   *
   * @return the message or builder for the field as the base class interface
   */
  @suppresswarnings("unchecked")
  public itype getmessageorbuilder() {
    if (builder != null) {
      return  (itype) builder;
    } else {
      return (itype) message;
    }
  }

  /**
   * sets a  message for the field replacing any existing value.
   *
   * @param message the message to set
   * @return the builder
   */
  public singlefieldbuilder<mtype, btype, itype> setmessage(
      mtype message) {
    if (message == null) {
      throw new nullpointerexception();
    }
    this.message = message;
    if (builder != null) {
      builder.dispose();
      builder = null;
    }
    onchanged();
    return this;
  }

  /**
   * merges the field from another field.
   *
   * @param value the value to merge from
   * @return the builder
   */
  public singlefieldbuilder<mtype, btype, itype> mergefrom(
      mtype value) {
    if (builder == null && message == message.getdefaultinstancefortype()) {
      message = value;
    } else {
      getbuilder().mergefrom(value);
    }
    onchanged();
    return this;
  }

  /**
   * clears the value of the field.
   *
   * @return the builder
   */
  @suppresswarnings("unchecked")
  public singlefieldbuilder<mtype, btype, itype> clear() {
    message = (mtype) (message != null ?
        message.getdefaultinstancefortype() :
        builder.getdefaultinstancefortype());
    if (builder != null) {
      builder.dispose();
      builder = null;
    }
    onchanged();
    return this;
  }

  /**
   * called when a the builder or one of its nested children has changed
   * and any parent should be notified of its invalidation.
   */
  private void onchanged() {
    // if builder is null, this is the case where onchanged is being called
    // from setmessage or clear.
    if (builder != null) {
      message = null;
    }
    if (isclean && parent != null) {
      parent.markdirty();

      // don't keep dispatching invalidations until build is called again.
      isclean = false;
    }
  }

  //@override (java 1.6 override semantics, but we must support 1.5)
  public void markdirty() {
    onchanged();
  }
}
