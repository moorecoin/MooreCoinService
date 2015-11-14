// copyright 2005 google inc.
// all rights reserved.
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

// ----
// author: lar@google.com (laramie leavitt)
//
// template metaprogramming utility functions.
//
// this code is compiled directly on many platforms, including client
// platforms like windows, mac, and embedded systems.  before making
// any changes here, make sure that you're not breaking any platforms.
//
//
// the names choosen here reflect those used in tr1 and the boost::mpl
// library, there are similar operations used in the loki library as
// well.  i prefer the boost names for 2 reasons:
// 1.  i think that portions of the boost libraries are more likely to
// be included in the c++ standard.
// 2.  it is not impossible that some of the boost libraries will be
// included in our own build in the future.
// both of these outcomes means that we may be able to directly replace
// some of these with boost equivalents.
//
#ifndef google_protobuf_template_util_h_
#define google_protobuf_template_util_h_

namespace google {
namespace protobuf {
namespace internal {

// types small_ and big_ are guaranteed such that sizeof(small_) <
// sizeof(big_)
typedef char small_;

struct big_ {
  char dummy[2];
};

// identity metafunction.
template <class t>
struct identity_ {
  typedef t type;
};

// integral_constant, defined in tr1, is a wrapper for an integer
// value. we don't really need this generality; we could get away
// with hardcoding the integer type to bool. we use the fully
// general integer_constant for compatibility with tr1.

template<class t, t v>
struct integral_constant {
  static const t value = v;
  typedef t value_type;
  typedef integral_constant<t, v> type;
};

template <class t, t v> const t integral_constant<t, v>::value;


// abbreviations: true_type and false_type are structs that represent boolean
// true and false values. also define the boost::mpl versions of those names,
// true_ and false_.
typedef integral_constant<bool, true>  true_type;
typedef integral_constant<bool, false> false_type;
typedef true_type  true_;
typedef false_type false_;

// if_ is a templatized conditional statement.
// if_<cond, a, b> is a compile time evaluation of cond.
// if_<>::type contains a if cond is true, b otherwise.
template<bool cond, typename a, typename b>
struct if_{
  typedef a type;
};

template<typename a, typename b>
struct if_<false, a, b> {
  typedef b type;
};


// type_equals_ is a template type comparator, similar to loki issametype.
// type_equals_<a, b>::value is true iff "a" is the same type as "b".
//
// new code should prefer base::is_same, defined in base/type_traits.h.
// it is functionally identical, but is_same is the standard spelling.
template<typename a, typename b>
struct type_equals_ : public false_ {
};

template<typename a>
struct type_equals_<a, a> : public true_ {
};

// and_ is a template && operator.
// and_<a, b>::value evaluates "a::value && b::value".
template<typename a, typename b>
struct and_ : public integral_constant<bool, (a::value && b::value)> {
};

// or_ is a template || operator.
// or_<a, b>::value evaluates "a::value || b::value".
template<typename a, typename b>
struct or_ : public integral_constant<bool, (a::value || b::value)> {
};


}  // namespace internal
}  // namespace protobuf
}  // namespace google

#endif  // google_protobuf_template_util_h_
