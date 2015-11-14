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
//
// deals with the fact that hash_map is not defined everywhere.

#ifndef google_protobuf_stubs_hash_h__
#define google_protobuf_stubs_hash_h__

#include <string.h>
#include <google/protobuf/stubs/common.h>
#include "config.h"

#if defined(have_hash_map) && defined(have_hash_set)
#include hash_map_h
#include hash_set_h
#else
#define missing_hash
#include <map>
#include <set>
#endif

namespace google {
namespace protobuf {

#ifdef missing_hash

// this system doesn't have hash_map or hash_set.  emulate them using map and
// set.

// make hash<t> be the same as less<t>.  note that everywhere where custom
// hash functions are defined in the protobuf code, they are also defined such
// that they can be used as "less" functions, which is required by msvc anyway.
template <typename key>
struct hash {
  // dummy, just to make derivative hash functions compile.
  int operator()(const key& key) {
    google_log(fatal) << "should never be called.";
    return 0;
  }

  inline bool operator()(const key& a, const key& b) const {
    return a < b;
  }
};

// make sure char* is compared by value.
template <>
struct hash<const char*> {
  // dummy, just to make derivative hash functions compile.
  int operator()(const char* key) {
    google_log(fatal) << "should never be called.";
    return 0;
  }

  inline bool operator()(const char* a, const char* b) const {
    return strcmp(a, b) < 0;
  }
};

template <typename key, typename data,
          typename hashfcn = hash<key>,
          typename equalkey = int >
class hash_map : public std::map<key, data, hashfcn> {
 public:
  hash_map(int = 0) {}
};

template <typename key,
          typename hashfcn = hash<key>,
          typename equalkey = int >
class hash_set : public std::set<key, hashfcn> {
 public:
  hash_set(int = 0) {}
};

#elif defined(_msc_ver) && !defined(_stlport_version)

template <typename key>
struct hash : public hash_namespace::hash_compare<key> {
};

// msvc's hash_compare<const char*> hashes based on the string contents but
// compares based on the string pointer.  wtf?
class cstringless {
 public:
  inline bool operator()(const char* a, const char* b) const {
    return strcmp(a, b) < 0;
  }
};

template <>
struct hash<const char*>
  : public hash_namespace::hash_compare<const char*, cstringless> {
};

template <typename key, typename data,
          typename hashfcn = hash<key>,
          typename equalkey = int >
class hash_map : public hash_namespace::hash_map<
    key, data, hashfcn> {
 public:
  hash_map(int = 0) {}
};

template <typename key,
          typename hashfcn = hash<key>,
          typename equalkey = int >
class hash_set : public hash_namespace::hash_set<
    key, hashfcn> {
 public:
  hash_set(int = 0) {}
};

#else

template <typename key>
struct hash : public hash_namespace::hash<key> {
};

template <typename key>
struct hash<const key*> {
  inline size_t operator()(const key* key) const {
    return reinterpret_cast<size_t>(key);
  }
};

// unlike the old sgi version, the tr1 "hash" does not special-case char*.  so,
// we go ahead and provide our own implementation.
template <>
struct hash<const char*> {
  inline size_t operator()(const char* str) const {
    size_t result = 0;
    for (; *str != '\0'; str++) {
      result = 5 * result + *str;
    }
    return result;
  }
};

template <typename key, typename data,
          typename hashfcn = hash<key>,
          typename equalkey = std::equal_to<key> >
class hash_map : public hash_namespace::hash_map_class<
    key, data, hashfcn, equalkey> {
 public:
  hash_map(int = 0) {}
};

template <typename key,
          typename hashfcn = hash<key>,
          typename equalkey = std::equal_to<key> >
class hash_set : public hash_namespace::hash_set_class<
    key, hashfcn, equalkey> {
 public:
  hash_set(int = 0) {}
};

#endif

template <>
struct hash<string> {
  inline size_t operator()(const string& key) const {
    return hash<const char*>()(key.c_str());
  }

  static const size_t bucket_size = 4;
  static const size_t min_buckets = 8;
  inline size_t operator()(const string& a, const string& b) const {
    return a < b;
  }
};

template <typename first, typename second>
struct hash<pair<first, second> > {
  inline size_t operator()(const pair<first, second>& key) const {
    size_t first_hash = hash<first>()(key.first);
    size_t second_hash = hash<second>()(key.second);

    // fixme(kenton):  what is the best way to compute this hash?  i have
    // no idea!  this seems a bit better than an xor.
    return first_hash * ((1 << 16) - 1) + second_hash;
  }

  static const size_t bucket_size = 4;
  static const size_t min_buckets = 8;
  inline size_t operator()(const pair<first, second>& a,
                           const pair<first, second>& b) const {
    return a < b;
  }
};

// used by gcc/sgi stl only.  (why isn't this provided by the standard
// library?  :( )
struct streq {
  inline bool operator()(const char* a, const char* b) const {
    return strcmp(a, b) == 0;
  }
};

}  // namespace protobuf
}  // namespace google

#endif  // google_protobuf_stubs_hash_h__
