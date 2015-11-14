// copyright 2005-2008 google inc. all rights reserved.
// author: jrm@google.com (jim meehan)

#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {
namespace internal {

// these four-byte entries compactly encode how many bytes 0..255 to delete
// in making a string replacement, how many bytes to add 0..255, and the offset
// 0..64k-1 of the replacement string in remap_string.
struct remapentry {
  uint8 delete_bytes;
  uint8 add_bytes;
  uint16 bytes_offset;
};

// exit type codes for state tables. all but the first get stuffed into
// signed one-byte entries. the first is only generated by executable code.
// to distinguish from next-state entries, these must be contiguous and
// all <= kexitnone
typedef enum {
  kexitdstspacefull = 239,
  kexitillegalstructure,  // 240
  kexitok,                // 241
  kexitreject,            // ...
  kexitreplace1,
  kexitreplace2,
  kexitreplace3,
  kexitreplace21,
  kexitreplace31,
  kexitreplace32,
  kexitreplaceoffset1,
  kexitreplaceoffset2,
  kexitreplace1s0,
  kexitspecial,
  kexitdoagain,
  kexitrejectalt,
  kexitnone               // 255
} exitreason;


// this struct represents one entire state table. the three initialized byte
// areas are state_table, remap_base, and remap_string. state0 and state0_size
// give the byte offset and length within state_table of the initial state --
// table lookups are expected to start and end in this state, but for
// truncated utf-8 strings, may end in a different state. these allow a quick
// test for that condition. entry_shift is 8 for tables subscripted by a full
// byte value and 6 for space-optimized tables subscripted by only six
// significant bits in utf-8 continuation bytes.
typedef struct {
  const uint32 state0;
  const uint32 state0_size;
  const uint32 total_size;
  const int max_expand;
  const int entry_shift;
  const int bytes_per_entry;
  const uint32 losub;
  const uint32 hiadd;
  const uint8* state_table;
  const remapentry* remap_base;
  const uint8* remap_string;
  const uint8* fast_state;
} utf8statemachineobj;

typedef utf8statemachineobj utf8scanobj;

#define x__ (kexitillegalstructure)
#define rj_ (kexitreject)
#define s1_ (kexitreplace1)
#define s2_ (kexitreplace2)
#define s3_ (kexitreplace3)
#define s21 (kexitreplace21)
#define s31 (kexitreplace31)
#define s32 (kexitreplace32)
#define t1_ (kexitreplaceoffset1)
#define t2_ (kexitreplaceoffset2)
#define s11 (kexitreplace1s0)
#define sp_ (kexitspecial)
#define d__ (kexitdoagain)
#define rja (kexitrejectalt)

//  entire table has 9 state blocks of 256 entries each
static const unsigned int utf8acceptnonsurrogates_state0 = 0;     // state[0]
static const unsigned int utf8acceptnonsurrogates_state0_size = 256;  // =[1]
static const unsigned int utf8acceptnonsurrogates_total_size = 2304;
static const unsigned int utf8acceptnonsurrogates_max_expand_x4 = 0;
static const unsigned int utf8acceptnonsurrogates_shift = 8;
static const unsigned int utf8acceptnonsurrogates_bytes = 1;
static const unsigned int utf8acceptnonsurrogates_losub = 0x20202020;
static const unsigned int utf8acceptnonsurrogates_hiadd = 0x00000000;

static const uint8 utf8acceptnonsurrogates[] = {
// state[0] 0x000000 byte 1
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,

  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,

x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

x__, x__,   1,   1,   1,   1,   1,   1,    1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   1,    1,   1,   1,   1,   1,   1,   1,   1,
  2,   3,   3,   3,   3,   3,   3,   3,    3,   3,   3,   3,   3,   7,   3,   3,
  4,   5,   5,   5,   6, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

// state[1] 0x000080 byte 2 of 2
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,

x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

// state[2] 0x000000 byte 2 of 3
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
  1,   1,   1,   1,   1,   1,   1,   1,    1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   1,    1,   1,   1,   1,   1,   1,   1,   1,

x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

// state[3] 0x001000 byte 2 of 3
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

  1,   1,   1,   1,   1,   1,   1,   1,    1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   1,    1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   1,    1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   1,    1,   1,   1,   1,   1,   1,   1,   1,

x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

// state[4] 0x000000 byte 2 of 4
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
  3,   3,   3,   3,   3,   3,   3,   3,    3,   3,   3,   3,   3,   3,   3,   3,
  3,   3,   3,   3,   3,   3,   3,   3,    3,   3,   3,   3,   3,   3,   3,   3,
  3,   3,   3,   3,   3,   3,   3,   3,    3,   3,   3,   3,   3,   3,   3,   3,

x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

// state[5] 0x040000 byte 2 of 4
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

  3,   3,   3,   3,   3,   3,   3,   3,    3,   3,   3,   3,   3,   3,   3,   3,
  3,   3,   3,   3,   3,   3,   3,   3,    3,   3,   3,   3,   3,   3,   3,   3,
  3,   3,   3,   3,   3,   3,   3,   3,    3,   3,   3,   3,   3,   3,   3,   3,
  3,   3,   3,   3,   3,   3,   3,   3,    3,   3,   3,   3,   3,   3,   3,   3,

x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

// state[6] 0x100000 byte 2 of 4
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

  3,   3,   3,   3,   3,   3,   3,   3,    3,   3,   3,   3,   3,   3,   3,   3,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

// state[7] 0x00d000 byte 2 of 3
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

  1,   1,   1,   1,   1,   1,   1,   1,    1,   1,   1,   1,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   1,    1,   1,   1,   1,   1,   1,   1,   1,
  8,   8,   8,   8,   8,   8,   8,   8,    8,   8,   8,   8,   8,   8,   8,   8,
  8,   8,   8,   8,   8,   8,   8,   8,    8,   8,   8,   8,   8,   8,   8,   8,

x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

// state[8] 0x00d800 byte 3 of 3
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,

rj_, rj_, rj_, rj_, rj_, rj_, rj_, rj_,  rj_, rj_, rj_, rj_, rj_, rj_, rj_, rj_,
rj_, rj_, rj_, rj_, rj_, rj_, rj_, rj_,  rj_, rj_, rj_, rj_, rj_, rj_, rj_, rj_,
rj_, rj_, rj_, rj_, rj_, rj_, rj_, rj_,  rj_, rj_, rj_, rj_, rj_, rj_, rj_, rj_,
rj_, rj_, rj_, rj_, rj_, rj_, rj_, rj_,  rj_, rj_, rj_, rj_, rj_, rj_, rj_, rj_,

x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
x__, x__, x__, x__, x__, x__, x__, x__,  x__, x__, x__, x__, x__, x__, x__, x__,
};

// remap base[0] = (del, add, string_offset)
static const remapentry utf8acceptnonsurrogates_remap_base[] = {
{0, 0, 0} };

// remap string[0]
static const unsigned char utf8acceptnonsurrogates_remap_string[] = {
0 };

static const unsigned char utf8acceptnonsurrogates_fast[256] = {
0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,

0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,

1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,

1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
};

static const utf8scanobj utf8acceptnonsurrogates_obj = {
  utf8acceptnonsurrogates_state0,
  utf8acceptnonsurrogates_state0_size,
  utf8acceptnonsurrogates_total_size,
  utf8acceptnonsurrogates_max_expand_x4,
  utf8acceptnonsurrogates_shift,
  utf8acceptnonsurrogates_bytes,
  utf8acceptnonsurrogates_losub,
  utf8acceptnonsurrogates_hiadd,
  utf8acceptnonsurrogates,
  utf8acceptnonsurrogates_remap_base,
  utf8acceptnonsurrogates_remap_string,
  utf8acceptnonsurrogates_fast
};


#undef x__
#undef rj_
#undef s1_
#undef s2_
#undef s3_
#undef s21
#undef s31
#undef s32
#undef t1_
#undef t2_
#undef s11
#undef sp_
#undef d__
#undef rja

// return true if current tbl pointer is within state0 range
// note that unsigned compare checks both ends of range simultaneously
static inline bool instatezero(const utf8scanobj* st, const uint8* tbl) {
  const uint8* tbl0 = &st->state_table[st->state0];
  return (static_cast<uint32>(tbl - tbl0) < st->state0_size);
}

// scan a utf-8 string based on state table.
// always scan complete utf-8 characters
// set number of bytes scanned. return reason for exiting
int utf8genericscan(const utf8scanobj* st,
                    const char * str,
                    int str_length,
                    int* bytes_consumed) {
  *bytes_consumed = 0;
  if (str_length == 0) return kexitok;

  int eshift = st->entry_shift;
  const uint8* isrc = reinterpret_cast<const uint8*>(str);
  const uint8* src = isrc;
  const uint8* srclimit = isrc + str_length;
  const uint8* srclimit8 = srclimit - 7;
  const uint8* tbl_0 = &st->state_table[st->state0];

 doagain:
  // do state-table scan
  int e = 0;
  uint8 c;
  const uint8* tbl2 = &st->fast_state[0];
  const uint32 losub = st->losub;
  const uint32 hiadd = st->hiadd;
  // check initial few bytes one at a time until 8-byte aligned
  //----------------------------
  while ((((uintptr_t)src & 0x07) != 0) &&
         (src < srclimit) &&
         tbl2[src[0]] == 0) {
    src++;
  }
  if (((uintptr_t)src & 0x07) == 0) {
    // do fast for groups of 8 identity bytes.
    // this covers a lot of 7-bit ascii ~8x faster then the 1-byte loop,
    // including slowing slightly on cr/lf/ht
    //----------------------------
    while (src < srclimit8) {
      uint32 s0123 = (reinterpret_cast<const uint32 *>(src))[0];
      uint32 s4567 = (reinterpret_cast<const uint32 *>(src))[1];
      src += 8;
      // this is a fast range check for all bytes in [lowsub..0x80-hiadd)
      uint32 temp = (s0123 - losub) | (s0123 + hiadd) |
                    (s4567 - losub) | (s4567 + hiadd);
      if ((temp & 0x80808080) != 0) {
        // we typically end up here on cr/lf/ht; src was incremented
        int e0123 = (tbl2[src[-8]] | tbl2[src[-7]]) |
                    (tbl2[src[-6]] | tbl2[src[-5]]);
        if (e0123 != 0) {
          src -= 8;
          break;
        }    // exit on non-interchange
        e0123 = (tbl2[src[-4]] | tbl2[src[-3]]) |
                (tbl2[src[-2]] | tbl2[src[-1]]);
        if (e0123 != 0) {
          src -= 4;
          break;
        }    // exit on non-interchange
        // else ok, go around again
      }
    }
  }
  //----------------------------

  // byte-at-a-time scan
  //----------------------------
  const uint8* tbl = tbl_0;
  while (src < srclimit) {
    c = *src;
    e = tbl[c];
    src++;
    if (e >= kexitillegalstructure) {break;}
    tbl = &tbl_0[e << eshift];
  }
  //----------------------------


  // exit posibilities:
  //  some exit code, !state0, back up over last char
  //  some exit code, state0, back up one byte exactly
  //  source consumed, !state0, back up over partial char
  //  source consumed, state0, exit ok
  // for illegal byte in state0, avoid backup up over previous char
  // for truncated last char, back up to beginning of it

  if (e >= kexitillegalstructure) {
    // back up over exactly one byte of rejected/illegal utf-8 character
    src--;
    // back up more if needed
    if (!instatezero(st, tbl)) {
      do {
        src--;
      } while ((src > isrc) && ((src[0] & 0xc0) == 0x80));
    }
  } else if (!instatezero(st, tbl)) {
    // back up over truncated utf-8 character
    e = kexitillegalstructure;
    do {
      src--;
    } while ((src > isrc) && ((src[0] & 0xc0) == 0x80));
  } else {
    // normal termination, source fully consumed
    e = kexitok;
  }

  if (e == kexitdoagain) {
    // loop back up to the fast scan
    goto doagain;
  }

  *bytes_consumed = src - isrc;
  return e;
}

int utf8genericscanfastascii(const utf8scanobj* st,
                    const char * str,
                    int str_length,
                    int* bytes_consumed) {
  *bytes_consumed = 0;
  if (str_length == 0) return kexitok;

  const uint8* isrc =  reinterpret_cast<const uint8*>(str);
  const uint8* src = isrc;
  const uint8* srclimit = isrc + str_length;
  const uint8* srclimit8 = srclimit - 7;
  int n;
  int rest_consumed;
  int exit_reason;
  do {
    // check initial few bytes one at a time until 8-byte aligned
    while ((((uintptr_t)src & 0x07) != 0) &&
           (src < srclimit) && (src[0] < 0x80)) {
      src++;
    }
    if (((uintptr_t)src & 0x07) == 0) {
      while ((src < srclimit8) &&
             (((reinterpret_cast<const uint32*>(src)[0] |
                reinterpret_cast<const uint32*>(src)[1]) & 0x80808080) == 0)) {
        src += 8;
      }
    }
    while ((src < srclimit) && (src[0] < 0x80)) {
      src++;
    }
    // run state table on the rest
    n = src - isrc;
    exit_reason = utf8genericscan(st, str + n, str_length - n, &rest_consumed);
    src += rest_consumed;
  } while ( exit_reason == kexitdoagain );

  *bytes_consumed = src - isrc;
  return exit_reason;
}

// hack:  on some compilers the static tables are initialized at startup.
//   we can't use them until they are initialized.  however, some protocol
//   buffer parsing happens at static init time and may try to validate
//   utf-8 strings.  since utf-8 validation is only used for debugging
//   anyway, we simply always return success if initialization hasn't
//   occurred yet.
namespace {

bool module_initialized_ = false;

struct initdetector {
  initdetector() {
    module_initialized_ = true;
  }
};
initdetector init_detector;

}  // namespace

bool isstructurallyvalidutf8(const char* buf, int len) {
  if (!module_initialized_) return true;
  
  int bytes_consumed = 0;
  utf8genericscanfastascii(&utf8acceptnonsurrogates_obj,
                           buf, len, &bytes_consumed);
  return (bytes_consumed == len);
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google