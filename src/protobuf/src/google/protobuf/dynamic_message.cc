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
// dynamicmessage is implemented by constructing a data structure which
// has roughly the same memory layout as a generated message would have.
// then, we use generatedmessagereflection to implement our reflection
// interface.  all the other operations we need to implement (e.g.
// parsing, copying, etc.) are already implemented in terms of
// reflection, so the rest is easy.
//
// the up side of this strategy is that it's very efficient.  we don't
// need to use hash_maps or generic representations of fields.  the
// down side is that this is a low-level memory management hack which
// can be tricky to get right.
//
// as mentioned in the header, we only expose a dynamicmessagefactory
// publicly, not the dynamicmessage class itself.  this is because
// genericmessagereflection wants to have a pointer to a "default"
// copy of the class, with all fields initialized to their default
// values.  we only want to construct one of these per message type,
// so dynamicmessagefactory stores a cache of default messages for
// each type it sees (each unique descriptor pointer).  the code
// refers to the "default" copy of the class as the "prototype".
//
// note on memory allocation:  this module often calls "operator new()"
// to allocate untyped memory, rather than calling something like
// "new uint8[]".  this is because "operator new()" means "give me some
// space which i can use as i please." while "new uint8[]" means "give
// me an array of 8-bit integers.".  in practice, the later may return
// a pointer that is not aligned correctly for general use.  i believe
// item 8 of "more effective c++" discusses this in more detail, though
// i don't have the book on me right now so i'm not sure.

#include <algorithm>
#include <google/protobuf/stubs/hash.h>

#include <google/protobuf/stubs/common.h>

#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/generated_message_util.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/extension_set.h>
#include <google/protobuf/wire_format.h>

namespace google {
namespace protobuf {

using internal::wireformat;
using internal::extensionset;
using internal::generatedmessagereflection;


// ===================================================================
// some helper tables and functions...

namespace {

// compute the byte size of the in-memory representation of the field.
int fieldspaceused(const fielddescriptor* field) {
  typedef fielddescriptor fd;  // avoid line wrapping
  if (field->label() == fd::label_repeated) {
    switch (field->cpp_type()) {
      case fd::cpptype_int32  : return sizeof(repeatedfield<int32   >);
      case fd::cpptype_int64  : return sizeof(repeatedfield<int64   >);
      case fd::cpptype_uint32 : return sizeof(repeatedfield<uint32  >);
      case fd::cpptype_uint64 : return sizeof(repeatedfield<uint64  >);
      case fd::cpptype_double : return sizeof(repeatedfield<double  >);
      case fd::cpptype_float  : return sizeof(repeatedfield<float   >);
      case fd::cpptype_bool   : return sizeof(repeatedfield<bool    >);
      case fd::cpptype_enum   : return sizeof(repeatedfield<int     >);
      case fd::cpptype_message: return sizeof(repeatedptrfield<message>);

      case fd::cpptype_string:
        switch (field->options().ctype()) {
          default:  // todo(kenton):  support other string reps.
          case fieldoptions::string:
            return sizeof(repeatedptrfield<string>);
        }
        break;
    }
  } else {
    switch (field->cpp_type()) {
      case fd::cpptype_int32  : return sizeof(int32   );
      case fd::cpptype_int64  : return sizeof(int64   );
      case fd::cpptype_uint32 : return sizeof(uint32  );
      case fd::cpptype_uint64 : return sizeof(uint64  );
      case fd::cpptype_double : return sizeof(double  );
      case fd::cpptype_float  : return sizeof(float   );
      case fd::cpptype_bool   : return sizeof(bool    );
      case fd::cpptype_enum   : return sizeof(int     );

      case fd::cpptype_message:
        return sizeof(message*);

      case fd::cpptype_string:
        switch (field->options().ctype()) {
          default:  // todo(kenton):  support other string reps.
          case fieldoptions::string:
            return sizeof(string*);
        }
        break;
    }
  }

  google_log(dfatal) << "can't get here.";
  return 0;
}

inline int divideroundingup(int i, int j) {
  return (i + (j - 1)) / j;
}

static const int ksafealignment = sizeof(uint64);

inline int alignto(int offset, int alignment) {
  return divideroundingup(offset, alignment) * alignment;
}

// rounds the given byte offset up to the next offset aligned such that any
// type may be stored at it.
inline int alignoffset(int offset) {
  return alignto(offset, ksafealignment);
}

#define bitsizeof(t) (sizeof(t) * 8)

}  // namespace

// ===================================================================

class dynamicmessage : public message {
 public:
  struct typeinfo {
    int size;
    int has_bits_offset;
    int unknown_fields_offset;
    int extensions_offset;

    // not owned by the typeinfo.
    dynamicmessagefactory* factory;  // the factory that created this object.
    const descriptorpool* pool;      // the factory's descriptorpool.
    const descriptor* type;          // type of this dynamicmessage.

    // warning:  the order in which the following pointers are defined is
    //   important (the prototype must be deleted *before* the offsets).
    scoped_array<int> offsets;
    scoped_ptr<const generatedmessagereflection> reflection;
    // don't use a scoped_ptr to hold the prototype: the destructor for
    // dynamicmessage needs to know whether it is the prototype, and does so by
    // looking back at this field. this would assume details about the
    // implementation of scoped_ptr.
    const dynamicmessage* prototype;

    typeinfo() : prototype(null) {}

    ~typeinfo() {
      delete prototype;
    }
  };

  dynamicmessage(const typeinfo* type_info);
  ~dynamicmessage();

  // called on the prototype after construction to initialize message fields.
  void crosslinkprototypes();

  // implements message ----------------------------------------------

  message* new() const;

  int getcachedsize() const;
  void setcachedsize(int size) const;

  metadata getmetadata() const;

 private:
  google_disallow_evil_constructors(dynamicmessage);

  inline bool is_prototype() const {
    return type_info_->prototype == this ||
           // if type_info_->prototype is null, then we must be constructing
           // the prototype now, which means we must be the prototype.
           type_info_->prototype == null;
  }

  inline void* offsettopointer(int offset) {
    return reinterpret_cast<uint8*>(this) + offset;
  }
  inline const void* offsettopointer(int offset) const {
    return reinterpret_cast<const uint8*>(this) + offset;
  }

  const typeinfo* type_info_;

  // todo(kenton):  make this an atomic<int> when c++ supports it.
  mutable int cached_byte_size_;
};

dynamicmessage::dynamicmessage(const typeinfo* type_info)
  : type_info_(type_info),
    cached_byte_size_(0) {
  // we need to call constructors for various fields manually and set
  // default values where appropriate.  we use placement new to call
  // constructors.  if you haven't heard of placement new, i suggest googling
  // it now.  we use placement new even for primitive types that don't have
  // constructors for consistency.  (in theory, placement new should be used
  // any time you are trying to convert untyped memory to typed memory, though
  // in practice that's not strictly necessary for types that don't have a
  // constructor.)

  const descriptor* descriptor = type_info_->type;

  new(offsettopointer(type_info_->unknown_fields_offset)) unknownfieldset;

  if (type_info_->extensions_offset != -1) {
    new(offsettopointer(type_info_->extensions_offset)) extensionset;
  }

  for (int i = 0; i < descriptor->field_count(); i++) {
    const fielddescriptor* field = descriptor->field(i);
    void* field_ptr = offsettopointer(type_info_->offsets[i]);
    switch (field->cpp_type()) {
#define handle_type(cpptype, type)                                           \
      case fielddescriptor::cpptype_##cpptype:                               \
        if (!field->is_repeated()) {                                         \
          new(field_ptr) type(field->default_value_##type());                \
        } else {                                                             \
          new(field_ptr) repeatedfield<type>();                              \
        }                                                                    \
        break;

      handle_type(int32 , int32 );
      handle_type(int64 , int64 );
      handle_type(uint32, uint32);
      handle_type(uint64, uint64);
      handle_type(double, double);
      handle_type(float , float );
      handle_type(bool  , bool  );
#undef handle_type

      case fielddescriptor::cpptype_enum:
        if (!field->is_repeated()) {
          new(field_ptr) int(field->default_value_enum()->number());
        } else {
          new(field_ptr) repeatedfield<int>();
        }
        break;

      case fielddescriptor::cpptype_string:
        switch (field->options().ctype()) {
          default:  // todo(kenton):  support other string reps.
          case fieldoptions::string:
            if (!field->is_repeated()) {
              if (is_prototype()) {
                new(field_ptr) const string*(&field->default_value_string());
              } else {
                string* default_value =
                  *reinterpret_cast<string* const*>(
                    type_info_->prototype->offsettopointer(
                      type_info_->offsets[i]));
                new(field_ptr) string*(default_value);
              }
            } else {
              new(field_ptr) repeatedptrfield<string>();
            }
            break;
        }
        break;

      case fielddescriptor::cpptype_message: {
        if (!field->is_repeated()) {
          new(field_ptr) message*(null);
        } else {
          new(field_ptr) repeatedptrfield<message>();
        }
        break;
      }
    }
  }
}

dynamicmessage::~dynamicmessage() {
  const descriptor* descriptor = type_info_->type;

  reinterpret_cast<unknownfieldset*>(
    offsettopointer(type_info_->unknown_fields_offset))->~unknownfieldset();

  if (type_info_->extensions_offset != -1) {
    reinterpret_cast<extensionset*>(
      offsettopointer(type_info_->extensions_offset))->~extensionset();
  }

  // we need to manually run the destructors for repeated fields and strings,
  // just as we ran their constructors in the the dynamicmessage constructor.
  // additionally, if any singular embedded messages have been allocated, we
  // need to delete them, unless we are the prototype message of this type,
  // in which case any embedded messages are other prototypes and shouldn't
  // be touched.
  for (int i = 0; i < descriptor->field_count(); i++) {
    const fielddescriptor* field = descriptor->field(i);
    void* field_ptr = offsettopointer(type_info_->offsets[i]);

    if (field->is_repeated()) {
      switch (field->cpp_type()) {
#define handle_type(uppercase, lowercase)                                     \
        case fielddescriptor::cpptype_##uppercase :                           \
          reinterpret_cast<repeatedfield<lowercase>*>(field_ptr)              \
              ->~repeatedfield<lowercase>();                                  \
          break

        handle_type( int32,  int32);
        handle_type( int64,  int64);
        handle_type(uint32, uint32);
        handle_type(uint64, uint64);
        handle_type(double, double);
        handle_type( float,  float);
        handle_type(  bool,   bool);
        handle_type(  enum,    int);
#undef handle_type

        case fielddescriptor::cpptype_string:
          switch (field->options().ctype()) {
            default:  // todo(kenton):  support other string reps.
            case fieldoptions::string:
              reinterpret_cast<repeatedptrfield<string>*>(field_ptr)
                  ->~repeatedptrfield<string>();
              break;
          }
          break;

        case fielddescriptor::cpptype_message:
          reinterpret_cast<repeatedptrfield<message>*>(field_ptr)
              ->~repeatedptrfield<message>();
          break;
      }

    } else if (field->cpp_type() == fielddescriptor::cpptype_string) {
      switch (field->options().ctype()) {
        default:  // todo(kenton):  support other string reps.
        case fieldoptions::string: {
          string* ptr = *reinterpret_cast<string**>(field_ptr);
          if (ptr != &field->default_value_string()) {
            delete ptr;
          }
          break;
        }
      }
    } else if (field->cpp_type() == fielddescriptor::cpptype_message) {
      if (!is_prototype()) {
        message* message = *reinterpret_cast<message**>(field_ptr);
        if (message != null) {
          delete message;
        }
      }
    }
  }
}

void dynamicmessage::crosslinkprototypes() {
  // this should only be called on the prototype message.
  google_check(is_prototype());

  dynamicmessagefactory* factory = type_info_->factory;
  const descriptor* descriptor = type_info_->type;

  // cross-link default messages.
  for (int i = 0; i < descriptor->field_count(); i++) {
    const fielddescriptor* field = descriptor->field(i);
    void* field_ptr = offsettopointer(type_info_->offsets[i]);

    if (field->cpp_type() == fielddescriptor::cpptype_message &&
        !field->is_repeated()) {
      // for fields with message types, we need to cross-link with the
      // prototype for the field's type.
      // for singular fields, the field is just a pointer which should
      // point to the prototype.
      *reinterpret_cast<const message**>(field_ptr) =
        factory->getprototypenolock(field->message_type());
    }
  }
}

message* dynamicmessage::new() const {
  void* new_base = operator new(type_info_->size);
  memset(new_base, 0, type_info_->size);
  return new(new_base) dynamicmessage(type_info_);
}

int dynamicmessage::getcachedsize() const {
  return cached_byte_size_;
}

void dynamicmessage::setcachedsize(int size) const {
  // this is theoretically not thread-compatible, but in practice it works
  // because if multiple threads write this simultaneously, they will be
  // writing the exact same value.
  cached_byte_size_ = size;
}

metadata dynamicmessage::getmetadata() const {
  metadata metadata;
  metadata.descriptor = type_info_->type;
  metadata.reflection = type_info_->reflection.get();
  return metadata;
}

// ===================================================================

struct dynamicmessagefactory::prototypemap {
  typedef hash_map<const descriptor*, const dynamicmessage::typeinfo*> map;
  map map_;
};

dynamicmessagefactory::dynamicmessagefactory()
  : pool_(null), delegate_to_generated_factory_(false),
    prototypes_(new prototypemap) {
}

dynamicmessagefactory::dynamicmessagefactory(const descriptorpool* pool)
  : pool_(pool), delegate_to_generated_factory_(false),
    prototypes_(new prototypemap) {
}

dynamicmessagefactory::~dynamicmessagefactory() {
  for (prototypemap::map::iterator iter = prototypes_->map_.begin();
       iter != prototypes_->map_.end(); ++iter) {
    delete iter->second;
  }
}

const message* dynamicmessagefactory::getprototype(const descriptor* type) {
  mutexlock lock(&prototypes_mutex_);
  return getprototypenolock(type);
}

const message* dynamicmessagefactory::getprototypenolock(
    const descriptor* type) {
  if (delegate_to_generated_factory_ &&
      type->file()->pool() == descriptorpool::generated_pool()) {
    return messagefactory::generated_factory()->getprototype(type);
  }

  const dynamicmessage::typeinfo** target = &prototypes_->map_[type];
  if (*target != null) {
    // already exists.
    return (*target)->prototype;
  }

  dynamicmessage::typeinfo* type_info = new dynamicmessage::typeinfo;
  *target = type_info;

  type_info->type = type;
  type_info->pool = (pool_ == null) ? type->file()->pool() : pool_;
  type_info->factory = this;

  // we need to construct all the structures passed to
  // generatedmessagereflection's constructor.  this includes:
  // - a block of memory that contains space for all the message's fields.
  // - an array of integers indicating the byte offset of each field within
  //   this block.
  // - a big bitfield containing a bit for each field indicating whether
  //   or not that field is set.

  // compute size and offsets.
  int* offsets = new int[type->field_count()];
  type_info->offsets.reset(offsets);

  // decide all field offsets by packing in order.
  // we place the dynamicmessage object itself at the beginning of the allocated
  // space.
  int size = sizeof(dynamicmessage);
  size = alignoffset(size);

  // next the has_bits, which is an array of uint32s.
  type_info->has_bits_offset = size;
  int has_bits_array_size =
    divideroundingup(type->field_count(), bitsizeof(uint32));
  size += has_bits_array_size * sizeof(uint32);
  size = alignoffset(size);

  // the extensionset, if any.
  if (type->extension_range_count() > 0) {
    type_info->extensions_offset = size;
    size += sizeof(extensionset);
    size = alignoffset(size);
  } else {
    // no extensions.
    type_info->extensions_offset = -1;
  }

  // all the fields.
  for (int i = 0; i < type->field_count(); i++) {
    // make sure field is aligned to avoid bus errors.
    int field_size = fieldspaceused(type->field(i));
    size = alignto(size, min(ksafealignment, field_size));
    offsets[i] = size;
    size += field_size;
  }

  // add the unknownfieldset to the end.
  size = alignoffset(size);
  type_info->unknown_fields_offset = size;
  size += sizeof(unknownfieldset);

  // align the final size to make sure no clever allocators think that
  // alignment is not necessary.
  size = alignoffset(size);
  type_info->size = size;

  // allocate the prototype.
  void* base = operator new(size);
  memset(base, 0, size);
  dynamicmessage* prototype = new(base) dynamicmessage(type_info);
  type_info->prototype = prototype;

  // construct the reflection object.
  type_info->reflection.reset(
    new generatedmessagereflection(
      type_info->type,
      type_info->prototype,
      type_info->offsets.get(),
      type_info->has_bits_offset,
      type_info->unknown_fields_offset,
      type_info->extensions_offset,
      type_info->pool,
      this,
      type_info->size));

  // cross link prototypes.
  prototype->crosslinkprototypes();

  return prototype;
}

}  // namespace protobuf
}  // namespace google
