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

import java.util.abstractlist;
import java.util.arraylist;
import java.util.collection;
import java.util.collections;
import java.util.list;

/**
 * {@code repeatedfieldbuilder} implements a structure that a protocol
 * message uses to hold a repeated field of other protocol messages. it supports
 * the classical use case of adding immutable {@link message}'s to the
 * repeated field and is highly optimized around this (no extra memory
 * allocations and sharing of immutable arrays).
 * <br>
 * it also supports the additional use case of adding a {@link message.builder}
 * to the repeated field and deferring conversion of that {@code builder}
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
public class repeatedfieldbuilder
    <mtype extends generatedmessage,
     btype extends generatedmessage.builder,
     itype extends messageorbuilder>
    implements generatedmessage.builderparent {

  // parent to send changes to.
  private generatedmessage.builderparent parent;

  // list of messages. never null. it may be immutable, in which case
  // ismessageslistimmutable will be true. see note below.
  private list<mtype> messages;

  // whether messages is an mutable array that can be modified.
  private boolean ismessageslistmutable;

  // list of builders. may be null, in which case, no nested builders were
  // created. if not null, entries represent the builder for that index.
  private list<singlefieldbuilder<mtype, btype, itype>> builders;

  // here are the invariants for messages and builders:
  // 1. messages is never null and its count corresponds to the number of items
  //    in the repeated field.
  // 2. if builders is non-null, messages and builders must always
  //    contain the same number of items.
  // 3. entries in either array can be null, but for any index, there must be
  //    either a message in messages or a builder in builders.
  // 4. if the builder at an index is non-null, the builder is
  //    authoritative. this is the case where a builder was set on the index.
  //    any message in the messages array must be ignored.
  // t. if the builder at an index is null, the message in the messages
  //    list is authoritative. this is the case where a message (not a builder)
  //    was set directly for an index.

  // indicates that we've built a message and so we are now obligated
  // to dispatch dirty invalidations. see generatedmessage.builderlistener.
  private boolean isclean;

  // a view of this builder that exposes a list interface of messages. this is
  // initialized on demand. this is fully backed by this object and all changes
  // are reflected in it. access to any item converts it to a message if it
  // was a builder.
  private messageexternallist<mtype, btype, itype> externalmessagelist;

  // a view of this builder that exposes a list interface of builders. this is
  // initialized on demand. this is fully backed by this object and all changes
  // are reflected in it. access to any item converts it to a builder if it
  // was a message.
  private builderexternallist<mtype, btype, itype> externalbuilderlist;

  // a view of this builder that exposes a list interface of the interface
  // implemented by messages and builders. this is initialized on demand. this
  // is fully backed by this object and all changes are reflected in it.
  // access to any item returns either a builder or message depending on
  // what is most efficient.
  private messageorbuilderexternallist<mtype, btype, itype>
      externalmessageorbuilderlist;

  /**
   * constructs a new builder with an empty list of messages.
   *
   * @param messages the current list of messages
   * @param ismessageslistmutable whether the messages list is mutable
   * @param parent a listener to notify of changes
   * @param isclean whether the builder is initially marked clean
   */
  public repeatedfieldbuilder(
      list<mtype> messages,
      boolean ismessageslistmutable,
      generatedmessage.builderparent parent,
      boolean isclean) {
    this.messages = messages;
    this.ismessageslistmutable = ismessageslistmutable;
    this.parent = parent;
    this.isclean = isclean;
  }

  public void dispose() {
    // null out parent so we stop sending it invalidations.
    parent = null;
  }

  /**
   * ensures that the list of messages is mutable so it can be updated. if it's
   * immutable, a copy is made.
   */
  private void ensuremutablemessagelist() {
    if (!ismessageslistmutable) {
      messages = new arraylist<mtype>(messages);
      ismessageslistmutable = true;
    }
  }

  /**
   * ensures that the list of builders is not null. if it's null, the list is
   * created and initialized to be the same size as the messages list with
   * null entries.
   */
  private void ensurebuilders() {
    if (this.builders == null) {
      this.builders =
          new arraylist<singlefieldbuilder<mtype, btype, itype>>(
              messages.size());
      for (int i = 0; i < messages.size(); i++) {
        builders.add(null);
      }
    }
  }

  /**
   * gets the count of items in the list.
   *
   * @return the count of items in the list.
   */
  public int getcount() {
    return messages.size();
  }

  /**
   * gets whether the list is empty.
   *
   * @return whether the list is empty
   */
  public boolean isempty() {
    return messages.isempty();
  }

  /**
   * get the message at the specified index. if the message is currently stored
   * as a {@code builder}, it is converted to a {@code message} by
   * calling {@link message.builder#buildpartial} on it.
   *
   * @param index the index of the message to get
   * @return the message for the specified index
   */
  public mtype getmessage(int index) {
    return getmessage(index, false);
  }

  /**
   * get the message at the specified index. if the message is currently stored
   * as a {@code builder}, it is converted to a {@code message} by
   * calling {@link message.builder#buildpartial} on it.
   *
   * @param index the index of the message to get
   * @param forbuild this is being called for build so we want to make sure
   *     we singlefieldbuilder.build to send dirty invalidations
   * @return the message for the specified index
   */
  private mtype getmessage(int index, boolean forbuild) {
    if (this.builders == null) {
      // we don't have any builders -- return the current message.
      // this is the case where no builder was created, so we must have a
      // message.
      return messages.get(index);
    }

    singlefieldbuilder<mtype, btype, itype> builder = builders.get(index);
    if (builder == null) {
      // we don't have a builder -- return the current message.
      // this is the case where no builder was created for the entry at index,
      // so we must have a message.
      return messages.get(index);

    } else {
      return forbuild ? builder.build() : builder.getmessage();
    }
  }

  /**
   * gets a builder for the specified index. if no builder has been created for
   * that index, a builder is created on demand by calling
   * {@link message#tobuilder}.
   *
   * @param index the index of the message to get
   * @return the builder for that index
   */
  public btype getbuilder(int index) {
    ensurebuilders();
    singlefieldbuilder<mtype, btype, itype> builder = builders.get(index);
    if (builder == null) {
      mtype message = messages.get(index);
      builder = new singlefieldbuilder<mtype, btype, itype>(
          message, this, isclean);
      builders.set(index, builder);
    }
    return builder.getbuilder();
  }

  /**
   * gets the base class interface for the specified index. this may either be
   * a builder or a message. it will return whatever is more efficient.
   *
   * @param index the index of the message to get
   * @return the message or builder for the index as the base class interface
   */
  @suppresswarnings("unchecked")
  public itype getmessageorbuilder(int index) {
    if (this.builders == null) {
      // we don't have any builders -- return the current message.
      // this is the case where no builder was created, so we must have a
      // message.
      return (itype) messages.get(index);
    }

    singlefieldbuilder<mtype, btype, itype> builder = builders.get(index);
    if (builder == null) {
      // we don't have a builder -- return the current message.
      // this is the case where no builder was created for the entry at index,
      // so we must have a message.
      return (itype) messages.get(index);

    } else {
      return builder.getmessageorbuilder();
    }
  }

  /**
   * sets a  message at the specified index replacing the existing item at
   * that index.
   *
   * @param index the index to set.
   * @param message the message to set
   * @return the builder
   */
  public repeatedfieldbuilder<mtype, btype, itype> setmessage(
      int index, mtype message) {
    if (message == null) {
      throw new nullpointerexception();
    }
    ensuremutablemessagelist();
    messages.set(index, message);
    if (builders != null) {
      singlefieldbuilder<mtype, btype, itype> entry =
          builders.set(index, null);
      if (entry != null) {
        entry.dispose();
      }
    }
    onchanged();
    incrementmodcounts();
    return this;
  }

  /**
   * appends the specified element to the end of this list.
   *
   * @param message the message to add
   * @return the builder
   */
  public repeatedfieldbuilder<mtype, btype, itype> addmessage(
      mtype message) {
    if (message == null) {
      throw new nullpointerexception();
    }
    ensuremutablemessagelist();
    messages.add(message);
    if (builders != null) {
      builders.add(null);
    }
    onchanged();
    incrementmodcounts();
    return this;
  }

  /**
   * inserts the specified message at the specified position in this list.
   * shifts the element currently at that position (if any) and any subsequent
   * elements to the right (adds one to their indices).
   *
   * @param index the index at which to insert the message
   * @param message the message to add
   * @return the builder
   */
  public repeatedfieldbuilder<mtype, btype, itype> addmessage(
      int index, mtype message) {
    if (message == null) {
      throw new nullpointerexception();
    }
    ensuremutablemessagelist();
    messages.add(index, message);
    if (builders != null) {
      builders.add(index, null);
    }
    onchanged();
    incrementmodcounts();
    return this;
  }

  /**
   * appends all of the messages in the specified collection to the end of
   * this list, in the order that they are returned by the specified
   * collection's iterator.
   *
   * @param values the messages to add
   * @return the builder
   */
  public repeatedfieldbuilder<mtype, btype, itype> addallmessages(
      iterable<? extends mtype> values) {
    for (final mtype value : values) {
      if (value == null) {
        throw new nullpointerexception();
      }
    }
    if (values instanceof collection) {
      @suppresswarnings("unchecked") final
      collection<mtype> collection = (collection<mtype>) values;
      if (collection.size() == 0) {
        return this;
      }
      ensuremutablemessagelist();
      for (mtype value : values) {
        addmessage(value);
      }
    } else {
      ensuremutablemessagelist();
      for (mtype value : values) {
        addmessage(value);
      }
    }
    onchanged();
    incrementmodcounts();
    return this;
  }

  /**
   * appends a new builder to the end of this list and returns the builder.
   *
   * @param message the message to add which is the basis of the builder
   * @return the new builder
   */
  public btype addbuilder(mtype message) {
    ensuremutablemessagelist();
    ensurebuilders();
    singlefieldbuilder<mtype, btype, itype> builder =
        new singlefieldbuilder<mtype, btype, itype>(
            message, this, isclean);
    messages.add(null);
    builders.add(builder);
    onchanged();
    incrementmodcounts();
    return builder.getbuilder();
  }

  /**
   * inserts a new builder at the specified position in this list.
   * shifts the element currently at that position (if any) and any subsequent
   * elements to the right (adds one to their indices).
   *
   * @param index the index at which to insert the builder
   * @param message the message to add which is the basis of the builder
   * @return the builder
   */
  public btype addbuilder(int index, mtype message) {
    ensuremutablemessagelist();
    ensurebuilders();
    singlefieldbuilder<mtype, btype, itype> builder =
        new singlefieldbuilder<mtype, btype, itype>(
            message, this, isclean);
    messages.add(index, null);
    builders.add(index, builder);
    onchanged();
    incrementmodcounts();
    return builder.getbuilder();
  }

  /**
   * removes the element at the specified position in this list. shifts any
   * subsequent elements to the left (subtracts one from their indices).
   * returns the element that was removed from the list.
   *
   * @param index the index at which to remove the message
   */
  public void remove(int index) {
    ensuremutablemessagelist();
    messages.remove(index);
    if (builders != null) {
      singlefieldbuilder<mtype, btype, itype> entry =
          builders.remove(index);
      if (entry != null) {
        entry.dispose();
      }
    }
    onchanged();
    incrementmodcounts();
  }

  /**
   * removes all of the elements from this list.
   * the list will be empty after this call returns.
   */
  public void clear() {
    messages = collections.emptylist();
    ismessageslistmutable = false;
    if (builders != null) {
      for (singlefieldbuilder<mtype, btype, itype> entry :
          builders) {
        if (entry != null) {
          entry.dispose();
        }
      }
      builders = null;
    }
    onchanged();
    incrementmodcounts();
  }

  /**
   * builds the list of messages from the builder and returns them.
   *
   * @return an immutable list of messages
   */
  public list<mtype> build() {
    // now that build has been called, we are required to dispatch
    // invalidations.
    isclean = true;

    if (!ismessageslistmutable && builders == null) {
      // we still have an immutable list and we never created a builder.
      return messages;
    }

    boolean allmessagesinsync = true;
    if (!ismessageslistmutable) {
      // we still have an immutable list. let's see if any of them are out
      // of sync with their builders.
      for (int i = 0; i < messages.size(); i++) {
        message message = messages.get(i);
        singlefieldbuilder<mtype, btype, itype> builder = builders.get(i);
        if (builder != null) {
          if (builder.build() != message) {
            allmessagesinsync = false;
            break;
          }
        }
      }
      if (allmessagesinsync) {
        // immutable list is still in sync.
        return messages;
      }
    }

    // need to make sure messages is up to date
    ensuremutablemessagelist();
    for (int i = 0; i < messages.size(); i++) {
      messages.set(i, getmessage(i, true));
    }

    // we're going to return our list as immutable so we mark that we can
    // no longer update it.
    messages = collections.unmodifiablelist(messages);
    ismessageslistmutable = false;
    return messages;
  }

  /**
   * gets a view of the builder as a list of messages. the returned list is live
   * and will reflect any changes to the underlying builder.
   *
   * @return the messages in the list
   */
  public list<mtype> getmessagelist() {
    if (externalmessagelist == null) {
      externalmessagelist =
          new messageexternallist<mtype, btype, itype>(this);
    }
    return externalmessagelist;
  }

  /**
   * gets a view of the builder as a list of builders. this returned list is
   * live and will reflect any changes to the underlying builder.
   *
   * @return the builders in the list
   */
  public list<btype> getbuilderlist() {
    if (externalbuilderlist == null) {
      externalbuilderlist =
          new builderexternallist<mtype, btype, itype>(this);
    }
    return externalbuilderlist;
  }

  /**
   * gets a view of the builder as a list of messageorbuilders. this returned
   * list is live and will reflect any changes to the underlying builder.
   *
   * @return the builders in the list
   */
  public list<itype> getmessageorbuilderlist() {
    if (externalmessageorbuilderlist == null) {
      externalmessageorbuilderlist =
          new messageorbuilderexternallist<mtype, btype, itype>(this);
    }
    return externalmessageorbuilderlist;
  }

  /**
   * called when a the builder or one of its nested children has changed
   * and any parent should be notified of its invalidation.
   */
  private void onchanged() {
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

  /**
   * increments the mod counts so that an concurrentmodificationexception can
   * be thrown if calling code tries to modify the builder while its iterating
   * the list.
   */
  private void incrementmodcounts() {
    if (externalmessagelist != null) {
      externalmessagelist.incrementmodcount();
    }
    if (externalbuilderlist != null) {
      externalbuilderlist.incrementmodcount();
    }
    if (externalmessageorbuilderlist != null) {
      externalmessageorbuilderlist.incrementmodcount();
    }
  }

  /**
   * provides a live view of the builder as a list of messages.
   *
   * @param <mtype> the type of message for the field
   * @param <btype> the type of builder for the field
   * @param <itype> the common interface for the message and the builder
   */
  private static class messageexternallist<
      mtype extends generatedmessage,
      btype extends generatedmessage.builder,
      itype extends messageorbuilder>
      extends abstractlist<mtype> implements list<mtype> {

    repeatedfieldbuilder<mtype, btype, itype> builder;

    messageexternallist(
        repeatedfieldbuilder<mtype, btype, itype> builder) {
      this.builder = builder;
    }

    public int size() {
      return this.builder.getcount();
    }

    public mtype get(int index) {
      return builder.getmessage(index);
    }

    void incrementmodcount() {
      modcount++;
    }
  }

  /**
   * provides a live view of the builder as a list of builders.
   *
   * @param <mtype> the type of message for the field
   * @param <btype> the type of builder for the field
   * @param <itype> the common interface for the message and the builder
   */
  private static class builderexternallist<
      mtype extends generatedmessage,
      btype extends generatedmessage.builder,
      itype extends messageorbuilder>
      extends abstractlist<btype> implements list<btype> {

    repeatedfieldbuilder<mtype, btype, itype> builder;

    builderexternallist(
        repeatedfieldbuilder<mtype, btype, itype> builder) {
      this.builder = builder;
    }

    public int size() {
      return this.builder.getcount();
    }

    public btype get(int index) {
      return builder.getbuilder(index);
    }

    void incrementmodcount() {
      modcount++;
    }
  }

  /**
   * provides a live view of the builder as a list of builders.
   *
   * @param <mtype> the type of message for the field
   * @param <btype> the type of builder for the field
   * @param <itype> the common interface for the message and the builder
   */
  private static class messageorbuilderexternallist<
      mtype extends generatedmessage,
      btype extends generatedmessage.builder,
      itype extends messageorbuilder>
      extends abstractlist<itype> implements list<itype> {

    repeatedfieldbuilder<mtype, btype, itype> builder;

    messageorbuilderexternallist(
        repeatedfieldbuilder<mtype, btype, itype> builder) {
      this.builder = builder;
    }

    public int size() {
      return this.builder.getcount();
    }

    public itype get(int index) {
      return builder.getmessageorbuilder(index);
    }

    void incrementmodcount() {
      modcount++;
    }
  }
}
