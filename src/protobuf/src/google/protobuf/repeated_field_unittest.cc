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
// todo(kenton):  improve this unittest to bring it up to the standards of
//   other proto2 unittests.

#include <algorithm>
#include <limits>
#include <list>
#include <vector>

#include <google/protobuf/repeated_field.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>
#include <google/protobuf/stubs/stl_util.h>

namespace google {
using protobuf_unittest::testalltypes;

namespace protobuf {
namespace {

// test operations on a small repeatedfield.
test(repeatedfield, small) {
  repeatedfield<int> field;

  expect_eq(field.size(), 0);

  field.add(5);

  expect_eq(field.size(), 1);
  expect_eq(field.get(0), 5);

  field.add(42);

  expect_eq(field.size(), 2);
  expect_eq(field.get(0), 5);
  expect_eq(field.get(1), 42);

  field.set(1, 23);

  expect_eq(field.size(), 2);
  expect_eq(field.get(0), 5);
  expect_eq(field.get(1), 23);

  field.removelast();

  expect_eq(field.size(), 1);
  expect_eq(field.get(0), 5);

  field.clear();

  expect_eq(field.size(), 0);
  int expected_usage = 4 * sizeof(int);
  expect_eq(field.spaceusedexcludingself(), expected_usage);
}


// test operations on a repeatedfield which is large enough to allocate a
// separate array.
test(repeatedfield, large) {
  repeatedfield<int> field;

  for (int i = 0; i < 16; i++) {
    field.add(i * i);
  }

  expect_eq(field.size(), 16);

  for (int i = 0; i < 16; i++) {
    expect_eq(field.get(i), i * i);
  }

  int expected_usage = 16 * sizeof(int);
  expect_ge(field.spaceusedexcludingself(), expected_usage);
}

// test swapping between various types of repeatedfields.
test(repeatedfield, swapsmallsmall) {
  repeatedfield<int> field1;
  repeatedfield<int> field2;

  field1.add(5);
  field1.add(42);

  field1.swap(&field2);

  expect_eq(field1.size(), 0);
  expect_eq(field2.size(), 2);
  expect_eq(field2.get(0), 5);
  expect_eq(field2.get(1), 42);
}

test(repeatedfield, swaplargesmall) {
  repeatedfield<int> field1;
  repeatedfield<int> field2;

  for (int i = 0; i < 16; i++) {
    field1.add(i * i);
  }
  field2.add(5);
  field2.add(42);
  field1.swap(&field2);

  expect_eq(field1.size(), 2);
  expect_eq(field1.get(0), 5);
  expect_eq(field1.get(1), 42);
  expect_eq(field2.size(), 16);
  for (int i = 0; i < 16; i++) {
    expect_eq(field2.get(i), i * i);
  }
}

test(repeatedfield, swaplargelarge) {
  repeatedfield<int> field1;
  repeatedfield<int> field2;

  field1.add(5);
  field1.add(42);
  for (int i = 0; i < 16; i++) {
    field1.add(i);
    field2.add(i * i);
  }
  field2.swap(&field1);

  expect_eq(field1.size(), 16);
  for (int i = 0; i < 16; i++) {
    expect_eq(field1.get(i), i * i);
  }
  expect_eq(field2.size(), 18);
  expect_eq(field2.get(0), 5);
  expect_eq(field2.get(1), 42);
  for (int i = 2; i < 18; i++) {
    expect_eq(field2.get(i), i - 2);
  }
}

// determines how much space was reserved by the given field by adding elements
// to it until it re-allocates its space.
static int reservedspace(repeatedfield<int>* field) {
  const int* ptr = field->data();
  do {
    field->add(0);
  } while (field->data() == ptr);

  return field->size() - 1;
}

test(repeatedfield, reservemorethandouble) {
  // reserve more than double the previous space in the field and expect the
  // field to reserve exactly the amount specified.
  repeatedfield<int> field;
  field.reserve(20);

  expect_eq(20, reservedspace(&field));
}

test(repeatedfield, reservelessthandouble) {
  // reserve less than double the previous space in the field and expect the
  // field to grow by double instead.
  repeatedfield<int> field;
  field.reserve(20);
  field.reserve(30);

  expect_eq(40, reservedspace(&field));
}

test(repeatedfield, reservelessthanexisting) {
  // reserve less than the previous space in the field and expect the
  // field to not re-allocate at all.
  repeatedfield<int> field;
  field.reserve(20);
  const int* previous_ptr = field.data();
  field.reserve(10);

  expect_eq(previous_ptr, field.data());
  expect_eq(20, reservedspace(&field));
}

test(repeatedfield, mergefrom) {
  repeatedfield<int> source, destination;
  source.add(4);
  source.add(5);
  destination.add(1);
  destination.add(2);
  destination.add(3);

  destination.mergefrom(source);

  assert_eq(5, destination.size());
  expect_eq(1, destination.get(0));
  expect_eq(2, destination.get(1));
  expect_eq(3, destination.get(2));
  expect_eq(4, destination.get(3));
  expect_eq(5, destination.get(4));
}

test(repeatedfield, copyfrom) {
  repeatedfield<int> source, destination;
  source.add(4);
  source.add(5);
  destination.add(1);
  destination.add(2);
  destination.add(3);

  destination.copyfrom(source);

  assert_eq(2, destination.size());
  expect_eq(4, destination.get(0));
  expect_eq(5, destination.get(1));
}

test(repeatedfield, copyconstruct) {
  repeatedfield<int> source;
  source.add(1);
  source.add(2);

  repeatedfield<int> destination(source);

  assert_eq(2, destination.size());
  expect_eq(1, destination.get(0));
  expect_eq(2, destination.get(1));
}

test(repeatedfield, iteratorconstruct) {
  vector<int> values;
  values.push_back(1);
  values.push_back(2);

  repeatedfield<int> field(values.begin(), values.end());
  assert_eq(values.size(), field.size());
  expect_eq(values[0], field.get(0));
  expect_eq(values[1], field.get(1));

  repeatedfield<int> other(field.begin(), field.end());
  assert_eq(values.size(), other.size());
  expect_eq(values[0], other.get(0));
  expect_eq(values[1], other.get(1));
}

test(repeatedfield, copyassign) {
  repeatedfield<int> source, destination;
  source.add(4);
  source.add(5);
  destination.add(1);
  destination.add(2);
  destination.add(3);

  destination = source;

  assert_eq(2, destination.size());
  expect_eq(4, destination.get(0));
  expect_eq(5, destination.get(1));
}

test(repeatedfield, selfassign) {
  // verify that assignment to self does not destroy data.
  repeatedfield<int> source, *p;
  p = &source;
  source.add(7);
  source.add(8);

  *p = source;

  assert_eq(2, source.size());
  expect_eq(7, source.get(0));
  expect_eq(8, source.get(1));
}

test(repeatedfield, mutabledataismutable) {
  repeatedfield<int> field;
  field.add(1);
  expect_eq(1, field.get(0));
  // the fact that this line compiles would be enough, but we'll check the
  // value anyway.
  *field.mutable_data() = 2;
  expect_eq(2, field.get(0));
}

test(repeatedfield, truncate) {
  repeatedfield<int> field;

  field.add(12);
  field.add(34);
  field.add(56);
  field.add(78);
  expect_eq(4, field.size());

  field.truncate(3);
  expect_eq(3, field.size());

  field.add(90);
  expect_eq(4, field.size());
  expect_eq(90, field.get(3));

  // truncations that don't change the size are allowed, but growing is not
  // allowed.
  field.truncate(field.size());
#ifdef protobuf_has_death_test
  expect_debug_death(field.truncate(field.size() + 1), "new_size");
#endif
}


test(repeatedfield, extractsubrange) {
  // exhaustively test every subrange in arrays of all sizes from 0 through 9.
  for (int sz = 0; sz < 10; ++sz) {
    for (int num = 0; num <= sz; ++num) {
      for (int start = 0; start < sz - num; ++start) {
        // create repeatedfield with sz elements having values 0 through sz-1.
        repeatedfield<int32> field;
        for (int i = 0; i < sz; ++i)
          field.add(i);
        expect_eq(field.size(), sz);

        // create a catcher array and call extractsubrange.
        int32 catcher[10];
        for (int i = 0; i < 10; ++i)
          catcher[i] = -1;
        field.extractsubrange(start, num, catcher);

        // does the resulting array have the right size?
        expect_eq(field.size(), sz - num);

        // were the removed elements extracted into the catcher array?
        for (int i = 0; i < num; ++i)
          expect_eq(catcher[i], start + i);
        expect_eq(catcher[num], -1);

        // does the resulting array contain the right values?
        for (int i = 0; i < start; ++i)
          expect_eq(field.get(i), i);
        for (int i = start; i < field.size(); ++i)
          expect_eq(field.get(i), i + num);
      }
    }
  }
}

// ===================================================================
// repeatedptrfield tests.  these pretty much just mirror the repeatedfield
// tests above.

test(repeatedptrfield, small) {
  repeatedptrfield<string> field;

  expect_eq(field.size(), 0);

  field.add()->assign("foo");

  expect_eq(field.size(), 1);
  expect_eq(field.get(0), "foo");

  field.add()->assign("bar");

  expect_eq(field.size(), 2);
  expect_eq(field.get(0), "foo");
  expect_eq(field.get(1), "bar");

  field.mutable(1)->assign("baz");

  expect_eq(field.size(), 2);
  expect_eq(field.get(0), "foo");
  expect_eq(field.get(1), "baz");

  field.removelast();

  expect_eq(field.size(), 1);
  expect_eq(field.get(0), "foo");

  field.clear();

  expect_eq(field.size(), 0);
}


test(repeatedptrfield, large) {
  repeatedptrfield<string> field;

  for (int i = 0; i < 16; i++) {
    *field.add() += 'a' + i;
  }

  expect_eq(field.size(), 16);

  for (int i = 0; i < 16; i++) {
    expect_eq(field.get(i).size(), 1);
    expect_eq(field.get(i)[0], 'a' + i);
  }

  int min_expected_usage = 16 * sizeof(string);
  expect_ge(field.spaceusedexcludingself(), min_expected_usage);
}

test(repeatedptrfield, swapsmallsmall) {
  repeatedptrfield<string> field1;
  repeatedptrfield<string> field2;

  field1.add()->assign("foo");
  field1.add()->assign("bar");
  field1.swap(&field2);

  expect_eq(field1.size(), 0);
  expect_eq(field2.size(), 2);
  expect_eq(field2.get(0), "foo");
  expect_eq(field2.get(1), "bar");
}

test(repeatedptrfield, swaplargesmall) {
  repeatedptrfield<string> field1;
  repeatedptrfield<string> field2;

  field2.add()->assign("foo");
  field2.add()->assign("bar");
  for (int i = 0; i < 16; i++) {
    *field1.add() += 'a' + i;
  }
  field1.swap(&field2);

  expect_eq(field1.size(), 2);
  expect_eq(field1.get(0), "foo");
  expect_eq(field1.get(1), "bar");
  expect_eq(field2.size(), 16);
  for (int i = 0; i < 16; i++) {
    expect_eq(field2.get(i).size(), 1);
    expect_eq(field2.get(i)[0], 'a' + i);
  }
}

test(repeatedptrfield, swaplargelarge) {
  repeatedptrfield<string> field1;
  repeatedptrfield<string> field2;

  field1.add()->assign("foo");
  field1.add()->assign("bar");
  for (int i = 0; i < 16; i++) {
    *field1.add() += 'a' + i;
    *field2.add() += 'a' + i;
  }
  field2.swap(&field1);

  expect_eq(field1.size(), 16);
  for (int i = 0; i < 16; i++) {
    expect_eq(field1.get(i).size(), 1);
    expect_eq(field1.get(i)[0], 'a' + i);
  }
  expect_eq(field2.size(), 18);
  expect_eq(field2.get(0), "foo");
  expect_eq(field2.get(1), "bar");
  for (int i = 2; i < 18; i++) {
    expect_eq(field2.get(i).size(), 1);
    expect_eq(field2.get(i)[0], 'a' + i - 2);
  }
}

static int reservedspace(repeatedptrfield<string>* field) {
  const string* const* ptr = field->data();
  do {
    field->add();
  } while (field->data() == ptr);

  return field->size() - 1;
}

test(repeatedptrfield, reservemorethandouble) {
  repeatedptrfield<string> field;
  field.reserve(20);

  expect_eq(20, reservedspace(&field));
}

test(repeatedptrfield, reservelessthandouble) {
  repeatedptrfield<string> field;
  field.reserve(20);
  field.reserve(30);

  expect_eq(40, reservedspace(&field));
}

test(repeatedptrfield, reservelessthanexisting) {
  repeatedptrfield<string> field;
  field.reserve(20);
  const string* const* previous_ptr = field.data();
  field.reserve(10);

  expect_eq(previous_ptr, field.data());
  expect_eq(20, reservedspace(&field));
}

test(repeatedptrfield, reservedoesntloseallocated) {
  // check that a bug is fixed:  an earlier implementation of reserve()
  // failed to copy pointers to allocated-but-cleared objects, possibly
  // leading to segfaults.
  repeatedptrfield<string> field;
  string* first = field.add();
  field.removelast();

  field.reserve(20);
  expect_eq(first, field.add());
}

// clearing elements is tricky with repeatedptrfields since the memory for
// the elements is retained and reused.
test(repeatedptrfield, clearedelements) {
  repeatedptrfield<string> field;

  string* original = field.add();
  *original = "foo";

  expect_eq(field.clearedcount(), 0);

  field.removelast();
  expect_true(original->empty());
  expect_eq(field.clearedcount(), 1);

  expect_eq(field.add(), original);  // should return same string for reuse.

  expect_eq(field.releaselast(), original);  // we take ownership.
  expect_eq(field.clearedcount(), 0);

  expect_ne(field.add(), original);  // should not return the same string.
  expect_eq(field.clearedcount(), 0);

  field.addallocated(original);  // give ownership back.
  expect_eq(field.clearedcount(), 0);
  expect_eq(field.mutable(1), original);

  field.clear();
  expect_eq(field.clearedcount(), 2);
  expect_eq(field.releasecleared(), original);  // take ownership again.
  expect_eq(field.clearedcount(), 1);
  expect_ne(field.add(), original);
  expect_eq(field.clearedcount(), 0);
  expect_ne(field.add(), original);
  expect_eq(field.clearedcount(), 0);

  field.addcleared(original);  // give ownership back, but as a cleared object.
  expect_eq(field.clearedcount(), 1);
  expect_eq(field.add(), original);
  expect_eq(field.clearedcount(), 0);
}

// test all code paths in addallocated().
test(repeatedptrfield, addalocated) {
  repeatedptrfield<string> field;
  while (field.size() < field.capacity()) {
    field.add()->assign("filler");
  }

  int index = field.size();

  // first branch:  field is at capacity with no cleared objects.
  string* foo = new string("foo");
  field.addallocated(foo);
  expect_eq(index + 1, field.size());
  expect_eq(0, field.clearedcount());
  expect_eq(foo, &field.get(index));

  // last branch:  field is not at capacity and there are no cleared objects.
  string* bar = new string("bar");
  field.addallocated(bar);
  ++index;
  expect_eq(index + 1, field.size());
  expect_eq(0, field.clearedcount());
  expect_eq(bar, &field.get(index));

  // third branch:  field is not at capacity and there are no cleared objects.
  field.removelast();
  string* baz = new string("baz");
  field.addallocated(baz);
  expect_eq(index + 1, field.size());
  expect_eq(1, field.clearedcount());
  expect_eq(baz, &field.get(index));

  // second branch:  field is at capacity but has some cleared objects.
  while (field.size() < field.capacity()) {
    field.add()->assign("filler2");
  }
  field.removelast();
  index = field.size();
  string* qux = new string("qux");
  field.addallocated(qux);
  expect_eq(index + 1, field.size());
  // we should have discarded the cleared object.
  expect_eq(0, field.clearedcount());
  expect_eq(qux, &field.get(index));
}

test(repeatedptrfield, mergefrom) {
  repeatedptrfield<string> source, destination;
  source.add()->assign("4");
  source.add()->assign("5");
  destination.add()->assign("1");
  destination.add()->assign("2");
  destination.add()->assign("3");

  destination.mergefrom(source);

  assert_eq(5, destination.size());
  expect_eq("1", destination.get(0));
  expect_eq("2", destination.get(1));
  expect_eq("3", destination.get(2));
  expect_eq("4", destination.get(3));
  expect_eq("5", destination.get(4));
}

test(repeatedptrfield, copyfrom) {
  repeatedptrfield<string> source, destination;
  source.add()->assign("4");
  source.add()->assign("5");
  destination.add()->assign("1");
  destination.add()->assign("2");
  destination.add()->assign("3");

  destination.copyfrom(source);

  assert_eq(2, destination.size());
  expect_eq("4", destination.get(0));
  expect_eq("5", destination.get(1));
}

test(repeatedptrfield, copyconstruct) {
  repeatedptrfield<string> source;
  source.add()->assign("1");
  source.add()->assign("2");

  repeatedptrfield<string> destination(source);

  assert_eq(2, destination.size());
  expect_eq("1", destination.get(0));
  expect_eq("2", destination.get(1));
}

test(repeatedptrfield, iteratorconstruct_string) {
  vector<string> values;
  values.push_back("1");
  values.push_back("2");

  repeatedptrfield<string> field(values.begin(), values.end());
  assert_eq(values.size(), field.size());
  expect_eq(values[0], field.get(0));
  expect_eq(values[1], field.get(1));

  repeatedptrfield<string> other(field.begin(), field.end());
  assert_eq(values.size(), other.size());
  expect_eq(values[0], other.get(0));
  expect_eq(values[1], other.get(1));
}

test(repeatedptrfield, iteratorconstruct_proto) {
  typedef testalltypes::nestedmessage nested;
  vector<nested> values;
  values.push_back(nested());
  values.back().set_bb(1);
  values.push_back(nested());
  values.back().set_bb(2);

  repeatedptrfield<nested> field(values.begin(), values.end());
  assert_eq(values.size(), field.size());
  expect_eq(values[0].bb(), field.get(0).bb());
  expect_eq(values[1].bb(), field.get(1).bb());

  repeatedptrfield<nested> other(field.begin(), field.end());
  assert_eq(values.size(), other.size());
  expect_eq(values[0].bb(), other.get(0).bb());
  expect_eq(values[1].bb(), other.get(1).bb());
}

test(repeatedptrfield, copyassign) {
  repeatedptrfield<string> source, destination;
  source.add()->assign("4");
  source.add()->assign("5");
  destination.add()->assign("1");
  destination.add()->assign("2");
  destination.add()->assign("3");

  destination = source;

  assert_eq(2, destination.size());
  expect_eq("4", destination.get(0));
  expect_eq("5", destination.get(1));
}

test(repeatedptrfield, selfassign) {
  // verify that assignment to self does not destroy data.
  repeatedptrfield<string> source, *p;
  p = &source;
  source.add()->assign("7");
  source.add()->assign("8");

  *p = source;

  assert_eq(2, source.size());
  expect_eq("7", source.get(0));
  expect_eq("8", source.get(1));
}

test(repeatedptrfield, mutabledataismutable) {
  repeatedptrfield<string> field;
  *field.add() = "1";
  expect_eq("1", field.get(0));
  // the fact that this line compiles would be enough, but we'll check the
  // value anyway.
  string** data = field.mutable_data();
  **data = "2";
  expect_eq("2", field.get(0));
}

test(repeatedptrfield, extractsubrange) {
  // exhaustively test every subrange in arrays of all sizes from 0 through 9
  // with 0 through 3 cleared elements at the end.
  for (int sz = 0; sz < 10; ++sz) {
    for (int num = 0; num <= sz; ++num) {
      for (int start = 0; start < sz - num; ++start) {
        for (int extra = 0; extra < 4; ++extra) {
          vector<string*> subject;

          // create an array with "sz" elements and "extra" cleared elements.
          repeatedptrfield<string> field;
          for (int i = 0; i < sz + extra; ++i) {
            subject.push_back(new string());
            field.addallocated(subject[i]);
          }
          expect_eq(field.size(), sz + extra);
          for (int i = 0; i < extra; ++i)
            field.removelast();
          expect_eq(field.size(), sz);
          expect_eq(field.clearedcount(), extra);

          // create a catcher array and call extractsubrange.
          string* catcher[10];
          for (int i = 0; i < 10; ++i)
            catcher[i] = null;
          field.extractsubrange(start, num, catcher);

          // does the resulting array have the right size?
          expect_eq(field.size(), sz - num);

          // were the removed elements extracted into the catcher array?
          for (int i = 0; i < num; ++i)
            expect_eq(catcher[i], subject[start + i]);
          expect_eq(null, catcher[num]);

          // does the resulting array contain the right values?
          for (int i = 0; i < start; ++i)
            expect_eq(field.mutable(i), subject[i]);
          for (int i = start; i < field.size(); ++i)
            expect_eq(field.mutable(i), subject[i + num]);

          // reinstate the cleared elements.
          expect_eq(field.clearedcount(), extra);
          for (int i = 0; i < extra; ++i)
            field.add();
          expect_eq(field.clearedcount(), 0);
          expect_eq(field.size(), sz - num + extra);

          // make sure the extra elements are all there (in some order).
          for (int i = sz; i < sz + extra; ++i) {
            int count = 0;
            for (int j = sz; j < sz + extra; ++j) {
              if (field.mutable(j - num) == subject[i])
                count += 1;
            }
            expect_eq(count, 1);
          }

          // release the caught elements.
          for (int i = 0; i < num; ++i)
            delete catcher[i];
        }
      }
    }
  }
}

test(repeatedptrfield, deletesubrange) {
  // deletesubrange is a trivial extension of extendsubrange.
}

// ===================================================================

// iterator tests stolen from net/proto/proto-array_unittest.
class repeatedfielditeratortest : public testing::test {
 protected:
  virtual void setup() {
    for (int i = 0; i < 3; ++i) {
      proto_array_.add(i);
    }
  }

  repeatedfield<int> proto_array_;
};

test_f(repeatedfielditeratortest, convertible) {
  repeatedfield<int>::iterator iter = proto_array_.begin();
  repeatedfield<int>::const_iterator c_iter = iter;
  repeatedfield<int>::value_type value = *c_iter;
  expect_eq(0, value);
}

test_f(repeatedfielditeratortest, mutableiteration) {
  repeatedfield<int>::iterator iter = proto_array_.begin();
  expect_eq(0, *iter);
  ++iter;
  expect_eq(1, *iter++);
  expect_eq(2, *iter);
  ++iter;
  expect_true(proto_array_.end() == iter);

  expect_eq(2, *(proto_array_.end() - 1));
}

test_f(repeatedfielditeratortest, constiteration) {
  const repeatedfield<int>& const_proto_array = proto_array_;
  repeatedfield<int>::const_iterator iter = const_proto_array.begin();
  expect_eq(0, *iter);
  ++iter;
  expect_eq(1, *iter++);
  expect_eq(2, *iter);
  ++iter;
  expect_true(proto_array_.end() == iter);
  expect_eq(2, *(proto_array_.end() - 1));
}

test_f(repeatedfielditeratortest, mutation) {
  repeatedfield<int>::iterator iter = proto_array_.begin();
  *iter = 7;
  expect_eq(7, proto_array_.get(0));
}

// -------------------------------------------------------------------

class repeatedptrfielditeratortest : public testing::test {
 protected:
  virtual void setup() {
    proto_array_.add()->assign("foo");
    proto_array_.add()->assign("bar");
    proto_array_.add()->assign("baz");
  }

  repeatedptrfield<string> proto_array_;
};

test_f(repeatedptrfielditeratortest, convertible) {
  repeatedptrfield<string>::iterator iter = proto_array_.begin();
  repeatedptrfield<string>::const_iterator c_iter = iter;
  repeatedptrfield<string>::value_type value = *c_iter;
  expect_eq("foo", value);
}

test_f(repeatedptrfielditeratortest, mutableiteration) {
  repeatedptrfield<string>::iterator iter = proto_array_.begin();
  expect_eq("foo", *iter);
  ++iter;
  expect_eq("bar", *(iter++));
  expect_eq("baz", *iter);
  ++iter;
  expect_true(proto_array_.end() == iter);
  expect_eq("baz", *(--proto_array_.end()));
}

test_f(repeatedptrfielditeratortest, constiteration) {
  const repeatedptrfield<string>& const_proto_array = proto_array_;
  repeatedptrfield<string>::const_iterator iter = const_proto_array.begin();
  expect_eq("foo", *iter);
  ++iter;
  expect_eq("bar", *(iter++));
  expect_eq("baz", *iter);
  ++iter;
  expect_true(const_proto_array.end() == iter);
  expect_eq("baz", *(--const_proto_array.end()));
}

test_f(repeatedptrfielditeratortest, mutablereverseiteration) {
  repeatedptrfield<string>::reverse_iterator iter = proto_array_.rbegin();
  expect_eq("baz", *iter);
  ++iter;
  expect_eq("bar", *(iter++));
  expect_eq("foo", *iter);
  ++iter;
  expect_true(proto_array_.rend() == iter);
  expect_eq("foo", *(--proto_array_.rend()));
}

test_f(repeatedptrfielditeratortest, constreverseiteration) {
  const repeatedptrfield<string>& const_proto_array = proto_array_;
  repeatedptrfield<string>::const_reverse_iterator iter
      = const_proto_array.rbegin();
  expect_eq("baz", *iter);
  ++iter;
  expect_eq("bar", *(iter++));
  expect_eq("foo", *iter);
  ++iter;
  expect_true(const_proto_array.rend() == iter);
  expect_eq("foo", *(--const_proto_array.rend()));
}

test_f(repeatedptrfielditeratortest, randomaccess) {
  repeatedptrfield<string>::iterator iter = proto_array_.begin();
  repeatedptrfield<string>::iterator iter2 = iter;
  ++iter2;
  ++iter2;
  expect_true(iter + 2 == iter2);
  expect_true(iter == iter2 - 2);
  expect_eq("baz", iter[2]);
  expect_eq("baz", *(iter + 2));
  expect_eq(3, proto_array_.end() - proto_array_.begin());
}

test_f(repeatedptrfielditeratortest, comparable) {
  repeatedptrfield<string>::const_iterator iter = proto_array_.begin();
  repeatedptrfield<string>::const_iterator iter2 = iter + 1;
  expect_true(iter == iter);
  expect_true(iter != iter2);
  expect_true(iter < iter2);
  expect_true(iter <= iter2);
  expect_true(iter <= iter);
  expect_true(iter2 > iter);
  expect_true(iter2 >= iter);
  expect_true(iter >= iter);
}

// uninitialized iterator does not point to any of the repeatedptrfield.
test_f(repeatedptrfielditeratortest, uninitializediterator) {
  repeatedptrfield<string>::iterator iter;
  expect_true(iter != proto_array_.begin());
  expect_true(iter != proto_array_.begin() + 1);
  expect_true(iter != proto_array_.begin() + 2);
  expect_true(iter != proto_array_.begin() + 3);
  expect_true(iter != proto_array_.end());
}

test_f(repeatedptrfielditeratortest, stlalgorithms_lower_bound) {
  proto_array_.clear();
  proto_array_.add()->assign("a");
  proto_array_.add()->assign("c");
  proto_array_.add()->assign("d");
  proto_array_.add()->assign("n");
  proto_array_.add()->assign("p");
  proto_array_.add()->assign("x");
  proto_array_.add()->assign("y");

  string v = "f";
  repeatedptrfield<string>::const_iterator it =
      lower_bound(proto_array_.begin(), proto_array_.end(), v);

  expect_eq(*it, "n");
  expect_true(it == proto_array_.begin() + 3);
}

test_f(repeatedptrfielditeratortest, mutation) {
  repeatedptrfield<string>::iterator iter = proto_array_.begin();
  *iter = "qux";
  expect_eq("qux", proto_array_.get(0));
}

// -------------------------------------------------------------------

class repeatedptrfieldptrsiteratortest : public testing::test {
 protected:
  virtual void setup() {
    proto_array_.add()->assign("foo");
    proto_array_.add()->assign("bar");
    proto_array_.add()->assign("baz");
    const_proto_array_ = &proto_array_;
  }

  repeatedptrfield<string> proto_array_;
  const repeatedptrfield<string>* const_proto_array_;
};

test_f(repeatedptrfieldptrsiteratortest, convertibleptr) {
  repeatedptrfield<string>::pointer_iterator iter =
      proto_array_.pointer_begin();
  (void) iter;
}

test_f(repeatedptrfieldptrsiteratortest, convertibleconstptr) {
  repeatedptrfield<string>::const_pointer_iterator iter =
      const_proto_array_->pointer_begin();
  (void) iter;
}

test_f(repeatedptrfieldptrsiteratortest, mutableptriteration) {
  repeatedptrfield<string>::pointer_iterator iter =
      proto_array_.pointer_begin();
  expect_eq("foo", **iter);
  ++iter;
  expect_eq("bar", **(iter++));
  expect_eq("baz", **iter);
  ++iter;
  expect_true(proto_array_.pointer_end() == iter);
  expect_eq("baz", **(--proto_array_.pointer_end()));
}

test_f(repeatedptrfieldptrsiteratortest, mutableconstptriteration) {
  repeatedptrfield<string>::const_pointer_iterator iter =
      const_proto_array_->pointer_begin();
  expect_eq("foo", **iter);
  ++iter;
  expect_eq("bar", **(iter++));
  expect_eq("baz", **iter);
  ++iter;
  expect_true(const_proto_array_->pointer_end() == iter);
  expect_eq("baz", **(--const_proto_array_->pointer_end()));
}

test_f(repeatedptrfieldptrsiteratortest, randomptraccess) {
  repeatedptrfield<string>::pointer_iterator iter =
      proto_array_.pointer_begin();
  repeatedptrfield<string>::pointer_iterator iter2 = iter;
  ++iter2;
  ++iter2;
  expect_true(iter + 2 == iter2);
  expect_true(iter == iter2 - 2);
  expect_eq("baz", *iter[2]);
  expect_eq("baz", **(iter + 2));
  expect_eq(3, proto_array_.end() - proto_array_.begin());
}

test_f(repeatedptrfieldptrsiteratortest, randomconstptraccess) {
  repeatedptrfield<string>::const_pointer_iterator iter =
      const_proto_array_->pointer_begin();
  repeatedptrfield<string>::const_pointer_iterator iter2 = iter;
  ++iter2;
  ++iter2;
  expect_true(iter + 2 == iter2);
  expect_true(iter == iter2 - 2);
  expect_eq("baz", *iter[2]);
  expect_eq("baz", **(iter + 2));
  expect_eq(3, const_proto_array_->end() - const_proto_array_->begin());
}

test_f(repeatedptrfieldptrsiteratortest, comparableptr) {
  repeatedptrfield<string>::pointer_iterator iter =
      proto_array_.pointer_begin();
  repeatedptrfield<string>::pointer_iterator iter2 = iter + 1;
  expect_true(iter == iter);
  expect_true(iter != iter2);
  expect_true(iter < iter2);
  expect_true(iter <= iter2);
  expect_true(iter <= iter);
  expect_true(iter2 > iter);
  expect_true(iter2 >= iter);
  expect_true(iter >= iter);
}

test_f(repeatedptrfieldptrsiteratortest, comparableconstptr) {
  repeatedptrfield<string>::const_pointer_iterator iter =
      const_proto_array_->pointer_begin();
  repeatedptrfield<string>::const_pointer_iterator iter2 = iter + 1;
  expect_true(iter == iter);
  expect_true(iter != iter2);
  expect_true(iter < iter2);
  expect_true(iter <= iter2);
  expect_true(iter <= iter);
  expect_true(iter2 > iter);
  expect_true(iter2 >= iter);
  expect_true(iter >= iter);
}

// uninitialized iterator does not point to any of the repeatedptroverptrs.
// dereferencing an uninitialized iterator crashes the process.
test_f(repeatedptrfieldptrsiteratortest, uninitializedptriterator) {
  repeatedptrfield<string>::pointer_iterator iter;
  expect_true(iter != proto_array_.pointer_begin());
  expect_true(iter != proto_array_.pointer_begin() + 1);
  expect_true(iter != proto_array_.pointer_begin() + 2);
  expect_true(iter != proto_array_.pointer_begin() + 3);
  expect_true(iter != proto_array_.pointer_end());
}

test_f(repeatedptrfieldptrsiteratortest, uninitializedconstptriterator) {
  repeatedptrfield<string>::const_pointer_iterator iter;
  expect_true(iter != const_proto_array_->pointer_begin());
  expect_true(iter != const_proto_array_->pointer_begin() + 1);
  expect_true(iter != const_proto_array_->pointer_begin() + 2);
  expect_true(iter != const_proto_array_->pointer_begin() + 3);
  expect_true(iter != const_proto_array_->pointer_end());
}

// this comparison functor is required by the tests for repeatedptroverptrs.
// they operate on strings and need to compare strings as strings in
// any stl algorithm, even though the iterator returns a pointer to a string
// - i.e. *iter has type string*.
struct stringlessthan {
  bool operator()(const string* z, const string& y) {
    return *z < y;
  }
  bool operator()(const string* z, const string* y) {
    return *z < *y;
  }
};

test_f(repeatedptrfieldptrsiteratortest, ptrstlalgorithms_lower_bound) {
  proto_array_.clear();
  proto_array_.add()->assign("a");
  proto_array_.add()->assign("c");
  proto_array_.add()->assign("d");
  proto_array_.add()->assign("n");
  proto_array_.add()->assign("p");
  proto_array_.add()->assign("x");
  proto_array_.add()->assign("y");

  {
    string v = "f";
    repeatedptrfield<string>::pointer_iterator it =
        lower_bound(proto_array_.pointer_begin(), proto_array_.pointer_end(),
                    &v, stringlessthan());

    google_check(*it != null);

    expect_eq(**it, "n");
    expect_true(it == proto_array_.pointer_begin() + 3);
  }
  {
    string v = "f";
    repeatedptrfield<string>::const_pointer_iterator it =
        lower_bound(const_proto_array_->pointer_begin(),
                    const_proto_array_->pointer_end(),
                    &v, stringlessthan());

    google_check(*it != null);

    expect_eq(**it, "n");
    expect_true(it == const_proto_array_->pointer_begin() + 3);
  }
}

test_f(repeatedptrfieldptrsiteratortest, ptrmutation) {
  repeatedptrfield<string>::pointer_iterator iter =
      proto_array_.pointer_begin();
  **iter = "qux";
  expect_eq("qux", proto_array_.get(0));

  expect_eq("bar", proto_array_.get(1));
  expect_eq("baz", proto_array_.get(2));
  ++iter;
  delete *iter;
  *iter = new string("a");
  ++iter;
  delete *iter;
  *iter = new string("b");
  expect_eq("a", proto_array_.get(1));
  expect_eq("b", proto_array_.get(2));
}

test_f(repeatedptrfieldptrsiteratortest, sort) {
  proto_array_.add()->assign("c");
  proto_array_.add()->assign("d");
  proto_array_.add()->assign("n");
  proto_array_.add()->assign("p");
  proto_array_.add()->assign("a");
  proto_array_.add()->assign("y");
  proto_array_.add()->assign("x");
  expect_eq("foo", proto_array_.get(0));
  expect_eq("n", proto_array_.get(5));
  expect_eq("x", proto_array_.get(9));
  sort(proto_array_.pointer_begin(),
       proto_array_.pointer_end(),
       stringlessthan());
  expect_eq("a", proto_array_.get(0));
  expect_eq("baz", proto_array_.get(2));
  expect_eq("y", proto_array_.get(9));
}


// -----------------------------------------------------------------------------
// unit-tests for the insert iterators
// google::protobuf::repeatedfieldbackinserter,
// google::protobuf::allocatedrepeatedptrfieldbackinserter
// ported from util/gtl/proto-array-iterators_unittest.

class repeatedfieldinsertioniteratorstest : public testing::test {
 protected:
  std::list<double> halves;
  std::list<int> fibonacci;
  std::vector<string> words;
  typedef testalltypes::nestedmessage nested;
  nested nesteds[2];
  std::vector<nested*> nested_ptrs;
  testalltypes protobuffer;

  virtual void setup() {
    fibonacci.push_back(1);
    fibonacci.push_back(1);
    fibonacci.push_back(2);
    fibonacci.push_back(3);
    fibonacci.push_back(5);
    fibonacci.push_back(8);
    std::copy(fibonacci.begin(), fibonacci.end(),
              repeatedfieldbackinserter(protobuffer.mutable_repeated_int32()));

    halves.push_back(1.0);
    halves.push_back(0.5);
    halves.push_back(0.25);
    halves.push_back(0.125);
    halves.push_back(0.0625);
    std::copy(halves.begin(), halves.end(),
              repeatedfieldbackinserter(protobuffer.mutable_repeated_double()));

    words.push_back("able");
    words.push_back("was");
    words.push_back("i");
    words.push_back("ere");
    words.push_back("i");
    words.push_back("saw");
    words.push_back("elba");
    std::copy(words.begin(), words.end(),
              repeatedfieldbackinserter(protobuffer.mutable_repeated_string()));

    nesteds[0].set_bb(17);
    nesteds[1].set_bb(4711);
    std::copy(&nesteds[0], &nesteds[2],
              repeatedfieldbackinserter(
                  protobuffer.mutable_repeated_nested_message()));

    nested_ptrs.push_back(new nested);
    nested_ptrs.back()->set_bb(170);
    nested_ptrs.push_back(new nested);
    nested_ptrs.back()->set_bb(47110);
    std::copy(nested_ptrs.begin(), nested_ptrs.end(),
              repeatedfieldbackinserter(
                  protobuffer.mutable_repeated_nested_message()));

  }

  virtual void teardown() {
    stldeletecontainerpointers(nested_ptrs.begin(), nested_ptrs.end());
  }
};

test_f(repeatedfieldinsertioniteratorstest, fibonacci) {
  expect_true(std::equal(fibonacci.begin(),
                         fibonacci.end(),
                         protobuffer.repeated_int32().begin()));
  expect_true(std::equal(protobuffer.repeated_int32().begin(),
                         protobuffer.repeated_int32().end(),
                         fibonacci.begin()));
}

test_f(repeatedfieldinsertioniteratorstest, halves) {
  expect_true(std::equal(halves.begin(),
                         halves.end(),
                         protobuffer.repeated_double().begin()));
  expect_true(std::equal(protobuffer.repeated_double().begin(),
                         protobuffer.repeated_double().end(),
                         halves.begin()));
}

test_f(repeatedfieldinsertioniteratorstest, words) {
  assert_eq(words.size(), protobuffer.repeated_string_size());
  for (int i = 0; i < words.size(); ++i)
    expect_eq(words.at(i), protobuffer.repeated_string(i));
}

test_f(repeatedfieldinsertioniteratorstest, words2) {
  words.clear();
  words.push_back("sing");
  words.push_back("a");
  words.push_back("song");
  words.push_back("of");
  words.push_back("six");
  words.push_back("pence");
  protobuffer.mutable_repeated_string()->clear();
  std::copy(words.begin(), words.end(), repeatedptrfieldbackinserter(
      protobuffer.mutable_repeated_string()));
  assert_eq(words.size(), protobuffer.repeated_string_size());
  for (int i = 0; i < words.size(); ++i)
    expect_eq(words.at(i), protobuffer.repeated_string(i));
}

test_f(repeatedfieldinsertioniteratorstest, nesteds) {
  assert_eq(protobuffer.repeated_nested_message_size(), 4);
  expect_eq(protobuffer.repeated_nested_message(0).bb(), 17);
  expect_eq(protobuffer.repeated_nested_message(1).bb(), 4711);
  expect_eq(protobuffer.repeated_nested_message(2).bb(), 170);
  expect_eq(protobuffer.repeated_nested_message(3).bb(), 47110);
}

test_f(repeatedfieldinsertioniteratorstest,
       allocatedrepeatedptrfieldwithstringintdata) {
  vector<nested*> data;
  testalltypes goldenproto;
  for (int i = 0; i < 10; ++i) {
    nested* new_data = new nested;
    new_data->set_bb(i);
    data.push_back(new_data);

    new_data = goldenproto.add_repeated_nested_message();
    new_data->set_bb(i);
  }
  testalltypes testproto;
  copy(data.begin(), data.end(),
       allocatedrepeatedptrfieldbackinserter(
           testproto.mutable_repeated_nested_message()));
  expect_eq(testproto.debugstring(), goldenproto.debugstring());
}

test_f(repeatedfieldinsertioniteratorstest,
       allocatedrepeatedptrfieldwithstring) {
  vector<string*> data;
  testalltypes goldenproto;
  for (int i = 0; i < 10; ++i) {
    string* new_data = new string;
    *new_data = "name-" + simpleitoa(i);
    data.push_back(new_data);

    new_data = goldenproto.add_repeated_string();
    *new_data = "name-" + simpleitoa(i);
  }
  testalltypes testproto;
  copy(data.begin(), data.end(),
       allocatedrepeatedptrfieldbackinserter(
           testproto.mutable_repeated_string()));
  expect_eq(testproto.debugstring(), goldenproto.debugstring());
}

}  // namespace

}  // namespace protobuf
}  // namespace google
