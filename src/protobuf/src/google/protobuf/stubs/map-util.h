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

// from google3/util/gtl/map-util.h
// author: anton carver

#ifndef google_protobuf_stubs_map_util_h__
#define google_protobuf_stubs_map_util_h__

#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {

// perform a lookup in a map or hash_map.
// if the key is present in the map then the value associated with that
// key is returned, otherwise the value passed as a default is returned.
template <class collection>
const typename collection::value_type::second_type&
findwithdefault(const collection& collection,
                const typename collection::value_type::first_type& key,
                const typename collection::value_type::second_type& value) {
  typename collection::const_iterator it = collection.find(key);
  if (it == collection.end()) {
    return value;
  }
  return it->second;
}

// perform a lookup in a map or hash_map.
// if the key is present a const pointer to the associated value is returned,
// otherwise a null pointer is returned.
template <class collection>
const typename collection::value_type::second_type*
findornull(const collection& collection,
           const typename collection::value_type::first_type& key) {
  typename collection::const_iterator it = collection.find(key);
  if (it == collection.end()) {
    return 0;
  }
  return &it->second;
}

// perform a lookup in a map or hash_map, assuming that the key exists.
// crash if it does not.
//
// this is intended as a replacement for operator[] as an rvalue (for reading)
// when the key is guaranteed to exist.
//
// operator[] is discouraged for several reasons:
//  * it has a side-effect of inserting missing keys
//  * it is not thread-safe (even when it is not inserting, it can still
//      choose to resize the underlying storage)
//  * it invalidates iterators (when it chooses to resize)
//  * it default constructs a value object even if it doesn't need to
//
// this version assumes the key is printable, and includes it in the fatal log
// message.
template <class collection>
const typename collection::value_type::second_type&
findordie(const collection& collection,
          const typename collection::value_type::first_type& key) {
  typename collection::const_iterator it = collection.find(key);
  google_check(it != collection.end()) << "map key not found: " << key;
  return it->second;
}

// perform a lookup in a map or hash_map whose values are pointers.
// if the key is present a const pointer to the associated value is returned,
// otherwise a null pointer is returned.
// this function does not distinguish between a missing key and a key mapped
// to a null value.
template <class collection>
const typename collection::value_type::second_type
findptrornull(const collection& collection,
              const typename collection::value_type::first_type& key) {
  typename collection::const_iterator it = collection.find(key);
  if (it == collection.end()) {
    return 0;
  }
  return it->second;
}

// change the value associated with a particular key in a map or hash_map.
// if the key is not present in the map the key and value are inserted,
// otherwise the value is updated to be a copy of the value provided.
// true indicates that an insert took place, false indicates an update.
template <class collection, class key, class value>
bool insertorupdate(collection * const collection,
                   const key& key, const value& value) {
  pair<typename collection::iterator, bool> ret =
    collection->insert(typename collection::value_type(key, value));
  if (!ret.second) {
    // update
    ret.first->second = value;
    return false;
  }
  return true;
}

// insert a new key and value into a map or hash_map.
// if the key is not present in the map the key and value are
// inserted, otherwise nothing happens. true indicates that an insert
// took place, false indicates the key was already present.
template <class collection, class key, class value>
bool insertifnotpresent(collection * const collection,
                        const key& key, const value& value) {
  pair<typename collection::iterator, bool> ret =
    collection->insert(typename collection::value_type(key, value));
  return ret.second;
}

}  // namespace protobuf
}  // namespace google

#endif  // google_protobuf_stubs_map_util_h__
