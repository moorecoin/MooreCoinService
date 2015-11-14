/* protobuf config.h for msvc.  on other platforms, this is generated
 * automatically by autoheader / autoconf / configure. */

/* the location of <hash_map> */
#define hash_map_h <hash_map>

/* the namespace of hash_map/hash_set */
// apparently microsoft decided to move hash_map *back* to the std namespace
// in msvc 2010:
//   http://blogs.msdn.com/vcblog/archive/2009/05/25/stl-breaking-changes-in-visual-studio-2010-beta-1.aspx
// todo(kenton):  use unordered_map instead, which is available in msvc 2010.
#if _msc_ver < 1310 || _msc_ver >= 1600
#define hash_namespace std
#else
#define hash_namespace stdext
#endif

/* the location of <hash_set> */
#define hash_set_h <hash_set>

/* define if the compiler has hash_map */
#define have_hash_map 1

/* define if the compiler has hash_set */
#define have_hash_set 1

/* define if you want to use zlib.  see readme.txt for additional
 * requirements. */
// #define have_zlib 1
