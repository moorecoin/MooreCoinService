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

import java.io.ioexception;
import java.io.inputstream;
import java.io.outputstream;
import java.io.unsupportedencodingexception;
import java.io.bytearrayinputstream;
import java.nio.bytebuffer;
import java.util.arraylist;
import java.util.arrays;
import java.util.iterator;
import java.util.list;
import java.util.nosuchelementexception;
import java.util.stack;

/**
 * class to represent {@code bytestrings} formed by concatenation of other
 * bytestrings, without copying the data in the pieces. the concatenation is
 * represented as a tree whose leaf nodes are each a {@link literalbytestring}.
 *
 * <p>most of the operation here is inspired by the now-famous paper <a
 * href="http://www.cs.ubc.ca/local/reading/proceedings/spe91-95/spe/vol25/issue12/spe986.pdf">
 * bap95 </a> ropes: an alternative to strings hans-j. boehm, russ atkinson and
 * michael plass
 *
 * <p>the algorithms described in the paper have been implemented for character
 * strings in {@link com.google.common.string.rope} and in the c++ class {@code
 * cord.cc}.
 *
 * <p>fundamentally the rope algorithm represents the collection of pieces as a
 * binary tree. bap95 uses a fibonacci bound relating depth to a minimum
 * sequence length, sequences that are too short relative to their depth cause a
 * tree rebalance.  more precisely, a tree of depth d is "balanced" in the
 * terminology of bap95 if its length is at least f(d+2), where f(n) is the
 * n-the fibonacci number. thus for depths 0, 1, 2, 3, 4, 5,... we have minimum
 * lengths 1, 2, 3, 5, 8, 13,...
 *
 * @author carlanton@google.com (carl haverl)
 */
class ropebytestring extends bytestring {

  /**
   * bap95. let fn be the nth fibonacci number. a {@link ropebytestring} of
   * depth n is "balanced", i.e flat enough, if its length is at least fn+2,
   * e.g. a "balanced" {@link ropebytestring} of depth 1 must have length at
   * least 2, of depth 4 must have length >= 8, etc.
   *
   * <p>there's nothing special about using the fibonacci numbers for this, but
   * they are a reasonable sequence for encapsulating the idea that we are ok
   * with longer strings being encoded in deeper binary trees.
   *
   * <p>for 32-bit integers, this array has length 46.
   */
  private static final int[] minlengthbydepth;

  static {
    // dynamically generate the list of fibonacci numbers the first time this
    // class is accessed.
    list<integer> numbers = new arraylist<integer>();

    // we skip the first fibonacci number (1).  so instead of: 1 1 2 3 5 8 ...
    // we have: 1 2 3 5 8 ...
    int f1 = 1;
    int f2 = 1;

    // get all the values until we roll over.
    while (f2 > 0) {
      numbers.add(f2);
      int temp = f1 + f2;
      f1 = f2;
      f2 = temp;
    }

    // we include this here so that we can index this array to [x + 1] in the
    // loops below.
    numbers.add(integer.max_value);
    minlengthbydepth = new int[numbers.size()];
    for (int i = 0; i < minlengthbydepth.length; i++) {
      // unbox all the values
      minlengthbydepth[i] = numbers.get(i);
    }
  }

  private final int totallength;
  private final bytestring left;
  private final bytestring right;
  private final int leftlength;
  private final int treedepth;

  /**
   * create a new ropebytestring, which can be thought of as a new tree node, by
   * recording references to the two given strings.
   *
   * @param left  string on the left of this node, should have {@code size() >
   *              0}
   * @param right string on the right of this node, should have {@code size() >
   *              0}
   */
  private ropebytestring(bytestring left, bytestring right) {
    this.left = left;
    this.right = right;
    leftlength = left.size();
    totallength = leftlength + right.size();
    treedepth = math.max(left.gettreedepth(), right.gettreedepth()) + 1;
  }

  /**
   * concatenate the given strings while performing various optimizations to
   * slow the growth rate of tree depth and tree node count. the result is
   * either a {@link literalbytestring} or a {@link ropebytestring}
   * depending on which optimizations, if any, were applied.
   *
   * <p>small pieces of length less than {@link
   * bytestring#concatenate_by_copy_size} may be copied by value here, as in
   * bap95.  large pieces are referenced without copy.
   *
   * @param left  string on the left
   * @param right string on the right
   * @return concatenation representing the same sequence as the given strings
   */
  static bytestring concatenate(bytestring left, bytestring right) {
    bytestring result;
    ropebytestring leftrope =
        (left instanceof ropebytestring) ? (ropebytestring) left : null;
    if (right.size() == 0) {
      result = left;
    } else if (left.size() == 0) {
      result = right;
    } else {
      int newlength = left.size() + right.size();
      if (newlength < bytestring.concatenate_by_copy_size) {
        // optimization from bap95: for short (leaves in paper, but just short
        // here) total length, do a copy of data to a new leaf.
        result = concatenatebytes(left, right);
      } else if (leftrope != null
          && leftrope.right.size() + right.size() < concatenate_by_copy_size) {
        // optimization from bap95: as an optimization of the case where the
        // bytestring is constructed by repeated concatenate, recognize the case
        // where a short string is concatenated to a left-hand node whose
        // right-hand branch is short.  in the paper this applies to leaves, but
        // we just look at the length here. this has the advantage of shedding
        // references to unneeded data when substrings have been taken.
        //
        // when we recognize this case, we do a copy of the data and create a
        // new parent node so that the depth of the result is the same as the
        // given left tree.
        bytestring newright = concatenatebytes(leftrope.right, right);
        result = new ropebytestring(leftrope.left, newright);
      } else if (leftrope != null
          && leftrope.left.gettreedepth() > leftrope.right.gettreedepth()
          && leftrope.gettreedepth() > right.gettreedepth()) {
        // typically for concatenate-built strings the left-side is deeper than
        // the right.  this is our final attempt to concatenate without
        // increasing the tree depth.  we'll redo the the node on the rhs.  this
        // is yet another optimization for building the string by repeatedly
        // concatenating on the right.
        bytestring newright = new ropebytestring(leftrope.right, right);
        result = new ropebytestring(leftrope.left, newright);
      } else {
        // fine, we'll add a node and increase the tree depth--unless we
        // rebalance ;^)
        int newdepth = math.max(left.gettreedepth(), right.gettreedepth()) + 1;
        if (newlength >= minlengthbydepth[newdepth]) {
          // the tree is shallow enough, so don't rebalance
          result = new ropebytestring(left, right);
        } else {
          result = new balancer().balance(left, right);
        }
      }
    }
    return result;
  }

  /**
   * concatenates two strings by copying data values. this is called in a few
   * cases in order to reduce the growth of the number of tree nodes.
   *
   * @param left  string on the left
   * @param right string on the right
   * @return string formed by copying data bytes
   */
  private static literalbytestring concatenatebytes(bytestring left,
      bytestring right) {
    int leftsize = left.size();
    int rightsize = right.size();
    byte[] bytes = new byte[leftsize + rightsize];
    left.copyto(bytes, 0, 0, leftsize);
    right.copyto(bytes, 0, leftsize, rightsize);
    return new literalbytestring(bytes);  // constructor wraps bytes
  }

  /**
   * create a new ropebytestring for testing only while bypassing all the
   * defenses of {@link #concatenate(bytestring, bytestring)}. this allows
   * testing trees of specific structure. we are also able to insert empty
   * leaves, though these are dis-allowed, so that we can make sure the
   * implementation can withstand their presence.
   *
   * @param left  string on the left of this node
   * @param right string on the right of this node
   * @return an unsafe instance for testing only
   */
  static ropebytestring newinstancefortest(bytestring left, bytestring right) {
    return new ropebytestring(left, right);
  }

  /**
   * gets the byte at the given index.
   * throws {@link arrayindexoutofboundsexception} for backwards-compatibility
   * reasons although it would more properly be {@link
   * indexoutofboundsexception}.
   *
   * @param index index of byte
   * @return the value
   * @throws arrayindexoutofboundsexception {@code index} is < 0 or >= size
   */
  @override
  public byte byteat(int index) {
    if (index < 0) {
      throw new arrayindexoutofboundsexception("index < 0: " + index);
    }
    if (index > totallength) {
      throw new arrayindexoutofboundsexception(
          "index > length: " + index + ", " + totallength);
    }

    byte result;
    // find the relevant piece by recursive descent
    if (index < leftlength) {
      result = left.byteat(index);
    } else {
      result = right.byteat(index - leftlength);
    }
    return result;
  }

  @override
  public int size() {
    return totallength;
  }

  // =================================================================
  // pieces

  @override
  protected int gettreedepth() {
    return treedepth;
  }

  /**
   * determines if the tree is balanced according to bap95, which means the tree
   * is flat-enough with respect to the bounds. note that this definition of
   * balanced is one where sub-trees of balanced trees are not necessarily
   * balanced.
   *
   * @return true if the tree is balanced
   */
  @override
  protected boolean isbalanced() {
    return totallength >= minlengthbydepth[treedepth];
  }

  /**
   * takes a substring of this one. this involves recursive descent along the
   * left and right edges of the substring, and referencing any wholly contained
   * segments in between. any leaf nodes entirely uninvolved in the substring
   * will not be referenced by the substring.
   *
   * <p>substrings of {@code length < 2} should result in at most a single
   * recursive call chain, terminating at a leaf node. thus the result will be a
   * {@link literalbytestring}. {@link #ropebytestring(bytestring,
   * bytestring)}.
   *
   * @param beginindex start at this index
   * @param endindex   the last character is the one before this index
   * @return substring leaf node or tree
   */
  @override
  public bytestring substring(int beginindex, int endindex) {
    if (beginindex < 0) {
      throw new indexoutofboundsexception(
          "beginning index: " + beginindex + " < 0");
    }
    if (endindex > totallength) {
      throw new indexoutofboundsexception(
          "end index: " + endindex + " > " + totallength);
    }
    int substringlength = endindex - beginindex;
    if (substringlength < 0) {
      throw new indexoutofboundsexception(
          "beginning index larger than ending index: " + beginindex + ", "
              + endindex);
    }

    bytestring result;
    if (substringlength == 0) {
      // empty substring
      result = bytestring.empty;
    } else if (substringlength == totallength) {
      // the whole string
      result = this;
    } else {
      // proper substring
      if (endindex <= leftlength) {
        // substring on the left
        result = left.substring(beginindex, endindex);
      } else if (beginindex >= leftlength) {
        // substring on the right
        result = right
            .substring(beginindex - leftlength, endindex - leftlength);
      } else {
        // split substring
        bytestring leftsub = left.substring(beginindex);
        bytestring rightsub = right.substring(0, endindex - leftlength);
        // intentionally not rebalancing, since in many cases these two
        // substrings will already be less deep than the top-level
        // ropebytestring we're taking a substring of.
        result = new ropebytestring(leftsub, rightsub);
      }
    }
    return result;
  }

  // =================================================================
  // bytestring -> byte[]

  @override
  protected void copytointernal(byte[] target, int sourceoffset,
      int targetoffset, int numbertocopy) {
   if (sourceoffset + numbertocopy <= leftlength) {
      left.copytointernal(target, sourceoffset, targetoffset, numbertocopy);
    } else if (sourceoffset >= leftlength) {
      right.copytointernal(target, sourceoffset - leftlength, targetoffset,
          numbertocopy);
    } else {
      int leftlength = this.leftlength - sourceoffset;
      left.copytointernal(target, sourceoffset, targetoffset, leftlength);
      right.copytointernal(target, 0, targetoffset + leftlength,
          numbertocopy - leftlength);
    }
  }

  @override
  public void copyto(bytebuffer target) {
    left.copyto(target);
    right.copyto(target);
  }

  @override
  public bytebuffer asreadonlybytebuffer() {
    bytebuffer bytebuffer = bytebuffer.wrap(tobytearray());
    return bytebuffer.asreadonlybuffer();
  }

  @override
  public list<bytebuffer> asreadonlybytebufferlist() {
    // walk through the list of literalbytestring's that make up this
    // rope, and add each one as a read-only bytebuffer.
    list<bytebuffer> result = new arraylist<bytebuffer>();
    pieceiterator pieces = new pieceiterator(this);
    while (pieces.hasnext()) {
      literalbytestring bytestring = pieces.next();
      result.add(bytestring.asreadonlybytebuffer());
    }
    return result;
  }

  @override
  public void writeto(outputstream outputstream) throws ioexception {
    left.writeto(outputstream);
    right.writeto(outputstream);
  }

  @override
  public string tostring(string charsetname)
      throws unsupportedencodingexception {
    return new string(tobytearray(), charsetname);
  }

  // =================================================================
  // utf-8 decoding

  @override
  public boolean isvalidutf8() {
    int leftpartial = left.partialisvalidutf8(utf8.complete, 0, leftlength);
    int state = right.partialisvalidutf8(leftpartial, 0, right.size());
    return state == utf8.complete;
  }

  @override
  protected int partialisvalidutf8(int state, int offset, int length) {
    int toindex = offset + length;
    if (toindex <= leftlength) {
      return left.partialisvalidutf8(state, offset, length);
    } else if (offset >= leftlength) {
      return right.partialisvalidutf8(state, offset - leftlength, length);
    } else {
      int leftlength = this.leftlength - offset;
      int leftpartial = left.partialisvalidutf8(state, offset, leftlength);
      return right.partialisvalidutf8(leftpartial, 0, length - leftlength);
    }
  }

  // =================================================================
  // equals() and hashcode()

  @override
  public boolean equals(object other) {
    if (other == this) {
      return true;
    }
    if (!(other instanceof bytestring)) {
      return false;
    }

    bytestring otherbytestring = (bytestring) other;
    if (totallength != otherbytestring.size()) {
      return false;
    }
    if (totallength == 0) {
      return true;
    }

    // you don't really want to be calling equals on long strings, but since
    // we cache the hashcode, we effectively cache inequality. we use the cached
    // hashcode if it's already computed.  it's arguable we should compute the
    // hashcode here, and if we're going to be testing a bunch of bytestrings,
    // it might even make sense.
    if (hash != 0) {
      int cachedotherhash = otherbytestring.peekcachedhashcode();
      if (cachedotherhash != 0 && hash != cachedotherhash) {
        return false;
      }
    }

    return equalsfragments(otherbytestring);
  }

  /**
   * determines if this string is equal to another of the same length by
   * iterating over the leaf nodes. on each step of the iteration, the
   * overlapping segments of the leaves are compared.
   *
   * @param other string of the same length as this one
   * @return true if the values of this string equals the value of the given
   *         one
   */
  private boolean equalsfragments(bytestring other) {
    int thisoffset = 0;
    iterator<literalbytestring> thisiter = new pieceiterator(this);
    literalbytestring thisstring = thisiter.next();

    int thatoffset = 0;
    iterator<literalbytestring> thatiter = new pieceiterator(other);
    literalbytestring thatstring = thatiter.next();

    int pos = 0;
    while (true) {
      int thisremaining = thisstring.size() - thisoffset;
      int thatremaining = thatstring.size() - thatoffset;
      int bytestocompare = math.min(thisremaining, thatremaining);

      // at least one of the offsets will be zero
      boolean stillequal = (thisoffset == 0)
          ? thisstring.equalsrange(thatstring, thatoffset, bytestocompare)
          : thatstring.equalsrange(thisstring, thisoffset, bytestocompare);
      if (!stillequal) {
        return false;
      }

      pos += bytestocompare;
      if (pos >= totallength) {
        if (pos == totallength) {
          return true;
        }
        throw new illegalstateexception();
      }
      // we always get to the end of at least one of the pieces
      if (bytestocompare == thisremaining) { // if reached end of this
        thisoffset = 0;
        thisstring = thisiter.next();
      } else {
        thisoffset += bytestocompare;
      }
      if (bytestocompare == thatremaining) { // if reached end of that
        thatoffset = 0;
        thatstring = thatiter.next();
      } else {
        thatoffset += bytestocompare;
      }
    }
  }

  /**
   * cached hash value.  intentionally accessed via a data race, which is safe
   * because of the java memory model's "no out-of-thin-air values" guarantees
   * for ints.
   */
  private int hash = 0;

  @override
  public int hashcode() {
    int h = hash;

    if (h == 0) {
      h = totallength;
      h = partialhash(h, 0, totallength);
      if (h == 0) {
        h = 1;
      }
      hash = h;
    }
    return h;
  }

  @override
  protected int peekcachedhashcode() {
    return hash;
  }

  @override
  protected int partialhash(int h, int offset, int length) {
    int toindex = offset + length;
    if (toindex <= leftlength) {
      return left.partialhash(h, offset, length);
    } else if (offset >= leftlength) {
      return right.partialhash(h, offset - leftlength, length);
    } else {
      int leftlength = this.leftlength - offset;
      int leftpartial = left.partialhash(h, offset, leftlength);
      return right.partialhash(leftpartial, 0, length - leftlength);
    }
  }

  // =================================================================
  // input stream

  @override
  public codedinputstream newcodedinput() {
    return codedinputstream.newinstance(new ropeinputstream());
  }

  @override
  public inputstream newinput() {
    return new ropeinputstream();
  }

  /**
   * this class implements the balancing algorithm of bap95. in the paper the
   * authors use an array to keep track of pieces, while here we use a stack.
   * the tree is balanced by traversing subtrees in left to right order, and the
   * stack always contains the part of the string we've traversed so far.
   *
   * <p>one surprising aspect of the algorithm is the result of balancing is not
   * necessarily balanced, though it is nearly balanced.  for details, see
   * bap95.
   */
  private static class balancer {
    // stack containing the part of the string, starting from the left, that
    // we've already traversed.  the final string should be the equivalent of
    // concatenating the strings on the stack from bottom to top.
    private final stack<bytestring> prefixesstack = new stack<bytestring>();

    private bytestring balance(bytestring left, bytestring right) {
      dobalance(left);
      dobalance(right);

      // sweep stack to gather the result
      bytestring partialstring = prefixesstack.pop();
      while (!prefixesstack.isempty()) {
        bytestring newleft = prefixesstack.pop();
        partialstring = new ropebytestring(newleft, partialstring);
      }
      // we should end up with a ropebytestring since at a minimum we will
      // create one from concatenating left and right
      return partialstring;
    }

    private void dobalance(bytestring root) {
      // bap95: insert balanced subtrees whole. this means the result might not
      // be balanced, leading to repeated rebalancings on concatenate. however,
      // these rebalancings are shallow due to ignoring balanced subtrees, and
      // relatively few calls to insert() result.
      if (root.isbalanced()) {
        insert(root);
      } else if (root instanceof ropebytestring) {
        ropebytestring rbs = (ropebytestring) root;
        dobalance(rbs.left);
        dobalance(rbs.right);
      } else {
        throw new illegalargumentexception(
            "has a new type of bytestring been created? found " +
                root.getclass());
      }
    }

    /**
     * push a string on the balance stack (bap95).  bap95 uses an array and
     * calls the elements in the array 'bins'.  we instead use a stack, so the
     * 'bins' of lengths are represented by differences between the elements of
     * minlengthbydepth.
     *
     * <p>if the length bin for our string, and all shorter length bins, are
     * empty, we just push it on the stack.  otherwise, we need to start
     * concatenating, putting the given string in the "middle" and continuing
     * until we land in an empty length bin that matches the length of our
     * concatenation.
     *
     * @param bytestring string to place on the balance stack
     */
    private void insert(bytestring bytestring) {
      int depthbin = getdepthbinforlength(bytestring.size());
      int binend = minlengthbydepth[depthbin + 1];

      // bap95: concatenate all trees occupying bins representing the length of
      // our new piece or of shorter pieces, to the extent that is possible.
      // the goal is to clear the bin which our piece belongs in, but that may
      // not be entirely possible if there aren't enough longer bins occupied.
      if (prefixesstack.isempty() || prefixesstack.peek().size() >= binend) {
        prefixesstack.push(bytestring);
      } else {
        int binstart = minlengthbydepth[depthbin];

        // concatenate the subtrees of shorter length
        bytestring newtree = prefixesstack.pop();
        while (!prefixesstack.isempty()
            && prefixesstack.peek().size() < binstart) {
          bytestring left = prefixesstack.pop();
          newtree = new ropebytestring(left, newtree);
        }

        // concatenate the given string
        newtree = new ropebytestring(newtree, bytestring);

        // continue concatenating until we land in an empty bin
        while (!prefixesstack.isempty()) {
          depthbin = getdepthbinforlength(newtree.size());
          binend = minlengthbydepth[depthbin + 1];
          if (prefixesstack.peek().size() < binend) {
            bytestring left = prefixesstack.pop();
            newtree = new ropebytestring(left, newtree);
          } else {
            break;
          }
        }
        prefixesstack.push(newtree);
      }
    }

    private int getdepthbinforlength(int length) {
      int depth = arrays.binarysearch(minlengthbydepth, length);
      if (depth < 0) {
        // it wasn't an exact match, so convert to the index of the containing
        // fragment, which is one less even than the insertion point.
        int insertionpoint = -(depth + 1);
        depth = insertionpoint - 1;
      }

      return depth;
    }
  }

  /**
   * this class is a continuable tree traversal, which keeps the state
   * information which would exist on the stack in a recursive traversal instead
   * on a stack of "bread crumbs". the maximum depth of the stack in this
   * iterator is the same as the depth of the tree being traversed.
   *
   * <p>this iterator is used to implement
   * {@link ropebytestring#equalsfragments(bytestring)}.
   */
  private static class pieceiterator implements iterator<literalbytestring> {

    private final stack<ropebytestring> breadcrumbs =
        new stack<ropebytestring>();
    private literalbytestring next;

    private pieceiterator(bytestring root) {
      next = getleafbyleft(root);
    }

    private literalbytestring getleafbyleft(bytestring root) {
      bytestring pos = root;
      while (pos instanceof ropebytestring) {
        ropebytestring rbs = (ropebytestring) pos;
        breadcrumbs.push(rbs);
        pos = rbs.left;
      }
      return (literalbytestring) pos;
    }

    private literalbytestring getnextnonemptyleaf() {
      while (true) {
        // almost always, we go through this loop exactly once.  however, if
        // we discover an empty string in the rope, we toss it and try again.
        if (breadcrumbs.isempty()) {
          return null;
        } else {
          literalbytestring result = getleafbyleft(breadcrumbs.pop().right);
          if (!result.isempty()) {
            return result;
          }
        }
      }
    }

    public boolean hasnext() {
      return next != null;
    }

    /**
     * returns the next item and advances one {@code literalbytestring}.
     *
     * @return next non-empty literalbytestring or {@code null}
     */
    public literalbytestring next() {
      if (next == null) {
        throw new nosuchelementexception();
      }
      literalbytestring result = next;
      next = getnextnonemptyleaf();
      return result;
    }

    public void remove() {
      throw new unsupportedoperationexception();
    }
  }

  // =================================================================
  // byteiterator

  @override
  public byteiterator iterator() {
    return new ropebyteiterator();
  }

  private class ropebyteiterator implements bytestring.byteiterator {

    private final pieceiterator pieces;
    private byteiterator bytes;
    int bytesremaining;

    private ropebyteiterator() {
      pieces = new pieceiterator(ropebytestring.this);
      bytes = pieces.next().iterator();
      bytesremaining = size();
    }

    public boolean hasnext() {
      return (bytesremaining > 0);
    }

    public byte next() {
      return nextbyte(); // does not instantiate a byte
    }

    public byte nextbyte() {
      if (!bytes.hasnext()) {
        bytes = pieces.next().iterator();
      }
      --bytesremaining;
      return bytes.nextbyte();
    }

    public void remove() {
      throw new unsupportedoperationexception();
    }
  }

  /**
   * this class is the {@link ropebytestring} equivalent for
   * {@link bytearrayinputstream}.
   */
  private class ropeinputstream extends inputstream {
    // iterates through the pieces of the rope
    private pieceiterator pieceiterator;
    // the current piece
    private literalbytestring currentpiece;
    // the size of the current piece
    private int currentpiecesize;
    // the index of the next byte to read in the current piece
    private int currentpieceindex;
    // the offset of the start of the current piece in the rope byte string
    private int currentpieceoffsetinrope;
    // offset in the buffer at which user called mark();
    private int mark;

    public ropeinputstream() {
      initialize();
    }

    @override
    public int read(byte b[], int offset, int length)  {
      if (b == null) {
        throw new nullpointerexception();
      } else if (offset < 0 || length < 0 || length > b.length - offset) {
        throw new indexoutofboundsexception();
      }
      return readskipinternal(b, offset, length);
    }

    @override
    public long skip(long length) {
      if (length < 0) {
        throw new indexoutofboundsexception();
      } else if (length > integer.max_value) {
        length = integer.max_value;
      }
      return readskipinternal(null, 0, (int) length);
    }

    /**
     * internal implementation of read and skip.  if b != null, then read the
     * next {@code length} bytes into the buffer {@code b} at
     * offset {@code offset}.  if b == null, then skip the next {@code length)
     * bytes.
     * <p>
     * this method assumes that all error checking has already happened.
     * <p>
     * returns the actual number of bytes read or skipped.
     */
    private int readskipinternal(byte b[], int offset, int length)  {
      int bytesremaining = length;
      while (bytesremaining > 0) {
        advanceifcurrentpiecefullyread();
        if (currentpiece == null) {
          if (bytesremaining == length) {
             // we didn't manage to read anything
             return -1;
           }
          break;
        } else {
          // copy the bytes from this piece.
          int currentpieceremaining = currentpiecesize - currentpieceindex;
          int count = math.min(currentpieceremaining, bytesremaining);
          if (b != null) {
            currentpiece.copyto(b, currentpieceindex, offset, count);
            offset += count;
          }
          currentpieceindex += count;
          bytesremaining -= count;
        }
      }
       // return the number of bytes read.
      return length - bytesremaining;
    }

    @override
    public int read() throws ioexception {
      advanceifcurrentpiecefullyread();
      if (currentpiece == null) {
        return -1;
      } else {
        return currentpiece.byteat(currentpieceindex++) & 0xff;
      }
    }

    @override
    public int available() throws ioexception {
      int bytesread = currentpieceoffsetinrope + currentpieceindex;
      return ropebytestring.this.size() - bytesread;
    }

    @override
    public boolean marksupported() {
      return true;
    }

    @override
    public void mark(int readaheadlimit) {
      // set the mark to our position in the byte string
      mark = currentpieceoffsetinrope + currentpieceindex;
    }

    @override
    public synchronized void reset() {
      // just reinitialize and skip the specified number of bytes.
      initialize();
      readskipinternal(null, 0, mark);
    }

    /** common initialization code used by both the constructor and reset() */
    private void initialize() {
      pieceiterator = new pieceiterator(ropebytestring.this);
      currentpiece = pieceiterator.next();
      currentpiecesize = currentpiece.size();
      currentpieceindex = 0;
      currentpieceoffsetinrope = 0;
    }

    /**
     * skips to the next piece if we have read all the data in the current
     * piece.  sets currentpiece to null if we have reached the end of the
     * input.
     */
    private void advanceifcurrentpiecefullyread() {
      if (currentpiece != null && currentpieceindex == currentpiecesize) {
        // generally, we can only go through this loop at most once, since
        // empty strings can't end up in a rope.  but better to test.
        currentpieceoffsetinrope += currentpiecesize;
        currentpieceindex = 0;
        if (pieceiterator.hasnext()) {
          currentpiece = pieceiterator.next();
          currentpiecesize = currentpiece.size();
        } else {
          currentpiece = null;
          currentpiecesize = 0;
        }
      }
    }
  }
}
