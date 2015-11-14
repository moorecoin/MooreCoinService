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
// interface for manipulating databases of descriptors.

#ifndef google_protobuf_descriptor_database_h__
#define google_protobuf_descriptor_database_h__

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/descriptor.h>

namespace google {
namespace protobuf {

// defined in this file.
class descriptordatabase;
class simpledescriptordatabase;
class encodeddescriptordatabase;
class descriptorpooldatabase;
class mergeddescriptordatabase;

// abstract interface for a database of descriptors.
//
// this is useful if you want to create a descriptorpool which loads
// descriptors on-demand from some sort of large database.  if the database
// is large, it may be inefficient to enumerate every .proto file inside it
// calling descriptorpool::buildfile() for each one.  instead, a descriptorpool
// can be created which wraps a descriptordatabase and only builds particular
// descriptors when they are needed.
class libprotobuf_export descriptordatabase {
 public:
  inline descriptordatabase() {}
  virtual ~descriptordatabase();

  // find a file by file name.  fills in in *output and returns true if found.
  // otherwise, returns false, leaving the contents of *output undefined.
  virtual bool findfilebyname(const string& filename,
                              filedescriptorproto* output) = 0;

  // find the file that declares the given fully-qualified symbol name.
  // if found, fills in *output and returns true, otherwise returns false
  // and leaves *output undefined.
  virtual bool findfilecontainingsymbol(const string& symbol_name,
                                        filedescriptorproto* output) = 0;

  // find the file which defines an extension extending the given message type
  // with the given field number.  if found, fills in *output and returns true,
  // otherwise returns false and leaves *output undefined.  containing_type
  // must be a fully-qualified type name.
  virtual bool findfilecontainingextension(const string& containing_type,
                                           int field_number,
                                           filedescriptorproto* output) = 0;

  // finds the tag numbers used by all known extensions of
  // extendee_type, and appends them to output in an undefined
  // order. this method is best-effort: it's not guaranteed that the
  // database will find all extensions, and it's not guaranteed that
  // findfilecontainingextension will return true on all of the found
  // numbers. returns true if the search was successful, otherwise
  // returns false and leaves output unchanged.
  //
  // this method has a default implementation that always returns
  // false.
  virtual bool findallextensionnumbers(const string& extendee_type,
                                       vector<int>* output) {
    return false;
  }

 private:
  google_disallow_evil_constructors(descriptordatabase);
};

// a descriptordatabase into which you can insert files manually.
//
// findfilecontainingsymbol() is fully-implemented.  when you add a file, its
// symbols will be indexed for this purpose.  note that the implementation
// may return false positives, but only if it isn't possible for the symbol
// to be defined in any other file.  in particular, if a file defines a symbol
// "foo", then searching for "foo.[anything]" will match that file.  this way,
// the database does not need to aggressively index all children of a symbol.
//
// findfilecontainingextension() is mostly-implemented.  it works if and only
// if the original fielddescriptorproto defining the extension has a
// fully-qualified type name in its "extendee" field (i.e. starts with a '.').
// if the extendee is a relative name, simpledescriptordatabase will not
// attempt to resolve the type, so it will not know what type the extension is
// extending.  therefore, calling findfilecontainingextension() with the
// extension's containing type will never actually find that extension.  note
// that this is an unlikely problem, as all filedescriptorprotos created by the
// protocol compiler (as well as ones created by calling
// filedescriptor::copyto()) will always use fully-qualified names for all
// types.  you only need to worry if you are constructing filedescriptorprotos
// yourself, or are calling compiler::parser directly.
class libprotobuf_export simpledescriptordatabase : public descriptordatabase {
 public:
  simpledescriptordatabase();
  ~simpledescriptordatabase();

  // adds the filedescriptorproto to the database, making a copy.  the object
  // can be deleted after add() returns.  returns false if the file conflicted
  // with a file already in the database, in which case an error will have
  // been written to google_log(error).
  bool add(const filedescriptorproto& file);

  // adds the filedescriptorproto to the database and takes ownership of it.
  bool addandown(const filedescriptorproto* file);

  // implements descriptordatabase -----------------------------------
  bool findfilebyname(const string& filename,
                      filedescriptorproto* output);
  bool findfilecontainingsymbol(const string& symbol_name,
                                filedescriptorproto* output);
  bool findfilecontainingextension(const string& containing_type,
                                   int field_number,
                                   filedescriptorproto* output);
  bool findallextensionnumbers(const string& extendee_type,
                               vector<int>* output);

 private:
  // so that it can use descriptorindex.
  friend class encodeddescriptordatabase;

  // an index mapping file names, symbol names, and extension numbers to
  // some sort of values.
  template <typename value>
  class descriptorindex {
   public:
    // helpers to recursively add particular descriptors and all their contents
    // to the index.
    bool addfile(const filedescriptorproto& file,
                 value value);
    bool addsymbol(const string& name, value value);
    bool addnestedextensions(const descriptorproto& message_type,
                             value value);
    bool addextension(const fielddescriptorproto& field,
                      value value);

    value findfile(const string& filename);
    value findsymbol(const string& name);
    value findextension(const string& containing_type, int field_number);
    bool findallextensionnumbers(const string& containing_type,
                                 vector<int>* output);

   private:
    map<string, value> by_name_;
    map<string, value> by_symbol_;
    map<pair<string, int>, value> by_extension_;

    // invariant:  the by_symbol_ map does not contain any symbols which are
    // prefixes of other symbols in the map.  for example, "foo.bar" is a
    // prefix of "foo.bar.baz" (but is not a prefix of "foo.barbaz").
    //
    // this invariant is important because it means that given a symbol name,
    // we can find a key in the map which is a prefix of the symbol in o(lg n)
    // time, and we know that there is at most one such key.
    //
    // the prefix lookup algorithm works like so:
    // 1) find the last key in the map which is less than or equal to the
    //    search key.
    // 2) if the found key is a prefix of the search key, then return it.
    //    otherwise, there is no match.
    //
    // i am sure this algorithm has been described elsewhere, but since i
    // wasn't able to find it quickly i will instead prove that it works
    // myself.  the key to the algorithm is that if a match exists, step (1)
    // will find it.  proof:
    // 1) define the "search key" to be the key we are looking for, the "found
    //    key" to be the key found in step (1), and the "match key" to be the
    //    key which actually matches the serach key (i.e. the key we're trying
    //    to find).
    // 2) the found key must be less than or equal to the search key by
    //    definition.
    // 3) the match key must also be less than or equal to the search key
    //    (because it is a prefix).
    // 4) the match key cannot be greater than the found key, because if it
    //    were, then step (1) of the algorithm would have returned the match
    //    key instead (since it finds the *greatest* key which is less than or
    //    equal to the search key).
    // 5) therefore, the found key must be between the match key and the search
    //    key, inclusive.
    // 6) since the search key must be a sub-symbol of the match key, if it is
    //    not equal to the match key, then search_key[match_key.size()] must
    //    be '.'.
    // 7) since '.' sorts before any other character that is valid in a symbol
    //    name, then if the found key is not equal to the match key, then
    //    found_key[match_key.size()] must also be '.', because any other value
    //    would make it sort after the search key.
    // 8) therefore, if the found key is not equal to the match key, then the
    //    found key must be a sub-symbol of the match key.  however, this would
    //    contradict our map invariant which says that no symbol in the map is
    //    a sub-symbol of any other.
    // 9) therefore, the found key must match the match key.
    //
    // the above proof assumes the match key exists.  in the case that the
    // match key does not exist, then step (1) will return some other symbol.
    // that symbol cannot be a super-symbol of the search key since if it were,
    // then it would be a match, and we're assuming the match key doesn't exist.
    // therefore, step 2 will correctly return no match.

    // find the last entry in the by_symbol_ map whose key is less than or
    // equal to the given name.
    typename map<string, value>::iterator findlastlessorequal(
        const string& name);

    // true if either the arguments are equal or super_symbol identifies a
    // parent symbol of sub_symbol (e.g. "foo.bar" is a parent of
    // "foo.bar.baz", but not a parent of "foo.barbaz").
    bool issubsymbol(const string& sub_symbol, const string& super_symbol);

    // returns true if and only if all characters in the name are alphanumerics,
    // underscores, or periods.
    bool validatesymbolname(const string& name);
  };


  descriptorindex<const filedescriptorproto*> index_;
  vector<const filedescriptorproto*> files_to_delete_;

  // if file is non-null, copy it into *output and return true, otherwise
  // return false.
  bool maybecopy(const filedescriptorproto* file,
                 filedescriptorproto* output);

  google_disallow_evil_constructors(simpledescriptordatabase);
};

// very similar to simpledescriptordatabase, but stores all the descriptors
// as raw bytes and generally tries to use as little memory as possible.
//
// the same caveats regarding findfilecontainingextension() apply as with
// simpledescriptordatabase.
class libprotobuf_export encodeddescriptordatabase : public descriptordatabase {
 public:
  encodeddescriptordatabase();
  ~encodeddescriptordatabase();

  // adds the filedescriptorproto to the database.  the descriptor is provided
  // in encoded form.  the database does not make a copy of the bytes, nor
  // does it take ownership; it's up to the caller to make sure the bytes
  // remain valid for the life of the database.  returns false and logs an error
  // if the bytes are not a valid filedescriptorproto or if the file conflicted
  // with a file already in the database.
  bool add(const void* encoded_file_descriptor, int size);

  // like add(), but makes a copy of the data, so that the caller does not
  // need to keep it around.
  bool addcopy(const void* encoded_file_descriptor, int size);

  // like findfilecontainingsymbol but returns only the name of the file.
  bool findnameoffilecontainingsymbol(const string& symbol_name,
                                      string* output);

  // implements descriptordatabase -----------------------------------
  bool findfilebyname(const string& filename,
                      filedescriptorproto* output);
  bool findfilecontainingsymbol(const string& symbol_name,
                                filedescriptorproto* output);
  bool findfilecontainingextension(const string& containing_type,
                                   int field_number,
                                   filedescriptorproto* output);
  bool findallextensionnumbers(const string& extendee_type,
                               vector<int>* output);

 private:
  simpledescriptordatabase::descriptorindex<pair<const void*, int> > index_;
  vector<void*> files_to_delete_;

  // if encoded_file.first is non-null, parse the data into *output and return
  // true, otherwise return false.
  bool maybeparse(pair<const void*, int> encoded_file,
                  filedescriptorproto* output);

  google_disallow_evil_constructors(encodeddescriptordatabase);
};

// a descriptordatabase that fetches files from a given pool.
class libprotobuf_export descriptorpooldatabase : public descriptordatabase {
 public:
  descriptorpooldatabase(const descriptorpool& pool);
  ~descriptorpooldatabase();

  // implements descriptordatabase -----------------------------------
  bool findfilebyname(const string& filename,
                      filedescriptorproto* output);
  bool findfilecontainingsymbol(const string& symbol_name,
                                filedescriptorproto* output);
  bool findfilecontainingextension(const string& containing_type,
                                   int field_number,
                                   filedescriptorproto* output);
  bool findallextensionnumbers(const string& extendee_type,
                               vector<int>* output);

 private:
  const descriptorpool& pool_;
  google_disallow_evil_constructors(descriptorpooldatabase);
};

// a descriptordatabase that wraps two or more others.  it first searches the
// first database and, if that fails, tries the second, and so on.
class libprotobuf_export mergeddescriptordatabase : public descriptordatabase {
 public:
  // merge just two databases.  the sources remain property of the caller.
  mergeddescriptordatabase(descriptordatabase* source1,
                           descriptordatabase* source2);
  // merge more than two databases.  the sources remain property of the caller.
  // the vector may be deleted after the constructor returns but the
  // descriptordatabases need to stick around.
  mergeddescriptordatabase(const vector<descriptordatabase*>& sources);
  ~mergeddescriptordatabase();

  // implements descriptordatabase -----------------------------------
  bool findfilebyname(const string& filename,
                      filedescriptorproto* output);
  bool findfilecontainingsymbol(const string& symbol_name,
                                filedescriptorproto* output);
  bool findfilecontainingextension(const string& containing_type,
                                   int field_number,
                                   filedescriptorproto* output);
  // merges the results of calling all databases. returns true iff any
  // of the databases returned true.
  bool findallextensionnumbers(const string& extendee_type,
                               vector<int>* output);

 private:
  vector<descriptordatabase*> sources_;
  google_disallow_evil_constructors(mergeddescriptordatabase);
};

}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_descriptor_database_h__
