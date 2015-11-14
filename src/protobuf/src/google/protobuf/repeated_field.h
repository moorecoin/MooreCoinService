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
// repeatedfield and repeatedptrfield are used by generated protocol message
// classes to manipulate repeated fields.  these classes are very similar to
// stl's vector, but include a number of optimizations found to be useful
// specifically in the case of protocol buffers.  repeatedptrfield is
// particularly different from stl vector as it manages ownership of the
// pointers that it contains.
//
// typically, clients should not need to access repeatedfield objects directly,
// but should instead use the accessor functions generated automatically by the
// protocol compiler.

#ifndef google_protobuf_repeated_field_h__
#define google_protobuf_repeated_field_h__

#include <algorithm>
#include <string>
#include <iterator>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/type_traits.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/message_lite.h>

namespace google {

namespace upb {
namespace google_opensource {
class gmr_handlers;
}  // namespace google_opensource
}  // namespace upb

namespace protobuf {

class message;

namespace internal {

static const int kminrepeatedfieldallocationsize = 4;

// a utility function for logging that doesn't need any template types.
void logindexoutofbounds(int index, int size);
}  // namespace internal


// repeatedfield is used to represent repeated fields of a primitive type (in
// other words, everything except strings and nested messages).  most users will
// not ever use a repeatedfield directly; they will use the get-by-index,
// set-by-index, and add accessors that are generated for all repeated fields.
template <typename element>
class repeatedfield {
 public:
  repeatedfield();
  repeatedfield(const repeatedfield& other);
  template <typename iter>
  repeatedfield(iter begin, const iter& end);
  ~repeatedfield();

  repeatedfield& operator=(const repeatedfield& other);

  int size() const;

  const element& get(int index) const;
  element* mutable(int index);
  void set(int index, const element& value);
  void add(const element& value);
  element* add();
  // remove the last element in the array.
  void removelast();

  // extract elements with indices in "[start .. start+num-1]".
  // copy them into "elements[0 .. num-1]" if "elements" is not null.
  // caution: implementation also moves elements with indices [start+num ..].
  // calling this routine inside a loop can cause quadratic behavior.
  void extractsubrange(int start, int num, element* elements);

  void clear();
  void mergefrom(const repeatedfield& other);
  void copyfrom(const repeatedfield& other);

  // reserve space to expand the field to at least the given size.  if the
  // array is grown, it will always be at least doubled in size.
  void reserve(int new_size);

  // resize the repeatedfield to a new, smaller size.  this is o(1).
  void truncate(int new_size);

  void addalreadyreserved(const element& value);
  element* addalreadyreserved();
  int capacity() const;

  // gets the underlying array.  this pointer is possibly invalidated by
  // any add or remove operation.
  element* mutable_data();
  const element* data() const;

  // swap entire contents with "other".
  void swap(repeatedfield* other);

  // swap two elements.
  void swapelements(int index1, int index2);

  // stl-like iterator support
  typedef element* iterator;
  typedef const element* const_iterator;
  typedef element value_type;
  typedef value_type& reference;
  typedef const value_type& const_reference;
  typedef value_type* pointer;
  typedef const value_type* const_pointer;
  typedef int size_type;
  typedef ptrdiff_t difference_type;

  iterator begin();
  const_iterator begin() const;
  iterator end();
  const_iterator end() const;

  // reverse iterator support
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  reverse_iterator rbegin() {
    return reverse_iterator(end());
  }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }
  reverse_iterator rend() {
    return reverse_iterator(begin());
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }

  // returns the number of bytes used by the repeated field, excluding
  // sizeof(*this)
  int spaceusedexcludingself() const;

 private:
  static const int kinitialsize = 0;

  element* elements_;
  int      current_size_;
  int      total_size_;

  // move the contents of |from| into |to|, possibly clobbering |from| in the
  // process.  for primitive types this is just a memcpy(), but it could be
  // specialized for non-primitive types to, say, swap each element instead.
  void movearray(element to[], element from[], int size);

  // copy the elements of |from| into |to|.
  void copyarray(element to[], const element from[], int size);
};

namespace internal {
template <typename it> class repeatedptriterator;
template <typename it, typename voidptr> class repeatedptroverptrsiterator;
}  // namespace internal

namespace internal {

// this is a helper template to copy an array of elements effeciently when they
// have a trivial copy constructor, and correctly otherwise. this really
// shouldn't be necessary, but our compiler doesn't optimize std::copy very
// effectively.
template <typename element,
          bool hastrivialcopy = has_trivial_copy<element>::value>
struct elementcopier {
  void operator()(element to[], const element from[], int array_size);
};

}  // namespace internal

namespace internal {

// this is the common base class for repeatedptrfields.  it deals only in void*
// pointers.  users should not use this interface directly.
//
// the methods of this interface correspond to the methods of repeatedptrfield,
// but may have a template argument called typehandler.  its signature is:
//   class typehandler {
//    public:
//     typedef mytype type;
//     static type* new();
//     static void delete(type*);
//     static void clear(type*);
//     static void merge(const type& from, type* to);
//
//     // only needs to be implemented if spaceusedexcludingself() is called.
//     static int spaceused(const type&);
//   };
class libprotobuf_export repeatedptrfieldbase {
 protected:
  // the reflection implementation needs to call protected methods directly,
  // reinterpreting pointers as being to message instead of a specific message
  // subclass.
  friend class generatedmessagereflection;

  // extensionset stores repeated message extensions as
  // repeatedptrfield<messagelite>, but non-lite extensionsets need to
  // implement spaceused(), and thus need to call spaceusedexcludingself()
  // reinterpreting messagelite as message.  extensionset also needs to make
  // use of addfromcleared(), which is not part of the public interface.
  friend class extensionset;

  // to parse directly into a proto2 generated class, the upb class gmr_handlers
  // needs to be able to modify a repeatedptrfieldbase directly.
  friend class libprotobuf_export upb::google_opensource::gmr_handlers;

  repeatedptrfieldbase();

  // must be called from destructor.
  template <typename typehandler>
  void destroy();

  int size() const;

  template <typename typehandler>
  const typename typehandler::type& get(int index) const;
  template <typename typehandler>
  typename typehandler::type* mutable(int index);
  template <typename typehandler>
  typename typehandler::type* add();
  template <typename typehandler>
  void removelast();
  template <typename typehandler>
  void clear();
  template <typename typehandler>
  void mergefrom(const repeatedptrfieldbase& other);
  template <typename typehandler>
  void copyfrom(const repeatedptrfieldbase& other);

  void closegap(int start, int num) {
    // close up a gap of "num" elements starting at offset "start".
    for (int i = start + num; i < allocated_size_; ++i)
      elements_[i - num] = elements_[i];
    current_size_ -= num;
    allocated_size_ -= num;
  }

  void reserve(int new_size);

  int capacity() const;

  // used for constructing iterators.
  void* const* raw_data() const;
  void** raw_mutable_data() const;

  template <typename typehandler>
  typename typehandler::type** mutable_data();
  template <typename typehandler>
  const typename typehandler::type* const* data() const;

  void swap(repeatedptrfieldbase* other);

  void swapelements(int index1, int index2);

  template <typename typehandler>
  int spaceusedexcludingself() const;


  // advanced memory management --------------------------------------

  // like add(), but if there are no cleared objects to use, returns null.
  template <typename typehandler>
  typename typehandler::type* addfromcleared();

  template <typename typehandler>
  void addallocated(typename typehandler::type* value);
  template <typename typehandler>
  typename typehandler::type* releaselast();

  int clearedcount() const;
  template <typename typehandler>
  void addcleared(typename typehandler::type* value);
  template <typename typehandler>
  typename typehandler::type* releasecleared();

 private:
  google_disallow_evil_constructors(repeatedptrfieldbase);

  static const int kinitialsize = 0;

  void** elements_;
  int    current_size_;
  int    allocated_size_;
  int    total_size_;

  template <typename typehandler>
  static inline typename typehandler::type* cast(void* element) {
    return reinterpret_cast<typename typehandler::type*>(element);
  }
  template <typename typehandler>
  static inline const typename typehandler::type* cast(const void* element) {
    return reinterpret_cast<const typename typehandler::type*>(element);
  }
};

template <typename generictype>
class generictypehandler {
 public:
  typedef generictype type;
  static generictype* new() { return new generictype; }
  static void delete(generictype* value) { delete value; }
  static void clear(generictype* value) { value->clear(); }
  static void merge(const generictype& from, generictype* to) {
    to->mergefrom(from);
  }
  static int spaceused(const generictype& value) { return value.spaceused(); }
  static const type& default_instance() { return type::default_instance(); }
};

template <>
inline void generictypehandler<messagelite>::merge(
    const messagelite& from, messagelite* to) {
  to->checktypeandmergefrom(from);
}

template <>
inline const messagelite& generictypehandler<messagelite>::default_instance() {
  // yes, the behavior of the code is undefined, but this function is only
  // called when we're already deep into the world of undefined, because the
  // caller called get(index) out of bounds.
  messagelite* null = null;
  return *null;
}

template <>
inline const message& generictypehandler<message>::default_instance() {
  // yes, the behavior of the code is undefined, but this function is only
  // called when we're already deep into the world of undefined, because the
  // caller called get(index) out of bounds.
  message* null = null;
  return *null;
}


// hack:  if a class is declared as dll-exported in msvc, it insists on
//   generating copies of all its methods -- even inline ones -- to include
//   in the dll.  but spaceused() calls stringspaceusedexcludingself() which
//   isn't in the lite library, therefore the lite library cannot link if
//   stringtypehandler is exported.  so, we factor out stringtypehandlerbase,
//   export that, then make stringtypehandler be a subclass which is not
//   exported.
// todo(kenton):  there has to be a better way.
class libprotobuf_export stringtypehandlerbase {
 public:
  typedef string type;
  static string* new();
  static void delete(string* value);
  static void clear(string* value) { value->clear(); }
  static void merge(const string& from, string* to) { *to = from; }
  static const type& default_instance() {
    return ::google::protobuf::internal::kemptystring;
  }
};

class stringtypehandler : public stringtypehandlerbase {
 public:
  static int spaceused(const string& value)  {
    return sizeof(value) + stringspaceusedexcludingself(value);
  }
};


}  // namespace internal

// repeatedptrfield is like repeatedfield, but used for repeated strings or
// messages.
template <typename element>
class repeatedptrfield : public internal::repeatedptrfieldbase {
 public:
  repeatedptrfield();
  repeatedptrfield(const repeatedptrfield& other);
  template <typename iter>
  repeatedptrfield(iter begin, const iter& end);
  ~repeatedptrfield();

  repeatedptrfield& operator=(const repeatedptrfield& other);

  int size() const;

  const element& get(int index) const;
  element* mutable(int index);
  element* add();

  // remove the last element in the array.
  // ownership of the element is retained by the array.
  void removelast();

  // delete elements with indices in the range [start .. start+num-1].
  // caution: implementation moves all elements with indices [start+num .. ].
  // calling this routine inside a loop can cause quadratic behavior.
  void deletesubrange(int start, int num);

  void clear();
  void mergefrom(const repeatedptrfield& other);
  void copyfrom(const repeatedptrfield& other);

  // reserve space to expand the field to at least the given size.  this only
  // resizes the pointer array; it doesn't allocate any objects.  if the
  // array is grown, it will always be at least doubled in size.
  void reserve(int new_size);

  int capacity() const;

  // gets the underlying array.  this pointer is possibly invalidated by
  // any add or remove operation.
  element** mutable_data();
  const element* const* data() const;

  // swap entire contents with "other".
  void swap(repeatedptrfield* other);

  // swap two elements.
  void swapelements(int index1, int index2);

  // stl-like iterator support
  typedef internal::repeatedptriterator<element> iterator;
  typedef internal::repeatedptriterator<const element> const_iterator;
  typedef element value_type;
  typedef value_type& reference;
  typedef const value_type& const_reference;
  typedef value_type* pointer;
  typedef const value_type* const_pointer;
  typedef int size_type;
  typedef ptrdiff_t difference_type;

  iterator begin();
  const_iterator begin() const;
  iterator end();
  const_iterator end() const;

  // reverse iterator support
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  reverse_iterator rbegin() {
    return reverse_iterator(end());
  }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }
  reverse_iterator rend() {
    return reverse_iterator(begin());
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }

  // custom stl-like iterator that iterates over and returns the underlying
  // pointers to element rather than element itself.
  typedef internal::repeatedptroverptrsiterator<element, void*>
  pointer_iterator;
  typedef internal::repeatedptroverptrsiterator<const element, const void*>
  const_pointer_iterator;
  pointer_iterator pointer_begin();
  const_pointer_iterator pointer_begin() const;
  pointer_iterator pointer_end();
  const_pointer_iterator pointer_end() const;

  // returns (an estimate of) the number of bytes used by the repeated field,
  // excluding sizeof(*this).
  int spaceusedexcludingself() const;

  // advanced memory management --------------------------------------
  // when hardcore memory management becomes necessary -- as it sometimes
  // does here at google -- the following methods may be useful.

  // add an already-allocated object, passing ownership to the
  // repeatedptrfield.
  void addallocated(element* value);
  // remove the last element and return it, passing ownership to the caller.
  // requires:  size() > 0
  element* releaselast();

  // extract elements with indices in the range "[start .. start+num-1]".
  // the caller assumes ownership of the extracted elements and is responsible
  // for deleting them when they are no longer needed.
  // if "elements" is non-null, then pointers to the extracted elements
  // are stored in "elements[0 .. num-1]" for the convenience of the caller.
  // if "elements" is null, then the caller must use some other mechanism
  // to perform any further operations (like deletion) on these elements.
  // caution: implementation also moves elements with indices [start+num ..].
  // calling this routine inside a loop can cause quadratic behavior.
  void extractsubrange(int start, int num, element** elements);

  // when elements are removed by calls to removelast() or clear(), they
  // are not actually freed.  instead, they are cleared and kept so that
  // they can be reused later.  this can save lots of cpu time when
  // repeatedly reusing a protocol message for similar purposes.
  //
  // hardcore programs may choose to manipulate these cleared objects
  // to better optimize memory management using the following routines.

  // get the number of cleared objects that are currently being kept
  // around for reuse.
  int clearedcount() const;
  // add an element to the pool of cleared objects, passing ownership to
  // the repeatedptrfield.  the element must be cleared prior to calling
  // this method.
  void addcleared(element* value);
  // remove a single element from the cleared pool and return it, passing
  // ownership to the caller.  the element is guaranteed to be cleared.
  // requires:  clearedcount() > 0
  element* releasecleared();

 protected:
  // note:  repeatedptrfield should not be subclassed by users.  we only
  //   subclass it in one place as a hack for compatibility with proto1.  the
  //   subclass needs to know about typehandler in order to call protected
  //   methods on repeatedptrfieldbase.
  class typehandler;

};

// implementation ====================================================

template <typename element>
inline repeatedfield<element>::repeatedfield()
  : elements_(null),
    current_size_(0),
    total_size_(kinitialsize) {
}

template <typename element>
inline repeatedfield<element>::repeatedfield(const repeatedfield& other)
  : elements_(null),
    current_size_(0),
    total_size_(kinitialsize) {
  copyfrom(other);
}

template <typename element>
template <typename iter>
inline repeatedfield<element>::repeatedfield(iter begin, const iter& end)
  : elements_(null),
    current_size_(0),
    total_size_(kinitialsize) {
  for (; begin != end; ++begin) {
    add(*begin);
  }
}

template <typename element>
repeatedfield<element>::~repeatedfield() {
  delete [] elements_;
}

template <typename element>
inline repeatedfield<element>&
repeatedfield<element>::operator=(const repeatedfield& other) {
  if (this != &other)
    copyfrom(other);
  return *this;
}

template <typename element>
inline int repeatedfield<element>::size() const {
  return current_size_;
}

template <typename element>
inline int repeatedfield<element>::capacity() const {
  return total_size_;
}

template<typename element>
inline void repeatedfield<element>::addalreadyreserved(const element& value) {
  google_dcheck_lt(size(), capacity());
  elements_[current_size_++] = value;
}

template<typename element>
inline element* repeatedfield<element>::addalreadyreserved() {
  google_dcheck_lt(size(), capacity());
  return &elements_[current_size_++];
}

template <typename element>
inline const element& repeatedfield<element>::get(int index) const {
  google_dcheck_lt(index, size());
  return elements_[index];
}

template <typename element>
inline element* repeatedfield<element>::mutable(int index) {
  google_dcheck_lt(index, size());
  return elements_ + index;
}

template <typename element>
inline void repeatedfield<element>::set(int index, const element& value) {
  google_dcheck_lt(index, size());
  elements_[index] = value;
}

template <typename element>
inline void repeatedfield<element>::add(const element& value) {
  if (current_size_ == total_size_) reserve(total_size_ + 1);
  elements_[current_size_++] = value;
}

template <typename element>
inline element* repeatedfield<element>::add() {
  if (current_size_ == total_size_) reserve(total_size_ + 1);
  return &elements_[current_size_++];
}

template <typename element>
inline void repeatedfield<element>::removelast() {
  google_dcheck_gt(current_size_, 0);
  --current_size_;
}

template <typename element>
void repeatedfield<element>::extractsubrange(
    int start, int num, element* elements) {
  google_dcheck_ge(start, 0);
  google_dcheck_ge(num, 0);
  google_dcheck_le(start + num, this->size());

  // save the values of the removed elements if requested.
  if (elements != null) {
    for (int i = 0; i < num; ++i)
      elements[i] = this->get(i + start);
  }

  // slide remaining elements down to fill the gap.
  if (num > 0) {
    for (int i = start + num; i < this->size(); ++i)
      this->set(i - num, this->get(i));
    this->truncate(this->size() - num);
  }
}

template <typename element>
inline void repeatedfield<element>::clear() {
  current_size_ = 0;
}

template <typename element>
inline void repeatedfield<element>::mergefrom(const repeatedfield& other) {
  if (other.current_size_ != 0) {
    reserve(current_size_ + other.current_size_);
    copyarray(elements_ + current_size_, other.elements_, other.current_size_);
    current_size_ += other.current_size_;
  }
}

template <typename element>
inline void repeatedfield<element>::copyfrom(const repeatedfield& other) {
  clear();
  mergefrom(other);
}

template <typename element>
inline element* repeatedfield<element>::mutable_data() {
  return elements_;
}

template <typename element>
inline const element* repeatedfield<element>::data() const {
  return elements_;
}


template <typename element>
void repeatedfield<element>::swap(repeatedfield* other) {
  if (this == other) return;
  element* swap_elements     = elements_;
  int      swap_current_size = current_size_;
  int      swap_total_size   = total_size_;

  elements_     = other->elements_;
  current_size_ = other->current_size_;
  total_size_   = other->total_size_;

  other->elements_     = swap_elements;
  other->current_size_ = swap_current_size;
  other->total_size_   = swap_total_size;
}

template <typename element>
void repeatedfield<element>::swapelements(int index1, int index2) {
  std::swap(elements_[index1], elements_[index2]);
}

template <typename element>
inline typename repeatedfield<element>::iterator
repeatedfield<element>::begin() {
  return elements_;
}
template <typename element>
inline typename repeatedfield<element>::const_iterator
repeatedfield<element>::begin() const {
  return elements_;
}
template <typename element>
inline typename repeatedfield<element>::iterator
repeatedfield<element>::end() {
  return elements_ + current_size_;
}
template <typename element>
inline typename repeatedfield<element>::const_iterator
repeatedfield<element>::end() const {
  return elements_ + current_size_;
}

template <typename element>
inline int repeatedfield<element>::spaceusedexcludingself() const {
  return (elements_ != null) ? total_size_ * sizeof(elements_[0]) : 0;
}

// avoid inlining of reserve(): new, copy, and delete[] lead to a significant
// amount of code bloat.
template <typename element>
void repeatedfield<element>::reserve(int new_size) {
  if (total_size_ >= new_size) return;

  element* old_elements = elements_;
  total_size_ = max(google::protobuf::internal::kminrepeatedfieldallocationsize,
                    max(total_size_ * 2, new_size));
  elements_ = new element[total_size_];
  if (old_elements != null) {
    movearray(elements_, old_elements, current_size_);
    delete [] old_elements;
  }
}

template <typename element>
inline void repeatedfield<element>::truncate(int new_size) {
  google_dcheck_le(new_size, current_size_);
  current_size_ = new_size;
}

template <typename element>
inline void repeatedfield<element>::movearray(
    element to[], element from[], int array_size) {
  copyarray(to, from, array_size);
}

template <typename element>
inline void repeatedfield<element>::copyarray(
    element to[], const element from[], int array_size) {
  internal::elementcopier<element>()(to, from, array_size);
}

namespace internal {

template <typename element, bool hastrivialcopy>
void elementcopier<element, hastrivialcopy>::operator()(
    element to[], const element from[], int array_size) {
  std::copy(from, from + array_size, to);
}

template <typename element>
struct elementcopier<element, true> {
  void operator()(element to[], const element from[], int array_size) {
    memcpy(to, from, array_size * sizeof(element));
  }
};

}  // namespace internal


// -------------------------------------------------------------------

namespace internal {

inline repeatedptrfieldbase::repeatedptrfieldbase()
  : elements_(null),
    current_size_(0),
    allocated_size_(0),
    total_size_(kinitialsize) {
}

template <typename typehandler>
void repeatedptrfieldbase::destroy() {
  for (int i = 0; i < allocated_size_; i++) {
    typehandler::delete(cast<typehandler>(elements_[i]));
  }
  delete [] elements_;
}

inline int repeatedptrfieldbase::size() const {
  return current_size_;
}

template <typename typehandler>
inline const typename typehandler::type&
repeatedptrfieldbase::get(int index) const {
  google_dcheck_lt(index, size());
  return *cast<typehandler>(elements_[index]);
}


template <typename typehandler>
inline typename typehandler::type*
repeatedptrfieldbase::mutable(int index) {
  google_dcheck_lt(index, size());
  return cast<typehandler>(elements_[index]);
}

template <typename typehandler>
inline typename typehandler::type* repeatedptrfieldbase::add() {
  if (current_size_ < allocated_size_) {
    return cast<typehandler>(elements_[current_size_++]);
  }
  if (allocated_size_ == total_size_) reserve(total_size_ + 1);
  ++allocated_size_;
  typename typehandler::type* result = typehandler::new();
  elements_[current_size_++] = result;
  return result;
}

template <typename typehandler>
inline void repeatedptrfieldbase::removelast() {
  google_dcheck_gt(current_size_, 0);
  typehandler::clear(cast<typehandler>(elements_[--current_size_]));
}

template <typename typehandler>
void repeatedptrfieldbase::clear() {
  for (int i = 0; i < current_size_; i++) {
    typehandler::clear(cast<typehandler>(elements_[i]));
  }
  current_size_ = 0;
}

template <typename typehandler>
inline void repeatedptrfieldbase::mergefrom(const repeatedptrfieldbase& other) {
  reserve(current_size_ + other.current_size_);
  for (int i = 0; i < other.current_size_; i++) {
    typehandler::merge(other.template get<typehandler>(i), add<typehandler>());
  }
}

template <typename typehandler>
inline void repeatedptrfieldbase::copyfrom(const repeatedptrfieldbase& other) {
  repeatedptrfieldbase::clear<typehandler>();
  repeatedptrfieldbase::mergefrom<typehandler>(other);
}

inline int repeatedptrfieldbase::capacity() const {
  return total_size_;
}

inline void* const* repeatedptrfieldbase::raw_data() const {
  return elements_;
}

inline void** repeatedptrfieldbase::raw_mutable_data() const {
  return elements_;
}

template <typename typehandler>
inline typename typehandler::type** repeatedptrfieldbase::mutable_data() {
  // todo(kenton):  breaks c++ aliasing rules.  we should probably remove this
  //   method entirely.
  return reinterpret_cast<typename typehandler::type**>(elements_);
}

template <typename typehandler>
inline const typename typehandler::type* const*
repeatedptrfieldbase::data() const {
  // todo(kenton):  breaks c++ aliasing rules.  we should probably remove this
  //   method entirely.
  return reinterpret_cast<const typename typehandler::type* const*>(elements_);
}

inline void repeatedptrfieldbase::swapelements(int index1, int index2) {
  std::swap(elements_[index1], elements_[index2]);
}

template <typename typehandler>
inline int repeatedptrfieldbase::spaceusedexcludingself() const {
  int allocated_bytes =
      (elements_ != null) ? total_size_ * sizeof(elements_[0]) : 0;
  for (int i = 0; i < allocated_size_; ++i) {
    allocated_bytes += typehandler::spaceused(*cast<typehandler>(elements_[i]));
  }
  return allocated_bytes;
}

template <typename typehandler>
inline typename typehandler::type* repeatedptrfieldbase::addfromcleared() {
  if (current_size_ < allocated_size_) {
    return cast<typehandler>(elements_[current_size_++]);
  } else {
    return null;
  }
}

template <typename typehandler>
void repeatedptrfieldbase::addallocated(
    typename typehandler::type* value) {
  // make room for the new pointer.
  if (current_size_ == total_size_) {
    // the array is completely full with no cleared objects, so grow it.
    reserve(total_size_ + 1);
    ++allocated_size_;
  } else if (allocated_size_ == total_size_) {
    // there is no more space in the pointer array because it contains some
    // cleared objects awaiting reuse.  we don't want to grow the array in this
    // case because otherwise a loop calling addallocated() followed by clear()
    // would leak memory.
    typehandler::delete(cast<typehandler>(elements_[current_size_]));
  } else if (current_size_ < allocated_size_) {
    // we have some cleared objects.  we don't care about their order, so we
    // can just move the first one to the end to make space.
    elements_[allocated_size_] = elements_[current_size_];
    ++allocated_size_;
  } else {
    // there are no cleared objects.
    ++allocated_size_;
  }

  elements_[current_size_++] = value;
}

template <typename typehandler>
inline typename typehandler::type* repeatedptrfieldbase::releaselast() {
  google_dcheck_gt(current_size_, 0);
  typename typehandler::type* result =
      cast<typehandler>(elements_[--current_size_]);
  --allocated_size_;
  if (current_size_ < allocated_size_) {
    // there are cleared elements on the end; replace the removed element
    // with the last allocated element.
    elements_[current_size_] = elements_[allocated_size_];
  }
  return result;
}

inline int repeatedptrfieldbase::clearedcount() const {
  return allocated_size_ - current_size_;
}

template <typename typehandler>
inline void repeatedptrfieldbase::addcleared(
    typename typehandler::type* value) {
  if (allocated_size_ == total_size_) reserve(total_size_ + 1);
  elements_[allocated_size_++] = value;
}

template <typename typehandler>
inline typename typehandler::type* repeatedptrfieldbase::releasecleared() {
  google_dcheck_gt(allocated_size_, current_size_);
  return cast<typehandler>(elements_[--allocated_size_]);
}

}  // namespace internal

// -------------------------------------------------------------------

template <typename element>
class repeatedptrfield<element>::typehandler
    : public internal::generictypehandler<element> {
};

template <>
class repeatedptrfield<string>::typehandler
    : public internal::stringtypehandler {
};


template <typename element>
inline repeatedptrfield<element>::repeatedptrfield() {}

template <typename element>
inline repeatedptrfield<element>::repeatedptrfield(
    const repeatedptrfield& other) {
  copyfrom(other);
}

template <typename element>
template <typename iter>
inline repeatedptrfield<element>::repeatedptrfield(
    iter begin, const iter& end) {
  for (; begin != end; ++begin) {
    *add() = *begin;
  }
}

template <typename element>
repeatedptrfield<element>::~repeatedptrfield() {
  destroy<typehandler>();
}

template <typename element>
inline repeatedptrfield<element>& repeatedptrfield<element>::operator=(
    const repeatedptrfield& other) {
  if (this != &other)
    copyfrom(other);
  return *this;
}

template <typename element>
inline int repeatedptrfield<element>::size() const {
  return repeatedptrfieldbase::size();
}

template <typename element>
inline const element& repeatedptrfield<element>::get(int index) const {
  return repeatedptrfieldbase::get<typehandler>(index);
}


template <typename element>
inline element* repeatedptrfield<element>::mutable(int index) {
  return repeatedptrfieldbase::mutable<typehandler>(index);
}

template <typename element>
inline element* repeatedptrfield<element>::add() {
  return repeatedptrfieldbase::add<typehandler>();
}

template <typename element>
inline void repeatedptrfield<element>::removelast() {
  repeatedptrfieldbase::removelast<typehandler>();
}

template <typename element>
inline void repeatedptrfield<element>::deletesubrange(int start, int num) {
  google_dcheck_ge(start, 0);
  google_dcheck_ge(num, 0);
  google_dcheck_le(start + num, size());
  for (int i = 0; i < num; ++i)
    delete repeatedptrfieldbase::mutable<typehandler>(start + i);
  extractsubrange(start, num, null);
}

template <typename element>
inline void repeatedptrfield<element>::extractsubrange(
    int start, int num, element** elements) {
  google_dcheck_ge(start, 0);
  google_dcheck_ge(num, 0);
  google_dcheck_le(start + num, size());

  if (num > 0) {
    // save the values of the removed elements if requested.
    if (elements != null) {
      for (int i = 0; i < num; ++i)
        elements[i] = repeatedptrfieldbase::mutable<typehandler>(i + start);
    }
    closegap(start, num);
  }
}

template <typename element>
inline void repeatedptrfield<element>::clear() {
  repeatedptrfieldbase::clear<typehandler>();
}

template <typename element>
inline void repeatedptrfield<element>::mergefrom(
    const repeatedptrfield& other) {
  repeatedptrfieldbase::mergefrom<typehandler>(other);
}

template <typename element>
inline void repeatedptrfield<element>::copyfrom(
    const repeatedptrfield& other) {
  repeatedptrfieldbase::copyfrom<typehandler>(other);
}

template <typename element>
inline element** repeatedptrfield<element>::mutable_data() {
  return repeatedptrfieldbase::mutable_data<typehandler>();
}

template <typename element>
inline const element* const* repeatedptrfield<element>::data() const {
  return repeatedptrfieldbase::data<typehandler>();
}

template <typename element>
void repeatedptrfield<element>::swap(repeatedptrfield* other) {
  repeatedptrfieldbase::swap(other);
}

template <typename element>
void repeatedptrfield<element>::swapelements(int index1, int index2) {
  repeatedptrfieldbase::swapelements(index1, index2);
}

template <typename element>
inline int repeatedptrfield<element>::spaceusedexcludingself() const {
  return repeatedptrfieldbase::spaceusedexcludingself<typehandler>();
}

template <typename element>
inline void repeatedptrfield<element>::addallocated(element* value) {
  repeatedptrfieldbase::addallocated<typehandler>(value);
}

template <typename element>
inline element* repeatedptrfield<element>::releaselast() {
  return repeatedptrfieldbase::releaselast<typehandler>();
}


template <typename element>
inline int repeatedptrfield<element>::clearedcount() const {
  return repeatedptrfieldbase::clearedcount();
}

template <typename element>
inline void repeatedptrfield<element>::addcleared(element* value) {
  return repeatedptrfieldbase::addcleared<typehandler>(value);
}

template <typename element>
inline element* repeatedptrfield<element>::releasecleared() {
  return repeatedptrfieldbase::releasecleared<typehandler>();
}

template <typename element>
inline void repeatedptrfield<element>::reserve(int new_size) {
  return repeatedptrfieldbase::reserve(new_size);
}

template <typename element>
inline int repeatedptrfield<element>::capacity() const {
  return repeatedptrfieldbase::capacity();
}

// -------------------------------------------------------------------

namespace internal {

// stl-like iterator implementation for repeatedptrfield.  you should not
// refer to this class directly; use repeatedptrfield<t>::iterator instead.
//
// the iterator for repeatedptrfield<t>, repeatedptriterator<t>, is
// very similar to iterator_ptr<t**> in util/gtl/iterator_adaptors.h,
// but adds random-access operators and is modified to wrap a void** base
// iterator (since repeatedptrfield stores its array as a void* array and
// casting void** to t** would violate c++ aliasing rules).
//
// this code based on net/proto/proto-array-internal.h by jeffrey yasskin
// (jyasskin@google.com).
template<typename element>
class repeatedptriterator
    : public std::iterator<
          std::random_access_iterator_tag, element> {
 public:
  typedef repeatedptriterator<element> iterator;
  typedef std::iterator<
          std::random_access_iterator_tag, element> superclass;

  // let the compiler know that these are type names, so we don't have to
  // write "typename" in front of them everywhere.
  typedef typename superclass::reference reference;
  typedef typename superclass::pointer pointer;
  typedef typename superclass::difference_type difference_type;

  repeatedptriterator() : it_(null) {}
  explicit repeatedptriterator(void* const* it) : it_(it) {}

  // allow "upcasting" from repeatedptriterator<t**> to
  // repeatedptriterator<const t*const*>.
  template<typename otherelement>
  repeatedptriterator(const repeatedptriterator<otherelement>& other)
      : it_(other.it_) {
    // force a compiler error if the other type is not convertible to ours.
    if (false) {
      implicit_cast<element*, otherelement*>(0);
    }
  }

  // dereferenceable
  reference operator*() const { return *reinterpret_cast<element*>(*it_); }
  pointer   operator->() const { return &(operator*()); }

  // {inc,dec}rementable
  iterator& operator++() { ++it_; return *this; }
  iterator  operator++(int) { return iterator(it_++); }
  iterator& operator--() { --it_; return *this; }
  iterator  operator--(int) { return iterator(it_--); }

  // equality_comparable
  bool operator==(const iterator& x) const { return it_ == x.it_; }
  bool operator!=(const iterator& x) const { return it_ != x.it_; }

  // less_than_comparable
  bool operator<(const iterator& x) const { return it_ < x.it_; }
  bool operator<=(const iterator& x) const { return it_ <= x.it_; }
  bool operator>(const iterator& x) const { return it_ > x.it_; }
  bool operator>=(const iterator& x) const { return it_ >= x.it_; }

  // addable, subtractable
  iterator& operator+=(difference_type d) {
    it_ += d;
    return *this;
  }
  friend iterator operator+(iterator it, difference_type d) {
    it += d;
    return it;
  }
  friend iterator operator+(difference_type d, iterator it) {
    it += d;
    return it;
  }
  iterator& operator-=(difference_type d) {
    it_ -= d;
    return *this;
  }
  friend iterator operator-(iterator it, difference_type d) {
    it -= d;
    return it;
  }

  // indexable
  reference operator[](difference_type d) const { return *(*this + d); }

  // random access iterator
  difference_type operator-(const iterator& x) const { return it_ - x.it_; }

 private:
  template<typename otherelement>
  friend class repeatedptriterator;

  // the internal iterator.
  void* const* it_;
};

// provide an iterator that operates on pointers to the underlying objects
// rather than the objects themselves as repeatedptriterator does.
// consider using this when working with stl algorithms that change
// the array.
// the voidptr template parameter holds the type-agnostic pointer value
// referenced by the iterator.  it should either be "void *" for a mutable
// iterator, or "const void *" for a constant iterator.
template<typename element, typename voidptr>
class repeatedptroverptrsiterator
    : public std::iterator<std::random_access_iterator_tag, element*> {
 public:
  typedef repeatedptroverptrsiterator<element, voidptr> iterator;
  typedef std::iterator<
          std::random_access_iterator_tag, element*> superclass;

  // let the compiler know that these are type names, so we don't have to
  // write "typename" in front of them everywhere.
  typedef typename superclass::reference reference;
  typedef typename superclass::pointer pointer;
  typedef typename superclass::difference_type difference_type;

  repeatedptroverptrsiterator() : it_(null) {}
  explicit repeatedptroverptrsiterator(voidptr* it) : it_(it) {}

  // dereferenceable
  reference operator*() const { return *reinterpret_cast<element**>(it_); }
  pointer   operator->() const { return &(operator*()); }

  // {inc,dec}rementable
  iterator& operator++() { ++it_; return *this; }
  iterator  operator++(int) { return iterator(it_++); }
  iterator& operator--() { --it_; return *this; }
  iterator  operator--(int) { return iterator(it_--); }

  // equality_comparable
  bool operator==(const iterator& x) const { return it_ == x.it_; }
  bool operator!=(const iterator& x) const { return it_ != x.it_; }

  // less_than_comparable
  bool operator<(const iterator& x) const { return it_ < x.it_; }
  bool operator<=(const iterator& x) const { return it_ <= x.it_; }
  bool operator>(const iterator& x) const { return it_ > x.it_; }
  bool operator>=(const iterator& x) const { return it_ >= x.it_; }

  // addable, subtractable
  iterator& operator+=(difference_type d) {
    it_ += d;
    return *this;
  }
  friend iterator operator+(iterator it, difference_type d) {
    it += d;
    return it;
  }
  friend iterator operator+(difference_type d, iterator it) {
    it += d;
    return it;
  }
  iterator& operator-=(difference_type d) {
    it_ -= d;
    return *this;
  }
  friend iterator operator-(iterator it, difference_type d) {
    it -= d;
    return it;
  }

  // indexable
  reference operator[](difference_type d) const { return *(*this + d); }

  // random access iterator
  difference_type operator-(const iterator& x) const { return it_ - x.it_; }

 private:
  template<typename otherelement>
  friend class repeatedptriterator;

  // the internal iterator.
  voidptr* it_;
};

}  // namespace internal

template <typename element>
inline typename repeatedptrfield<element>::iterator
repeatedptrfield<element>::begin() {
  return iterator(raw_data());
}
template <typename element>
inline typename repeatedptrfield<element>::const_iterator
repeatedptrfield<element>::begin() const {
  return iterator(raw_data());
}
template <typename element>
inline typename repeatedptrfield<element>::iterator
repeatedptrfield<element>::end() {
  return iterator(raw_data() + size());
}
template <typename element>
inline typename repeatedptrfield<element>::const_iterator
repeatedptrfield<element>::end() const {
  return iterator(raw_data() + size());
}

template <typename element>
inline typename repeatedptrfield<element>::pointer_iterator
repeatedptrfield<element>::pointer_begin() {
  return pointer_iterator(raw_mutable_data());
}
template <typename element>
inline typename repeatedptrfield<element>::const_pointer_iterator
repeatedptrfield<element>::pointer_begin() const {
  return const_pointer_iterator(const_cast<const void**>(raw_mutable_data()));
}
template <typename element>
inline typename repeatedptrfield<element>::pointer_iterator
repeatedptrfield<element>::pointer_end() {
  return pointer_iterator(raw_mutable_data() + size());
}
template <typename element>
inline typename repeatedptrfield<element>::const_pointer_iterator
repeatedptrfield<element>::pointer_end() const {
  return const_pointer_iterator(
      const_cast<const void**>(raw_mutable_data() + size()));
}


// iterators and helper functions that follow the spirit of the stl
// std::back_insert_iterator and std::back_inserter but are tailor-made
// for repeatedfield and repatedptrfield. typical usage would be:
//
//   std::copy(some_sequence.begin(), some_sequence.end(),
//             google::protobuf::repeatedfieldbackinserter(proto.mutable_sequence()));
//
// ported by johannes from util/gtl/proto-array-iterators.h

namespace internal {
// a back inserter for repeatedfield objects.
template<typename t> class repeatedfieldbackinsertiterator
    : public std::iterator<std::output_iterator_tag, t> {
 public:
  explicit repeatedfieldbackinsertiterator(
      repeatedfield<t>* const mutable_field)
      : field_(mutable_field) {
  }
  repeatedfieldbackinsertiterator<t>& operator=(const t& value) {
    field_->add(value);
    return *this;
  }
  repeatedfieldbackinsertiterator<t>& operator*() {
    return *this;
  }
  repeatedfieldbackinsertiterator<t>& operator++() {
    return *this;
  }
  repeatedfieldbackinsertiterator<t>& operator++(int /* unused */) {
    return *this;
  }

 private:
  repeatedfield<t>* field_;
};

// a back inserter for repeatedptrfield objects.
template<typename t> class repeatedptrfieldbackinsertiterator
    : public std::iterator<std::output_iterator_tag, t> {
 public:
  repeatedptrfieldbackinsertiterator(
      repeatedptrfield<t>* const mutable_field)
      : field_(mutable_field) {
  }
  repeatedptrfieldbackinsertiterator<t>& operator=(const t& value) {
    *field_->add() = value;
    return *this;
  }
  repeatedptrfieldbackinsertiterator<t>& operator=(
      const t* const ptr_to_value) {
    *field_->add() = *ptr_to_value;
    return *this;
  }
  repeatedptrfieldbackinsertiterator<t>& operator*() {
    return *this;
  }
  repeatedptrfieldbackinsertiterator<t>& operator++() {
    return *this;
  }
  repeatedptrfieldbackinsertiterator<t>& operator++(int /* unused */) {
    return *this;
  }

 private:
  repeatedptrfield<t>* field_;
};

// a back inserter for repeatedptrfields that inserts by transfering ownership
// of a pointer.
template<typename t> class allocatedrepeatedptrfieldbackinsertiterator
    : public std::iterator<std::output_iterator_tag, t> {
 public:
  explicit allocatedrepeatedptrfieldbackinsertiterator(
      repeatedptrfield<t>* const mutable_field)
      : field_(mutable_field) {
  }
  allocatedrepeatedptrfieldbackinsertiterator<t>& operator=(
      t* const ptr_to_value) {
    field_->addallocated(ptr_to_value);
    return *this;
  }
  allocatedrepeatedptrfieldbackinsertiterator<t>& operator*() {
    return *this;
  }
  allocatedrepeatedptrfieldbackinsertiterator<t>& operator++() {
    return *this;
  }
  allocatedrepeatedptrfieldbackinsertiterator<t>& operator++(
      int /* unused */) {
    return *this;
  }

 private:
  repeatedptrfield<t>* field_;
};
}  // namespace internal

// provides a back insert iterator for repeatedfield instances,
// similar to std::back_inserter().
template<typename t> internal::repeatedfieldbackinsertiterator<t>
repeatedfieldbackinserter(repeatedfield<t>* const mutable_field) {
  return internal::repeatedfieldbackinsertiterator<t>(mutable_field);
}

// provides a back insert iterator for repeatedptrfield instances,
// similar to std::back_inserter().
template<typename t> internal::repeatedptrfieldbackinsertiterator<t>
repeatedptrfieldbackinserter(repeatedptrfield<t>* const mutable_field) {
  return internal::repeatedptrfieldbackinsertiterator<t>(mutable_field);
}

// special back insert iterator for repeatedptrfield instances, just in
// case someone wants to write generic template code that can access both
// repeatedfields and repeatedptrfields using a common name.
template<typename t> internal::repeatedptrfieldbackinsertiterator<t>
repeatedfieldbackinserter(repeatedptrfield<t>* const mutable_field) {
  return internal::repeatedptrfieldbackinsertiterator<t>(mutable_field);
}

// provides a back insert iterator for repeatedptrfield instances
// similar to std::back_inserter() which transfers the ownership while
// copying elements.
template<typename t> internal::allocatedrepeatedptrfieldbackinsertiterator<t>
allocatedrepeatedptrfieldbackinserter(
    repeatedptrfield<t>* const mutable_field) {
  return internal::allocatedrepeatedptrfieldbackinsertiterator<t>(
      mutable_field);
}

}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_repeated_field_h__
