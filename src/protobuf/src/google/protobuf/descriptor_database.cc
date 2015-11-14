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

#include <google/protobuf/descriptor_database.h>

#include <set>

#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/wire_format_lite_inl.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/stl_util.h>
#include <google/protobuf/stubs/map-util.h>

namespace google {
namespace protobuf {

descriptordatabase::~descriptordatabase() {}

// ===================================================================

template <typename value>
bool simpledescriptordatabase::descriptorindex<value>::addfile(
    const filedescriptorproto& file,
    value value) {
  if (!insertifnotpresent(&by_name_, file.name(), value)) {
    google_log(error) << "file already exists in database: " << file.name();
    return false;
  }

  // we must be careful here -- calling file.package() if file.has_package() is
  // false could access an uninitialized static-storage variable if we are being
  // run at startup time.
  string path = file.has_package() ? file.package() : string();
  if (!path.empty()) path += '.';

  for (int i = 0; i < file.message_type_size(); i++) {
    if (!addsymbol(path + file.message_type(i).name(), value)) return false;
    if (!addnestedextensions(file.message_type(i), value)) return false;
  }
  for (int i = 0; i < file.enum_type_size(); i++) {
    if (!addsymbol(path + file.enum_type(i).name(), value)) return false;
  }
  for (int i = 0; i < file.extension_size(); i++) {
    if (!addsymbol(path + file.extension(i).name(), value)) return false;
    if (!addextension(file.extension(i), value)) return false;
  }
  for (int i = 0; i < file.service_size(); i++) {
    if (!addsymbol(path + file.service(i).name(), value)) return false;
  }

  return true;
}

template <typename value>
bool simpledescriptordatabase::descriptorindex<value>::addsymbol(
    const string& name, value value) {
  // we need to make sure not to violate our map invariant.

  // if the symbol name is invalid it could break our lookup algorithm (which
  // relies on the fact that '.' sorts before all other characters that are
  // valid in symbol names).
  if (!validatesymbolname(name)) {
    google_log(error) << "invalid symbol name: " << name;
    return false;
  }

  // try to look up the symbol to make sure a super-symbol doesn't already
  // exist.
  typename map<string, value>::iterator iter = findlastlessorequal(name);

  if (iter == by_symbol_.end()) {
    // apparently the map is currently empty.  just insert and be done with it.
    by_symbol_.insert(typename map<string, value>::value_type(name, value));
    return true;
  }

  if (issubsymbol(iter->first, name)) {
    google_log(error) << "symbol name \"" << name << "\" conflicts with the existing "
                  "symbol \"" << iter->first << "\".";
    return false;
  }

  // ok, that worked.  now we have to make sure that no symbol in the map is
  // a sub-symbol of the one we are inserting.  the only symbol which could
  // be so is the first symbol that is greater than the new symbol.  since
  // |iter| points at the last symbol that is less than or equal, we just have
  // to increment it.
  ++iter;

  if (iter != by_symbol_.end() && issubsymbol(name, iter->first)) {
    google_log(error) << "symbol name \"" << name << "\" conflicts with the existing "
                  "symbol \"" << iter->first << "\".";
    return false;
  }

  // ok, no conflicts.

  // insert the new symbol using the iterator as a hint, the new entry will
  // appear immediately before the one the iterator is pointing at.
  by_symbol_.insert(iter, typename map<string, value>::value_type(name, value));

  return true;
}

template <typename value>
bool simpledescriptordatabase::descriptorindex<value>::addnestedextensions(
    const descriptorproto& message_type,
    value value) {
  for (int i = 0; i < message_type.nested_type_size(); i++) {
    if (!addnestedextensions(message_type.nested_type(i), value)) return false;
  }
  for (int i = 0; i < message_type.extension_size(); i++) {
    if (!addextension(message_type.extension(i), value)) return false;
  }
  return true;
}

template <typename value>
bool simpledescriptordatabase::descriptorindex<value>::addextension(
    const fielddescriptorproto& field,
    value value) {
  if (!field.extendee().empty() && field.extendee()[0] == '.') {
    // the extension is fully-qualified.  we can use it as a lookup key in
    // the by_symbol_ table.
    if (!insertifnotpresent(&by_extension_,
                            make_pair(field.extendee().substr(1),
                                      field.number()),
                            value)) {
      google_log(error) << "extension conflicts with extension already in database: "
                    "extend " << field.extendee() << " { "
                 << field.name() << " = " << field.number() << " }";
      return false;
    }
  } else {
    // not fully-qualified.  we can't really do anything here, unfortunately.
    // we don't consider this an error, though, because the descriptor is
    // valid.
  }
  return true;
}

template <typename value>
value simpledescriptordatabase::descriptorindex<value>::findfile(
    const string& filename) {
  return findwithdefault(by_name_, filename, value());
}

template <typename value>
value simpledescriptordatabase::descriptorindex<value>::findsymbol(
    const string& name) {
  typename map<string, value>::iterator iter = findlastlessorequal(name);

  return (iter != by_symbol_.end() && issubsymbol(iter->first, name)) ?
         iter->second : value();
}

template <typename value>
value simpledescriptordatabase::descriptorindex<value>::findextension(
    const string& containing_type,
    int field_number) {
  return findwithdefault(by_extension_,
                         make_pair(containing_type, field_number),
                         value());
}

template <typename value>
bool simpledescriptordatabase::descriptorindex<value>::findallextensionnumbers(
    const string& containing_type,
    vector<int>* output) {
  typename map<pair<string, int>, value >::const_iterator it =
      by_extension_.lower_bound(make_pair(containing_type, 0));
  bool success = false;

  for (; it != by_extension_.end() && it->first.first == containing_type;
       ++it) {
    output->push_back(it->first.second);
    success = true;
  }

  return success;
}

template <typename value>
typename map<string, value>::iterator
simpledescriptordatabase::descriptorindex<value>::findlastlessorequal(
    const string& name) {
  // find the last key in the map which sorts less than or equal to the
  // symbol name.  since upper_bound() returns the *first* key that sorts
  // *greater* than the input, we want the element immediately before that.
  typename map<string, value>::iterator iter = by_symbol_.upper_bound(name);
  if (iter != by_symbol_.begin()) --iter;
  return iter;
}

template <typename value>
bool simpledescriptordatabase::descriptorindex<value>::issubsymbol(
    const string& sub_symbol, const string& super_symbol) {
  return sub_symbol == super_symbol ||
         (hasprefixstring(super_symbol, sub_symbol) &&
             super_symbol[sub_symbol.size()] == '.');
}

template <typename value>
bool simpledescriptordatabase::descriptorindex<value>::validatesymbolname(
    const string& name) {
  for (int i = 0; i < name.size(); i++) {
    // i don't trust ctype.h due to locales.  :(
    if (name[i] != '.' && name[i] != '_' &&
        (name[i] < '0' || name[i] > '9') &&
        (name[i] < 'a' || name[i] > 'z') &&
        (name[i] < 'a' || name[i] > 'z')) {
      return false;
    }
  }
  return true;
}

// -------------------------------------------------------------------

simpledescriptordatabase::simpledescriptordatabase() {}
simpledescriptordatabase::~simpledescriptordatabase() {
  stldeleteelements(&files_to_delete_);
}

bool simpledescriptordatabase::add(const filedescriptorproto& file) {
  filedescriptorproto* new_file = new filedescriptorproto;
  new_file->copyfrom(file);
  return addandown(new_file);
}

bool simpledescriptordatabase::addandown(const filedescriptorproto* file) {
  files_to_delete_.push_back(file);
  return index_.addfile(*file, file);
}

bool simpledescriptordatabase::findfilebyname(
    const string& filename,
    filedescriptorproto* output) {
  return maybecopy(index_.findfile(filename), output);
}

bool simpledescriptordatabase::findfilecontainingsymbol(
    const string& symbol_name,
    filedescriptorproto* output) {
  return maybecopy(index_.findsymbol(symbol_name), output);
}

bool simpledescriptordatabase::findfilecontainingextension(
    const string& containing_type,
    int field_number,
    filedescriptorproto* output) {
  return maybecopy(index_.findextension(containing_type, field_number), output);
}

bool simpledescriptordatabase::findallextensionnumbers(
    const string& extendee_type,
    vector<int>* output) {
  return index_.findallextensionnumbers(extendee_type, output);
}

bool simpledescriptordatabase::maybecopy(const filedescriptorproto* file,
                                         filedescriptorproto* output) {
  if (file == null) return false;
  output->copyfrom(*file);
  return true;
}

// -------------------------------------------------------------------

encodeddescriptordatabase::encodeddescriptordatabase() {}
encodeddescriptordatabase::~encodeddescriptordatabase() {
  for (int i = 0; i < files_to_delete_.size(); i++) {
    operator delete(files_to_delete_[i]);
  }
}

bool encodeddescriptordatabase::add(
    const void* encoded_file_descriptor, int size) {
  filedescriptorproto file;
  if (file.parsefromarray(encoded_file_descriptor, size)) {
    return index_.addfile(file, make_pair(encoded_file_descriptor, size));
  } else {
    google_log(error) << "invalid file descriptor data passed to "
                  "encodeddescriptordatabase::add().";
    return false;
  }
}

bool encodeddescriptordatabase::addcopy(
    const void* encoded_file_descriptor, int size) {
  void* copy = operator new(size);
  memcpy(copy, encoded_file_descriptor, size);
  files_to_delete_.push_back(copy);
  return add(copy, size);
}

bool encodeddescriptordatabase::findfilebyname(
    const string& filename,
    filedescriptorproto* output) {
  return maybeparse(index_.findfile(filename), output);
}

bool encodeddescriptordatabase::findfilecontainingsymbol(
    const string& symbol_name,
    filedescriptorproto* output) {
  return maybeparse(index_.findsymbol(symbol_name), output);
}

bool encodeddescriptordatabase::findnameoffilecontainingsymbol(
    const string& symbol_name,
    string* output) {
  pair<const void*, int> encoded_file = index_.findsymbol(symbol_name);
  if (encoded_file.first == null) return false;

  // optimization:  the name should be the first field in the encoded message.
  //   try to just read it directly.
  io::codedinputstream input(reinterpret_cast<const uint8*>(encoded_file.first),
                             encoded_file.second);

  const uint32 knametag = internal::wireformatlite::maketag(
      filedescriptorproto::knamefieldnumber,
      internal::wireformatlite::wiretype_length_delimited);

  if (input.readtag() == knametag) {
    // success!
    return internal::wireformatlite::readstring(&input, output);
  } else {
    // slow path.  parse whole message.
    filedescriptorproto file_proto;
    if (!file_proto.parsefromarray(encoded_file.first, encoded_file.second)) {
      return false;
    }
    *output = file_proto.name();
    return true;
  }
}

bool encodeddescriptordatabase::findfilecontainingextension(
    const string& containing_type,
    int field_number,
    filedescriptorproto* output) {
  return maybeparse(index_.findextension(containing_type, field_number),
                    output);
}

bool encodeddescriptordatabase::findallextensionnumbers(
    const string& extendee_type,
    vector<int>* output) {
  return index_.findallextensionnumbers(extendee_type, output);
}

bool encodeddescriptordatabase::maybeparse(
    pair<const void*, int> encoded_file,
    filedescriptorproto* output) {
  if (encoded_file.first == null) return false;
  return output->parsefromarray(encoded_file.first, encoded_file.second);
}

// ===================================================================

descriptorpooldatabase::descriptorpooldatabase(const descriptorpool& pool)
  : pool_(pool) {}
descriptorpooldatabase::~descriptorpooldatabase() {}

bool descriptorpooldatabase::findfilebyname(
    const string& filename,
    filedescriptorproto* output) {
  const filedescriptor* file = pool_.findfilebyname(filename);
  if (file == null) return false;
  output->clear();
  file->copyto(output);
  return true;
}

bool descriptorpooldatabase::findfilecontainingsymbol(
    const string& symbol_name,
    filedescriptorproto* output) {
  const filedescriptor* file = pool_.findfilecontainingsymbol(symbol_name);
  if (file == null) return false;
  output->clear();
  file->copyto(output);
  return true;
}

bool descriptorpooldatabase::findfilecontainingextension(
    const string& containing_type,
    int field_number,
    filedescriptorproto* output) {
  const descriptor* extendee = pool_.findmessagetypebyname(containing_type);
  if (extendee == null) return false;

  const fielddescriptor* extension =
    pool_.findextensionbynumber(extendee, field_number);
  if (extension == null) return false;

  output->clear();
  extension->file()->copyto(output);
  return true;
}

bool descriptorpooldatabase::findallextensionnumbers(
    const string& extendee_type,
    vector<int>* output) {
  const descriptor* extendee = pool_.findmessagetypebyname(extendee_type);
  if (extendee == null) return false;

  vector<const fielddescriptor*> extensions;
  pool_.findallextensions(extendee, &extensions);

  for (int i = 0; i < extensions.size(); ++i) {
    output->push_back(extensions[i]->number());
  }

  return true;
}

// ===================================================================

mergeddescriptordatabase::mergeddescriptordatabase(
    descriptordatabase* source1,
    descriptordatabase* source2) {
  sources_.push_back(source1);
  sources_.push_back(source2);
}
mergeddescriptordatabase::mergeddescriptordatabase(
    const vector<descriptordatabase*>& sources)
  : sources_(sources) {}
mergeddescriptordatabase::~mergeddescriptordatabase() {}

bool mergeddescriptordatabase::findfilebyname(
    const string& filename,
    filedescriptorproto* output) {
  for (int i = 0; i < sources_.size(); i++) {
    if (sources_[i]->findfilebyname(filename, output)) {
      return true;
    }
  }
  return false;
}

bool mergeddescriptordatabase::findfilecontainingsymbol(
    const string& symbol_name,
    filedescriptorproto* output) {
  for (int i = 0; i < sources_.size(); i++) {
    if (sources_[i]->findfilecontainingsymbol(symbol_name, output)) {
      // the symbol was found in source i.  however, if one of the previous
      // sources defines a file with the same name (which presumably doesn't
      // contain the symbol, since it wasn't found in that source), then we
      // must hide it from the caller.
      filedescriptorproto temp;
      for (int j = 0; j < i; j++) {
        if (sources_[j]->findfilebyname(output->name(), &temp)) {
          // found conflicting file in a previous source.
          return false;
        }
      }
      return true;
    }
  }
  return false;
}

bool mergeddescriptordatabase::findfilecontainingextension(
    const string& containing_type,
    int field_number,
    filedescriptorproto* output) {
  for (int i = 0; i < sources_.size(); i++) {
    if (sources_[i]->findfilecontainingextension(
          containing_type, field_number, output)) {
      // the symbol was found in source i.  however, if one of the previous
      // sources defines a file with the same name (which presumably doesn't
      // contain the symbol, since it wasn't found in that source), then we
      // must hide it from the caller.
      filedescriptorproto temp;
      for (int j = 0; j < i; j++) {
        if (sources_[j]->findfilebyname(output->name(), &temp)) {
          // found conflicting file in a previous source.
          return false;
        }
      }
      return true;
    }
  }
  return false;
}

bool mergeddescriptordatabase::findallextensionnumbers(
    const string& extendee_type,
    vector<int>* output) {
  set<int> merged_results;
  vector<int> results;
  bool success = false;

  for (int i = 0; i < sources_.size(); i++) {
    if (sources_[i]->findallextensionnumbers(extendee_type, &results)) {
      copy(results.begin(), results.end(),
           insert_iterator<set<int> >(merged_results, merged_results.begin()));
      success = true;
    }
    results.clear();
  }

  copy(merged_results.begin(), merged_results.end(),
       insert_iterator<vector<int> >(*output, output->end()));

  return success;
}

}  // namespace protobuf
}  // namespace google
