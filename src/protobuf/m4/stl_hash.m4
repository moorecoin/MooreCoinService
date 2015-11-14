# we check two things: where the include file is for
# unordered_map/hash_map (we prefer the first form), and what
# namespace unordered/hash_map lives in within that include file.  we
# include ac_try_compile for all the combinations we've seen in the
# wild.  we define hash_map_h to the location of the header file, and
# hash_namespace to the namespace the class (unordered_map or
# hash_map) is in.  we define have_unordered_map if the class we found
# is named unordered_map, or leave it undefined if not.

# this also checks if unordered map exists.
ac_defun([ac_cxx_stl_hash],
  [
   ac_msg_checking(the location of hash_map)
   ac_lang_save
   ac_lang_cplusplus
   ac_cv_cxx_hash_map=""
   # first try unordered_map, but not on gcc's before 4.2 -- i've
   # seen unexplainable unordered_map bugs with -o2 on older gcc's.
   ac_try_compile([#if defined(__gnuc__) && (__gnuc__ < 4 || (__gnuc__ == 4 && __gnuc_minor__ < 2))
                   # error gcc too old for unordered_map
                   #endif
                   ],
                   [/* no program body necessary */],
                   [stl_hash_old_gcc=no],
                   [stl_hash_old_gcc=yes])
   for location in unordered_map tr1/unordered_map; do
     for namespace in std std::tr1; do
       if test -z "$ac_cv_cxx_hash_map" -a "$stl_hash_old_gcc" != yes; then
         # some older gcc's have a buggy tr1, so test a bit of code.
         ac_try_compile([#include <$location>],
                        [const ${namespace}::unordered_map<int, int> t;
                         return t.find(5) == t.end();],
                        [ac_cv_cxx_hash_map="<$location>";
                         ac_cv_cxx_hash_namespace="$namespace";
                         ac_cv_cxx_hash_map_class="unordered_map";])
       fi
     done
   done
   # now try hash_map
   for location in ext/hash_map hash_map; do
     for namespace in __gnu_cxx "" std stdext; do
       if test -z "$ac_cv_cxx_hash_map"; then
         ac_try_compile([#include <$location>],
                        [${namespace}::hash_map<int, int> t],
                        [ac_cv_cxx_hash_map="<$location>";
                         ac_cv_cxx_hash_namespace="$namespace";
                         ac_cv_cxx_hash_map_class="hash_map";])
       fi
     done
   done
   ac_cv_cxx_hash_set=`echo "$ac_cv_cxx_hash_map" | sed s/map/set/`;
   ac_cv_cxx_hash_set_class=`echo "$ac_cv_cxx_hash_map_class" | sed s/map/set/`;
   if test -n "$ac_cv_cxx_hash_map"; then
      ac_define(have_hash_map, 1, [define if the compiler has hash_map])
      ac_define(have_hash_set, 1, [define if the compiler has hash_set])
      ac_define_unquoted(hash_map_h,$ac_cv_cxx_hash_map,
                         [the location of <unordered_map> or <hash_map>])
      ac_define_unquoted(hash_set_h,$ac_cv_cxx_hash_set,
                         [the location of <unordered_set> or <hash_set>])
      ac_define_unquoted(hash_namespace,$ac_cv_cxx_hash_namespace,
                         [the namespace of hash_map/hash_set])
      ac_define_unquoted(hash_map_class,$ac_cv_cxx_hash_map_class,
                         [the name of <hash_map>])
      ac_define_unquoted(hash_set_class,$ac_cv_cxx_hash_set_class,
                         [the name of <hash_set>])
      ac_msg_result([$ac_cv_cxx_hash_map])
   else
      ac_msg_result()
      ac_msg_warn([could not find an stl hash_map])
   fi
])

