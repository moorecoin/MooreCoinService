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

// author: tgs@google.com (tom szymanski)
//
// test reflection methods for aggregate access to repeated[ptr]fields.
// this test proto2 methods on a proto2 layout.

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/stringprintf.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/test_util.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {

using unittest::foreignmessage;
using unittest::testalltypes;
using unittest::testallextensions;

namespace {

static int func(int i, int j) {
  return i * j;
}

static string strfunc(int i, int j) {
  string str;
  sstringprintf(&str, "%d", func(i, 4));
  return str;
}


test(repeatedfieldreflectiontest, regularfields) {
  testalltypes message;
  const reflection* refl = message.getreflection();
  const descriptor* desc = message.getdescriptor();

  for (int i = 0; i < 10; ++i) {
    message.add_repeated_int32(func(i, 1));
    message.add_repeated_double(func(i, 2));
    message.add_repeated_string(strfunc(i, 5));
    message.add_repeated_foreign_message()->set_c(func(i, 6));
  }

  // get fielddescriptors for all the fields of interest.
  const fielddescriptor* fd_repeated_int32 =
      desc->findfieldbyname("repeated_int32");
  const fielddescriptor* fd_repeated_double =
      desc->findfieldbyname("repeated_double");
  const fielddescriptor* fd_repeated_string =
      desc->findfieldbyname("repeated_string");
  const fielddescriptor* fd_repeated_foreign_message =
      desc->findfieldbyname("repeated_foreign_message");

  // get repeatedfield objects for all fields of interest.
  const repeatedfield<int32>& rf_int32 =
      refl->getrepeatedfield<int32>(message, fd_repeated_int32);
  const repeatedfield<double>& rf_double =
      refl->getrepeatedfield<double>(message, fd_repeated_double);

  // get mutable repeatedfield objects for all fields of interest.
  repeatedfield<int32>* mrf_int32 =
      refl->mutablerepeatedfield<int32>(&message, fd_repeated_int32);
  repeatedfield<double>* mrf_double =
      refl->mutablerepeatedfield<double>(&message, fd_repeated_double);

  // get repeatedptrfield objects for all fields of interest.
  const repeatedptrfield<string>& rpf_string =
      refl->getrepeatedptrfield<string>(message, fd_repeated_string);
  const repeatedptrfield<foreignmessage>& rpf_foreign_message =
      refl->getrepeatedptrfield<foreignmessage>(
          message, fd_repeated_foreign_message);
  const repeatedptrfield<message>& rpf_message =
      refl->getrepeatedptrfield<message>(
          message, fd_repeated_foreign_message);

  // get mutable repeatedptrfield objects for all fields of interest.
  repeatedptrfield<string>* mrpf_string =
      refl->mutablerepeatedptrfield<string>(&message, fd_repeated_string);
  repeatedptrfield<foreignmessage>* mrpf_foreign_message =
      refl->mutablerepeatedptrfield<foreignmessage>(
          &message, fd_repeated_foreign_message);
  repeatedptrfield<message>* mrpf_message =
      refl->mutablerepeatedptrfield<message>(
          &message, fd_repeated_foreign_message);

  // make sure we can do get and sets through the repeated[ptr]field objects.
  for (int i = 0; i < 10; ++i) {
    // check gets through const objects.
    expect_eq(rf_int32.get(i), func(i, 1));
    expect_eq(rf_double.get(i), func(i, 2));
    expect_eq(rpf_string.get(i), strfunc(i, 5));
    expect_eq(rpf_foreign_message.get(i).c(), func(i, 6));
    expect_eq(down_cast<const foreignmessage*>(&rpf_message.get(i))->c(),
              func(i, 6));

    // check gets through mutable objects.
    expect_eq(mrf_int32->get(i), func(i, 1));
    expect_eq(mrf_double->get(i), func(i, 2));
    expect_eq(mrpf_string->get(i), strfunc(i, 5));
    expect_eq(mrpf_foreign_message->get(i).c(), func(i, 6));
    expect_eq(down_cast<const foreignmessage*>(&mrpf_message->get(i))->c(),
              func(i, 6));

    // check sets through mutable objects.
    mrf_int32->set(i, func(i, -1));
    mrf_double->set(i, func(i, -2));
    mrpf_string->mutable(i)->assign(strfunc(i, -5));
    mrpf_foreign_message->mutable(i)->set_c(func(i, -6));
    expect_eq(message.repeated_int32(i), func(i, -1));
    expect_eq(message.repeated_double(i), func(i, -2));
    expect_eq(message.repeated_string(i), strfunc(i, -5));
    expect_eq(message.repeated_foreign_message(i).c(), func(i, -6));
    down_cast<foreignmessage*>(mrpf_message->mutable(i))->set_c(func(i, 7));
    expect_eq(message.repeated_foreign_message(i).c(), func(i, 7));
  }

#ifdef protobuf_has_death_test
  // make sure types are checked correctly at runtime.
  const fielddescriptor* fd_optional_int32 =
      desc->findfieldbyname("optional_int32");
  expect_death(refl->getrepeatedfield<int32>(
      message, fd_optional_int32), "requires a repeated field");
  expect_death(refl->getrepeatedfield<double>(
      message, fd_repeated_int32), "not the right type");
  expect_death(refl->getrepeatedptrfield<testalltypes>(
      message, fd_repeated_foreign_message), "wrong submessage type");
#endif  // protobuf_has_death_test
}




test(repeatedfieldreflectiontest, extensionfields) {
  testallextensions extended_message;
  const reflection* refl = extended_message.getreflection();
  const descriptor* desc = extended_message.getdescriptor();

  for (int i = 0; i < 10; ++i) {
    extended_message.addextension(
        unittest::repeated_int64_extension, func(i, 1));
  }

  const fielddescriptor* fd_repeated_int64_extension =
      desc->file()->findextensionbyname("repeated_int64_extension");
  google_check(fd_repeated_int64_extension != null);

  const repeatedfield<int64>& rf_int64_extension =
      refl->getrepeatedfield<int64>(extended_message,
                                    fd_repeated_int64_extension);

  repeatedfield<int64>* mrf_int64_extension =
      refl->mutablerepeatedfield<int64>(&extended_message,
                                        fd_repeated_int64_extension);

  for (int i = 0; i < 10; ++i) {
    expect_eq(func(i, 1), rf_int64_extension.get(i));
    mrf_int64_extension->set(i, func(i, -1));
    expect_eq(func(i, -1),
        extended_message.getextension(unittest::repeated_int64_extension, i));
  }
}

}  // namespace
}  // namespace protobuf
}  // namespace google
