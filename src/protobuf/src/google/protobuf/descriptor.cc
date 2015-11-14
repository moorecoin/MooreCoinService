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

#include <google/protobuf/stubs/hash.h>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <limits>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor_database.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/unknown_field_set.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/once.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>
#include <google/protobuf/stubs/map-util.h>
#include <google/protobuf/stubs/stl_util.h>

#undef package  // autoheader #defines this.  :(

namespace google {
namespace protobuf {

const fielddescriptor::cpptype
fielddescriptor::ktypetocpptypemap[max_type + 1] = {
  static_cast<cpptype>(0),  // 0 is reserved for errors

  cpptype_double,   // type_double
  cpptype_float,    // type_float
  cpptype_int64,    // type_int64
  cpptype_uint64,   // type_uint64
  cpptype_int32,    // type_int32
  cpptype_uint64,   // type_fixed64
  cpptype_uint32,   // type_fixed32
  cpptype_bool,     // type_bool
  cpptype_string,   // type_string
  cpptype_message,  // type_group
  cpptype_message,  // type_message
  cpptype_string,   // type_bytes
  cpptype_uint32,   // type_uint32
  cpptype_enum,     // type_enum
  cpptype_int32,    // type_sfixed32
  cpptype_int64,    // type_sfixed64
  cpptype_int32,    // type_sint32
  cpptype_int64,    // type_sint64
};

const char * const fielddescriptor::ktypetoname[max_type + 1] = {
  "error",     // 0 is reserved for errors

  "double",    // type_double
  "float",     // type_float
  "int64",     // type_int64
  "uint64",    // type_uint64
  "int32",     // type_int32
  "fixed64",   // type_fixed64
  "fixed32",   // type_fixed32
  "bool",      // type_bool
  "string",    // type_string
  "group",     // type_group
  "message",   // type_message
  "bytes",     // type_bytes
  "uint32",    // type_uint32
  "enum",      // type_enum
  "sfixed32",  // type_sfixed32
  "sfixed64",  // type_sfixed64
  "sint32",    // type_sint32
  "sint64",    // type_sint64
};

const char * const fielddescriptor::kcpptypetoname[max_cpptype + 1] = {
  "error",     // 0 is reserved for errors

  "int32",     // cpptype_int32
  "int64",     // cpptype_int64
  "uint32",    // cpptype_uint32
  "uint64",    // cpptype_uint64
  "double",    // cpptype_double
  "float",     // cpptype_float
  "bool",      // cpptype_bool
  "enum",      // cpptype_enum
  "string",    // cpptype_string
  "message",   // cpptype_message
};

const char * const fielddescriptor::klabeltoname[max_label + 1] = {
  "error",     // 0 is reserved for errors

  "optional",  // label_optional
  "required",  // label_required
  "repeated",  // label_repeated
};

#ifndef _msc_ver  // msvc doesn't need these and won't even accept them.
const int fielddescriptor::kmaxnumber;
const int fielddescriptor::kfirstreservednumber;
const int fielddescriptor::klastreservednumber;
#endif

namespace {

const string kemptystring;

string tocamelcase(const string& input) {
  bool capitalize_next = false;
  string result;
  result.reserve(input.size());

  for (int i = 0; i < input.size(); i++) {
    if (input[i] == '_') {
      capitalize_next = true;
    } else if (capitalize_next) {
      // note:  i distrust ctype.h due to locales.
      if ('a' <= input[i] && input[i] <= 'z') {
        result.push_back(input[i] - 'a' + 'a');
      } else {
        result.push_back(input[i]);
      }
      capitalize_next = false;
    } else {
      result.push_back(input[i]);
    }
  }

  // lower-case the first letter.
  if (!result.empty() && 'a' <= result[0] && result[0] <= 'z') {
    result[0] = result[0] - 'a' + 'a';
  }

  return result;
}

// a descriptorpool contains a bunch of hash_maps to implement the
// various find*by*() methods.  since hashtable lookups are o(1), it's
// most efficient to construct a fixed set of large hash_maps used by
// all objects in the pool rather than construct one or more small
// hash_maps for each object.
//
// the keys to these hash_maps are (parent, name) or (parent, number)
// pairs.  unfortunately stl doesn't provide hash functions for pair<>,
// so we must invent our own.
//
// todo(kenton):  use stringpiece rather than const char* in keys?  it would
//   be a lot cleaner but we'd just have to convert it back to const char*
//   for the open source release.

typedef pair<const void*, const char*> pointerstringpair;

struct pointerstringpairequal {
  inline bool operator()(const pointerstringpair& a,
                         const pointerstringpair& b) const {
    return a.first == b.first && strcmp(a.second, b.second) == 0;
  }
};

template<typename pairtype>
struct pointerintegerpairhash {
  size_t operator()(const pairtype& p) const {
    // fixme(kenton):  what is the best way to compute this hash?  i have
    // no idea!  this seems a bit better than an xor.
    return reinterpret_cast<intptr_t>(p.first) * ((1 << 16) - 1) + p.second;
  }

  // used only by msvc and platforms where hash_map is not available.
  static const size_t bucket_size = 4;
  static const size_t min_buckets = 8;
  inline bool operator()(const pairtype& a, const pairtype& b) const {
    return a.first < b.first ||
          (a.first == b.first && a.second < b.second);
  }
};

typedef pair<const descriptor*, int> descriptorintpair;
typedef pair<const enumdescriptor*, int> enumintpair;

struct pointerstringpairhash {
  size_t operator()(const pointerstringpair& p) const {
    // fixme(kenton):  what is the best way to compute this hash?  i have
    // no idea!  this seems a bit better than an xor.
    hash<const char*> cstring_hash;
    return reinterpret_cast<intptr_t>(p.first) * ((1 << 16) - 1) +
           cstring_hash(p.second);
  }

  // used only by msvc and platforms where hash_map is not available.
  static const size_t bucket_size = 4;
  static const size_t min_buckets = 8;
  inline bool operator()(const pointerstringpair& a,
                         const pointerstringpair& b) const {
    if (a.first < b.first) return true;
    if (a.first > b.first) return false;
    return strcmp(a.second, b.second) < 0;
  }
};


struct symbol {
  enum type {
    null_symbol, message, field, enum, enum_value, service, method,
    package
  };
  type type;
  union {
    const descriptor* descriptor;
    const fielddescriptor* field_descriptor;
    const enumdescriptor* enum_descriptor;
    const enumvaluedescriptor* enum_value_descriptor;
    const servicedescriptor* service_descriptor;
    const methoddescriptor* method_descriptor;
    const filedescriptor* package_file_descriptor;
  };

  inline symbol() : type(null_symbol) { descriptor = null; }
  inline bool isnull() const { return type == null_symbol; }
  inline bool istype() const {
    return type == message || type == enum;
  }
  inline bool isaggregate() const {
    return type == message || type == package
        || type == enum || type == service;
  }

#define constructor(type, type_constant, field)  \
  inline explicit symbol(const type* value) {    \
    type = type_constant;                        \
    this->field = value;                         \
  }

  constructor(descriptor         , message   , descriptor             )
  constructor(fielddescriptor    , field     , field_descriptor       )
  constructor(enumdescriptor     , enum      , enum_descriptor        )
  constructor(enumvaluedescriptor, enum_value, enum_value_descriptor  )
  constructor(servicedescriptor  , service   , service_descriptor     )
  constructor(methoddescriptor   , method    , method_descriptor      )
  constructor(filedescriptor     , package   , package_file_descriptor)
#undef constructor

  const filedescriptor* getfile() const {
    switch (type) {
      case null_symbol: return null;
      case message    : return descriptor           ->file();
      case field      : return field_descriptor     ->file();
      case enum       : return enum_descriptor      ->file();
      case enum_value : return enum_value_descriptor->type()->file();
      case service    : return service_descriptor   ->file();
      case method     : return method_descriptor    ->service()->file();
      case package    : return package_file_descriptor;
    }
    return null;
  }
};

const symbol knullsymbol;

typedef hash_map<const char*, symbol,
                 hash<const char*>, streq>
  symbolsbynamemap;
typedef hash_map<pointerstringpair, symbol,
                 pointerstringpairhash, pointerstringpairequal>
  symbolsbyparentmap;
typedef hash_map<const char*, const filedescriptor*,
                 hash<const char*>, streq>
  filesbynamemap;
typedef hash_map<pointerstringpair, const fielddescriptor*,
                 pointerstringpairhash, pointerstringpairequal>
  fieldsbynamemap;
typedef hash_map<descriptorintpair, const fielddescriptor*,
                 pointerintegerpairhash<descriptorintpair> >
  fieldsbynumbermap;
typedef hash_map<enumintpair, const enumvaluedescriptor*,
                 pointerintegerpairhash<enumintpair> >
  enumvaluesbynumbermap;
// this is a map rather than a hash_map, since we use it to iterate
// through all the extensions that extend a given descriptor, and an
// ordered data structure that implements lower_bound is convenient
// for that.
typedef map<descriptorintpair, const fielddescriptor*>
  extensionsgroupedbydescriptormap;

}  // anonymous namespace

// ===================================================================
// descriptorpool::tables

class descriptorpool::tables {
 public:
  tables();
  ~tables();

  // record the current state of the tables to the stack of checkpoints.
  // each call to addcheckpoint() must be paired with exactly one call to either
  // clearlastcheckpoint() or rollbacktolastcheckpoint().
  //
  // this is used when building files, since some kinds of validation errors
  // cannot be detected until the file's descriptors have already been added to
  // the tables.
  //
  // this supports recursive checkpoints, since building a file may trigger
  // recursive building of other files. note that recursive checkpoints are not
  // normally necessary; explicit dependencies are built prior to checkpointing.
  // so although we recursively build transitive imports, there is at most one
  // checkpoint in the stack during dependency building.
  //
  // recursive checkpoints only arise during cross-linking of the descriptors.
  // symbol references must be resolved, via descriptorbuilder::findsymbol and
  // friends. if the pending file references an unknown symbol
  // (e.g., it is not defined in the pending file's explicit dependencies), and
  // the pool is using a fallback database, and that database contains a file
  // defining that symbol, and that file has not yet been built by the pool,
  // the pool builds the file during cross-linking, leading to another
  // checkpoint.
  void addcheckpoint();

  // mark the last checkpoint as having cleared successfully, removing it from
  // the stack. if the stack is empty, all pending symbols will be committed.
  //
  // note that this does not guarantee that the symbols added since the last
  // checkpoint won't be rolled back: if a checkpoint gets rolled back,
  // everything past that point gets rolled back, including symbols added after
  // checkpoints that were pushed onto the stack after it and marked as cleared.
  void clearlastcheckpoint();

  // roll back the tables to the state of the checkpoint at the top of the
  // stack, removing everything that was added after that point.
  void rollbacktolastcheckpoint();

  // the stack of files which are currently being built.  used to detect
  // cyclic dependencies when loading files from a descriptordatabase.  not
  // used when fallback_database_ == null.
  vector<string> pending_files_;

  // a set of files which we have tried to load from the fallback database
  // and encountered errors.  we will not attempt to load them again.
  // not used when fallback_database_ == null.
  hash_set<string> known_bad_files_;

  // the set of descriptors for which we've already loaded the full
  // set of extensions numbers from fallback_database_.
  hash_set<const descriptor*> extensions_loaded_from_db_;

  // -----------------------------------------------------------------
  // finding items.

  // find symbols.  this returns a null symbol (symbol.isnull() is true)
  // if not found.
  inline symbol findsymbol(const string& key) const;

  // this implements the body of descriptorpool::find*byname().  it should
  // really be a private method of descriptorpool, but that would require
  // declaring symbol in descriptor.h, which would drag all kinds of other
  // stuff into the header.  yay c++.
  symbol findbynamehelper(
    const descriptorpool* pool, const string& name) const;

  // these return null if not found.
  inline const filedescriptor* findfile(const string& key) const;
  inline const fielddescriptor* findextension(const descriptor* extendee,
                                              int number);
  inline void findallextensions(const descriptor* extendee,
                                vector<const fielddescriptor*>* out) const;

  // -----------------------------------------------------------------
  // adding items.

  // these add items to the corresponding tables.  they return false if
  // the key already exists in the table.  for addsymbol(), the string passed
  // in must be one that was constructed using allocatestring(), as it will
  // be used as a key in the symbols_by_name_ map without copying.
  bool addsymbol(const string& full_name, symbol symbol);
  bool addfile(const filedescriptor* file);
  bool addextension(const fielddescriptor* field);

  // -----------------------------------------------------------------
  // allocating memory.

  // allocate an object which will be reclaimed when the pool is
  // destroyed.  note that the object's destructor will never be called,
  // so its fields must be plain old data (primitive data types and
  // pointers).  all of the descriptor types are such objects.
  template<typename type> type* allocate();

  // allocate an array of objects which will be reclaimed when the
  // pool in destroyed.  again, destructors are never called.
  template<typename type> type* allocatearray(int count);

  // allocate a string which will be destroyed when the pool is destroyed.
  // the string is initialized to the given value for convenience.
  string* allocatestring(const string& value);

  // allocate a protocol message object.  some older versions of gcc have
  // trouble understanding explicit template instantiations in some cases, so
  // in those cases we have to pass a dummy pointer of the right type as the
  // parameter instead of specifying the type explicitly.
  template<typename type> type* allocatemessage(type* dummy = null);

  // allocate a filedescriptortables object.
  filedescriptortables* allocatefiletables();

 private:
  vector<string*> strings_;    // all strings in the pool.
  vector<message*> messages_;  // all messages in the pool.
  vector<filedescriptortables*> file_tables_;  // all file tables in the pool.
  vector<void*> allocations_;  // all other memory allocated in the pool.

  symbolsbynamemap      symbols_by_name_;
  filesbynamemap        files_by_name_;
  extensionsgroupedbydescriptormap extensions_;

  struct checkpoint {
    explicit checkpoint(const tables* tables)
      : strings_before_checkpoint(tables->strings_.size()),
        messages_before_checkpoint(tables->messages_.size()),
        file_tables_before_checkpoint(tables->file_tables_.size()),
        allocations_before_checkpoint(tables->allocations_.size()),
        pending_symbols_before_checkpoint(
            tables->symbols_after_checkpoint_.size()),
        pending_files_before_checkpoint(
            tables->files_after_checkpoint_.size()),
        pending_extensions_before_checkpoint(
            tables->extensions_after_checkpoint_.size()) {
    }
    int strings_before_checkpoint;
    int messages_before_checkpoint;
    int file_tables_before_checkpoint;
    int allocations_before_checkpoint;
    int pending_symbols_before_checkpoint;
    int pending_files_before_checkpoint;
    int pending_extensions_before_checkpoint;
  };
  vector<checkpoint> checkpoints_;
  vector<const char*      > symbols_after_checkpoint_;
  vector<const char*      > files_after_checkpoint_;
  vector<descriptorintpair> extensions_after_checkpoint_;

  // allocate some bytes which will be reclaimed when the pool is
  // destroyed.
  void* allocatebytes(int size);
};

// contains tables specific to a particular file.  these tables are not
// modified once the file has been constructed, so they need not be
// protected by a mutex.  this makes operations that depend only on the
// contents of a single file -- e.g. descriptor::findfieldbyname() --
// lock-free.
//
// for historical reasons, the definitions of the methods of
// filedescriptortables and descriptorpool::tables are interleaved below.
// these used to be a single class.
class filedescriptortables {
 public:
  filedescriptortables();
  ~filedescriptortables();

  // empty table, used with placeholder files.
  static const filedescriptortables kempty;

  // -----------------------------------------------------------------
  // finding items.

  // find symbols.  these return a null symbol (symbol.isnull() is true)
  // if not found.
  inline symbol findnestedsymbol(const void* parent,
                                 const string& name) const;
  inline symbol findnestedsymboloftype(const void* parent,
                                       const string& name,
                                       const symbol::type type) const;

  // these return null if not found.
  inline const fielddescriptor* findfieldbynumber(
    const descriptor* parent, int number) const;
  inline const fielddescriptor* findfieldbylowercasename(
    const void* parent, const string& lowercase_name) const;
  inline const fielddescriptor* findfieldbycamelcasename(
    const void* parent, const string& camelcase_name) const;
  inline const enumvaluedescriptor* findenumvaluebynumber(
    const enumdescriptor* parent, int number) const;

  // -----------------------------------------------------------------
  // adding items.

  // these add items to the corresponding tables.  they return false if
  // the key already exists in the table.  for addaliasunderparent(), the
  // string passed in must be one that was constructed using allocatestring(),
  // as it will be used as a key in the symbols_by_parent_ map without copying.
  bool addaliasunderparent(const void* parent, const string& name,
                           symbol symbol);
  bool addfieldbynumber(const fielddescriptor* field);
  bool addenumvaluebynumber(const enumvaluedescriptor* value);

  // adds the field to the lowercase_name and camelcase_name maps.  never
  // fails because we allow duplicates; the first field by the name wins.
  void addfieldbystylizednames(const fielddescriptor* field);

 private:
  symbolsbyparentmap    symbols_by_parent_;
  fieldsbynamemap       fields_by_lowercase_name_;
  fieldsbynamemap       fields_by_camelcase_name_;
  fieldsbynumbermap     fields_by_number_;       // not including extensions.
  enumvaluesbynumbermap enum_values_by_number_;
};

descriptorpool::tables::tables()
    // start some hash_map and hash_set objects with a small # of buckets
    : known_bad_files_(3),
      extensions_loaded_from_db_(3),
      symbols_by_name_(3),
      files_by_name_(3) {}


descriptorpool::tables::~tables() {
  google_dcheck(checkpoints_.empty());
  // note that the deletion order is important, since the destructors of some
  // messages may refer to objects in allocations_.
  stldeleteelements(&messages_);
  for (int i = 0; i < allocations_.size(); i++) {
    operator delete(allocations_[i]);
  }
  stldeleteelements(&strings_);
  stldeleteelements(&file_tables_);
}

filedescriptortables::filedescriptortables()
    // initialize all the hash tables to start out with a small # of buckets
    : symbols_by_parent_(3),
      fields_by_lowercase_name_(3),
      fields_by_camelcase_name_(3),
      fields_by_number_(3),
      enum_values_by_number_(3) {
}

filedescriptortables::~filedescriptortables() {}

const filedescriptortables filedescriptortables::kempty;

void descriptorpool::tables::addcheckpoint() {
  checkpoints_.push_back(checkpoint(this));
}

void descriptorpool::tables::clearlastcheckpoint() {
  google_dcheck(!checkpoints_.empty());
  checkpoints_.pop_back();
  if (checkpoints_.empty()) {
    // all checkpoints have been cleared: we can now commit all of the pending
    // data.
    symbols_after_checkpoint_.clear();
    files_after_checkpoint_.clear();
    extensions_after_checkpoint_.clear();
  }
}

void descriptorpool::tables::rollbacktolastcheckpoint() {
  google_dcheck(!checkpoints_.empty());
  const checkpoint& checkpoint = checkpoints_.back();

  for (int i = checkpoint.pending_symbols_before_checkpoint;
       i < symbols_after_checkpoint_.size();
       i++) {
    symbols_by_name_.erase(symbols_after_checkpoint_[i]);
  }
  for (int i = checkpoint.pending_files_before_checkpoint;
       i < files_after_checkpoint_.size();
       i++) {
    files_by_name_.erase(files_after_checkpoint_[i]);
  }
  for (int i = checkpoint.pending_extensions_before_checkpoint;
       i < extensions_after_checkpoint_.size();
       i++) {
    extensions_.erase(extensions_after_checkpoint_[i]);
  }

  symbols_after_checkpoint_.resize(
      checkpoint.pending_symbols_before_checkpoint);
  files_after_checkpoint_.resize(checkpoint.pending_files_before_checkpoint);
  extensions_after_checkpoint_.resize(
      checkpoint.pending_extensions_before_checkpoint);

  stldeletecontainerpointers(
      strings_.begin() + checkpoint.strings_before_checkpoint, strings_.end());
  stldeletecontainerpointers(
      messages_.begin() + checkpoint.messages_before_checkpoint,
      messages_.end());
  stldeletecontainerpointers(
      file_tables_.begin() + checkpoint.file_tables_before_checkpoint,
      file_tables_.end());
  for (int i = checkpoint.allocations_before_checkpoint;
       i < allocations_.size();
       i++) {
    operator delete(allocations_[i]);
  }

  strings_.resize(checkpoint.strings_before_checkpoint);
  messages_.resize(checkpoint.messages_before_checkpoint);
  file_tables_.resize(checkpoint.file_tables_before_checkpoint);
  allocations_.resize(checkpoint.allocations_before_checkpoint);
  checkpoints_.pop_back();
}

// -------------------------------------------------------------------

inline symbol descriptorpool::tables::findsymbol(const string& key) const {
  const symbol* result = findornull(symbols_by_name_, key.c_str());
  if (result == null) {
    return knullsymbol;
  } else {
    return *result;
  }
}

inline symbol filedescriptortables::findnestedsymbol(
    const void* parent, const string& name) const {
  const symbol* result =
    findornull(symbols_by_parent_, pointerstringpair(parent, name.c_str()));
  if (result == null) {
    return knullsymbol;
  } else {
    return *result;
  }
}

inline symbol filedescriptortables::findnestedsymboloftype(
    const void* parent, const string& name, const symbol::type type) const {
  symbol result = findnestedsymbol(parent, name);
  if (result.type != type) return knullsymbol;
  return result;
}

symbol descriptorpool::tables::findbynamehelper(
    const descriptorpool* pool, const string& name) const {
  mutexlockmaybe lock(pool->mutex_);
  symbol result = findsymbol(name);

  if (result.isnull() && pool->underlay_ != null) {
    // symbol not found; check the underlay.
    result =
      pool->underlay_->tables_->findbynamehelper(pool->underlay_, name);
  }

  if (result.isnull()) {
    // symbol still not found, so check fallback database.
    if (pool->tryfindsymbolinfallbackdatabase(name)) {
      result = findsymbol(name);
    }
  }

  return result;
}

inline const filedescriptor* descriptorpool::tables::findfile(
    const string& key) const {
  return findptrornull(files_by_name_, key.c_str());
}

inline const fielddescriptor* filedescriptortables::findfieldbynumber(
    const descriptor* parent, int number) const {
  return findptrornull(fields_by_number_, make_pair(parent, number));
}

inline const fielddescriptor* filedescriptortables::findfieldbylowercasename(
    const void* parent, const string& lowercase_name) const {
  return findptrornull(fields_by_lowercase_name_,
                       pointerstringpair(parent, lowercase_name.c_str()));
}

inline const fielddescriptor* filedescriptortables::findfieldbycamelcasename(
    const void* parent, const string& camelcase_name) const {
  return findptrornull(fields_by_camelcase_name_,
                       pointerstringpair(parent, camelcase_name.c_str()));
}

inline const enumvaluedescriptor* filedescriptortables::findenumvaluebynumber(
    const enumdescriptor* parent, int number) const {
  return findptrornull(enum_values_by_number_, make_pair(parent, number));
}

inline const fielddescriptor* descriptorpool::tables::findextension(
    const descriptor* extendee, int number) {
  return findptrornull(extensions_, make_pair(extendee, number));
}

inline void descriptorpool::tables::findallextensions(
    const descriptor* extendee, vector<const fielddescriptor*>* out) const {
  extensionsgroupedbydescriptormap::const_iterator it =
      extensions_.lower_bound(make_pair(extendee, 0));
  for (; it != extensions_.end() && it->first.first == extendee; ++it) {
    out->push_back(it->second);
  }
}

// -------------------------------------------------------------------

bool descriptorpool::tables::addsymbol(
    const string& full_name, symbol symbol) {
  if (insertifnotpresent(&symbols_by_name_, full_name.c_str(), symbol)) {
    symbols_after_checkpoint_.push_back(full_name.c_str());
    return true;
  } else {
    return false;
  }
}

bool filedescriptortables::addaliasunderparent(
    const void* parent, const string& name, symbol symbol) {
  pointerstringpair by_parent_key(parent, name.c_str());
  return insertifnotpresent(&symbols_by_parent_, by_parent_key, symbol);
}

bool descriptorpool::tables::addfile(const filedescriptor* file) {
  if (insertifnotpresent(&files_by_name_, file->name().c_str(), file)) {
    files_after_checkpoint_.push_back(file->name().c_str());
    return true;
  } else {
    return false;
  }
}

void filedescriptortables::addfieldbystylizednames(
    const fielddescriptor* field) {
  const void* parent;
  if (field->is_extension()) {
    if (field->extension_scope() == null) {
      parent = field->file();
    } else {
      parent = field->extension_scope();
    }
  } else {
    parent = field->containing_type();
  }

  pointerstringpair lowercase_key(parent, field->lowercase_name().c_str());
  insertifnotpresent(&fields_by_lowercase_name_, lowercase_key, field);

  pointerstringpair camelcase_key(parent, field->camelcase_name().c_str());
  insertifnotpresent(&fields_by_camelcase_name_, camelcase_key, field);
}

bool filedescriptortables::addfieldbynumber(const fielddescriptor* field) {
  descriptorintpair key(field->containing_type(), field->number());
  return insertifnotpresent(&fields_by_number_, key, field);
}

bool filedescriptortables::addenumvaluebynumber(
    const enumvaluedescriptor* value) {
  enumintpair key(value->type(), value->number());
  return insertifnotpresent(&enum_values_by_number_, key, value);
}

bool descriptorpool::tables::addextension(const fielddescriptor* field) {
  descriptorintpair key(field->containing_type(), field->number());
  if (insertifnotpresent(&extensions_, key, field)) {
    extensions_after_checkpoint_.push_back(key);
    return true;
  } else {
    return false;
  }
}

// -------------------------------------------------------------------

template<typename type>
type* descriptorpool::tables::allocate() {
  return reinterpret_cast<type*>(allocatebytes(sizeof(type)));
}

template<typename type>
type* descriptorpool::tables::allocatearray(int count) {
  return reinterpret_cast<type*>(allocatebytes(sizeof(type) * count));
}

string* descriptorpool::tables::allocatestring(const string& value) {
  string* result = new string(value);
  strings_.push_back(result);
  return result;
}

template<typename type>
type* descriptorpool::tables::allocatemessage(type* dummy) {
  type* result = new type;
  messages_.push_back(result);
  return result;
}

filedescriptortables* descriptorpool::tables::allocatefiletables() {
  filedescriptortables* result = new filedescriptortables;
  file_tables_.push_back(result);
  return result;
}

void* descriptorpool::tables::allocatebytes(int size) {
  // todo(kenton):  would it be worthwhile to implement this in some more
  // sophisticated way?  probably not for the open source release, but for
  // internal use we could easily plug in one of our existing memory pool
  // allocators...
  if (size == 0) return null;

  void* result = operator new(size);
  allocations_.push_back(result);
  return result;
}

// ===================================================================
// descriptorpool

descriptorpool::errorcollector::~errorcollector() {}

descriptorpool::descriptorpool()
  : mutex_(null),
    fallback_database_(null),
    default_error_collector_(null),
    underlay_(null),
    tables_(new tables),
    enforce_dependencies_(true),
    allow_unknown_(false) {}

descriptorpool::descriptorpool(descriptordatabase* fallback_database,
                               errorcollector* error_collector)
  : mutex_(new mutex),
    fallback_database_(fallback_database),
    default_error_collector_(error_collector),
    underlay_(null),
    tables_(new tables),
    enforce_dependencies_(true),
    allow_unknown_(false) {
}

descriptorpool::descriptorpool(const descriptorpool* underlay)
  : mutex_(null),
    fallback_database_(null),
    default_error_collector_(null),
    underlay_(underlay),
    tables_(new tables),
    enforce_dependencies_(true),
    allow_unknown_(false) {}

descriptorpool::~descriptorpool() {
  if (mutex_ != null) delete mutex_;
}

// descriptorpool::buildfile() defined later.
// descriptorpool::buildfilecollectingerrors() defined later.

void descriptorpool::internaldontenforcedependencies() {
  enforce_dependencies_ = false;
}

bool descriptorpool::internalisfileloaded(const string& filename) const {
  mutexlockmaybe lock(mutex_);
  return tables_->findfile(filename) != null;
}

// generated_pool ====================================================

namespace {


encodeddescriptordatabase* generated_database_ = null;
descriptorpool* generated_pool_ = null;
google_protobuf_declare_once(generated_pool_init_);

void deletegeneratedpool() {
  delete generated_database_;
  generated_database_ = null;
  delete generated_pool_;
  generated_pool_ = null;
}

static void initgeneratedpool() {
  generated_database_ = new encodeddescriptordatabase;
  generated_pool_ = new descriptorpool(generated_database_);

  internal::onshutdown(&deletegeneratedpool);
}

inline void initgeneratedpoolonce() {
  ::google::protobuf::googleonceinit(&generated_pool_init_, &initgeneratedpool);
}

}  // anonymous namespace

const descriptorpool* descriptorpool::generated_pool() {
  initgeneratedpoolonce();
  return generated_pool_;
}

descriptorpool* descriptorpool::internal_generated_pool() {
  initgeneratedpoolonce();
  return generated_pool_;
}

void descriptorpool::internaladdgeneratedfile(
    const void* encoded_file_descriptor, int size) {
  // so, this function is called in the process of initializing the
  // descriptors for generated proto classes.  each generated .pb.cc file
  // has an internal procedure called adddescriptors() which is called at
  // process startup, and that function calls this one in order to register
  // the raw bytes of the filedescriptorproto representing the file.
  //
  // we do not actually construct the descriptor objects right away.  we just
  // hang on to the bytes until they are actually needed.  we actually construct
  // the descriptor the first time one of the following things happens:
  // * someone calls a method like descriptor(), getdescriptor(), or
  //   getreflection() on the generated types, which requires returning the
  //   descriptor or an object based on it.
  // * someone looks up the descriptor in descriptorpool::generated_pool().
  //
  // once one of these happens, the descriptorpool actually parses the
  // filedescriptorproto and generates a filedescriptor (and all its children)
  // based on it.
  //
  // note that filedescriptorproto is itself a generated protocol message.
  // therefore, when we parse one, we have to be very careful to avoid using
  // any descriptor-based operations, since this might cause infinite recursion
  // or deadlock.
  initgeneratedpoolonce();
  google_check(generated_database_->add(encoded_file_descriptor, size));
}


// find*by* methods ==================================================

// todo(kenton):  there's a lot of repeated code here, but i'm not sure if
//   there's any good way to factor it out.  think about this some time when
//   there's nothing more important to do (read: never).

const filedescriptor* descriptorpool::findfilebyname(const string& name) const {
  mutexlockmaybe lock(mutex_);
  const filedescriptor* result = tables_->findfile(name);
  if (result != null) return result;
  if (underlay_ != null) {
    result = underlay_->findfilebyname(name);
    if (result != null) return result;
  }
  if (tryfindfileinfallbackdatabase(name)) {
    result = tables_->findfile(name);
    if (result != null) return result;
  }
  return null;
}

const filedescriptor* descriptorpool::findfilecontainingsymbol(
    const string& symbol_name) const {
  mutexlockmaybe lock(mutex_);
  symbol result = tables_->findsymbol(symbol_name);
  if (!result.isnull()) return result.getfile();
  if (underlay_ != null) {
    const filedescriptor* file_result =
      underlay_->findfilecontainingsymbol(symbol_name);
    if (file_result != null) return file_result;
  }
  if (tryfindsymbolinfallbackdatabase(symbol_name)) {
    result = tables_->findsymbol(symbol_name);
    if (!result.isnull()) return result.getfile();
  }
  return null;
}

const descriptor* descriptorpool::findmessagetypebyname(
    const string& name) const {
  symbol result = tables_->findbynamehelper(this, name);
  return (result.type == symbol::message) ? result.descriptor : null;
}

const fielddescriptor* descriptorpool::findfieldbyname(
    const string& name) const {
  symbol result = tables_->findbynamehelper(this, name);
  if (result.type == symbol::field &&
      !result.field_descriptor->is_extension()) {
    return result.field_descriptor;
  } else {
    return null;
  }
}

const fielddescriptor* descriptorpool::findextensionbyname(
    const string& name) const {
  symbol result = tables_->findbynamehelper(this, name);
  if (result.type == symbol::field &&
      result.field_descriptor->is_extension()) {
    return result.field_descriptor;
  } else {
    return null;
  }
}

const enumdescriptor* descriptorpool::findenumtypebyname(
    const string& name) const {
  symbol result = tables_->findbynamehelper(this, name);
  return (result.type == symbol::enum) ? result.enum_descriptor : null;
}

const enumvaluedescriptor* descriptorpool::findenumvaluebyname(
    const string& name) const {
  symbol result = tables_->findbynamehelper(this, name);
  return (result.type == symbol::enum_value) ?
    result.enum_value_descriptor : null;
}

const servicedescriptor* descriptorpool::findservicebyname(
    const string& name) const {
  symbol result = tables_->findbynamehelper(this, name);
  return (result.type == symbol::service) ? result.service_descriptor : null;
}

const methoddescriptor* descriptorpool::findmethodbyname(
    const string& name) const {
  symbol result = tables_->findbynamehelper(this, name);
  return (result.type == symbol::method) ? result.method_descriptor : null;
}

const fielddescriptor* descriptorpool::findextensionbynumber(
    const descriptor* extendee, int number) const {
  mutexlockmaybe lock(mutex_);
  const fielddescriptor* result = tables_->findextension(extendee, number);
  if (result != null) {
    return result;
  }
  if (underlay_ != null) {
    result = underlay_->findextensionbynumber(extendee, number);
    if (result != null) return result;
  }
  if (tryfindextensioninfallbackdatabase(extendee, number)) {
    result = tables_->findextension(extendee, number);
    if (result != null) {
      return result;
    }
  }
  return null;
}

void descriptorpool::findallextensions(
    const descriptor* extendee, vector<const fielddescriptor*>* out) const {
  mutexlockmaybe lock(mutex_);

  // initialize tables_->extensions_ from the fallback database first
  // (but do this only once per descriptor).
  if (fallback_database_ != null &&
      tables_->extensions_loaded_from_db_.count(extendee) == 0) {
    vector<int> numbers;
    if (fallback_database_->findallextensionnumbers(extendee->full_name(),
                                                    &numbers)) {
      for (int i = 0; i < numbers.size(); ++i) {
        int number = numbers[i];
        if (tables_->findextension(extendee, number) == null) {
          tryfindextensioninfallbackdatabase(extendee, number);
        }
      }
      tables_->extensions_loaded_from_db_.insert(extendee);
    }
  }

  tables_->findallextensions(extendee, out);
  if (underlay_ != null) {
    underlay_->findallextensions(extendee, out);
  }
}

// -------------------------------------------------------------------

const fielddescriptor*
descriptor::findfieldbynumber(int key) const {
  const fielddescriptor* result =
    file()->tables_->findfieldbynumber(this, key);
  if (result == null || result->is_extension()) {
    return null;
  } else {
    return result;
  }
}

const fielddescriptor*
descriptor::findfieldbylowercasename(const string& key) const {
  const fielddescriptor* result =
    file()->tables_->findfieldbylowercasename(this, key);
  if (result == null || result->is_extension()) {
    return null;
  } else {
    return result;
  }
}

const fielddescriptor*
descriptor::findfieldbycamelcasename(const string& key) const {
  const fielddescriptor* result =
    file()->tables_->findfieldbycamelcasename(this, key);
  if (result == null || result->is_extension()) {
    return null;
  } else {
    return result;
  }
}

const fielddescriptor*
descriptor::findfieldbyname(const string& key) const {
  symbol result =
    file()->tables_->findnestedsymboloftype(this, key, symbol::field);
  if (!result.isnull() && !result.field_descriptor->is_extension()) {
    return result.field_descriptor;
  } else {
    return null;
  }
}

const fielddescriptor*
descriptor::findextensionbyname(const string& key) const {
  symbol result =
    file()->tables_->findnestedsymboloftype(this, key, symbol::field);
  if (!result.isnull() && result.field_descriptor->is_extension()) {
    return result.field_descriptor;
  } else {
    return null;
  }
}

const fielddescriptor*
descriptor::findextensionbylowercasename(const string& key) const {
  const fielddescriptor* result =
    file()->tables_->findfieldbylowercasename(this, key);
  if (result == null || !result->is_extension()) {
    return null;
  } else {
    return result;
  }
}

const fielddescriptor*
descriptor::findextensionbycamelcasename(const string& key) const {
  const fielddescriptor* result =
    file()->tables_->findfieldbycamelcasename(this, key);
  if (result == null || !result->is_extension()) {
    return null;
  } else {
    return result;
  }
}

const descriptor*
descriptor::findnestedtypebyname(const string& key) const {
  symbol result =
    file()->tables_->findnestedsymboloftype(this, key, symbol::message);
  if (!result.isnull()) {
    return result.descriptor;
  } else {
    return null;
  }
}

const enumdescriptor*
descriptor::findenumtypebyname(const string& key) const {
  symbol result =
    file()->tables_->findnestedsymboloftype(this, key, symbol::enum);
  if (!result.isnull()) {
    return result.enum_descriptor;
  } else {
    return null;
  }
}

const enumvaluedescriptor*
descriptor::findenumvaluebyname(const string& key) const {
  symbol result =
    file()->tables_->findnestedsymboloftype(this, key, symbol::enum_value);
  if (!result.isnull()) {
    return result.enum_value_descriptor;
  } else {
    return null;
  }
}

const enumvaluedescriptor*
enumdescriptor::findvaluebyname(const string& key) const {
  symbol result =
    file()->tables_->findnestedsymboloftype(this, key, symbol::enum_value);
  if (!result.isnull()) {
    return result.enum_value_descriptor;
  } else {
    return null;
  }
}

const enumvaluedescriptor*
enumdescriptor::findvaluebynumber(int key) const {
  return file()->tables_->findenumvaluebynumber(this, key);
}

const methoddescriptor*
servicedescriptor::findmethodbyname(const string& key) const {
  symbol result =
    file()->tables_->findnestedsymboloftype(this, key, symbol::method);
  if (!result.isnull()) {
    return result.method_descriptor;
  } else {
    return null;
  }
}

const descriptor*
filedescriptor::findmessagetypebyname(const string& key) const {
  symbol result = tables_->findnestedsymboloftype(this, key, symbol::message);
  if (!result.isnull()) {
    return result.descriptor;
  } else {
    return null;
  }
}

const enumdescriptor*
filedescriptor::findenumtypebyname(const string& key) const {
  symbol result = tables_->findnestedsymboloftype(this, key, symbol::enum);
  if (!result.isnull()) {
    return result.enum_descriptor;
  } else {
    return null;
  }
}

const enumvaluedescriptor*
filedescriptor::findenumvaluebyname(const string& key) const {
  symbol result =
    tables_->findnestedsymboloftype(this, key, symbol::enum_value);
  if (!result.isnull()) {
    return result.enum_value_descriptor;
  } else {
    return null;
  }
}

const servicedescriptor*
filedescriptor::findservicebyname(const string& key) const {
  symbol result = tables_->findnestedsymboloftype(this, key, symbol::service);
  if (!result.isnull()) {
    return result.service_descriptor;
  } else {
    return null;
  }
}

const fielddescriptor*
filedescriptor::findextensionbyname(const string& key) const {
  symbol result = tables_->findnestedsymboloftype(this, key, symbol::field);
  if (!result.isnull() && result.field_descriptor->is_extension()) {
    return result.field_descriptor;
  } else {
    return null;
  }
}

const fielddescriptor*
filedescriptor::findextensionbylowercasename(const string& key) const {
  const fielddescriptor* result = tables_->findfieldbylowercasename(this, key);
  if (result == null || !result->is_extension()) {
    return null;
  } else {
    return result;
  }
}

const fielddescriptor*
filedescriptor::findextensionbycamelcasename(const string& key) const {
  const fielddescriptor* result = tables_->findfieldbycamelcasename(this, key);
  if (result == null || !result->is_extension()) {
    return null;
  } else {
    return result;
  }
}

bool descriptor::isextensionnumber(int number) const {
  // linear search should be fine because we don't expect a message to have
  // more than a couple extension ranges.
  for (int i = 0; i < extension_range_count(); i++) {
    if (number >= extension_range(i)->start &&
        number <  extension_range(i)->end) {
      return true;
    }
  }
  return false;
}

// -------------------------------------------------------------------

bool descriptorpool::tryfindfileinfallbackdatabase(const string& name) const {
  if (fallback_database_ == null) return false;

  if (tables_->known_bad_files_.count(name) > 0) return false;

  filedescriptorproto file_proto;
  if (!fallback_database_->findfilebyname(name, &file_proto) ||
      buildfilefromdatabase(file_proto) == null) {
    tables_->known_bad_files_.insert(name);
    return false;
  }

  return true;
}

bool descriptorpool::issubsymbolofbuilttype(const string& name) const {
  string prefix = name;
  for (;;) {
    string::size_type dot_pos = prefix.find_last_of('.');
    if (dot_pos == string::npos) {
      break;
    }
    prefix = prefix.substr(0, dot_pos);
    symbol symbol = tables_->findsymbol(prefix);
    // if the symbol type is anything other than package, then its complete
    // definition is already known.
    if (!symbol.isnull() && symbol.type != symbol::package) {
      return true;
    }
  }
  if (underlay_ != null) {
    // check to see if any prefix of this symbol exists in the underlay.
    return underlay_->issubsymbolofbuilttype(name);
  }
  return false;
}

bool descriptorpool::tryfindsymbolinfallbackdatabase(const string& name) const {
  if (fallback_database_ == null) return false;

  // we skip looking in the fallback database if the name is a sub-symbol of
  // any descriptor that already exists in the descriptor pool (except for
  // package descriptors).  this is valid because all symbols except for
  // packages are defined in a single file, so if the symbol exists then we
  // should already have its definition.
  //
  // the other reason to do this is to support "overriding" type definitions
  // by merging two databases that define the same type.  (yes, people do
  // this.)  the main difficulty with making this work is that
  // findfilecontainingsymbol() is allowed to return both false positives
  // (e.g., simpledescriptordatabase, upgradeddescriptordatabase) and false
  // negatives (e.g. protofileparser, sourcetreedescriptordatabase).  when two
  // such databases are merged, looking up a non-existent sub-symbol of a type
  // that already exists in the descriptor pool can result in an attempt to
  // load multiple definitions of the same type.  the check below avoids this.
  if (issubsymbolofbuilttype(name)) return false;

  filedescriptorproto file_proto;
  if (!fallback_database_->findfilecontainingsymbol(name, &file_proto)) {
    return false;
  }

  if (tables_->findfile(file_proto.name()) != null) {
    // we've already loaded this file, and it apparently doesn't contain the
    // symbol we're looking for.  some descriptordatabases return false
    // positives.
    return false;
  }

  if (buildfilefromdatabase(file_proto) == null) {
    return false;
  }

  return true;
}

bool descriptorpool::tryfindextensioninfallbackdatabase(
    const descriptor* containing_type, int field_number) const {
  if (fallback_database_ == null) return false;

  filedescriptorproto file_proto;
  if (!fallback_database_->findfilecontainingextension(
        containing_type->full_name(), field_number, &file_proto)) {
    return false;
  }

  if (tables_->findfile(file_proto.name()) != null) {
    // we've already loaded this file, and it apparently doesn't contain the
    // extension we're looking for.  some descriptordatabases return false
    // positives.
    return false;
  }

  if (buildfilefromdatabase(file_proto) == null) {
    return false;
  }

  return true;
}

// ===================================================================

string fielddescriptor::defaultvalueasstring(bool quote_string_type) const {
  google_check(has_default_value()) << "no default value";
  switch (cpp_type()) {
    case cpptype_int32:
      return simpleitoa(default_value_int32());
      break;
    case cpptype_int64:
      return simpleitoa(default_value_int64());
      break;
    case cpptype_uint32:
      return simpleitoa(default_value_uint32());
      break;
    case cpptype_uint64:
      return simpleitoa(default_value_uint64());
      break;
    case cpptype_float:
      return simpleftoa(default_value_float());
      break;
    case cpptype_double:
      return simpledtoa(default_value_double());
      break;
    case cpptype_bool:
      return default_value_bool() ? "true" : "false";
      break;
    case cpptype_string:
      if (quote_string_type) {
        return "\"" + cescape(default_value_string()) + "\"";
      } else {
        if (type() == type_bytes) {
          return cescape(default_value_string());
        } else {
          return default_value_string();
        }
      }
      break;
    case cpptype_enum:
      return default_value_enum()->name();
      break;
    case cpptype_message:
      google_log(dfatal) << "messages can't have default values!";
      break;
  }
  google_log(fatal) << "can't get here: failed to get default value as string";
  return "";
}

// copyto methods ====================================================

void filedescriptor::copyto(filedescriptorproto* proto) const {
  proto->set_name(name());
  if (!package().empty()) proto->set_package(package());

  for (int i = 0; i < dependency_count(); i++) {
    proto->add_dependency(dependency(i)->name());
  }

  for (int i = 0; i < public_dependency_count(); i++) {
    proto->add_public_dependency(public_dependencies_[i]);
  }

  for (int i = 0; i < weak_dependency_count(); i++) {
    proto->add_weak_dependency(weak_dependencies_[i]);
  }

  for (int i = 0; i < message_type_count(); i++) {
    message_type(i)->copyto(proto->add_message_type());
  }
  for (int i = 0; i < enum_type_count(); i++) {
    enum_type(i)->copyto(proto->add_enum_type());
  }
  for (int i = 0; i < service_count(); i++) {
    service(i)->copyto(proto->add_service());
  }
  for (int i = 0; i < extension_count(); i++) {
    extension(i)->copyto(proto->add_extension());
  }

  if (&options() != &fileoptions::default_instance()) {
    proto->mutable_options()->copyfrom(options());
  }
}

void filedescriptor::copysourcecodeinfoto(filedescriptorproto* proto) const {
  if (source_code_info_ != &sourcecodeinfo::default_instance()) {
    proto->mutable_source_code_info()->copyfrom(*source_code_info_);
  }
}

void descriptor::copyto(descriptorproto* proto) const {
  proto->set_name(name());

  for (int i = 0; i < field_count(); i++) {
    field(i)->copyto(proto->add_field());
  }
  for (int i = 0; i < nested_type_count(); i++) {
    nested_type(i)->copyto(proto->add_nested_type());
  }
  for (int i = 0; i < enum_type_count(); i++) {
    enum_type(i)->copyto(proto->add_enum_type());
  }
  for (int i = 0; i < extension_range_count(); i++) {
    descriptorproto::extensionrange* range = proto->add_extension_range();
    range->set_start(extension_range(i)->start);
    range->set_end(extension_range(i)->end);
  }
  for (int i = 0; i < extension_count(); i++) {
    extension(i)->copyto(proto->add_extension());
  }

  if (&options() != &messageoptions::default_instance()) {
    proto->mutable_options()->copyfrom(options());
  }
}

void fielddescriptor::copyto(fielddescriptorproto* proto) const {
  proto->set_name(name());
  proto->set_number(number());

  // some compilers do not allow static_cast directly between two enum types,
  // so we must cast to int first.
  proto->set_label(static_cast<fielddescriptorproto::label>(
                     implicit_cast<int>(label())));
  proto->set_type(static_cast<fielddescriptorproto::type>(
                    implicit_cast<int>(type())));

  if (is_extension()) {
    if (!containing_type()->is_unqualified_placeholder_) {
      proto->set_extendee(".");
    }
    proto->mutable_extendee()->append(containing_type()->full_name());
  }

  if (cpp_type() == cpptype_message) {
    if (message_type()->is_placeholder_) {
      // we don't actually know if the type is a message type.  it could be
      // an enum.
      proto->clear_type();
    }

    if (!message_type()->is_unqualified_placeholder_) {
      proto->set_type_name(".");
    }
    proto->mutable_type_name()->append(message_type()->full_name());
  } else if (cpp_type() == cpptype_enum) {
    if (!enum_type()->is_unqualified_placeholder_) {
      proto->set_type_name(".");
    }
    proto->mutable_type_name()->append(enum_type()->full_name());
  }

  if (has_default_value()) {
    proto->set_default_value(defaultvalueasstring(false));
  }

  if (&options() != &fieldoptions::default_instance()) {
    proto->mutable_options()->copyfrom(options());
  }
}

void enumdescriptor::copyto(enumdescriptorproto* proto) const {
  proto->set_name(name());

  for (int i = 0; i < value_count(); i++) {
    value(i)->copyto(proto->add_value());
  }

  if (&options() != &enumoptions::default_instance()) {
    proto->mutable_options()->copyfrom(options());
  }
}

void enumvaluedescriptor::copyto(enumvaluedescriptorproto* proto) const {
  proto->set_name(name());
  proto->set_number(number());

  if (&options() != &enumvalueoptions::default_instance()) {
    proto->mutable_options()->copyfrom(options());
  }
}

void servicedescriptor::copyto(servicedescriptorproto* proto) const {
  proto->set_name(name());

  for (int i = 0; i < method_count(); i++) {
    method(i)->copyto(proto->add_method());
  }

  if (&options() != &serviceoptions::default_instance()) {
    proto->mutable_options()->copyfrom(options());
  }
}

void methoddescriptor::copyto(methoddescriptorproto* proto) const {
  proto->set_name(name());

  if (!input_type()->is_unqualified_placeholder_) {
    proto->set_input_type(".");
  }
  proto->mutable_input_type()->append(input_type()->full_name());

  if (!output_type()->is_unqualified_placeholder_) {
    proto->set_output_type(".");
  }
  proto->mutable_output_type()->append(output_type()->full_name());

  if (&options() != &methodoptions::default_instance()) {
    proto->mutable_options()->copyfrom(options());
  }
}

// debugstring methods ===============================================

namespace {

// used by each of the option formatters.
bool retrieveoptions(int depth,
                     const message &options,
                     vector<string> *option_entries) {
  option_entries->clear();
  const reflection* reflection = options.getreflection();
  vector<const fielddescriptor*> fields;
  reflection->listfields(options, &fields);
  for (int i = 0; i < fields.size(); i++) {
    int count = 1;
    bool repeated = false;
    if (fields[i]->is_repeated()) {
      count = reflection->fieldsize(options, fields[i]);
      repeated = true;
    }
    for (int j = 0; j < count; j++) {
      string fieldval;
      if (fields[i]->cpp_type() == fielddescriptor::cpptype_message) {
        string tmp;
        textformat::printer printer;
        printer.setinitialindentlevel(depth + 1);
        printer.printfieldvaluetostring(options, fields[i],
                                        repeated ? j : -1, &tmp);
        fieldval.append("{\n");
        fieldval.append(tmp);
        fieldval.append(depth * 2, ' ');
        fieldval.append("}");
      } else {
        textformat::printfieldvaluetostring(options, fields[i],
                                            repeated ? j : -1, &fieldval);
      }
      string name;
      if (fields[i]->is_extension()) {
        name = "(." + fields[i]->full_name() + ")";
      } else {
        name = fields[i]->name();
      }
      option_entries->push_back(name + " = " + fieldval);
    }
  }
  return !option_entries->empty();
}

// formats options that all appear together in brackets. does not include
// brackets.
bool formatbracketedoptions(int depth, const message &options, string *output) {
  vector<string> all_options;
  if (retrieveoptions(depth, options, &all_options)) {
    output->append(joinstrings(all_options, ", "));
  }
  return !all_options.empty();
}

// formats options one per line
bool formatlineoptions(int depth, const message &options, string *output) {
  string prefix(depth * 2, ' ');
  vector<string> all_options;
  if (retrieveoptions(depth, options, &all_options)) {
    for (int i = 0; i < all_options.size(); i++) {
      strings::substituteandappend(output, "$0option $1;\n",
                                   prefix, all_options[i]);
    }
  }
  return !all_options.empty();
}

}  // anonymous namespace

string filedescriptor::debugstring() const {
  string contents = "syntax = \"proto2\";\n\n";

  set<int> public_dependencies;
  set<int> weak_dependencies;
  public_dependencies.insert(public_dependencies_,
                             public_dependencies_ + public_dependency_count_);
  weak_dependencies.insert(weak_dependencies_,
                           weak_dependencies_ + weak_dependency_count_);

  for (int i = 0; i < dependency_count(); i++) {
    if (public_dependencies.count(i) > 0) {
      strings::substituteandappend(&contents, "import public \"$0\";\n",
                                   dependency(i)->name());
    } else if (weak_dependencies.count(i) > 0) {
      strings::substituteandappend(&contents, "import weak \"$0\";\n",
                                   dependency(i)->name());
    } else {
      strings::substituteandappend(&contents, "import \"$0\";\n",
                                   dependency(i)->name());
    }
  }

  if (!package().empty()) {
    strings::substituteandappend(&contents, "package $0;\n\n", package());
  }

  if (formatlineoptions(0, options(), &contents)) {
    contents.append("\n");  // add some space if we had options
  }

  for (int i = 0; i < enum_type_count(); i++) {
    enum_type(i)->debugstring(0, &contents);
    contents.append("\n");
  }

  // find all the 'group' type extensions; we will not output their nested
  // definitions (those will be done with their group field descriptor).
  set<const descriptor*> groups;
  for (int i = 0; i < extension_count(); i++) {
    if (extension(i)->type() == fielddescriptor::type_group) {
      groups.insert(extension(i)->message_type());
    }
  }

  for (int i = 0; i < message_type_count(); i++) {
    if (groups.count(message_type(i)) == 0) {
      strings::substituteandappend(&contents, "message $0",
                                   message_type(i)->name());
      message_type(i)->debugstring(0, &contents);
      contents.append("\n");
    }
  }

  for (int i = 0; i < service_count(); i++) {
    service(i)->debugstring(&contents);
    contents.append("\n");
  }

  const descriptor* containing_type = null;
  for (int i = 0; i < extension_count(); i++) {
    if (extension(i)->containing_type() != containing_type) {
      if (i > 0) contents.append("}\n\n");
      containing_type = extension(i)->containing_type();
      strings::substituteandappend(&contents, "extend .$0 {\n",
                                   containing_type->full_name());
    }
    extension(i)->debugstring(1, &contents);
  }
  if (extension_count() > 0) contents.append("}\n\n");

  return contents;
}

string descriptor::debugstring() const {
  string contents;
  strings::substituteandappend(&contents, "message $0", name());
  debugstring(0, &contents);
  return contents;
}

void descriptor::debugstring(int depth, string *contents) const {
  string prefix(depth * 2, ' ');
  ++depth;
  contents->append(" {\n");

  formatlineoptions(depth, options(), contents);

  // find all the 'group' types for fields and extensions; we will not output
  // their nested definitions (those will be done with their group field
  // descriptor).
  set<const descriptor*> groups;
  for (int i = 0; i < field_count(); i++) {
    if (field(i)->type() == fielddescriptor::type_group) {
      groups.insert(field(i)->message_type());
    }
  }
  for (int i = 0; i < extension_count(); i++) {
    if (extension(i)->type() == fielddescriptor::type_group) {
      groups.insert(extension(i)->message_type());
    }
  }

  for (int i = 0; i < nested_type_count(); i++) {
    if (groups.count(nested_type(i)) == 0) {
      strings::substituteandappend(contents, "$0  message $1",
                                   prefix, nested_type(i)->name());
      nested_type(i)->debugstring(depth, contents);
    }
  }
  for (int i = 0; i < enum_type_count(); i++) {
    enum_type(i)->debugstring(depth, contents);
  }
  for (int i = 0; i < field_count(); i++) {
    field(i)->debugstring(depth, contents);
  }

  for (int i = 0; i < extension_range_count(); i++) {
    strings::substituteandappend(contents, "$0  extensions $1 to $2;\n",
                                 prefix,
                                 extension_range(i)->start,
                                 extension_range(i)->end - 1);
  }

  // group extensions by what they extend, so they can be printed out together.
  const descriptor* containing_type = null;
  for (int i = 0; i < extension_count(); i++) {
    if (extension(i)->containing_type() != containing_type) {
      if (i > 0) strings::substituteandappend(contents, "$0  }\n", prefix);
      containing_type = extension(i)->containing_type();
      strings::substituteandappend(contents, "$0  extend .$1 {\n",
                                   prefix, containing_type->full_name());
    }
    extension(i)->debugstring(depth + 1, contents);
  }
  if (extension_count() > 0)
    strings::substituteandappend(contents, "$0  }\n", prefix);

  strings::substituteandappend(contents, "$0}\n", prefix);
}

string fielddescriptor::debugstring() const {
  string contents;
  int depth = 0;
  if (is_extension()) {
    strings::substituteandappend(&contents, "extend .$0 {\n",
                                 containing_type()->full_name());
    depth = 1;
  }
  debugstring(depth, &contents);
  if (is_extension()) {
     contents.append("}\n");
  }
  return contents;
}

void fielddescriptor::debugstring(int depth, string *contents) const {
  string prefix(depth * 2, ' ');
  string field_type;
  switch (type()) {
    case type_message:
      field_type = "." + message_type()->full_name();
      break;
    case type_enum:
      field_type = "." + enum_type()->full_name();
      break;
    default:
      field_type = ktypetoname[type()];
  }

  strings::substituteandappend(contents, "$0$1 $2 $3 = $4",
                               prefix,
                               klabeltoname[label()],
                               field_type,
                               type() == type_group ? message_type()->name() :
                                                      name(),
                               number());

  bool bracketed = false;
  if (has_default_value()) {
    bracketed = true;
    strings::substituteandappend(contents, " [default = $0",
                                 defaultvalueasstring(true));
  }

  string formatted_options;
  if (formatbracketedoptions(depth, options(), &formatted_options)) {
    contents->append(bracketed ? ", " : " [");
    bracketed = true;
    contents->append(formatted_options);
  }

  if (bracketed) {
    contents->append("]");
  }

  if (type() == type_group) {
    message_type()->debugstring(depth, contents);
  } else {
    contents->append(";\n");
  }
}

string enumdescriptor::debugstring() const {
  string contents;
  debugstring(0, &contents);
  return contents;
}

void enumdescriptor::debugstring(int depth, string *contents) const {
  string prefix(depth * 2, ' ');
  ++depth;
  strings::substituteandappend(contents, "$0enum $1 {\n",
                               prefix, name());

  formatlineoptions(depth, options(), contents);

  for (int i = 0; i < value_count(); i++) {
    value(i)->debugstring(depth, contents);
  }
  strings::substituteandappend(contents, "$0}\n", prefix);
}

string enumvaluedescriptor::debugstring() const {
  string contents;
  debugstring(0, &contents);
  return contents;
}

void enumvaluedescriptor::debugstring(int depth, string *contents) const {
  string prefix(depth * 2, ' ');
  strings::substituteandappend(contents, "$0$1 = $2",
                               prefix, name(), number());

  string formatted_options;
  if (formatbracketedoptions(depth, options(), &formatted_options)) {
    strings::substituteandappend(contents, " [$0]", formatted_options);
  }
  contents->append(";\n");
}

string servicedescriptor::debugstring() const {
  string contents;
  debugstring(&contents);
  return contents;
}

void servicedescriptor::debugstring(string *contents) const {
  strings::substituteandappend(contents, "service $0 {\n", name());

  formatlineoptions(1, options(), contents);

  for (int i = 0; i < method_count(); i++) {
    method(i)->debugstring(1, contents);
  }

  contents->append("}\n");
}

string methoddescriptor::debugstring() const {
  string contents;
  debugstring(0, &contents);
  return contents;
}

void methoddescriptor::debugstring(int depth, string *contents) const {
  string prefix(depth * 2, ' ');
  ++depth;
  strings::substituteandappend(contents, "$0rpc $1(.$2) returns (.$3)",
                               prefix, name(),
                               input_type()->full_name(),
                               output_type()->full_name());

  string formatted_options;
  if (formatlineoptions(depth, options(), &formatted_options)) {
    strings::substituteandappend(contents, " {\n$0$1}\n",
                                 formatted_options, prefix);
  } else {
    contents->append(";\n");
  }
}


// location methods ===============================================

static bool pathsequal(const vector<int>& x, const repeatedfield<int32>& y) {
  if (x.size() != y.size()) return false;
  for (int i = 0; i < x.size(); ++i) {
    if (x[i] != y.get(i)) return false;
  }
  return true;
}

bool filedescriptor::getsourcelocation(const vector<int>& path,
                                       sourcelocation* out_location) const {
  google_check_notnull(out_location);
  const sourcecodeinfo* info = source_code_info_;
  for (int i = 0; info && i < info->location_size(); ++i) {
    if (pathsequal(path, info->location(i).path())) {
      const repeatedfield<int32>& span = info->location(i).span();
      if (span.size() == 3 || span.size() == 4) {
        out_location->start_line   = span.get(0);
        out_location->start_column = span.get(1);
        out_location->end_line     = span.get(span.size() == 3 ? 0 : 2);
        out_location->end_column   = span.get(span.size() - 1);

        out_location->leading_comments = info->location(i).leading_comments();
        out_location->trailing_comments = info->location(i).trailing_comments();
        return true;
      }
    }
  }
  return false;
}

bool fielddescriptor::is_packed() const {
  return is_packable() && (options_ != null) && options_->packed();
}

bool descriptor::getsourcelocation(sourcelocation* out_location) const {
  vector<int> path;
  getlocationpath(&path);
  return file()->getsourcelocation(path, out_location);
}

bool fielddescriptor::getsourcelocation(sourcelocation* out_location) const {
  vector<int> path;
  getlocationpath(&path);
  return file()->getsourcelocation(path, out_location);
}

bool enumdescriptor::getsourcelocation(sourcelocation* out_location) const {
  vector<int> path;
  getlocationpath(&path);
  return file()->getsourcelocation(path, out_location);
}

bool methoddescriptor::getsourcelocation(sourcelocation* out_location) const {
  vector<int> path;
  getlocationpath(&path);
  return service()->file()->getsourcelocation(path, out_location);
}

bool servicedescriptor::getsourcelocation(sourcelocation* out_location) const {
  vector<int> path;
  getlocationpath(&path);
  return file()->getsourcelocation(path, out_location);
}

bool enumvaluedescriptor::getsourcelocation(
    sourcelocation* out_location) const {
  vector<int> path;
  getlocationpath(&path);
  return type()->file()->getsourcelocation(path, out_location);
}

void descriptor::getlocationpath(vector<int>* output) const {
  if (containing_type()) {
    containing_type()->getlocationpath(output);
    output->push_back(descriptorproto::knestedtypefieldnumber);
    output->push_back(index());
  } else {
    output->push_back(filedescriptorproto::kmessagetypefieldnumber);
    output->push_back(index());
  }
}

void fielddescriptor::getlocationpath(vector<int>* output) const {
  containing_type()->getlocationpath(output);
  output->push_back(descriptorproto::kfieldfieldnumber);
  output->push_back(index());
}

void enumdescriptor::getlocationpath(vector<int>* output) const {
  if (containing_type()) {
    containing_type()->getlocationpath(output);
    output->push_back(descriptorproto::kenumtypefieldnumber);
    output->push_back(index());
  } else {
    output->push_back(filedescriptorproto::kenumtypefieldnumber);
    output->push_back(index());
  }
}

void enumvaluedescriptor::getlocationpath(vector<int>* output) const {
  type()->getlocationpath(output);
  output->push_back(enumdescriptorproto::kvaluefieldnumber);
  output->push_back(index());
}

void servicedescriptor::getlocationpath(vector<int>* output) const {
  output->push_back(filedescriptorproto::kservicefieldnumber);
  output->push_back(index());
}

void methoddescriptor::getlocationpath(vector<int>* output) const {
  service()->getlocationpath(output);
  output->push_back(servicedescriptorproto::kmethodfieldnumber);
  output->push_back(index());
}

// ===================================================================

namespace {

// represents an options message to interpret. extension names in the option
// name are respolved relative to name_scope. element_name and orig_opt are
// used only for error reporting (since the parser records locations against
// pointers in the original options, not the mutable copy). the message must be
// one of the options messages in descriptor.proto.
struct optionstointerpret {
  optionstointerpret(const string& ns,
                     const string& el,
                     const message* orig_opt,
                     message* opt)
      : name_scope(ns),
        element_name(el),
        original_options(orig_opt),
        options(opt) {
  }
  string name_scope;
  string element_name;
  const message* original_options;
  message* options;
};

}  // namespace

class descriptorbuilder {
 public:
  descriptorbuilder(const descriptorpool* pool,
                    descriptorpool::tables* tables,
                    descriptorpool::errorcollector* error_collector);
  ~descriptorbuilder();

  const filedescriptor* buildfile(const filedescriptorproto& proto);

 private:
  friend class optioninterpreter;

  const descriptorpool* pool_;
  descriptorpool::tables* tables_;  // for convenience
  descriptorpool::errorcollector* error_collector_;

  // as we build descriptors we store copies of the options messages in
  // them. we put pointers to those copies in this vector, as we build, so we
  // can later (after cross-linking) interpret those options.
  vector<optionstointerpret> options_to_interpret_;

  bool had_errors_;
  string filename_;
  filedescriptor* file_;
  filedescriptortables* file_tables_;
  set<const filedescriptor*> dependencies_;

  // if lookupsymbol() finds a symbol that is in a file which is not a declared
  // dependency of this file, it will fail, but will set
  // possible_undeclared_dependency_ to point at that file.  this is only used
  // by addnotdefinederror() to report a more useful error message.
  // possible_undeclared_dependency_name_ is the name of the symbol that was
  // actually found in possible_undeclared_dependency_, which may be a parent
  // of the symbol actually looked for.
  const filedescriptor* possible_undeclared_dependency_;
  string possible_undeclared_dependency_name_;

  void adderror(const string& element_name,
                const message& descriptor,
                descriptorpool::errorcollector::errorlocation location,
                const string& error);

  // adds an error indicating that undefined_symbol was not defined.  must
  // only be called after lookupsymbol() fails.
  void addnotdefinederror(
    const string& element_name,
    const message& descriptor,
    descriptorpool::errorcollector::errorlocation location,
    const string& undefined_symbol);

  // silly helper which determines if the given file is in the given package.
  // i.e., either file->package() == package_name or file->package() is a
  // nested package within package_name.
  bool isinpackage(const filedescriptor* file, const string& package_name);

  // helper function which finds all public dependencies of the given file, and
  // stores the them in the dependencies_ set in the builder.
  void recordpublicdependencies(const filedescriptor* file);

  // like tables_->findsymbol(), but additionally:
  // - search the pool's underlay if not found in tables_.
  // - insure that the resulting symbol is from one of the file's declared
  //   dependencies.
  symbol findsymbol(const string& name);

  // like findsymbol() but does not require that the symbol is in one of the
  // file's declared dependencies.
  symbol findsymbolnotenforcingdeps(const string& name);

  // this implements the body of findsymbolnotenforcingdeps().
  symbol findsymbolnotenforcingdepshelper(const descriptorpool* pool,
                                          const string& name);

  // like findsymbol(), but looks up the name relative to some other symbol
  // name.  this first searches siblings of relative_to, then siblings of its
  // parents, etc.  for example, lookupsymbol("foo.bar", "baz.qux.corge") makes
  // the following calls, returning the first non-null result:
  // findsymbol("baz.qux.foo.bar"), findsymbol("baz.foo.bar"),
  // findsymbol("foo.bar").  if allowunknowndependencies() has been called
  // on the descriptorpool, this will generate a placeholder type if
  // the name is not found (unless the name itself is malformed).  the
  // placeholder_type parameter indicates what kind of placeholder should be
  // constructed in this case.  the resolve_mode parameter determines whether
  // any symbol is returned, or only symbols that are types.  note, however,
  // that lookupsymbol may still return a non-type symbol in lookup_types mode,
  // if it believes that's all it could refer to.  the caller should always
  // check that it receives the type of symbol it was expecting.
  enum placeholdertype {
    placeholder_message,
    placeholder_enum,
    placeholder_extendable_message
  };
  enum resolvemode {
    lookup_all, lookup_types
  };
  symbol lookupsymbol(const string& name, const string& relative_to,
                      placeholdertype placeholder_type = placeholder_message,
                      resolvemode resolve_mode = lookup_all);

  // like lookupsymbol() but will not return a placeholder even if
  // allowunknowndependencies() has been used.
  symbol lookupsymbolnoplaceholder(const string& name,
                                   const string& relative_to,
                                   resolvemode resolve_mode = lookup_all);

  // creates a placeholder type suitable for return from lookupsymbol().  may
  // return knullsymbol if the name is not a valid type name.
  symbol newplaceholder(const string& name, placeholdertype placeholder_type);

  // creates a placeholder file.  never returns null.  this is used when an
  // import is not found and allowunknowndependencies() is enabled.
  const filedescriptor* newplaceholderfile(const string& name);

  // calls tables_->addsymbol() and records an error if it fails.  returns
  // true if successful or false if failed, though most callers can ignore
  // the return value since an error has already been recorded.
  bool addsymbol(const string& full_name,
                 const void* parent, const string& name,
                 const message& proto, symbol symbol);

  // like addsymbol(), but succeeds if the symbol is already defined as long
  // as the existing definition is also a package (because it's ok to define
  // the same package in two different files).  also adds all parents of the
  // packgae to the symbol table (e.g. addpackage("foo.bar", ...) will add
  // "foo.bar" and "foo" to the table).
  void addpackage(const string& name, const message& proto,
                  const filedescriptor* file);

  // checks that the symbol name contains only alphanumeric characters and
  // underscores.  records an error otherwise.
  void validatesymbolname(const string& name, const string& full_name,
                          const message& proto);

  // like validatesymbolname(), but the name is allowed to contain periods and
  // an error is indicated by returning false (not recording the error).
  bool validatequalifiedname(const string& name);

  // used by build_array macro (below) to avoid having to have the type
  // specified as a macro parameter.
  template <typename type>
  inline void allocatearray(int size, type** output) {
    *output = tables_->allocatearray<type>(size);
  }

  // allocates a copy of orig_options in tables_ and stores it in the
  // descriptor. remembers its uninterpreted options, to be interpreted
  // later. descriptort must be one of the descriptor messages from
  // descriptor.proto.
  template<class descriptort> void allocateoptions(
      const typename descriptort::optionstype& orig_options,
      descriptort* descriptor);
  // specialization for fileoptions.
  void allocateoptions(const fileoptions& orig_options,
                       filedescriptor* descriptor);

  // implementation for allocateoptions(). don't call this directly.
  template<class descriptort> void allocateoptionsimpl(
      const string& name_scope,
      const string& element_name,
      const typename descriptort::optionstype& orig_options,
      descriptort* descriptor);

  // these methods all have the same signature for the sake of the build_array
  // macro, below.
  void buildmessage(const descriptorproto& proto,
                    const descriptor* parent,
                    descriptor* result);
  void buildfieldorextension(const fielddescriptorproto& proto,
                             const descriptor* parent,
                             fielddescriptor* result,
                             bool is_extension);
  void buildfield(const fielddescriptorproto& proto,
                  const descriptor* parent,
                  fielddescriptor* result) {
    buildfieldorextension(proto, parent, result, false);
  }
  void buildextension(const fielddescriptorproto& proto,
                      const descriptor* parent,
                      fielddescriptor* result) {
    buildfieldorextension(proto, parent, result, true);
  }
  void buildextensionrange(const descriptorproto::extensionrange& proto,
                           const descriptor* parent,
                           descriptor::extensionrange* result);
  void buildenum(const enumdescriptorproto& proto,
                 const descriptor* parent,
                 enumdescriptor* result);
  void buildenumvalue(const enumvaluedescriptorproto& proto,
                      const enumdescriptor* parent,
                      enumvaluedescriptor* result);
  void buildservice(const servicedescriptorproto& proto,
                    const void* dummy,
                    servicedescriptor* result);
  void buildmethod(const methoddescriptorproto& proto,
                   const servicedescriptor* parent,
                   methoddescriptor* result);

  // must be run only after building.
  //
  // note: options will not be available during cross-linking, as they
  // have not yet been interpreted. defer any handling of options to the
  // validate*options methods.
  void crosslinkfile(filedescriptor* file, const filedescriptorproto& proto);
  void crosslinkmessage(descriptor* message, const descriptorproto& proto);
  void crosslinkfield(fielddescriptor* field,
                      const fielddescriptorproto& proto);
  void crosslinkenum(enumdescriptor* enum_type,
                     const enumdescriptorproto& proto);
  void crosslinkenumvalue(enumvaluedescriptor* enum_value,
                          const enumvaluedescriptorproto& proto);
  void crosslinkservice(servicedescriptor* service,
                        const servicedescriptorproto& proto);
  void crosslinkmethod(methoddescriptor* method,
                       const methoddescriptorproto& proto);

  // must be run only after cross-linking.
  void interpretoptions();

  // a helper class for interpreting options.
  class optioninterpreter {
   public:
    // creates an interpreter that operates in the context of the pool of the
    // specified builder, which must not be null. we don't take ownership of the
    // builder.
    explicit optioninterpreter(descriptorbuilder* builder);

    ~optioninterpreter();

    // interprets the uninterpreted options in the specified options message.
    // on error, calls adderror() on the underlying builder and returns false.
    // otherwise returns true.
    bool interpretoptions(optionstointerpret* options_to_interpret);

    class aggregateoptionfinder;

   private:
    // interprets uninterpreted_option_ on the specified message, which
    // must be the mutable copy of the original options message to which
    // uninterpreted_option_ belongs.
    bool interpretsingleoption(message* options);

    // adds the uninterpreted_option to the given options message verbatim.
    // used when allowunknowndependencies() is in effect and we can't find
    // the option's definition.
    void addwithoutinterpreting(const uninterpretedoption& uninterpreted_option,
                                message* options);

    // a recursive helper function that drills into the intermediate fields
    // in unknown_fields to check if field innermost_field is set on the
    // innermost message. returns false and sets an error if so.
    bool examineifoptionisset(
        vector<const fielddescriptor*>::const_iterator intermediate_fields_iter,
        vector<const fielddescriptor*>::const_iterator intermediate_fields_end,
        const fielddescriptor* innermost_field, const string& debug_msg_name,
        const unknownfieldset& unknown_fields);

    // validates the value for the option field of the currently interpreted
    // option and then sets it on the unknown_field.
    bool setoptionvalue(const fielddescriptor* option_field,
                        unknownfieldset* unknown_fields);

    // parses an aggregate value for a cpptype_message option and
    // saves it into *unknown_fields.
    bool setaggregateoption(const fielddescriptor* option_field,
                            unknownfieldset* unknown_fields);

    // convenience functions to set an int field the right way, depending on
    // its wire type (a single int cpptype can represent multiple wire types).
    void setint32(int number, int32 value, fielddescriptor::type type,
                  unknownfieldset* unknown_fields);
    void setint64(int number, int64 value, fielddescriptor::type type,
                  unknownfieldset* unknown_fields);
    void setuint32(int number, uint32 value, fielddescriptor::type type,
                   unknownfieldset* unknown_fields);
    void setuint64(int number, uint64 value, fielddescriptor::type type,
                   unknownfieldset* unknown_fields);

    // a helper function that adds an error at the specified location of the
    // option we're currently interpreting, and returns false.
    bool addoptionerror(descriptorpool::errorcollector::errorlocation location,
                        const string& msg) {
      builder_->adderror(options_to_interpret_->element_name,
                         *uninterpreted_option_, location, msg);
      return false;
    }

    // a helper function that adds an error at the location of the option name
    // and returns false.
    bool addnameerror(const string& msg) {
      return addoptionerror(descriptorpool::errorcollector::option_name, msg);
    }

    // a helper function that adds an error at the location of the option name
    // and returns false.
    bool addvalueerror(const string& msg) {
      return addoptionerror(descriptorpool::errorcollector::option_value, msg);
    }

    // we interpret against this builder's pool. is never null. we don't own
    // this pointer.
    descriptorbuilder* builder_;

    // the options we're currently interpreting, or null if we're not in a call
    // to interpretoptions.
    const optionstointerpret* options_to_interpret_;

    // the option we're currently interpreting within options_to_interpret_, or
    // null if we're not in a call to interpretoptions(). this points to a
    // submessage of the original option, not the mutable copy. therefore we
    // can use it to find locations recorded by the parser.
    const uninterpretedoption* uninterpreted_option_;

    // factory used to create the dynamic messages we need to parse
    // any aggregate option values we encounter.
    dynamicmessagefactory dynamic_factory_;

    google_disallow_evil_constructors(optioninterpreter);
  };

  // work-around for broken compilers:  according to the c++ standard,
  // optioninterpreter should have access to the private members of any class
  // which has declared descriptorbuilder as a friend.  unfortunately some old
  // versions of gcc and other compilers do not implement this correctly.  so,
  // we have to have these intermediate methods to provide access.  we also
  // redundantly declare optioninterpreter a friend just to make things extra
  // clear for these bad compilers.
  friend class optioninterpreter;
  friend class optioninterpreter::aggregateoptionfinder;

  static inline bool get_allow_unknown(const descriptorpool* pool) {
    return pool->allow_unknown_;
  }
  static inline bool get_is_placeholder(const descriptor* descriptor) {
    return descriptor->is_placeholder_;
  }
  static inline void assert_mutex_held(const descriptorpool* pool) {
    if (pool->mutex_ != null) {
      pool->mutex_->assertheld();
    }
  }

  // must be run only after options have been interpreted.
  //
  // note: validation code must only reference the options in the mutable
  // descriptors, which are the ones that have been interpreted. the const
  // proto references are passed in only so they can be provided to calls to
  // adderror(). do not look at their options, which have not been interpreted.
  void validatefileoptions(filedescriptor* file,
                           const filedescriptorproto& proto);
  void validatemessageoptions(descriptor* message,
                              const descriptorproto& proto);
  void validatefieldoptions(fielddescriptor* field,
                            const fielddescriptorproto& proto);
  void validateenumoptions(enumdescriptor* enm,
                           const enumdescriptorproto& proto);
  void validateenumvalueoptions(enumvaluedescriptor* enum_value,
                                const enumvaluedescriptorproto& proto);
  void validateserviceoptions(servicedescriptor* service,
                              const servicedescriptorproto& proto);
  void validatemethodoptions(methoddescriptor* method,
                             const methoddescriptorproto& proto);

  void validatemapkey(fielddescriptor* field,
                      const fielddescriptorproto& proto);

};

const filedescriptor* descriptorpool::buildfile(
    const filedescriptorproto& proto) {
  google_check(fallback_database_ == null)
    << "cannot call buildfile on a descriptorpool that uses a "
       "descriptordatabase.  you must instead find a way to get your file "
       "into the underlying database.";
  google_check(mutex_ == null);   // implied by the above google_check.
  return descriptorbuilder(this, tables_.get(), null).buildfile(proto);
}

const filedescriptor* descriptorpool::buildfilecollectingerrors(
    const filedescriptorproto& proto,
    errorcollector* error_collector) {
  google_check(fallback_database_ == null)
    << "cannot call buildfile on a descriptorpool that uses a "
       "descriptordatabase.  you must instead find a way to get your file "
       "into the underlying database.";
  google_check(mutex_ == null);   // implied by the above google_check.
  return descriptorbuilder(this, tables_.get(),
                           error_collector).buildfile(proto);
}

const filedescriptor* descriptorpool::buildfilefromdatabase(
    const filedescriptorproto& proto) const {
  mutex_->assertheld();
  return descriptorbuilder(this, tables_.get(),
                           default_error_collector_).buildfile(proto);
}

descriptorbuilder::descriptorbuilder(
    const descriptorpool* pool,
    descriptorpool::tables* tables,
    descriptorpool::errorcollector* error_collector)
  : pool_(pool),
    tables_(tables),
    error_collector_(error_collector),
    had_errors_(false),
    possible_undeclared_dependency_(null) {}

descriptorbuilder::~descriptorbuilder() {}

void descriptorbuilder::adderror(
    const string& element_name,
    const message& descriptor,
    descriptorpool::errorcollector::errorlocation location,
    const string& error) {
  if (error_collector_ == null) {
    if (!had_errors_) {
      google_log(error) << "invalid proto descriptor for file \"" << filename_
                 << "\":";
    }
    google_log(error) << "  " << element_name << ": " << error;
  } else {
    error_collector_->adderror(filename_, element_name,
                               &descriptor, location, error);
  }
  had_errors_ = true;
}

void descriptorbuilder::addnotdefinederror(
    const string& element_name,
    const message& descriptor,
    descriptorpool::errorcollector::errorlocation location,
    const string& undefined_symbol) {
  if (possible_undeclared_dependency_ == null) {
    adderror(element_name, descriptor, location,
             "\"" + undefined_symbol + "\" is not defined.");
  } else {
    adderror(element_name, descriptor, location,
             "\"" + possible_undeclared_dependency_name_ +
             "\" seems to be defined in \"" +
             possible_undeclared_dependency_->name() + "\", which is not "
             "imported by \"" + filename_ + "\".  to use it here, please "
             "add the necessary import.");
  }
}

bool descriptorbuilder::isinpackage(const filedescriptor* file,
                                    const string& package_name) {
  return hasprefixstring(file->package(), package_name) &&
           (file->package().size() == package_name.size() ||
            file->package()[package_name.size()] == '.');
}

void descriptorbuilder::recordpublicdependencies(const filedescriptor* file) {
  if (file == null || !dependencies_.insert(file).second) return;
  for (int i = 0; file != null && i < file->public_dependency_count(); i++) {
    recordpublicdependencies(file->public_dependency(i));
  }
}

symbol descriptorbuilder::findsymbolnotenforcingdepshelper(
    const descriptorpool* pool, const string& name) {
  // if we are looking at an underlay, we must lock its mutex_, since we are
  // accessing the underlay's tables_ directly.
  mutexlockmaybe lock((pool == pool_) ? null : pool->mutex_);

  symbol result = pool->tables_->findsymbol(name);
  if (result.isnull() && pool->underlay_ != null) {
    // symbol not found; check the underlay.
    result = findsymbolnotenforcingdepshelper(pool->underlay_, name);
  }

  if (result.isnull()) {
    // in theory, we shouldn't need to check fallback_database_ because the
    // symbol should be in one of its file's direct dependencies, and we have
    // already loaded those by the time we get here.  but we check anyway so
    // that we can generate better error message when dependencies are missing
    // (i.e., "missing dependency" rather than "type is not defined").
    if (pool->tryfindsymbolinfallbackdatabase(name)) {
      result = pool->tables_->findsymbol(name);
    }
  }

  return result;
}

symbol descriptorbuilder::findsymbolnotenforcingdeps(const string& name) {
  return findsymbolnotenforcingdepshelper(pool_, name);
}

symbol descriptorbuilder::findsymbol(const string& name) {
  symbol result = findsymbolnotenforcingdeps(name);

  if (result.isnull()) return result;

  if (!pool_->enforce_dependencies_) {
    // hack for compilerupgrader.
    return result;
  }

  // only find symbols which were defined in this file or one of its
  // dependencies.
  const filedescriptor* file = result.getfile();
  if (file == file_ || dependencies_.count(file) > 0) return result;

  if (result.type == symbol::package) {
    // arg, this is overcomplicated.  the symbol is a package name.  it could
    // be that the package was defined in multiple files.  result.getfile()
    // returns the first file we saw that used this package.  we've determined
    // that that file is not a direct dependency of the file we are currently
    // building, but it could be that some other file which *is* a direct
    // dependency also defines the same package.  we can't really rule out this
    // symbol unless none of the dependencies define it.
    if (isinpackage(file_, name)) return result;
    for (set<const filedescriptor*>::const_iterator it = dependencies_.begin();
         it != dependencies_.end(); ++it) {
      // note:  a dependency may be null if it was not found or had errors.
      if (*it != null && isinpackage(*it, name)) return result;
    }
  }

  possible_undeclared_dependency_ = file;
  possible_undeclared_dependency_name_ = name;
  return knullsymbol;
}

symbol descriptorbuilder::lookupsymbolnoplaceholder(
    const string& name, const string& relative_to, resolvemode resolve_mode) {
  possible_undeclared_dependency_ = null;

  if (name.size() > 0 && name[0] == '.') {
    // fully-qualified name.
    return findsymbol(name.substr(1));
  }

  // if name is something like "foo.bar.baz", and symbols named "foo" are
  // defined in multiple parent scopes, we only want to find "bar.baz" in the
  // innermost one.  e.g., the following should produce an error:
  //   message bar { message baz {} }
  //   message foo {
  //     message bar {
  //     }
  //     optional bar.baz baz = 1;
  //   }
  // so, we look for just "foo" first, then look for "bar.baz" within it if
  // found.
  string::size_type name_dot_pos = name.find_first_of('.');
  string first_part_of_name;
  if (name_dot_pos == string::npos) {
    first_part_of_name = name;
  } else {
    first_part_of_name = name.substr(0, name_dot_pos);
  }

  string scope_to_try(relative_to);

  while (true) {
    // chop off the last component of the scope.
    string::size_type dot_pos = scope_to_try.find_last_of('.');
    if (dot_pos == string::npos) {
      return findsymbol(name);
    } else {
      scope_to_try.erase(dot_pos);
    }

    // append ".first_part_of_name" and try to find.
    string::size_type old_size = scope_to_try.size();
    scope_to_try.append(1, '.');
    scope_to_try.append(first_part_of_name);
    symbol result = findsymbol(scope_to_try);
    if (!result.isnull()) {
      if (first_part_of_name.size() < name.size()) {
        // name is a compound symbol, of which we only found the first part.
        // now try to look up the rest of it.
        if (result.isaggregate()) {
          scope_to_try.append(name, first_part_of_name.size(),
                              name.size() - first_part_of_name.size());
          return findsymbol(scope_to_try);
        } else {
          // we found a symbol but it's not an aggregate.  continue the loop.
        }
      } else {
        if (resolve_mode == lookup_types && !result.istype()) {
          // we found a symbol but it's not a type.  continue the loop.
        } else {
          return result;
        }
      }
    }

    // not found.  remove the name so we can try again.
    scope_to_try.erase(old_size);
  }
}

symbol descriptorbuilder::lookupsymbol(
    const string& name, const string& relative_to,
    placeholdertype placeholder_type, resolvemode resolve_mode) {
  symbol result = lookupsymbolnoplaceholder(
      name, relative_to, resolve_mode);
  if (result.isnull() && pool_->allow_unknown_) {
    // not found, but allowunknowndependencies() is enabled.  return a
    // placeholder instead.
    result = newplaceholder(name, placeholder_type);
  }
  return result;
}

symbol descriptorbuilder::newplaceholder(const string& name,
                                         placeholdertype placeholder_type) {
  // compute names.
  const string* placeholder_full_name;
  const string* placeholder_name;
  const string* placeholder_package;

  if (!validatequalifiedname(name)) return knullsymbol;
  if (name[0] == '.') {
    // fully-qualified.
    placeholder_full_name = tables_->allocatestring(name.substr(1));
  } else {
    placeholder_full_name = tables_->allocatestring(name);
  }

  string::size_type dotpos = placeholder_full_name->find_last_of('.');
  if (dotpos != string::npos) {
    placeholder_package = tables_->allocatestring(
      placeholder_full_name->substr(0, dotpos));
    placeholder_name = tables_->allocatestring(
      placeholder_full_name->substr(dotpos + 1));
  } else {
    placeholder_package = &kemptystring;
    placeholder_name = placeholder_full_name;
  }

  // create the placeholders.
  filedescriptor* placeholder_file = tables_->allocate<filedescriptor>();
  memset(placeholder_file, 0, sizeof(*placeholder_file));

  placeholder_file->source_code_info_ = &sourcecodeinfo::default_instance();

  placeholder_file->name_ =
    tables_->allocatestring(*placeholder_full_name + ".placeholder.proto");
  placeholder_file->package_ = placeholder_package;
  placeholder_file->pool_ = pool_;
  placeholder_file->options_ = &fileoptions::default_instance();
  placeholder_file->tables_ = &filedescriptortables::kempty;
  // all other fields are zero or null.

  if (placeholder_type == placeholder_enum) {
    placeholder_file->enum_type_count_ = 1;
    placeholder_file->enum_types_ =
      tables_->allocatearray<enumdescriptor>(1);

    enumdescriptor* placeholder_enum = &placeholder_file->enum_types_[0];
    memset(placeholder_enum, 0, sizeof(*placeholder_enum));

    placeholder_enum->full_name_ = placeholder_full_name;
    placeholder_enum->name_ = placeholder_name;
    placeholder_enum->file_ = placeholder_file;
    placeholder_enum->options_ = &enumoptions::default_instance();
    placeholder_enum->is_placeholder_ = true;
    placeholder_enum->is_unqualified_placeholder_ = (name[0] != '.');

    // enums must have at least one value.
    placeholder_enum->value_count_ = 1;
    placeholder_enum->values_ = tables_->allocatearray<enumvaluedescriptor>(1);

    enumvaluedescriptor* placeholder_value = &placeholder_enum->values_[0];
    memset(placeholder_value, 0, sizeof(*placeholder_value));

    placeholder_value->name_ = tables_->allocatestring("placeholder_value");
    // note that enum value names are siblings of their type, not children.
    placeholder_value->full_name_ =
      placeholder_package->empty() ? placeholder_value->name_ :
        tables_->allocatestring(*placeholder_package + ".placeholder_value");

    placeholder_value->number_ = 0;
    placeholder_value->type_ = placeholder_enum;
    placeholder_value->options_ = &enumvalueoptions::default_instance();

    return symbol(placeholder_enum);
  } else {
    placeholder_file->message_type_count_ = 1;
    placeholder_file->message_types_ =
      tables_->allocatearray<descriptor>(1);

    descriptor* placeholder_message = &placeholder_file->message_types_[0];
    memset(placeholder_message, 0, sizeof(*placeholder_message));

    placeholder_message->full_name_ = placeholder_full_name;
    placeholder_message->name_ = placeholder_name;
    placeholder_message->file_ = placeholder_file;
    placeholder_message->options_ = &messageoptions::default_instance();
    placeholder_message->is_placeholder_ = true;
    placeholder_message->is_unqualified_placeholder_ = (name[0] != '.');

    if (placeholder_type == placeholder_extendable_message) {
      placeholder_message->extension_range_count_ = 1;
      placeholder_message->extension_ranges_ =
        tables_->allocatearray<descriptor::extensionrange>(1);
      placeholder_message->extension_ranges_->start = 1;
      // kmaxnumber + 1 because extensionrange::end is exclusive.
      placeholder_message->extension_ranges_->end =
        fielddescriptor::kmaxnumber + 1;
    }

    return symbol(placeholder_message);
  }
}

const filedescriptor* descriptorbuilder::newplaceholderfile(
    const string& name) {
  filedescriptor* placeholder = tables_->allocate<filedescriptor>();
  memset(placeholder, 0, sizeof(*placeholder));

  placeholder->name_ = tables_->allocatestring(name);
  placeholder->package_ = &kemptystring;
  placeholder->pool_ = pool_;
  placeholder->options_ = &fileoptions::default_instance();
  placeholder->tables_ = &filedescriptortables::kempty;
  // all other fields are zero or null.

  return placeholder;
}

bool descriptorbuilder::addsymbol(
    const string& full_name, const void* parent, const string& name,
    const message& proto, symbol symbol) {
  // if the caller passed null for the parent, the symbol is at file scope.
  // use its file as the parent instead.
  if (parent == null) parent = file_;

  if (tables_->addsymbol(full_name, symbol)) {
    if (!file_tables_->addaliasunderparent(parent, name, symbol)) {
      google_log(dfatal) << "\"" << full_name << "\" not previously defined in "
                     "symbols_by_name_, but was defined in symbols_by_parent_; "
                     "this shouldn't be possible.";
      return false;
    }
    return true;
  } else {
    const filedescriptor* other_file = tables_->findsymbol(full_name).getfile();
    if (other_file == file_) {
      string::size_type dot_pos = full_name.find_last_of('.');
      if (dot_pos == string::npos) {
        adderror(full_name, proto, descriptorpool::errorcollector::name,
                 "\"" + full_name + "\" is already defined.");
      } else {
        adderror(full_name, proto, descriptorpool::errorcollector::name,
                 "\"" + full_name.substr(dot_pos + 1) +
                 "\" is already defined in \"" +
                 full_name.substr(0, dot_pos) + "\".");
      }
    } else {
      // symbol seems to have been defined in a different file.
      adderror(full_name, proto, descriptorpool::errorcollector::name,
               "\"" + full_name + "\" is already defined in file \"" +
               other_file->name() + "\".");
    }
    return false;
  }
}

void descriptorbuilder::addpackage(
    const string& name, const message& proto, const filedescriptor* file) {
  if (tables_->addsymbol(name, symbol(file))) {
    // success.  also add parent package, if any.
    string::size_type dot_pos = name.find_last_of('.');
    if (dot_pos == string::npos) {
      // no parents.
      validatesymbolname(name, name, proto);
    } else {
      // has parent.
      string* parent_name = tables_->allocatestring(name.substr(0, dot_pos));
      addpackage(*parent_name, proto, file);
      validatesymbolname(name.substr(dot_pos + 1), name, proto);
    }
  } else {
    symbol existing_symbol = tables_->findsymbol(name);
    // it's ok to redefine a package.
    if (existing_symbol.type != symbol::package) {
      // symbol seems to have been defined in a different file.
      adderror(name, proto, descriptorpool::errorcollector::name,
               "\"" + name + "\" is already defined (as something other than "
               "a package) in file \"" + existing_symbol.getfile()->name() +
               "\".");
    }
  }
}

void descriptorbuilder::validatesymbolname(
    const string& name, const string& full_name, const message& proto) {
  if (name.empty()) {
    adderror(full_name, proto, descriptorpool::errorcollector::name,
             "missing name.");
  } else {
    for (int i = 0; i < name.size(); i++) {
      // i don't trust isalnum() due to locales.  :(
      if ((name[i] < 'a' || 'z' < name[i]) &&
          (name[i] < 'a' || 'z' < name[i]) &&
          (name[i] < '0' || '9' < name[i]) &&
          (name[i] != '_')) {
        adderror(full_name, proto, descriptorpool::errorcollector::name,
                 "\"" + name + "\" is not a valid identifier.");
      }
    }
  }
}

bool descriptorbuilder::validatequalifiedname(const string& name) {
  bool last_was_period = false;

  for (int i = 0; i < name.size(); i++) {
    // i don't trust isalnum() due to locales.  :(
    if (('a' <= name[i] && name[i] <= 'z') ||
        ('a' <= name[i] && name[i] <= 'z') ||
        ('0' <= name[i] && name[i] <= '9') ||
        (name[i] == '_')) {
      last_was_period = false;
    } else if (name[i] == '.') {
      if (last_was_period) return false;
      last_was_period = true;
    } else {
      return false;
    }
  }

  return !name.empty() && !last_was_period;
}

// -------------------------------------------------------------------

// this generic implementation is good for all descriptors except
// filedescriptor.
template<class descriptort> void descriptorbuilder::allocateoptions(
    const typename descriptort::optionstype& orig_options,
    descriptort* descriptor) {
  allocateoptionsimpl(descriptor->full_name(), descriptor->full_name(),
                      orig_options, descriptor);
}

// we specialize for filedescriptor.
void descriptorbuilder::allocateoptions(const fileoptions& orig_options,
                                        filedescriptor* descriptor) {
  // we add the dummy token so that lookupsymbol does the right thing.
  allocateoptionsimpl(descriptor->package() + ".dummy", descriptor->name(),
                      orig_options, descriptor);
}

template<class descriptort> void descriptorbuilder::allocateoptionsimpl(
    const string& name_scope,
    const string& element_name,
    const typename descriptort::optionstype& orig_options,
    descriptort* descriptor) {
  // we need to use a dummy pointer to work around a bug in older versions of
  // gcc.  otherwise, the following two lines could be replaced with:
  //   typename descriptort::optionstype* options =
  //       tables_->allocatemessage<typename descriptort::optionstype>();
  typename descriptort::optionstype* const dummy = null;
  typename descriptort::optionstype* options = tables_->allocatemessage(dummy);
  // avoid using mergefrom()/copyfrom() in this class to make it -fno-rtti
  // friendly. without rtti, mergefrom() and copyfrom() will fallback to the
  // reflection based method, which requires the descriptor. however, we are in
  // the middle of building the descriptors, thus the deadlock.
  options->parsefromstring(orig_options.serializeasstring());
  descriptor->options_ = options;

  // don't add to options_to_interpret_ unless there were uninterpreted
  // options.  this not only avoids unnecessary work, but prevents a
  // bootstrapping problem when building descriptors for descriptor.proto.
  // descriptor.proto does not contain any uninterpreted options, but
  // attempting to interpret options anyway will cause
  // optionstype::getdescriptor() to be called which may then deadlock since
  // we're still trying to build it.
  if (options->uninterpreted_option_size() > 0) {
    options_to_interpret_.push_back(
        optionstointerpret(name_scope, element_name, &orig_options, options));
  }
}


// a common pattern:  we want to convert a repeated field in the descriptor
// to an array of values, calling some method to build each value.
#define build_array(input, output, name, method, parent)             \
  output->name##_count_ = input.name##_size();                       \
  allocatearray(input.name##_size(), &output->name##s_);             \
  for (int i = 0; i < input.name##_size(); i++) {                    \
    method(input.name(i), parent, output->name##s_ + i);             \
  }

const filedescriptor* descriptorbuilder::buildfile(
    const filedescriptorproto& proto) {
  filename_ = proto.name();

  // check if the file already exists and is identical to the one being built.
  // note:  this only works if the input is canonical -- that is, it
  //   fully-qualifies all type names, has no uninterpretedoptions, etc.
  //   this is fine, because this idempotency "feature" really only exists to
  //   accomodate one hack in the proto1->proto2 migration layer.
  const filedescriptor* existing_file = tables_->findfile(filename_);
  if (existing_file != null) {
    // file already in pool.  compare the existing one to the input.
    filedescriptorproto existing_proto;
    existing_file->copyto(&existing_proto);
    if (existing_proto.serializeasstring() == proto.serializeasstring()) {
      // they're identical.  return the existing descriptor.
      return existing_file;
    }

    // not a match.  the error will be detected and handled later.
  }

  // check to see if this file is already on the pending files list.
  // todo(kenton):  allow recursive imports?  it may not work with some
  //   (most?) programming languages.  e.g., in c++, a forward declaration
  //   of a type is not sufficient to allow it to be used even in a
  //   generated header file due to inlining.  this could perhaps be
  //   worked around using tricks involving inserting #include statements
  //   mid-file, but that's pretty ugly, and i'm pretty sure there are
  //   some languages out there that do not allow recursive dependencies
  //   at all.
  for (int i = 0; i < tables_->pending_files_.size(); i++) {
    if (tables_->pending_files_[i] == proto.name()) {
      string error_message("file recursively imports itself: ");
      for (; i < tables_->pending_files_.size(); i++) {
        error_message.append(tables_->pending_files_[i]);
        error_message.append(" -> ");
      }
      error_message.append(proto.name());

      adderror(proto.name(), proto, descriptorpool::errorcollector::other,
               error_message);
      return null;
    }
  }

  // if we have a fallback_database_, attempt to load all dependencies now,
  // before checkpointing tables_.  this avoids confusion with recursive
  // checkpoints.
  if (pool_->fallback_database_ != null) {
    tables_->pending_files_.push_back(proto.name());
    for (int i = 0; i < proto.dependency_size(); i++) {
      if (tables_->findfile(proto.dependency(i)) == null &&
          (pool_->underlay_ == null ||
           pool_->underlay_->findfilebyname(proto.dependency(i)) == null)) {
        // we don't care what this returns since we'll find out below anyway.
        pool_->tryfindfileinfallbackdatabase(proto.dependency(i));
      }
    }
    tables_->pending_files_.pop_back();
  }

  // checkpoint the tables so that we can roll back if something goes wrong.
  tables_->addcheckpoint();

  filedescriptor* result = tables_->allocate<filedescriptor>();
  file_ = result;

  if (proto.has_source_code_info()) {
    sourcecodeinfo *info = tables_->allocatemessage<sourcecodeinfo>();
    info->copyfrom(proto.source_code_info());
    result->source_code_info_ = info;
  } else {
    result->source_code_info_ = &sourcecodeinfo::default_instance();
  }

  file_tables_ = tables_->allocatefiletables();
  file_->tables_ = file_tables_;

  if (!proto.has_name()) {
    adderror("", proto, descriptorpool::errorcollector::other,
             "missing field: filedescriptorproto.name.");
  }

  result->name_ = tables_->allocatestring(proto.name());
  if (proto.has_package()) {
    result->package_ = tables_->allocatestring(proto.package());
  } else {
    // we cannot rely on proto.package() returning a valid string if
    // proto.has_package() is false, because we might be running at static
    // initialization time, in which case default values have not yet been
    // initialized.
    result->package_ = tables_->allocatestring("");
  }
  result->pool_ = pool_;

  // add to tables.
  if (!tables_->addfile(result)) {
    adderror(proto.name(), proto, descriptorpool::errorcollector::other,
             "a file with this name is already in the pool.");
    // bail out early so that if this is actually the exact same file, we
    // don't end up reporting that every single symbol is already defined.
    tables_->rollbacktolastcheckpoint();
    return null;
  }
  if (!result->package().empty()) {
    addpackage(result->package(), proto, result);
  }

  // make sure all dependencies are loaded.
  set<string> seen_dependencies;
  result->dependency_count_ = proto.dependency_size();
  result->dependencies_ =
    tables_->allocatearray<const filedescriptor*>(proto.dependency_size());
  for (int i = 0; i < proto.dependency_size(); i++) {
    if (!seen_dependencies.insert(proto.dependency(i)).second) {
      adderror(proto.name(), proto,
               descriptorpool::errorcollector::other,
               "import \"" + proto.dependency(i) + "\" was listed twice.");
    }

    const filedescriptor* dependency = tables_->findfile(proto.dependency(i));
    if (dependency == null && pool_->underlay_ != null) {
      dependency = pool_->underlay_->findfilebyname(proto.dependency(i));
    }

    if (dependency == null) {
      if (pool_->allow_unknown_) {
        dependency = newplaceholderfile(proto.dependency(i));
      } else {
        string message;
        if (pool_->fallback_database_ == null) {
          message = "import \"" + proto.dependency(i) +
                    "\" has not been loaded.";
        } else {
          message = "import \"" + proto.dependency(i) +
                    "\" was not found or had errors.";
        }
        adderror(proto.name(), proto,
                 descriptorpool::errorcollector::other,
                 message);
      }
    }

    result->dependencies_[i] = dependency;
  }

  // check public dependencies.
  int public_dependency_count = 0;
  result->public_dependencies_ = tables_->allocatearray<int>(
      proto.public_dependency_size());
  for (int i = 0; i < proto.public_dependency_size(); i++) {
    // only put valid public dependency indexes.
    int index = proto.public_dependency(i);
    if (index >= 0 && index < proto.dependency_size()) {
      result->public_dependencies_[public_dependency_count++] = index;
    } else {
      adderror(proto.name(), proto,
               descriptorpool::errorcollector::other,
               "invalid public dependency index.");
    }
  }
  result->public_dependency_count_ = public_dependency_count;

  // build dependency set
  dependencies_.clear();
  for (int i = 0; i < result->dependency_count(); i++) {
    recordpublicdependencies(result->dependency(i));
  }

  // check weak dependencies.
  int weak_dependency_count = 0;
  result->weak_dependencies_ = tables_->allocatearray<int>(
      proto.weak_dependency_size());
  for (int i = 0; i < proto.weak_dependency_size(); i++) {
    int index = proto.weak_dependency(i);
    if (index >= 0 && index < proto.dependency_size()) {
      result->weak_dependencies_[weak_dependency_count++] = index;
    } else {
      adderror(proto.name(), proto,
               descriptorpool::errorcollector::other,
               "invalid weak dependency index.");
    }
  }
  result->weak_dependency_count_ = weak_dependency_count;

  // convert children.
  build_array(proto, result, message_type, buildmessage  , null);
  build_array(proto, result, enum_type   , buildenum     , null);
  build_array(proto, result, service     , buildservice  , null);
  build_array(proto, result, extension   , buildextension, null);

  // copy options.
  if (!proto.has_options()) {
    result->options_ = null;  // will set to default_instance later.
  } else {
    allocateoptions(proto.options(), result);
  }

  // note that the following steps must occur in exactly the specified order.

  // cross-link.
  crosslinkfile(result, proto);

  // interpret any remaining uninterpreted options gathered into
  // options_to_interpret_ during descriptor building.  cross-linking has made
  // extension options known, so all interpretations should now succeed.
  if (!had_errors_) {
    optioninterpreter option_interpreter(this);
    for (vector<optionstointerpret>::iterator iter =
             options_to_interpret_.begin();
         iter != options_to_interpret_.end(); ++iter) {
      option_interpreter.interpretoptions(&(*iter));
    }
    options_to_interpret_.clear();
  }

  // validate options.
  if (!had_errors_) {
    validatefileoptions(result, proto);
  }

  if (had_errors_) {
    tables_->rollbacktolastcheckpoint();
    return null;
  } else {
    tables_->clearlastcheckpoint();
    return result;
  }
}

void descriptorbuilder::buildmessage(const descriptorproto& proto,
                                     const descriptor* parent,
                                     descriptor* result) {
  const string& scope = (parent == null) ?
    file_->package() : parent->full_name();
  string* full_name = tables_->allocatestring(scope);
  if (!full_name->empty()) full_name->append(1, '.');
  full_name->append(proto.name());

  validatesymbolname(proto.name(), *full_name, proto);

  result->name_            = tables_->allocatestring(proto.name());
  result->full_name_       = full_name;
  result->file_            = file_;
  result->containing_type_ = parent;
  result->is_placeholder_  = false;
  result->is_unqualified_placeholder_ = false;

  build_array(proto, result, field          , buildfield         , result);
  build_array(proto, result, nested_type    , buildmessage       , result);
  build_array(proto, result, enum_type      , buildenum          , result);
  build_array(proto, result, extension_range, buildextensionrange, result);
  build_array(proto, result, extension      , buildextension     , result);

  // copy options.
  if (!proto.has_options()) {
    result->options_ = null;  // will set to default_instance later.
  } else {
    allocateoptions(proto.options(), result);
  }

  addsymbol(result->full_name(), parent, result->name(),
            proto, symbol(result));

  // check that no fields have numbers in extension ranges.
  for (int i = 0; i < result->field_count(); i++) {
    const fielddescriptor* field = result->field(i);
    for (int j = 0; j < result->extension_range_count(); j++) {
      const descriptor::extensionrange* range = result->extension_range(j);
      if (range->start <= field->number() && field->number() < range->end) {
        adderror(field->full_name(), proto.extension_range(j),
                 descriptorpool::errorcollector::number,
                 strings::substitute(
                   "extension range $0 to $1 includes field \"$2\" ($3).",
                   range->start, range->end - 1,
                   field->name(), field->number()));
      }
    }
  }

  // check that extension ranges don't overlap.
  for (int i = 0; i < result->extension_range_count(); i++) {
    const descriptor::extensionrange* range1 = result->extension_range(i);
    for (int j = i + 1; j < result->extension_range_count(); j++) {
      const descriptor::extensionrange* range2 = result->extension_range(j);
      if (range1->end > range2->start && range2->end > range1->start) {
        adderror(result->full_name(), proto.extension_range(j),
                 descriptorpool::errorcollector::number,
                 strings::substitute("extension range $0 to $1 overlaps with "
                                     "already-defined range $2 to $3.",
                                     range2->start, range2->end - 1,
                                     range1->start, range1->end - 1));
      }
    }
  }
}

void descriptorbuilder::buildfieldorextension(const fielddescriptorproto& proto,
                                              const descriptor* parent,
                                              fielddescriptor* result,
                                              bool is_extension) {
  const string& scope = (parent == null) ?
    file_->package() : parent->full_name();
  string* full_name = tables_->allocatestring(scope);
  if (!full_name->empty()) full_name->append(1, '.');
  full_name->append(proto.name());

  validatesymbolname(proto.name(), *full_name, proto);

  result->name_         = tables_->allocatestring(proto.name());
  result->full_name_    = full_name;
  result->file_         = file_;
  result->number_       = proto.number();
  result->is_extension_ = is_extension;

  // if .proto files follow the style guide then the name should already be
  // lower-cased.  if that's the case we can just reuse the string we already
  // allocated rather than allocate a new one.
  string lowercase_name(proto.name());
  lowerstring(&lowercase_name);
  if (lowercase_name == proto.name()) {
    result->lowercase_name_ = result->name_;
  } else {
    result->lowercase_name_ = tables_->allocatestring(lowercase_name);
  }

  // don't bother with the above optimization for camel-case names since
  // .proto files that follow the guide shouldn't be using names in this
  // format, so the optimization wouldn't help much.
  result->camelcase_name_ = tables_->allocatestring(tocamelcase(proto.name()));

  // some compilers do not allow static_cast directly between two enum types,
  // so we must cast to int first.
  result->type_  = static_cast<fielddescriptor::type>(
                     implicit_cast<int>(proto.type()));
  result->label_ = static_cast<fielddescriptor::label>(
                     implicit_cast<int>(proto.label()));

  // some of these may be filled in when cross-linking.
  result->containing_type_ = null;
  result->extension_scope_ = null;
  result->experimental_map_key_ = null;
  result->message_type_ = null;
  result->enum_type_ = null;

  result->has_default_value_ = proto.has_default_value();
  if (proto.has_default_value() && result->is_repeated()) {
    adderror(result->full_name(), proto,
             descriptorpool::errorcollector::default_value,
             "repeated fields can't have default values.");
  }

  if (proto.has_type()) {
    if (proto.has_default_value()) {
      char* end_pos = null;
      switch (result->cpp_type()) {
        case fielddescriptor::cpptype_int32:
          result->default_value_int32_ =
            strtol(proto.default_value().c_str(), &end_pos, 0);
          break;
        case fielddescriptor::cpptype_int64:
          result->default_value_int64_ =
            strto64(proto.default_value().c_str(), &end_pos, 0);
          break;
        case fielddescriptor::cpptype_uint32:
          result->default_value_uint32_ =
            strtoul(proto.default_value().c_str(), &end_pos, 0);
          break;
        case fielddescriptor::cpptype_uint64:
          result->default_value_uint64_ =
            strtou64(proto.default_value().c_str(), &end_pos, 0);
          break;
        case fielddescriptor::cpptype_float:
          if (proto.default_value() == "inf") {
            result->default_value_float_ = numeric_limits<float>::infinity();
          } else if (proto.default_value() == "-inf") {
            result->default_value_float_ = -numeric_limits<float>::infinity();
          } else if (proto.default_value() == "nan") {
            result->default_value_float_ = numeric_limits<float>::quiet_nan();
          } else  {
            result->default_value_float_ =
              nolocalestrtod(proto.default_value().c_str(), &end_pos);
          }
          break;
        case fielddescriptor::cpptype_double:
          if (proto.default_value() == "inf") {
            result->default_value_double_ = numeric_limits<double>::infinity();
          } else if (proto.default_value() == "-inf") {
            result->default_value_double_ = -numeric_limits<double>::infinity();
          } else if (proto.default_value() == "nan") {
            result->default_value_double_ = numeric_limits<double>::quiet_nan();
          } else  {
            result->default_value_double_ =
              nolocalestrtod(proto.default_value().c_str(), &end_pos);
          }
          break;
        case fielddescriptor::cpptype_bool:
          if (proto.default_value() == "true") {
            result->default_value_bool_ = true;
          } else if (proto.default_value() == "false") {
            result->default_value_bool_ = false;
          } else {
            adderror(result->full_name(), proto,
                     descriptorpool::errorcollector::default_value,
                     "boolean default must be true or false.");
          }
          break;
        case fielddescriptor::cpptype_enum:
          // this will be filled in when cross-linking.
          result->default_value_enum_ = null;
          break;
        case fielddescriptor::cpptype_string:
          if (result->type() == fielddescriptor::type_bytes) {
            result->default_value_string_ = tables_->allocatestring(
              unescapecescapestring(proto.default_value()));
          } else {
            result->default_value_string_ =
                tables_->allocatestring(proto.default_value());
          }
          break;
        case fielddescriptor::cpptype_message:
          adderror(result->full_name(), proto,
                   descriptorpool::errorcollector::default_value,
                   "messages can't have default values.");
          result->has_default_value_ = false;
          break;
      }

      if (end_pos != null) {
        // end_pos is only set non-null by the parsers for numeric types, above.
        // this checks that the default was non-empty and had no extra junk
        // after the end of the number.
        if (proto.default_value().empty() || *end_pos != '\0') {
          adderror(result->full_name(), proto,
                   descriptorpool::errorcollector::default_value,
                   "couldn't parse default value.");
        }
      }
    } else {
      // no explicit default value
      switch (result->cpp_type()) {
        case fielddescriptor::cpptype_int32:
          result->default_value_int32_ = 0;
          break;
        case fielddescriptor::cpptype_int64:
          result->default_value_int64_ = 0;
          break;
        case fielddescriptor::cpptype_uint32:
          result->default_value_uint32_ = 0;
          break;
        case fielddescriptor::cpptype_uint64:
          result->default_value_uint64_ = 0;
          break;
        case fielddescriptor::cpptype_float:
          result->default_value_float_ = 0.0f;
          break;
        case fielddescriptor::cpptype_double:
          result->default_value_double_ = 0.0;
          break;
        case fielddescriptor::cpptype_bool:
          result->default_value_bool_ = false;
          break;
        case fielddescriptor::cpptype_enum:
          // this will be filled in when cross-linking.
          result->default_value_enum_ = null;
          break;
        case fielddescriptor::cpptype_string:
          result->default_value_string_ = &kemptystring;
          break;
        case fielddescriptor::cpptype_message:
          break;
      }
    }
  }

  if (result->number() <= 0) {
    adderror(result->full_name(), proto, descriptorpool::errorcollector::number,
             "field numbers must be positive integers.");
  } else if (!is_extension && result->number() > fielddescriptor::kmaxnumber) {
    // only validate that the number is within the valid field range if it is
    // not an extension. since extension numbers are validated with the
    // extendee's valid set of extension numbers, and those are in turn
    // validated against the max allowed number, the check is unnecessary for
    // extension fields.
    // this avoids cross-linking issues that arise when attempting to check if
    // the extendee is a message_set_wire_format message, which has a higher max
    // on extension numbers.
    adderror(result->full_name(), proto, descriptorpool::errorcollector::number,
             strings::substitute("field numbers cannot be greater than $0.",
                                 fielddescriptor::kmaxnumber));
  } else if (result->number() >= fielddescriptor::kfirstreservednumber &&
             result->number() <= fielddescriptor::klastreservednumber) {
    adderror(result->full_name(), proto, descriptorpool::errorcollector::number,
             strings::substitute(
               "field numbers $0 through $1 are reserved for the protocol "
               "buffer library implementation.",
               fielddescriptor::kfirstreservednumber,
               fielddescriptor::klastreservednumber));
  }

  if (is_extension) {
    if (!proto.has_extendee()) {
      adderror(result->full_name(), proto,
               descriptorpool::errorcollector::extendee,
               "fielddescriptorproto.extendee not set for extension field.");
    }

    result->extension_scope_ = parent;
  } else {
    if (proto.has_extendee()) {
      adderror(result->full_name(), proto,
               descriptorpool::errorcollector::extendee,
               "fielddescriptorproto.extendee set for non-extension field.");
    }

    result->containing_type_ = parent;
  }

  // copy options.
  if (!proto.has_options()) {
    result->options_ = null;  // will set to default_instance later.
  } else {
    allocateoptions(proto.options(), result);
  }

  addsymbol(result->full_name(), parent, result->name(),
            proto, symbol(result));
}

void descriptorbuilder::buildextensionrange(
    const descriptorproto::extensionrange& proto,
    const descriptor* parent,
    descriptor::extensionrange* result) {
  result->start = proto.start();
  result->end = proto.end();
  if (result->start <= 0) {
    adderror(parent->full_name(), proto,
             descriptorpool::errorcollector::number,
             "extension numbers must be positive integers.");
  }

  // checking of the upper bound of the extension range is deferred until after
  // options interpreting. this allows messages with message_set_wire_format to
  // have extensions beyond fielddescriptor::kmaxnumber, since the extension
  // numbers are actually used as int32s in the message_set_wire_format.

  if (result->start >= result->end) {
    adderror(parent->full_name(), proto,
             descriptorpool::errorcollector::number,
             "extension range end number must be greater than start number.");
  }
}

void descriptorbuilder::buildenum(const enumdescriptorproto& proto,
                                  const descriptor* parent,
                                  enumdescriptor* result) {
  const string& scope = (parent == null) ?
    file_->package() : parent->full_name();
  string* full_name = tables_->allocatestring(scope);
  if (!full_name->empty()) full_name->append(1, '.');
  full_name->append(proto.name());

  validatesymbolname(proto.name(), *full_name, proto);

  result->name_            = tables_->allocatestring(proto.name());
  result->full_name_       = full_name;
  result->file_            = file_;
  result->containing_type_ = parent;
  result->is_placeholder_  = false;
  result->is_unqualified_placeholder_ = false;

  if (proto.value_size() == 0) {
    // we cannot allow enums with no values because this would mean there
    // would be no valid default value for fields of this type.
    adderror(result->full_name(), proto,
             descriptorpool::errorcollector::name,
             "enums must contain at least one value.");
  }

  build_array(proto, result, value, buildenumvalue, result);

  // copy options.
  if (!proto.has_options()) {
    result->options_ = null;  // will set to default_instance later.
  } else {
    allocateoptions(proto.options(), result);
  }

  addsymbol(result->full_name(), parent, result->name(),
            proto, symbol(result));
}

void descriptorbuilder::buildenumvalue(const enumvaluedescriptorproto& proto,
                                       const enumdescriptor* parent,
                                       enumvaluedescriptor* result) {
  result->name_   = tables_->allocatestring(proto.name());
  result->number_ = proto.number();
  result->type_   = parent;

  // note:  full_name for enum values is a sibling to the parent's name, not a
  //   child of it.
  string* full_name = tables_->allocatestring(*parent->full_name_);
  full_name->resize(full_name->size() - parent->name_->size());
  full_name->append(*result->name_);
  result->full_name_ = full_name;

  validatesymbolname(proto.name(), *full_name, proto);

  // copy options.
  if (!proto.has_options()) {
    result->options_ = null;  // will set to default_instance later.
  } else {
    allocateoptions(proto.options(), result);
  }

  // again, enum values are weird because we makes them appear as siblings
  // of the enum type instead of children of it.  so, we use
  // parent->containing_type() as the value's parent.
  bool added_to_outer_scope =
    addsymbol(result->full_name(), parent->containing_type(), result->name(),
              proto, symbol(result));

  // however, we also want to be able to search for values within a single
  // enum type, so we add it as a child of the enum type itself, too.
  // note:  this could fail, but if it does, the error has already been
  //   reported by the above addsymbol() call, so we ignore the return code.
  bool added_to_inner_scope =
    file_tables_->addaliasunderparent(parent, result->name(), symbol(result));

  if (added_to_inner_scope && !added_to_outer_scope) {
    // this value did not conflict with any values defined in the same enum,
    // but it did conflict with some other symbol defined in the enum type's
    // scope.  let's print an additional error to explain this.
    string outer_scope;
    if (parent->containing_type() == null) {
      outer_scope = file_->package();
    } else {
      outer_scope = parent->containing_type()->full_name();
    }

    if (outer_scope.empty()) {
      outer_scope = "the global scope";
    } else {
      outer_scope = "\"" + outer_scope + "\"";
    }

    adderror(result->full_name(), proto,
             descriptorpool::errorcollector::name,
             "note that enum values use c++ scoping rules, meaning that "
             "enum values are siblings of their type, not children of it.  "
             "therefore, \"" + result->name() + "\" must be unique within "
             + outer_scope + ", not just within \"" + parent->name() + "\".");
  }

  // an enum is allowed to define two numbers that refer to the same value.
  // findvaluebynumber() should return the first such value, so we simply
  // ignore addenumvaluebynumber()'s return code.
  file_tables_->addenumvaluebynumber(result);
}

void descriptorbuilder::buildservice(const servicedescriptorproto& proto,
                                     const void* dummy,
                                     servicedescriptor* result) {
  string* full_name = tables_->allocatestring(file_->package());
  if (!full_name->empty()) full_name->append(1, '.');
  full_name->append(proto.name());

  validatesymbolname(proto.name(), *full_name, proto);

  result->name_      = tables_->allocatestring(proto.name());
  result->full_name_ = full_name;
  result->file_      = file_;

  build_array(proto, result, method, buildmethod, result);

  // copy options.
  if (!proto.has_options()) {
    result->options_ = null;  // will set to default_instance later.
  } else {
    allocateoptions(proto.options(), result);
  }

  addsymbol(result->full_name(), null, result->name(),
            proto, symbol(result));
}

void descriptorbuilder::buildmethod(const methoddescriptorproto& proto,
                                    const servicedescriptor* parent,
                                    methoddescriptor* result) {
  result->name_    = tables_->allocatestring(proto.name());
  result->service_ = parent;

  string* full_name = tables_->allocatestring(parent->full_name());
  full_name->append(1, '.');
  full_name->append(*result->name_);
  result->full_name_ = full_name;

  validatesymbolname(proto.name(), *full_name, proto);

  // these will be filled in when cross-linking.
  result->input_type_ = null;
  result->output_type_ = null;

  // copy options.
  if (!proto.has_options()) {
    result->options_ = null;  // will set to default_instance later.
  } else {
    allocateoptions(proto.options(), result);
  }

  addsymbol(result->full_name(), parent, result->name(),
            proto, symbol(result));
}

#undef build_array

// -------------------------------------------------------------------

void descriptorbuilder::crosslinkfile(
    filedescriptor* file, const filedescriptorproto& proto) {
  if (file->options_ == null) {
    file->options_ = &fileoptions::default_instance();
  }

  for (int i = 0; i < file->message_type_count(); i++) {
    crosslinkmessage(&file->message_types_[i], proto.message_type(i));
  }

  for (int i = 0; i < file->extension_count(); i++) {
    crosslinkfield(&file->extensions_[i], proto.extension(i));
  }

  for (int i = 0; i < file->enum_type_count(); i++) {
    crosslinkenum(&file->enum_types_[i], proto.enum_type(i));
  }

  for (int i = 0; i < file->service_count(); i++) {
    crosslinkservice(&file->services_[i], proto.service(i));
  }
}

void descriptorbuilder::crosslinkmessage(
    descriptor* message, const descriptorproto& proto) {
  if (message->options_ == null) {
    message->options_ = &messageoptions::default_instance();
  }

  for (int i = 0; i < message->nested_type_count(); i++) {
    crosslinkmessage(&message->nested_types_[i], proto.nested_type(i));
  }

  for (int i = 0; i < message->enum_type_count(); i++) {
    crosslinkenum(&message->enum_types_[i], proto.enum_type(i));
  }

  for (int i = 0; i < message->field_count(); i++) {
    crosslinkfield(&message->fields_[i], proto.field(i));
  }

  for (int i = 0; i < message->extension_count(); i++) {
    crosslinkfield(&message->extensions_[i], proto.extension(i));
  }
}

void descriptorbuilder::crosslinkfield(
    fielddescriptor* field, const fielddescriptorproto& proto) {
  if (field->options_ == null) {
    field->options_ = &fieldoptions::default_instance();
  }

  if (proto.has_extendee()) {
    symbol extendee = lookupsymbol(proto.extendee(), field->full_name(),
                                   placeholder_extendable_message);
    if (extendee.isnull()) {
      addnotdefinederror(field->full_name(), proto,
                         descriptorpool::errorcollector::extendee,
                         proto.extendee());
      return;
    } else if (extendee.type != symbol::message) {
      adderror(field->full_name(), proto,
               descriptorpool::errorcollector::extendee,
               "\"" + proto.extendee() + "\" is not a message type.");
      return;
    }
    field->containing_type_ = extendee.descriptor;

    if (!field->containing_type()->isextensionnumber(field->number())) {
      adderror(field->full_name(), proto,
               descriptorpool::errorcollector::number,
               strings::substitute("\"$0\" does not declare $1 as an "
                                   "extension number.",
                                   field->containing_type()->full_name(),
                                   field->number()));
    }
  }

  if (proto.has_type_name()) {
    // assume we are expecting a message type unless the proto contains some
    // evidence that it expects an enum type.  this only makes a difference if
    // we end up creating a placeholder.
    bool expecting_enum = (proto.type() == fielddescriptorproto::type_enum) ||
                          proto.has_default_value();

    symbol type =
      lookupsymbol(proto.type_name(), field->full_name(),
                   expecting_enum ? placeholder_enum : placeholder_message,
                   lookup_types);

    if (type.isnull()) {
      addnotdefinederror(field->full_name(), proto,
                         descriptorpool::errorcollector::type,
                         proto.type_name());
      return;
    }

    if (!proto.has_type()) {
      // choose field type based on symbol.
      if (type.type == symbol::message) {
        field->type_ = fielddescriptor::type_message;
      } else if (type.type == symbol::enum) {
        field->type_ = fielddescriptor::type_enum;
      } else {
        adderror(field->full_name(), proto,
                 descriptorpool::errorcollector::type,
                 "\"" + proto.type_name() + "\" is not a type.");
        return;
      }
    }

    if (field->cpp_type() == fielddescriptor::cpptype_message) {
      if (type.type != symbol::message) {
        adderror(field->full_name(), proto,
                 descriptorpool::errorcollector::type,
                 "\"" + proto.type_name() + "\" is not a message type.");
        return;
      }
      field->message_type_ = type.descriptor;

      if (field->has_default_value()) {
        adderror(field->full_name(), proto,
                 descriptorpool::errorcollector::default_value,
                 "messages can't have default values.");
      }
    } else if (field->cpp_type() == fielddescriptor::cpptype_enum) {
      if (type.type != symbol::enum) {
        adderror(field->full_name(), proto,
                 descriptorpool::errorcollector::type,
                 "\"" + proto.type_name() + "\" is not an enum type.");
        return;
      }
      field->enum_type_ = type.enum_descriptor;

      if (field->enum_type()->is_placeholder_) {
        // we can't look up default values for placeholder types.  we'll have
        // to just drop them.
        field->has_default_value_ = false;
      }

      if (field->has_default_value()) {
        // we can't just use field->enum_type()->findvaluebyname() here
        // because that locks the pool's mutex, which we have already locked
        // at this point.
        symbol default_value =
          lookupsymbolnoplaceholder(proto.default_value(),
                                    field->enum_type()->full_name());

        if (default_value.type == symbol::enum_value &&
            default_value.enum_value_descriptor->type() == field->enum_type()) {
          field->default_value_enum_ = default_value.enum_value_descriptor;
        } else {
          adderror(field->full_name(), proto,
                   descriptorpool::errorcollector::default_value,
                   "enum type \"" + field->enum_type()->full_name() +
                   "\" has no value named \"" + proto.default_value() + "\".");
        }
      } else if (field->enum_type()->value_count() > 0) {
        // all enums must have at least one value, or we would have reported
        // an error elsewhere.  we use the first defined value as the default
        // if a default is not explicitly defined.
        field->default_value_enum_ = field->enum_type()->value(0);
      }
    } else {
      adderror(field->full_name(), proto, descriptorpool::errorcollector::type,
               "field with primitive type has type_name.");
    }
  } else {
    if (field->cpp_type() == fielddescriptor::cpptype_message ||
        field->cpp_type() == fielddescriptor::cpptype_enum) {
      adderror(field->full_name(), proto, descriptorpool::errorcollector::type,
               "field with message or enum type missing type_name.");
    }
  }

  // add the field to the fields-by-number table.
  // note:  we have to do this *after* cross-linking because extensions do not
  //   know their containing type until now.
  if (!file_tables_->addfieldbynumber(field)) {
    const fielddescriptor* conflicting_field =
      file_tables_->findfieldbynumber(field->containing_type(),
                                      field->number());
    if (field->is_extension()) {
      adderror(field->full_name(), proto,
               descriptorpool::errorcollector::number,
               strings::substitute("extension number $0 has already been used "
                                   "in \"$1\" by extension \"$2\".",
                                   field->number(),
                                   field->containing_type()->full_name(),
                                   conflicting_field->full_name()));
    } else {
      adderror(field->full_name(), proto,
               descriptorpool::errorcollector::number,
               strings::substitute("field number $0 has already been used in "
                                   "\"$1\" by field \"$2\".",
                                   field->number(),
                                   field->containing_type()->full_name(),
                                   conflicting_field->name()));
    }
  }

  if (field->is_extension()) {
    // no need for error checking: if the extension number collided,
    // we've already been informed of it by the if() above.
    tables_->addextension(field);
  }

  // add the field to the lowercase-name and camelcase-name tables.
  file_tables_->addfieldbystylizednames(field);
}

void descriptorbuilder::crosslinkenum(
    enumdescriptor* enum_type, const enumdescriptorproto& proto) {
  if (enum_type->options_ == null) {
    enum_type->options_ = &enumoptions::default_instance();
  }

  for (int i = 0; i < enum_type->value_count(); i++) {
    crosslinkenumvalue(&enum_type->values_[i], proto.value(i));
  }
}

void descriptorbuilder::crosslinkenumvalue(
    enumvaluedescriptor* enum_value, const enumvaluedescriptorproto& proto) {
  if (enum_value->options_ == null) {
    enum_value->options_ = &enumvalueoptions::default_instance();
  }
}

void descriptorbuilder::crosslinkservice(
    servicedescriptor* service, const servicedescriptorproto& proto) {
  if (service->options_ == null) {
    service->options_ = &serviceoptions::default_instance();
  }

  for (int i = 0; i < service->method_count(); i++) {
    crosslinkmethod(&service->methods_[i], proto.method(i));
  }
}

void descriptorbuilder::crosslinkmethod(
    methoddescriptor* method, const methoddescriptorproto& proto) {
  if (method->options_ == null) {
    method->options_ = &methodoptions::default_instance();
  }

  symbol input_type = lookupsymbol(proto.input_type(), method->full_name());
  if (input_type.isnull()) {
    addnotdefinederror(method->full_name(), proto,
                       descriptorpool::errorcollector::input_type,
                       proto.input_type());
  } else if (input_type.type != symbol::message) {
    adderror(method->full_name(), proto,
             descriptorpool::errorcollector::input_type,
             "\"" + proto.input_type() + "\" is not a message type.");
  } else {
    method->input_type_ = input_type.descriptor;
  }

  symbol output_type = lookupsymbol(proto.output_type(), method->full_name());
  if (output_type.isnull()) {
    addnotdefinederror(method->full_name(), proto,
                       descriptorpool::errorcollector::output_type,
                       proto.output_type());
  } else if (output_type.type != symbol::message) {
    adderror(method->full_name(), proto,
             descriptorpool::errorcollector::output_type,
             "\"" + proto.output_type() + "\" is not a message type.");
  } else {
    method->output_type_ = output_type.descriptor;
  }
}

// -------------------------------------------------------------------

#define validate_options_from_array(descriptor, array_name, type)  \
  for (int i = 0; i < descriptor->array_name##_count(); ++i) {     \
    validate##type##options(descriptor->array_name##s_ + i,        \
                            proto.array_name(i));                  \
  }

// determine if the file uses optimize_for = lite_runtime, being careful to
// avoid problems that exist at init time.
static bool islite(const filedescriptor* file) {
  // todo(kenton):  i don't even remember how many of these conditions are
  //   actually possible.  i'm just being super-safe.
  return file != null &&
         &file->options() != null &&
         &file->options() != &fileoptions::default_instance() &&
         file->options().optimize_for() == fileoptions::lite_runtime;
}

void descriptorbuilder::validatefileoptions(filedescriptor* file,
                                            const filedescriptorproto& proto) {
  validate_options_from_array(file, message_type, message);
  validate_options_from_array(file, enum_type, enum);
  validate_options_from_array(file, service, service);
  validate_options_from_array(file, extension, field);

  // lite files can only be imported by other lite files.
  if (!islite(file)) {
    for (int i = 0; i < file->dependency_count(); i++) {
      if (islite(file->dependency(i))) {
        adderror(
          file->name(), proto,
          descriptorpool::errorcollector::other,
          "files that do not use optimize_for = lite_runtime cannot import "
          "files which do use this option.  this file is not lite, but it "
          "imports \"" + file->dependency(i)->name() + "\" which is.");
        break;
      }
    }
  }
}

void descriptorbuilder::validatemessageoptions(descriptor* message,
                                               const descriptorproto& proto) {
  validate_options_from_array(message, field, field);
  validate_options_from_array(message, nested_type, message);
  validate_options_from_array(message, enum_type, enum);
  validate_options_from_array(message, extension, field);

  const int64 max_extension_range =
      static_cast<int64>(message->options().message_set_wire_format() ?
                         kint32max :
                         fielddescriptor::kmaxnumber);
  for (int i = 0; i < message->extension_range_count(); ++i) {
    if (message->extension_range(i)->end > max_extension_range + 1) {
      adderror(
          message->full_name(), proto.extension_range(i),
          descriptorpool::errorcollector::number,
          strings::substitute("extension numbers cannot be greater than $0.",
                              max_extension_range));
    }
  }
}

void descriptorbuilder::validatefieldoptions(fielddescriptor* field,
    const fielddescriptorproto& proto) {
  if (field->options().has_experimental_map_key()) {
    validatemapkey(field, proto);
  }

  // only message type fields may be lazy.
  if (field->options().lazy()) {
    if (field->type() != fielddescriptor::type_message) {
      adderror(field->full_name(), proto,
               descriptorpool::errorcollector::type,
               "[lazy = true] can only be specified for submessage fields.");
    }
  }

  // only repeated primitive fields may be packed.
  if (field->options().packed() && !field->is_packable()) {
    adderror(
      field->full_name(), proto,
      descriptorpool::errorcollector::type,
      "[packed = true] can only be specified for repeated primitive fields.");
  }

  // note:  default instance may not yet be initialized here, so we have to
  //   avoid reading from it.
  if (field->containing_type_ != null &&
      &field->containing_type()->options() !=
      &messageoptions::default_instance() &&
      field->containing_type()->options().message_set_wire_format()) {
    if (field->is_extension()) {
      if (!field->is_optional() ||
          field->type() != fielddescriptor::type_message) {
        adderror(field->full_name(), proto,
                 descriptorpool::errorcollector::type,
                 "extensions of messagesets must be optional messages.");
      }
    } else {
      adderror(field->full_name(), proto,
               descriptorpool::errorcollector::name,
               "messagesets cannot have fields, only extensions.");
    }
  }

  // lite extensions can only be of lite types.
  if (islite(field->file()) &&
      field->containing_type_ != null &&
      !islite(field->containing_type()->file())) {
    adderror(field->full_name(), proto,
             descriptorpool::errorcollector::extendee,
             "extensions to non-lite types can only be declared in non-lite "
             "files.  note that you cannot extend a non-lite type to contain "
             "a lite type, but the reverse is allowed.");
  }

}

void descriptorbuilder::validateenumoptions(enumdescriptor* enm,
                                            const enumdescriptorproto& proto) {
  validate_options_from_array(enm, value, enumvalue);
  if (!enm->options().has_allow_alias() || !enm->options().allow_alias()) {
    map<int, string> used_values;
    for (int i = 0; i < enm->value_count(); ++i) {
      const enumvaluedescriptor* enum_value = enm->value(i);
      if (used_values.find(enum_value->number()) != used_values.end()) {
        string error =
            "\"" + enum_value->full_name() +
            "\" uses the same enum value as \"" +
            used_values[enum_value->number()] + "\". if this is intended, set "
            "'option allow_alias = true;' to the enum definition.";
        if (!enm->options().allow_alias()) {
          // generate error if duplicated enum values are explicitly disallowed.
          adderror(enm->full_name(), proto,
                   descriptorpool::errorcollector::number,
                   error);
        } else {
          // generate warning if duplicated values are found but the option
          // isn't set.
          google_log(error) << error;
        }
      } else {
        used_values[enum_value->number()] = enum_value->full_name();
      }
    }
  }
}

void descriptorbuilder::validateenumvalueoptions(
    enumvaluedescriptor* enum_value, const enumvaluedescriptorproto& proto) {
  // nothing to do so far.
}
void descriptorbuilder::validateserviceoptions(servicedescriptor* service,
    const servicedescriptorproto& proto) {
  if (islite(service->file()) &&
      (service->file()->options().cc_generic_services() ||
       service->file()->options().java_generic_services())) {
    adderror(service->full_name(), proto,
             descriptorpool::errorcollector::name,
             "files with optimize_for = lite_runtime cannot define services "
             "unless you set both options cc_generic_services and "
             "java_generic_sevices to false.");
  }

  validate_options_from_array(service, method, method);
}

void descriptorbuilder::validatemethodoptions(methoddescriptor* method,
    const methoddescriptorproto& proto) {
  // nothing to do so far.
}

void descriptorbuilder::validatemapkey(fielddescriptor* field,
                                       const fielddescriptorproto& proto) {
  if (!field->is_repeated()) {
    adderror(field->full_name(), proto, descriptorpool::errorcollector::type,
             "map type is only allowed for repeated fields.");
    return;
  }

  if (field->cpp_type() != fielddescriptor::cpptype_message) {
    adderror(field->full_name(), proto, descriptorpool::errorcollector::type,
             "map type is only allowed for fields with a message type.");
    return;
  }

  const descriptor* item_type = field->message_type();
  if (item_type == null) {
    adderror(field->full_name(), proto, descriptorpool::errorcollector::type,
             "could not find field type.");
    return;
  }

  // find the field in item_type named by "experimental_map_key"
  const string& key_name = field->options().experimental_map_key();
  const symbol key_symbol = lookupsymbol(
      key_name,
      // we append ".key_name" to the containing type's name since
      // lookupsymbol() searches for peers of the supplied name, not
      // children of the supplied name.
      item_type->full_name() + "." + key_name);

  if (key_symbol.isnull() || key_symbol.field_descriptor->is_extension()) {
    adderror(field->full_name(), proto, descriptorpool::errorcollector::type,
             "could not find field named \"" + key_name + "\" in type \"" +
             item_type->full_name() + "\".");
    return;
  }
  const fielddescriptor* key_field = key_symbol.field_descriptor;

  if (key_field->is_repeated()) {
    adderror(field->full_name(), proto, descriptorpool::errorcollector::type,
             "map_key must not name a repeated field.");
    return;
  }

  if (key_field->cpp_type() == fielddescriptor::cpptype_message) {
    adderror(field->full_name(), proto, descriptorpool::errorcollector::type,
             "map key must name a scalar or string field.");
    return;
  }

  field->experimental_map_key_ = key_field;
}


#undef validate_options_from_array

// -------------------------------------------------------------------

descriptorbuilder::optioninterpreter::optioninterpreter(
    descriptorbuilder* builder) : builder_(builder) {
  google_check(builder_);
}

descriptorbuilder::optioninterpreter::~optioninterpreter() {
}

bool descriptorbuilder::optioninterpreter::interpretoptions(
    optionstointerpret* options_to_interpret) {
  // note that these may be in different pools, so we can't use the same
  // descriptor and reflection objects on both.
  message* options = options_to_interpret->options;
  const message* original_options = options_to_interpret->original_options;

  bool failed = false;
  options_to_interpret_ = options_to_interpret;

  // find the uninterpreted_option field in the mutable copy of the options
  // and clear them, since we're about to interpret them.
  const fielddescriptor* uninterpreted_options_field =
      options->getdescriptor()->findfieldbyname("uninterpreted_option");
  google_check(uninterpreted_options_field != null)
      << "no field named \"uninterpreted_option\" in the options proto.";
  options->getreflection()->clearfield(options, uninterpreted_options_field);

  // find the uninterpreted_option field in the original options.
  const fielddescriptor* original_uninterpreted_options_field =
      original_options->getdescriptor()->
          findfieldbyname("uninterpreted_option");
  google_check(original_uninterpreted_options_field != null)
      << "no field named \"uninterpreted_option\" in the options proto.";

  const int num_uninterpreted_options = original_options->getreflection()->
      fieldsize(*original_options, original_uninterpreted_options_field);
  for (int i = 0; i < num_uninterpreted_options; ++i) {
    uninterpreted_option_ = down_cast<const uninterpretedoption*>(
        &original_options->getreflection()->getrepeatedmessage(
            *original_options, original_uninterpreted_options_field, i));
    if (!interpretsingleoption(options)) {
      // error already added by interpretsingleoption().
      failed = true;
      break;
    }
  }
  // reset these, so we don't have any dangling pointers.
  uninterpreted_option_ = null;
  options_to_interpret_ = null;

  if (!failed) {
    // interpretsingleoption() added the interpreted options in the
    // unknownfieldset, in case the option isn't yet known to us.  now we
    // serialize the options message and deserialize it back.  that way, any
    // option fields that we do happen to know about will get moved from the
    // unknownfieldset into the real fields, and thus be available right away.
    // if they are not known, that's ok too. they will get reparsed into the
    // unknownfieldset and wait there until the message is parsed by something
    // that does know about the options.
    string buf;
    options->appendtostring(&buf);
    google_check(options->parsefromstring(buf))
        << "protocol message serialized itself in invalid fashion.";
  }

  return !failed;
}

bool descriptorbuilder::optioninterpreter::interpretsingleoption(
    message* options) {
  // first do some basic validation.
  if (uninterpreted_option_->name_size() == 0) {
    // this should never happen unless the parser has gone seriously awry or
    // someone has manually created the uninterpreted option badly.
    return addnameerror("option must have a name.");
  }
  if (uninterpreted_option_->name(0).name_part() == "uninterpreted_option") {
    return addnameerror("option must not use reserved name "
                        "\"uninterpreted_option\".");
  }

  const descriptor* options_descriptor = null;
  // get the options message's descriptor from the builder's pool, so that we
  // get the version that knows about any extension options declared in the
  // file we're currently building. the descriptor should be there as long as
  // the file we're building imported "google/protobuf/descriptors.proto".

  // note that we use descriptorbuilder::findsymbolnotenforcingdeps(), not
  // descriptorpool::findmessagetypebyname() because we're already holding the
  // pool's mutex, and the latter method locks it again.  we don't use
  // findsymbol() because files that use custom options only need to depend on
  // the file that defines the option, not descriptor.proto itself.
  symbol symbol = builder_->findsymbolnotenforcingdeps(
    options->getdescriptor()->full_name());
  if (!symbol.isnull() && symbol.type == symbol::message) {
    options_descriptor = symbol.descriptor;
  } else {
    // the options message's descriptor was not in the builder's pool, so use
    // the standard version from the generated pool. we're not holding the
    // generated pool's mutex, so we can search it the straightforward way.
    options_descriptor = options->getdescriptor();
  }
  google_check(options_descriptor);

  // we iterate over the name parts to drill into the submessages until we find
  // the leaf field for the option. as we drill down we remember the current
  // submessage's descriptor in |descriptor| and the next field in that
  // submessage in |field|. we also track the fields we're drilling down
  // through in |intermediate_fields|. as we go, we reconstruct the full option
  // name in |debug_msg_name|, for use in error messages.
  const descriptor* descriptor = options_descriptor;
  const fielddescriptor* field = null;
  vector<const fielddescriptor*> intermediate_fields;
  string debug_msg_name = "";

  for (int i = 0; i < uninterpreted_option_->name_size(); ++i) {
    const string& name_part = uninterpreted_option_->name(i).name_part();
    if (debug_msg_name.size() > 0) {
      debug_msg_name += ".";
    }
    if (uninterpreted_option_->name(i).is_extension()) {
      debug_msg_name += "(" + name_part + ")";
      // search for the extension's descriptor as an extension in the builder's
      // pool. note that we use descriptorbuilder::lookupsymbol(), not
      // descriptorpool::findextensionbyname(), for two reasons: 1) it allows
      // relative lookups, and 2) because we're already holding the pool's
      // mutex, and the latter method locks it again.
      symbol = builder_->lookupsymbol(name_part,
                                      options_to_interpret_->name_scope);
      if (!symbol.isnull() && symbol.type == symbol::field) {
        field = symbol.field_descriptor;
      }
      // if we don't find the field then the field's descriptor was not in the
      // builder's pool, but there's no point in looking in the generated
      // pool. we require that you import the file that defines any extensions
      // you use, so they must be present in the builder's pool.
    } else {
      debug_msg_name += name_part;
      // search for the field's descriptor as a regular field.
      field = descriptor->findfieldbyname(name_part);
    }

    if (field == null) {
      if (get_allow_unknown(builder_->pool_)) {
        // we can't find the option, but allowunknowndependencies() is enabled,
        // so we will just leave it as uninterpreted.
        addwithoutinterpreting(*uninterpreted_option_, options);
        return true;
      } else {
        return addnameerror("option \"" + debug_msg_name + "\" unknown.");
      }
    } else if (field->containing_type() != descriptor) {
      if (get_is_placeholder(field->containing_type())) {
        // the field is an extension of a placeholder type, so we can't
        // reliably verify whether it is a valid extension to use here (e.g.
        // we don't know if it is an extension of the correct *options message,
        // or if it has a valid field number, etc.).  just leave it as
        // uninterpreted instead.
        addwithoutinterpreting(*uninterpreted_option_, options);
        return true;
      } else {
        // this can only happen if, due to some insane misconfiguration of the
        // pools, we find the options message in one pool but the field in
        // another. this would probably imply a hefty bug somewhere.
        return addnameerror("option field \"" + debug_msg_name +
                            "\" is not a field or extension of message \"" +
                            descriptor->name() + "\".");
      }
    } else if (field->is_repeated()) {
      return addnameerror("option field \"" + debug_msg_name +
                          "\" is repeated. repeated options are not "
                          "supported.");
    } else if (i < uninterpreted_option_->name_size() - 1) {
      if (field->cpp_type() != fielddescriptor::cpptype_message) {
        return addnameerror("option \"" +  debug_msg_name +
                            "\" is an atomic type, not a message.");
      } else {
        // drill down into the submessage.
        intermediate_fields.push_back(field);
        descriptor = field->message_type();
      }
    }
  }

  // we've found the leaf field. now we use unknownfieldsets to set its value
  // on the options message. we do so because the message may not yet know
  // about its extension fields, so we may not be able to set the fields
  // directly. but the unknownfieldsets will serialize to the same wire-format
  // message, so reading that message back in once the extension fields are
  // known will populate them correctly.

  // first see if the option is already set.
  if (!examineifoptionisset(
          intermediate_fields.begin(),
          intermediate_fields.end(),
          field, debug_msg_name,
          options->getreflection()->getunknownfields(*options))) {
    return false;  // examineifoptionisset() already added the error.
  }


  // first set the value on the unknownfieldset corresponding to the
  // innermost message.
  scoped_ptr<unknownfieldset> unknown_fields(new unknownfieldset());
  if (!setoptionvalue(field, unknown_fields.get())) {
    return false;  // setoptionvalue() already added the error.
  }

  // now wrap the unknownfieldset with unknownfieldsets corresponding to all
  // the intermediate messages.
  for (vector<const fielddescriptor*>::reverse_iterator iter =
           intermediate_fields.rbegin();
       iter != intermediate_fields.rend(); ++iter) {
    scoped_ptr<unknownfieldset> parent_unknown_fields(new unknownfieldset());
    switch ((*iter)->type()) {
      case fielddescriptor::type_message: {
        io::stringoutputstream outstr(
            parent_unknown_fields->addlengthdelimited((*iter)->number()));
        io::codedoutputstream out(&outstr);
        internal::wireformat::serializeunknownfields(*unknown_fields, &out);
        google_check(!out.haderror())
            << "unexpected failure while serializing option submessage "
            << debug_msg_name << "\".";
        break;
      }

      case fielddescriptor::type_group: {
         parent_unknown_fields->addgroup((*iter)->number())
                              ->mergefrom(*unknown_fields);
        break;
      }

      default:
        google_log(fatal) << "invalid wire type for cpptype_message: "
                   << (*iter)->type();
        return false;
    }
    unknown_fields.reset(parent_unknown_fields.release());
  }

  // now merge the unknownfieldset corresponding to the top-level message into
  // the options message.
  options->getreflection()->mutableunknownfields(options)->mergefrom(
      *unknown_fields);

  return true;
}

void descriptorbuilder::optioninterpreter::addwithoutinterpreting(
    const uninterpretedoption& uninterpreted_option, message* options) {
  const fielddescriptor* field =
    options->getdescriptor()->findfieldbyname("uninterpreted_option");
  google_check(field != null);

  options->getreflection()->addmessage(options, field)
    ->copyfrom(uninterpreted_option);
}

bool descriptorbuilder::optioninterpreter::examineifoptionisset(
    vector<const fielddescriptor*>::const_iterator intermediate_fields_iter,
    vector<const fielddescriptor*>::const_iterator intermediate_fields_end,
    const fielddescriptor* innermost_field, const string& debug_msg_name,
    const unknownfieldset& unknown_fields) {
  // we do linear searches of the unknownfieldset and its sub-groups.  this
  // should be fine since it's unlikely that any one options structure will
  // contain more than a handful of options.

  if (intermediate_fields_iter == intermediate_fields_end) {
    // we're at the innermost submessage.
    for (int i = 0; i < unknown_fields.field_count(); i++) {
      if (unknown_fields.field(i).number() == innermost_field->number()) {
        return addnameerror("option \"" + debug_msg_name +
                            "\" was already set.");
      }
    }
    return true;
  }

  for (int i = 0; i < unknown_fields.field_count(); i++) {
    if (unknown_fields.field(i).number() ==
        (*intermediate_fields_iter)->number()) {
      const unknownfield* unknown_field = &unknown_fields.field(i);
      fielddescriptor::type type = (*intermediate_fields_iter)->type();
      // recurse into the next submessage.
      switch (type) {
        case fielddescriptor::type_message:
          if (unknown_field->type() == unknownfield::type_length_delimited) {
            unknownfieldset intermediate_unknown_fields;
            if (intermediate_unknown_fields.parsefromstring(
                    unknown_field->length_delimited()) &&
                !examineifoptionisset(intermediate_fields_iter + 1,
                                      intermediate_fields_end,
                                      innermost_field, debug_msg_name,
                                      intermediate_unknown_fields)) {
              return false;  // error already added.
            }
          }
          break;

        case fielddescriptor::type_group:
          if (unknown_field->type() == unknownfield::type_group) {
            if (!examineifoptionisset(intermediate_fields_iter + 1,
                                      intermediate_fields_end,
                                      innermost_field, debug_msg_name,
                                      unknown_field->group())) {
              return false;  // error already added.
            }
          }
          break;

        default:
          google_log(fatal) << "invalid wire type for cpptype_message: " << type;
          return false;
      }
    }
  }
  return true;
}

bool descriptorbuilder::optioninterpreter::setoptionvalue(
    const fielddescriptor* option_field,
    unknownfieldset* unknown_fields) {
  // we switch on the cpptype to validate.
  switch (option_field->cpp_type()) {

    case fielddescriptor::cpptype_int32:
      if (uninterpreted_option_->has_positive_int_value()) {
        if (uninterpreted_option_->positive_int_value() >
            static_cast<uint64>(kint32max)) {
          return addvalueerror("value out of range for int32 option \"" +
                               option_field->full_name() + "\".");
        } else {
          setint32(option_field->number(),
                   uninterpreted_option_->positive_int_value(),
                   option_field->type(), unknown_fields);
        }
      } else if (uninterpreted_option_->has_negative_int_value()) {
        if (uninterpreted_option_->negative_int_value() <
            static_cast<int64>(kint32min)) {
          return addvalueerror("value out of range for int32 option \"" +
                               option_field->full_name() + "\".");
        } else {
          setint32(option_field->number(),
                   uninterpreted_option_->negative_int_value(),
                   option_field->type(), unknown_fields);
        }
      } else {
        return addvalueerror("value must be integer for int32 option \"" +
                             option_field->full_name() + "\".");
      }
      break;

    case fielddescriptor::cpptype_int64:
      if (uninterpreted_option_->has_positive_int_value()) {
        if (uninterpreted_option_->positive_int_value() >
            static_cast<uint64>(kint64max)) {
          return addvalueerror("value out of range for int64 option \"" +
                               option_field->full_name() + "\".");
        } else {
          setint64(option_field->number(),
                   uninterpreted_option_->positive_int_value(),
                   option_field->type(), unknown_fields);
        }
      } else if (uninterpreted_option_->has_negative_int_value()) {
        setint64(option_field->number(),
                 uninterpreted_option_->negative_int_value(),
                 option_field->type(), unknown_fields);
      } else {
        return addvalueerror("value must be integer for int64 option \"" +
                             option_field->full_name() + "\".");
      }
      break;

    case fielddescriptor::cpptype_uint32:
      if (uninterpreted_option_->has_positive_int_value()) {
        if (uninterpreted_option_->positive_int_value() > kuint32max) {
          return addvalueerror("value out of range for uint32 option \"" +
                               option_field->name() + "\".");
        } else {
          setuint32(option_field->number(),
                    uninterpreted_option_->positive_int_value(),
                    option_field->type(), unknown_fields);
        }
      } else {
        return addvalueerror("value must be non-negative integer for uint32 "
                             "option \"" + option_field->full_name() + "\".");
      }
      break;

    case fielddescriptor::cpptype_uint64:
      if (uninterpreted_option_->has_positive_int_value()) {
        setuint64(option_field->number(),
                  uninterpreted_option_->positive_int_value(),
                  option_field->type(), unknown_fields);
      } else {
        return addvalueerror("value must be non-negative integer for uint64 "
                             "option \"" + option_field->full_name() + "\".");
      }
      break;

    case fielddescriptor::cpptype_float: {
      float value;
      if (uninterpreted_option_->has_double_value()) {
        value = uninterpreted_option_->double_value();
      } else if (uninterpreted_option_->has_positive_int_value()) {
        value = uninterpreted_option_->positive_int_value();
      } else if (uninterpreted_option_->has_negative_int_value()) {
        value = uninterpreted_option_->negative_int_value();
      } else {
        return addvalueerror("value must be number for float option \"" +
                             option_field->full_name() + "\".");
      }
      unknown_fields->addfixed32(option_field->number(),
          google::protobuf::internal::wireformatlite::encodefloat(value));
      break;
    }

    case fielddescriptor::cpptype_double: {
      double value;
      if (uninterpreted_option_->has_double_value()) {
        value = uninterpreted_option_->double_value();
      } else if (uninterpreted_option_->has_positive_int_value()) {
        value = uninterpreted_option_->positive_int_value();
      } else if (uninterpreted_option_->has_negative_int_value()) {
        value = uninterpreted_option_->negative_int_value();
      } else {
        return addvalueerror("value must be number for double option \"" +
                             option_field->full_name() + "\".");
      }
      unknown_fields->addfixed64(option_field->number(),
          google::protobuf::internal::wireformatlite::encodedouble(value));
      break;
    }

    case fielddescriptor::cpptype_bool:
      uint64 value;
      if (!uninterpreted_option_->has_identifier_value()) {
        return addvalueerror("value must be identifier for boolean option "
                             "\"" + option_field->full_name() + "\".");
      }
      if (uninterpreted_option_->identifier_value() == "true") {
        value = 1;
      } else if (uninterpreted_option_->identifier_value() == "false") {
        value = 0;
      } else {
        return addvalueerror("value must be \"true\" or \"false\" for boolean "
                             "option \"" + option_field->full_name() + "\".");
      }
      unknown_fields->addvarint(option_field->number(), value);
      break;

    case fielddescriptor::cpptype_enum: {
      if (!uninterpreted_option_->has_identifier_value()) {
        return addvalueerror("value must be identifier for enum-valued option "
                             "\"" + option_field->full_name() + "\".");
      }
      const enumdescriptor* enum_type = option_field->enum_type();
      const string& value_name = uninterpreted_option_->identifier_value();
      const enumvaluedescriptor* enum_value = null;

      if (enum_type->file()->pool() != descriptorpool::generated_pool()) {
        // note that the enum value's fully-qualified name is a sibling of the
        // enum's name, not a child of it.
        string fully_qualified_name = enum_type->full_name();
        fully_qualified_name.resize(fully_qualified_name.size() -
                                    enum_type->name().size());
        fully_qualified_name += value_name;

        // search for the enum value's descriptor in the builder's pool. note
        // that we use descriptorbuilder::findsymbolnotenforcingdeps(), not
        // descriptorpool::findenumvaluebyname() because we're already holding
        // the pool's mutex, and the latter method locks it again.
        symbol symbol =
          builder_->findsymbolnotenforcingdeps(fully_qualified_name);
        if (!symbol.isnull() && symbol.type == symbol::enum_value) {
          if (symbol.enum_value_descriptor->type() != enum_type) {
            return addvalueerror("enum type \"" + enum_type->full_name() +
                "\" has no value named \"" + value_name + "\" for option \"" +
                option_field->full_name() +
                "\". this appears to be a value from a sibling type.");
          } else {
            enum_value = symbol.enum_value_descriptor;
          }
        }
      } else {
        // the enum type is in the generated pool, so we can search for the
        // value there.
        enum_value = enum_type->findvaluebyname(value_name);
      }

      if (enum_value == null) {
        return addvalueerror("enum type \"" +
                             option_field->enum_type()->full_name() +
                             "\" has no value named \"" + value_name + "\" for "
                             "option \"" + option_field->full_name() + "\".");
      } else {
        // sign-extension is not a problem, since we cast directly from int32 to
        // uint64, without first going through uint32.
        unknown_fields->addvarint(option_field->number(),
          static_cast<uint64>(static_cast<int64>(enum_value->number())));
      }
      break;
    }

    case fielddescriptor::cpptype_string:
      if (!uninterpreted_option_->has_string_value()) {
        return addvalueerror("value must be quoted string for string option "
                             "\"" + option_field->full_name() + "\".");
      }
      // the string has already been unquoted and unescaped by the parser.
      unknown_fields->addlengthdelimited(option_field->number(),
          uninterpreted_option_->string_value());
      break;

    case fielddescriptor::cpptype_message:
      if (!setaggregateoption(option_field, unknown_fields)) {
        return false;
      }
      break;
  }

  return true;
}

class descriptorbuilder::optioninterpreter::aggregateoptionfinder
    : public textformat::finder {
 public:
  descriptorbuilder* builder_;

  virtual const fielddescriptor* findextension(
      message* message, const string& name) const {
    assert_mutex_held(builder_->pool_);
    const descriptor* descriptor = message->getdescriptor();
    symbol result = builder_->lookupsymbolnoplaceholder(
        name, descriptor->full_name());
    if (result.type == symbol::field &&
        result.field_descriptor->is_extension()) {
      return result.field_descriptor;
    } else if (result.type == symbol::message &&
               descriptor->options().message_set_wire_format()) {
      const descriptor* foreign_type = result.descriptor;
      // the text format allows messageset items to be specified using
      // the type name, rather than the extension identifier. if the symbol
      // lookup returned a message, and the enclosing message has
      // message_set_wire_format = true, then return the message set
      // extension, if one exists.
      for (int i = 0; i < foreign_type->extension_count(); i++) {
        const fielddescriptor* extension = foreign_type->extension(i);
        if (extension->containing_type() == descriptor &&
            extension->type() == fielddescriptor::type_message &&
            extension->is_optional() &&
            extension->message_type() == foreign_type) {
          // found it.
          return extension;
        }
      }
    }
    return null;
  }
};

// a custom error collector to record any text-format parsing errors
namespace {
class aggregateerrorcollector : public io::errorcollector {
 public:
  string error_;

  virtual void adderror(int line, int column, const string& message) {
    if (!error_.empty()) {
      error_ += "; ";
    }
    error_ += message;
  }

  virtual void addwarning(int line, int column, const string& message) {
    // ignore warnings
  }
};
}

// we construct a dynamic message of the type corresponding to
// option_field, parse the supplied text-format string into this
// message, and serialize the resulting message to produce the value.
bool descriptorbuilder::optioninterpreter::setaggregateoption(
    const fielddescriptor* option_field,
    unknownfieldset* unknown_fields) {
  if (!uninterpreted_option_->has_aggregate_value()) {
    return addvalueerror("option \"" + option_field->full_name() +
                         "\" is a message. to set the entire message, use "
                         "syntax like \"" + option_field->name() +
                         " = { <proto text format> }\". "
                         "to set fields within it, use "
                         "syntax like \"" + option_field->name() +
                         ".foo = value\".");
  }

  const descriptor* type = option_field->message_type();
  scoped_ptr<message> dynamic(dynamic_factory_.getprototype(type)->new());
  google_check(dynamic.get() != null)
      << "could not create an instance of " << option_field->debugstring();

  aggregateerrorcollector collector;
  aggregateoptionfinder finder;
  finder.builder_ = builder_;
  textformat::parser parser;
  parser.recorderrorsto(&collector);
  parser.setfinder(&finder);
  if (!parser.parsefromstring(uninterpreted_option_->aggregate_value(),
                              dynamic.get())) {
    addvalueerror("error while parsing option value for \"" +
                  option_field->name() + "\": " + collector.error_);
    return false;
  } else {
    string serial;
    dynamic->serializetostring(&serial);  // never fails
    if (option_field->type() == fielddescriptor::type_message) {
      unknown_fields->addlengthdelimited(option_field->number(), serial);
    } else {
      google_check_eq(option_field->type(),  fielddescriptor::type_group);
      unknownfieldset* group = unknown_fields->addgroup(option_field->number());
      group->parsefromstring(serial);
    }
    return true;
  }
}

void descriptorbuilder::optioninterpreter::setint32(int number, int32 value,
    fielddescriptor::type type, unknownfieldset* unknown_fields) {
  switch (type) {
    case fielddescriptor::type_int32:
      unknown_fields->addvarint(number,
        static_cast<uint64>(static_cast<int64>(value)));
      break;

    case fielddescriptor::type_sfixed32:
      unknown_fields->addfixed32(number, static_cast<uint32>(value));
      break;

    case fielddescriptor::type_sint32:
      unknown_fields->addvarint(number,
          google::protobuf::internal::wireformatlite::zigzagencode32(value));
      break;

    default:
      google_log(fatal) << "invalid wire type for cpptype_int32: " << type;
      break;
  }
}

void descriptorbuilder::optioninterpreter::setint64(int number, int64 value,
    fielddescriptor::type type, unknownfieldset* unknown_fields) {
  switch (type) {
    case fielddescriptor::type_int64:
      unknown_fields->addvarint(number, static_cast<uint64>(value));
      break;

    case fielddescriptor::type_sfixed64:
      unknown_fields->addfixed64(number, static_cast<uint64>(value));
      break;

    case fielddescriptor::type_sint64:
      unknown_fields->addvarint(number,
          google::protobuf::internal::wireformatlite::zigzagencode64(value));
      break;

    default:
      google_log(fatal) << "invalid wire type for cpptype_int64: " << type;
      break;
  }
}

void descriptorbuilder::optioninterpreter::setuint32(int number, uint32 value,
    fielddescriptor::type type, unknownfieldset* unknown_fields) {
  switch (type) {
    case fielddescriptor::type_uint32:
      unknown_fields->addvarint(number, static_cast<uint64>(value));
      break;

    case fielddescriptor::type_fixed32:
      unknown_fields->addfixed32(number, static_cast<uint32>(value));
      break;

    default:
      google_log(fatal) << "invalid wire type for cpptype_uint32: " << type;
      break;
  }
}

void descriptorbuilder::optioninterpreter::setuint64(int number, uint64 value,
    fielddescriptor::type type, unknownfieldset* unknown_fields) {
  switch (type) {
    case fielddescriptor::type_uint64:
      unknown_fields->addvarint(number, value);
      break;

    case fielddescriptor::type_fixed64:
      unknown_fields->addfixed64(number, value);
      break;

    default:
      google_log(fatal) << "invalid wire type for cpptype_uint64: " << type;
      break;
  }
}

}  // namespace protobuf
}  // namespace google
