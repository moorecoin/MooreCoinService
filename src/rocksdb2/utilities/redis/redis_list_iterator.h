// copyright 2013 facebook
/**
 * redislistiterator:
 * an abstraction over the "list" concept (e.g.: for redis lists).
 * provides functionality to read, traverse, edit, and write these lists.
 *
 * upon construction, the redislistiterator is given a block of list data.
 * internally, it stores a pointer to the data and a pointer to current item.
 * it also stores a "result" list that will be mutated over time.
 *
 * traversal and mutation are done by "forward iteration".
 * the push() and skip() methods will advance the iterator to the next item.
 * however, push() will also "write the current item to the result".
 * skip() will simply move to next item, causing current item to be dropped.
 *
 * upon completion, the result (accessible by writeresult()) will be saved.
 * all "skipped" items will be gone; all "pushed" items will remain.
 *
 * @throws any of the operations may throw a redislistexception if an invalid
 *          operation is performed or if the data is found to be corrupt.
 *
 * @notes by default, if writeresult() is called part-way through iteration,
 *        it will automatically advance the iterator to the end, and keep()
 *        all items that haven't been traversed yet. this may be subject
 *        to review.
 *
 * @notes can access the "current" item via getcurrent(), and other
 *        list-specific information such as length().
 *
 * @notes the internal representation is due to change at any time. presently,
 *        the list is represented as follows:
 *          - 32-bit integer header: the number of items in the list
 *          - for each item:
 *              - 32-bit int (n): the number of bytes representing this item
 *              - n bytes of data: the actual data.
 *
 * @author deon nicholas (dnicholas@fb.com)
 */

#ifndef rocksdb_lite
#pragma once

#include <string>

#include "redis_list_exception.h"
#include "rocksdb/slice.h"
#include "util/coding.h"

namespace rocksdb {

/// an abstraction over the "list" concept.
/// all operations may throw a redislistexception
class redislistiterator {
 public:
  /// construct a redis-list-iterator based on data.
  /// if the data is non-empty, it must formatted according to @notes above.
  ///
  /// if the data is valid, we can assume the following invariant(s):
  ///  a) length_, num_bytes_ are set correctly.
  ///  b) cur_byte_ always refers to the start of the current element,
  ///       just before the bytes that specify element length.
  ///  c) cur_elem_ is always the index of the current element.
  ///  d) cur_elem_length_ is always the number of bytes in current element,
  ///       excluding the 4-byte header itself.
  ///  e) result_ will always contain data_[0..cur_byte_) and a header
  ///  f) whenever corrupt data is encountered or an invalid operation is
  ///      attempted, a redislistexception will immediately be thrown.
  redislistiterator(const std::string& list_data)
      : data_(list_data.data()),
        num_bytes_(list_data.size()),
        cur_byte_(0),
        cur_elem_(0),
        cur_elem_length_(0),
        length_(0),
        result_() {

    // initialize the result_ (reserve enough space for header)
    initializeresult();

    // parse the data only if it is not empty.
    if (num_bytes_ == 0) {
      return;
    }

    // if non-empty, but less than 4 bytes, data must be corrupt
    if (num_bytes_ < sizeof(length_)) {
      throwerror("corrupt header.");    // will break control flow
    }

    // good. the first bytes specify the number of elements
    length_ = decodefixed32(data_);
    cur_byte_ = sizeof(length_);

    // if we have at least one element, point to that element.
    // also, read the first integer of the element (specifying the size),
    //   if possible.
    if (length_ > 0) {
      if (cur_byte_ + sizeof(cur_elem_length_) <= num_bytes_) {
        cur_elem_length_ = decodefixed32(data_+cur_byte_);
      } else {
        throwerror("corrupt data for first element.");
      }
    }

    // at this point, we are fully set-up.
    // the invariants described in the header should now be true.
  }

  /// reserve some space for the result_.
  /// equivalent to result_.reserve(bytes).
  void reserve(int bytes) {
    result_.reserve(bytes);
  }

  /// go to next element in data file.
  /// also writes the current element to result_.
  redislistiterator& push() {
    writecurrentelement();
    movenext();
    return *this;
  }

  /// go to next element in data file.
  /// drops/skips the current element. it will not be written to result_.
  redislistiterator& skip() {
    movenext();
    --length_;          // one less item
    --cur_elem_;        // we moved one forward, but index did not change
    return *this;
  }

  /// insert elem into the result_ (just before the current element / byte)
  /// note: if done() (i.e.: iterator points to end), this will append elem.
  void insertelement(const slice& elem) {
    // ensure we are in a valid state
    checkerrors();

    const int korigsize = result_.size();
    result_.resize(korigsize + sizeof(elem));
    encodefixed32(result_.data() + korigsize, elem.size());
    memcpy(result_.data() + korigsize + sizeof(uint32_t),
           elem.data(),
           elem.size());
    ++length_;
    ++cur_elem_;
  }

  /// access the current element, and save the result into *curelem
  void getcurrent(slice* curelem) {
    // ensure we are in a valid state
    checkerrors();

    // ensure that we are not past the last element.
    if (done()) {
      throwerror("invalid dereferencing.");
    }

    // dereference the element
    *curelem = slice(data_+cur_byte_+sizeof(cur_elem_length_),
                     cur_elem_length_);
  }

  // number of elements
  int length() const {
    return length_;
  }

  // number of bytes in the final representation (i.e: writeresult().size())
  int size() const {
    // result_ holds the currently written data
    // data_[cur_byte..num_bytes-1] is the remainder of the data
    return result_.size() + (num_bytes_ - cur_byte_);
  }

  // reached the end?
  bool done() const {
    return cur_byte_ >= num_bytes_ || cur_elem_ >= length_;
  }

  /// returns a string representing the final, edited, data.
  /// assumes that all bytes of data_ in the range [0,cur_byte_) have been read
  ///  and that result_ contains this data.
  /// the rest of the data must still be written.
  /// so, this method advances the iterator to the end before writing.
  slice writeresult() {
    checkerrors();

    // the header should currently be filled with dummy data (0's)
    // correctly update the header.
    // note, this is safe since result_ is a vector (guaranteed contiguous)
    encodefixed32(&result_[0],length_);

    // append the remainder of the data to the result.
    result_.insert(result_.end(),data_+cur_byte_, data_ +num_bytes_);

    // seek to end of file
    cur_byte_ = num_bytes_;
    cur_elem_ = length_;
    cur_elem_length_ = 0;

    // return the result
    return slice(result_.data(),result_.size());
  }

 public: // static public functions

  /// an upper-bound on the amount of bytes needed to store this element.
  /// this is used to hide representation information from the client.
  /// e.g. this can be used to compute the bytes we want to reserve().
  static uint32_t sizeof(const slice& elem) {
    // [integer length . data]
    return sizeof(uint32_t) + elem.size();
  }

 private: // private functions

  /// initializes the result_ string.
  /// it will fill the first few bytes with 0's so that there is
  ///  enough space for header information when we need to write later.
  /// currently, "header information" means: the length (number of elements)
  /// assumes that result_ is empty to begin with
  void initializeresult() {
    assert(result_.empty());            // should always be true.
    result_.resize(sizeof(uint32_t),0); // put a block of 0's as the header
  }

  /// go to the next element (used in push() and skip())
  void movenext() {
    checkerrors();

    // check to make sure we are not already in a finished state
    if (done()) {
      throwerror("attempting to iterate past end of list.");
    }

    // move forward one element.
    cur_byte_ += sizeof(cur_elem_length_) + cur_elem_length_;
    ++cur_elem_;

    // if we are at the end, finish
    if (done()) {
      cur_elem_length_ = 0;
      return;
    }

    // otherwise, we should be able to read the new element's length
    if (cur_byte_ + sizeof(cur_elem_length_) > num_bytes_) {
      throwerror("corrupt element data.");
    }

    // set the new element's length
    cur_elem_length_ = decodefixed32(data_+cur_byte_);

    return;
  }

  /// append the current element (pointed to by cur_byte_) to result_
  /// assumes result_ has already been reserved appropriately.
  void writecurrentelement() {
    // first verify that the iterator is still valid.
    checkerrors();
    if (done()) {
      throwerror("attempting to write invalid element.");
    }

    // append the cur element.
    result_.insert(result_.end(),
                   data_+cur_byte_,
                   data_+cur_byte_+ sizeof(uint32_t) + cur_elem_length_);
  }

  /// will throwerror() if neccessary.
  /// checks for common/ubiquitous errors that can arise after most operations.
  /// this method should be called before any reading operation.
  /// if this function succeeds, then we are guaranteed to be in a valid state.
  /// other member functions should check for errors and throwerror() also
  ///  if an error occurs that is specific to it even while in a valid state.
  void checkerrors() {
    // check if any crazy thing has happened recently
    if ((cur_elem_ > length_) ||                              // bad index
        (cur_byte_ > num_bytes_) ||                           // no more bytes
        (cur_byte_ + cur_elem_length_ > num_bytes_) ||        // item too large
        (cur_byte_ == num_bytes_ && cur_elem_ != length_) ||  // too many items
        (cur_elem_ == length_ && cur_byte_ != num_bytes_)) {  // too many bytes
      throwerror("corrupt data.");
    }
  }

  /// will throw an exception based on the passed-in message.
  /// this function is guaranteed to stop the control-flow.
  /// (i.e.: you do not have to call "return" after calling throwerror)
  void throwerror(const char* const msg = null) {
    // todo: for now we ignore the msg parameter. this can be expanded later.
    throw redislistexception();
  }

 private:
  const char* const data_;      // a pointer to the data (the first byte)
  const uint32_t num_bytes_;    // the number of bytes in this list

  uint32_t cur_byte_;           // the current byte being read
  uint32_t cur_elem_;           // the current element being read
  uint32_t cur_elem_length_;    // the number of bytes in current element

  uint32_t length_;             // the number of elements in this list
  std::vector<char> result_;    // the output data
};

} // namespace rocksdb
#endif  // rocksdb_lite
