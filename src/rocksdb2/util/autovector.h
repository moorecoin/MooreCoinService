//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
#pragma once

#include <algorithm>
#include <cassert>
#include <stdexcept>
#include <iterator>
#include <vector>

namespace rocksdb {

#ifdef rocksdb_lite
template <class t, size_t ksize = 8>
class autovector : public std::vector<t> {};
#else
// a vector that leverages pre-allocated stack-based array to achieve better
// performance for array with small amount of items.
//
// the interface resembles that of vector, but with less features since we aim
// to solve the problem that we have in hand, rather than implementing a
// full-fledged generic container.
//
// currently we don't support:
//  * reserve()/shrink_to_fit()
//     if used correctly, in most cases, people should not touch the
//     underlying vector at all.
//  * random insert()/erase(), please only use push_back()/pop_back().
//  * no move/swap operations. each autovector instance has a
//     stack-allocated array and if we want support move/swap operations, we
//     need to copy the arrays other than just swapping the pointers. in this
//     case we'll just explicitly forbid these operations since they may
//     lead users to make false assumption by thinking they are inexpensive
//     operations.
//
// naming style of public methods almost follows that of the stl's.
template <class t, size_t ksize = 8>
class autovector {
 public:
  // general stl-style container member types.
  typedef t value_type;
  typedef typename std::vector<t>::difference_type difference_type;
  typedef typename std::vector<t>::size_type size_type;
  typedef value_type& reference;
  typedef const value_type& const_reference;
  typedef value_type* pointer;
  typedef const value_type* const_pointer;

  // this class is the base for regular/const iterator
  template <class tautovector, class tvaluetype>
  class iterator_impl {
   public:
    // -- iterator traits
    typedef iterator_impl<tautovector, tvaluetype> self_type;
    typedef tvaluetype value_type;
    typedef tvaluetype& reference;
    typedef tvaluetype* pointer;
    typedef typename tautovector::difference_type difference_type;
    typedef std::random_access_iterator_tag iterator_category;

    iterator_impl(tautovector* vect, size_t index)
        : vect_(vect), index_(index) {};
    iterator_impl(const iterator_impl&) = default;
    ~iterator_impl() {}
    iterator_impl& operator=(const iterator_impl&) = default;

    // -- advancement
    // ++iterator
    self_type& operator++() {
      ++index_;
      return *this;
    }

    // iterator++
    self_type operator++(int) {
      auto old = *this;
      ++index_;
      return old;
    }

    // --iterator
    self_type& operator--() {
      --index_;
      return *this;
    }

    // iterator--
    self_type operator--(int) {
      auto old = *this;
      --index_;
      return old;
    }

    self_type operator-(difference_type len) {
      return self_type(vect_, index_ - len);
    }

    difference_type operator-(const self_type& other) {
      assert(vect_ == other.vect_);
      return index_ - other.index_;
    }

    self_type operator+(difference_type len) {
      return self_type(vect_, index_ + len);
    }

    self_type& operator+=(difference_type len) {
      index_ += len;
      return *this;
    }

    self_type& operator-=(difference_type len) {
      index_ -= len;
      return *this;
    }

    // -- reference
    reference operator*() {
      assert(vect_->size() >= index_);
      return (*vect_)[index_];
    }
    pointer operator->() {
      assert(vect_->size() >= index_);
      return &(*vect_)[index_];
    }

    // -- logical operators
    bool operator==(const self_type& other) const {
      assert(vect_ == other.vect_);
      return index_ == other.index_;
    }

    bool operator!=(const self_type& other) const { return !(*this == other); }

    bool operator>(const self_type& other) const {
      assert(vect_ == other.vect_);
      return index_ > other.index_;
    }

    bool operator<(const self_type& other) const {
      assert(vect_ == other.vect_);
      return index_ < other.index_;
    }

    bool operator>=(const self_type& other) const {
      assert(vect_ == other.vect_);
      return index_ >= other.index_;
    }

    bool operator<=(const self_type& other) const {
      assert(vect_ == other.vect_);
      return index_ <= other.index_;
    }

   private:
    tautovector* vect_ = nullptr;
    size_t index_ = 0;
  };

  typedef iterator_impl<autovector, value_type> iterator;
  typedef iterator_impl<const autovector, const value_type> const_iterator;
  typedef std::reverse_iterator<iterator> reverse_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  autovector() = default;
  ~autovector() = default;

  // -- immutable operations
  // indicate if all data resides in in-stack data structure.
  bool only_in_stack() const {
    // if no element was inserted at all, the vector's capacity will be `0`.
    return vect_.capacity() == 0;
  }

  size_type size() const { return num_stack_items_ + vect_.size(); }

  // resize does not guarantee anything about the contents of the newly
  // available elements
  void resize(size_type n) {
    if (n > ksize) {
      vect_.resize(n - ksize);
      num_stack_items_ = ksize;
    } else {
      vect_.clear();
      num_stack_items_ = n;
    }
  }

  bool empty() const { return size() == 0; }

  // will not check boundry
  const_reference operator[](size_type n) const {
    return n < ksize ? values_[n] : vect_[n - ksize];
  }

  reference operator[](size_type n) {
    return n < ksize ? values_[n] : vect_[n - ksize];
  }

  // will check boundry
  const_reference at(size_type n) const {
    if (n >= size()) {
      throw std::out_of_range("autovector: index out of range");
    }
    return (*this)[n];
  }

  reference at(size_type n) {
    if (n >= size()) {
      throw std::out_of_range("autovector: index out of range");
    }
    return (*this)[n];
  }

  reference front() {
    assert(!empty());
    return *begin();
  }

  const_reference front() const {
    assert(!empty());
    return *begin();
  }

  reference back() {
    assert(!empty());
    return *(end() - 1);
  }

  const_reference back() const {
    assert(!empty());
    return *(end() - 1);
  }

  // -- mutable operations
  void push_back(t&& item) {
    if (num_stack_items_ < ksize) {
      values_[num_stack_items_++] = std::move(item);
    } else {
      vect_.push_back(item);
    }
  }

  void push_back(const t& item) { push_back(value_type(item)); }

  template <class... args>
  void emplace_back(args&&... args) {
    push_back(value_type(args...));
  }

  void pop_back() {
    assert(!empty());
    if (!vect_.empty()) {
      vect_.pop_back();
    } else {
      --num_stack_items_;
    }
  }

  void clear() {
    num_stack_items_ = 0;
    vect_.clear();
  }

  // -- copy and assignment
  autovector& assign(const autovector& other);

  autovector(const autovector& other) { assign(other); }

  autovector& operator=(const autovector& other) { return assign(other); }

  // move operation are disallowed since it is very hard to make sure both
  // autovectors are allocated from the same function stack.
  autovector& operator=(autovector&& other) = delete;
  autovector(autovector&& other) = delete;

  // -- iterator operations
  iterator begin() { return iterator(this, 0); }

  const_iterator begin() const { return const_iterator(this, 0); }

  iterator end() { return iterator(this, this->size()); }

  const_iterator end() const { return const_iterator(this, this->size()); }

  reverse_iterator rbegin() { return reverse_iterator(end()); }

  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }

  reverse_iterator rend() { return reverse_iterator(begin()); }

  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }

 private:
  size_type num_stack_items_ = 0;  // current number of items
  value_type values_[ksize];       // the first `ksize` items
  // used only if there are more than `ksize` items.
  std::vector<t> vect_;
};

template <class t, size_t ksize>
autovector<t, ksize>& autovector<t, ksize>::assign(const autovector& other) {
  // copy the internal vector
  vect_.assign(other.vect_.begin(), other.vect_.end());

  // copy array
  num_stack_items_ = other.num_stack_items_;
  std::copy(other.values_, other.values_ + num_stack_items_, values_);

  return *this;
}
#endif  // rocksdb_lite
}  // namespace rocksdb
